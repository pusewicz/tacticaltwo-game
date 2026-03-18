# Domain Pitfalls: C Game Engine Refactoring

**Domain:** C23 game engine refactoring — hot-reloadable shared library, fat-struct entity system, multi-pass render pipeline
**Researched:** 2026-03-18
**Confidence:** HIGH (all pitfalls grounded in codebase-specific evidence or verified community sources)

---

## Critical Pitfalls

Mistakes that cause crashes, undefined behavior, or silent regressions requiring rewrites.

---

### Pitfall 1: Changing GameState/World Struct Layout During Hot Reload

**What goes wrong:** Adding, removing, or reordering fields in `GameState` or `World` (or any struct they embed — `Entity`, `LightingState`, `RainState`, etc.) invalidates the live pointer returned by `game_state()`. The host holds a raw pointer to the old memory layout. When `game_hot_reload()` restores it, the new code reads fields at wrong offsets. The result is silent data corruption, incorrect values, or crashes. No compiler warning is emitted because C struct layout mismatch across library boundaries is undefined behavior.

**Why it happens:** The hot-reload handshake in `game.c:game_hot_reload()` simply casts a raw `void*` back to `GameState*`. There is zero validation that the memory layout at that pointer still matches the new library's definition. This is the known-hardest limitation of shared-library hot reload: struct layout changes are invisible to the reload mechanism.

**Consequences:**
- Silent data corruption: entity positions wrong, lighting parameters scrambled, rain state invalid
- Crash if a new field reads past the old allocation's bounds
- Heisenbug: manifests frames after reload, not immediately
- Particularly dangerous for `World` because it contains 4096 × `Entity` (fat struct) — one field insertion shifts the entire array

**Prevention:**
- Add a `uint32_t state_version` magic/checksum as the first field of `GameState`. On `game_hot_reload()`, compare new `sizeof(GameState)` against expected. If different, trigger full reset (call `game_shutdown()` + `game_init()`) rather than blindly restoring old pointer.
- Never reorder existing fields in `GameState`, `World`, or `Entity` during incremental development. Append-only additions are safer; even then, validate size.
- Provide a `game_state_size()` export that returns `sizeof(GameState)`. Host checks this on every reload before restoring pointer.
- Consider a simple XOR checksum of all field offsets as a `GAME_STATE_LAYOUT_VERSION` compile-time constant compared on reload.

**Detection:** Implement a size comparison guard in `game_hot_reload()`. Log a `log_error` if `received_size != sizeof(GameState)`. Without this, the only symptom is weird values in the debug overlay after save.

**Phase:** Improve hot-reload robustness phase (architecture audit)

---

### Pitfall 2: Dangling Coroutine Pointer After Library Reload

**What goes wrong:** `PlayerStateComp.co` is a `CF_Coroutine` value stored inside the `Entity` fat struct, which lives in `World.entities[]` in the persistent `GameState`. CF coroutines contain function pointers to the coroutine body — which lives in the old `.dylib` address space. After a reload, the old library is unloaded (`dlclose`), those code pages are gone. If the coroutine resumes (even just once before `world_hot_reload()` destroys it), execution jumps to a freed text segment. This is a segfault or silent memory corruption.

**Why it happens:** `world_hot_reload()` does call `cf_coroutine_destroy()` on stale coroutines, but only if the coroutine is in the right state. There is a race: if `game_hot_reload()` is called mid-frame (between update and render) or during a player animation transition, the coroutine may be in a suspended state that `world_hot_reload()` doesn't enumerate correctly.

**Consequences:**
- Segfault when coroutine resumes with a stale function pointer
- Undefined behavior if `cf_coroutine_destroy()` is missed for any entity
- State loss: player animation frame, firing sequence, reload timer all lost on every hot-reload

**Prevention:**
- In `world_hot_reload()`, iterate the full `entities[]` array (not just `player`) and destroy any `CF_Coroutine` where `.co` is non-null and `player_state.enabled` is true. Use `cf_coroutine_state()` to detect SUSPENDED vs. DEAD.
- Treat `game_hot_reload()` as a mandatory safe point: all active coroutines must be destroyed synchronously before `world_hot_reload()` returns.
- Flag coroutine-driven state as "should reinitialize after reload" — on next `sys_player_coroutine()` call, detect null/dead coroutine and re-create it from the entity's `current` state enum.
- Verify: after a hot-reload, trigger a coroutine immediately (fire/crouch) and confirm no crash.

**Detection:** Enable address sanitizer (`-fsanitize=address`) during development builds. A dangling-pointer resume will trip ASAN immediately instead of silently corrupting. Add a `CF_ASSERT(cf_coroutine_state(co) == CF_COROUTINE_STATE_DEAD)` at the end of `world_hot_reload()`.

**Phase:** Hot-reload robustness phase

---

### Pitfall 3: Coordinate Conversion Divergence After Extracting Shared Module

**What goes wrong:** The coordinate conversion functions (`cf_x_to_grid`, `cf_y_to_grid`, `grid_cell_aabb`) are duplicated identically in `physics_system.c` and `render_system.c`. When extracted into `coords.h`, there is a window where one system is patched to use the shared version and the other is not. During that window, any edit to the shared version affects physics but not rendering, or vice versa. This produces collision mesh/visual position mismatches that look like floating entities or bullets hitting invisible walls — and do not trigger any compiler error.

**Why it happens:** C does not enforce that two copies of the same logic stay in sync. The refactor requires a cut-over that is easy to do incompletely. If the PR is split or partially applied, the divergence can persist silently.

**Consequences:**
- Player collider visually misaligned from sprite (render uses old formula, physics uses new)
- Debug collider overlay draws at different position than actual collision box
- Regression is purely visual/behavioral — builds succeed, game runs, but a coordinate edge case produces wrong behavior

**Prevention:**
- Do the extraction atomically: one commit that adds `src/game/coords.h`, removes both duplicate definitions, and replaces all call sites. No partial states.
- Immediately after, verify the debug collider overlay (F key + `debug_mode`) still precisely overlaps the sprite at level edges and tile boundaries.
- Write an assertion: `CF_ASSERT(cf_x_to_grid(known_x, level_w) == expected_col)` as a startup sanity check if no formal unit tests exist yet.
- Coordinate conversion is the highest-risk extraction because errors are spatially visible but numerically subtle.

**Detection:** After any coordinate refactor, run the game, walk the player to a right-side level wall, and confirm: (a) player stops at the correct pixel, (b) debug collider box aligns with sprite, (c) player does not clip through floor/ceiling tiles.

**Phase:** Coordinate extraction phase

---

### Pitfall 4: Render Pipeline Projection State Left Dirty Between Passes

**What goes wrong:** The 3-pass render pipeline in `game.c:game_render()` changes the CF projection matrix multiple times (Pass 1 uses canvas ortho, Pass 3 uses window ortho, then restores canvas ortho). If a refactor adds a new sub-pass, restructures the pass ordering, or fails to call `cf_draw_projection()` to restore state, subsequent frames render with the wrong projection. The result is sprites drawn at scaled/offset positions, pillarboxing applied to the wrong canvas, or the fluence multiply pass blitting at incorrect dimensions.

**Why it happens:** CF projection is global mutable state. There is no stack-based save/restore for the projection matrix (unlike `cf_draw_push_render_state()`). Any refactor that moves pass boundaries or adds early-return paths can leave projection dirty.

**Consequences:**
- Sprites rendered at wrong scale or offset — visually obvious but may look like a "feature" at first glance
- Game canvas blit offset (black bars appear on wrong side)
- Composite/debug canvas uses wrong ortho, ImGui game viewport appears stretched
- Difficult to debug because the error manifests visually, not as a crash

**Prevention:**
- Add a comment at every `cf_draw_projection()` call documenting the intended state it restores (e.g., `// restore canvas ortho for next frame`).
- Consider wrapping each pass in a helper that saves/restores projection explicitly, similar to how `cf_draw_push_render_state()` / `cf_draw_pop_render_state()` works.
- When adding new passes or restructuring, always check every exit path (including early-return on minimized window) leaves projection in the expected state for the next frame.
- The current `ensure_composite_canvas()` early-return in debug mode skips the projection restore — verify this path after any pass-restructuring refactor.

**Detection:** Check with window resizing: after each render refactor, resize the window while the game runs. If projection is left dirty, pillarbox/letterbox proportions will be wrong or sprites will visually jump.

**Phase:** Render pipeline refactor phase

---

### Pitfall 5: CF_Canvas Handle Use-After-Destroy (Composite Canvas Lifecycle)

**What goes wrong:** `state->composite_canvas` is destroyed and re-created in `ensure_composite_canvas()` when the window resizes. `state->composite_w` guards the destroy: it is only called if `composite_w > 0`. However, after `cf_destroy_canvas()`, the handle stored in `state->composite_canvas` is invalid. If any code path reads `state->composite_canvas` or calls `cf_canvas_get_target(state->composite_canvas)` between the destroy and the re-create (including from `game_update`'s ImGui block, which runs before `game_render`), it will use a stale handle. CF canvas handles are opaque integers, so this does not crash immediately — it silently samples garbage or a recycled texture.

**Why it happens:** `game_update()` reads `state->composite_canvas` via `cf_canvas_get_target()` to feed the ImGui image widget (line 139 in `game.c`). `game_render()` may call `ensure_composite_canvas()` which destroys it. If window resizes during frame, update reads old handle before render recreates it.

**Consequences:**
- ImGui game viewport shows garbage frame during resize
- In edge cases (recycled handles), wrong texture displayed without error
- Not a crash on most frames, easy to miss in testing

**Prevention:**
- Invalidate the ImGui image display when `composite_w == 0` or when the canvas handle has just been destroyed. The current `if (state->composite_w > 0)` guard in `game_update` helps but relies on the dimensions being zeroed atomically with destruction.
- Set `state->composite_w = 0` immediately in `ensure_composite_canvas()` after `cf_destroy_canvas()` and before `cf_make_canvas()`. This ensures any concurrent access (if update and render order ever changes) sees "not created" rather than stale handle.
- Document the lifecycle contract in a comment: "composite_canvas is invalid when composite_w == 0; never call cf_canvas_get_target on an invalid canvas."

**Detection:** Rapidly resize window while game runs in debug mode. Watch for frame tears in the ImGui game viewport or console log errors from CF's backend.

**Phase:** Render pipeline refactor phase

---

## Moderate Pitfalls

---

### Pitfall 6: Active Entity List Refactor Breaks System Ordering Invariants

**What goes wrong:** Adding a free-list or active-entity index to skip unused slots in all systems seems straightforward, but systems have implicit ordering invariants. For example, `sys_player_coroutine()` may create a new entity (e.g., a projectile) mid-iteration. If the active-entity list is built once per frame before system dispatch, the new entity is not in the list and is not processed by subsequent systems in the same frame. With the current full-array scan, the new entity IS found because it's just a slot that became `exists=true`.

**Why it happens:** The active-entity list is a snapshot. Mid-frame entity creation is invisible to it. The full-scan approach implicitly handles late additions because it reads live state every iteration.

**Consequences:**
- New entities are skipped for one frame (missed input, physics, animation tick)
- Visible as a one-frame "ghost" at spawn position
- Harder to reproduce: only triggered when entities are created mid-frame

**Prevention:**
- When adding active-entity tracking, decide: rebuild list once per frame before all systems (snapshot semantics) or maintain it dynamically via `world_add_entity()`/`world_remove_entity()`. Snapshot is simpler and safe if no mid-frame spawning occurs. Document this assumption.
- Audit all systems for any `world_add_entity()` calls before switching to snapshot semantics.
- Currently, only `make_player_at()` creates entities (at init/level load). If that stays true, snapshot is safe. If projectiles or enemies are added, revisit.

**Detection:** After refactor, verify a freshly spawned entity (level reload → player spawn) is visible and collidable on the first rendered frame.

**Phase:** Entity system iteration refactor phase

---

### Pitfall 7: LDtk Level Reload Race with Entity Coroutine State

**What goes wrong:** `init_world()` / `ldtk_spawn_entities()` destroys all entity state and re-spawns from scratch. If a player coroutine is mid-animation (e.g., mid-fire sequence), `world_remove_entity()` zeros the entity slot but the coroutine stack allocated inside `CF_Coroutine` is either leaked or double-freed. The player is then re-spawned at the LDtk spawn point with a fresh coroutine, losing position, facing direction, animation state, and any active effect timers.

**Why it happens:** `world_remove_entity()` calls `memset` to zero the slot. It does not call `cf_coroutine_destroy()` first. CF coroutine state (including stack memory) may not be cleaned up.

**Consequences:**
- Memory leak if `CF_Coroutine` holds heap-allocated stack
- Potentially re-entering a destroyed coroutine if old pointer is not zeroed before new entity reuses the same slot
- Player position jumps to spawn on every level reload, even hot-reload

**Prevention:**
- `world_remove_entity()` must explicitly destroy the coroutine before zeroing: check `player_state.enabled` and `player_state.co` and call `cf_coroutine_destroy()` if the coroutine is alive.
- Add a `entity_cleanup(Entity* e)` helper that handles all resource-owning fields (coroutines, sprites via `cf_destroy_sprite`) before the slot is zeroed. Call it from both `world_remove_entity()` and `shutdown_world()`.
- This is a prerequisite for the entity-iteration refactor: a correct remove function must exist before building a free-list on top of it.

**Detection:** Enable ASAN. Level-reload with an active coroutine running. Any leak or double-free will trip immediately.

**Phase:** Entity lifecycle correctness pass (prerequisite for iteration refactor)

---

### Pitfall 8: Partial Coordinate Module Migration Produces Aliased Inconsistency

**What goes wrong:** When extracting coordinate helpers to `coords.h`, it is tempting to include the header in physics first and defer render_system. During that window, if the shared version changes its formula (e.g., rounding `floorf` → `truncf`, or adjusting the Y-axis flip), physics is updated but debug-render still uses the old in-file formula. The debug collider overlay then disagrees with actual collision, making the debug tool misleading rather than helpful.

**Why it happens:** Incremental refactoring feels safer. But with pure coordinate math, the "safe" partial state is actually more dangerous than the status quo — at least duplicate code was consistent.

**Prevention:**
- As stated in Pitfall 3: do the extraction atomically in one commit.
- Use `static_assert` or a comment-guarded `#error` to fail compilation if both the old definition and the new header are included simultaneously. This makes partial migration impossible to ship.

**Detection:** Grepping for `cf_x_to_grid` or `cf_y_to_grid` in any `.c` file after the extraction commit should return zero results (only the header definition counts).

**Phase:** Coordinate extraction phase

---

### Pitfall 9: Silent Asset Load Failures Become Silent Render Failures

**What goes wrong:** Parallax sprites, player sprites, and LDtk tile layer PNGs are loaded with CF sprite/asset APIs that log a warning on failure but return a zero-initialized `CF_Sprite`. Systems then call `cf_draw_sprite()` on a sprite with a null or zero texture handle. CF may silently skip the draw, render a white square, or access invalid memory depending on its internal handle validation.

**Why it happens:** The codebase's error strategy is "log and continue." This is survivable at runtime but conceals the failure. A refactor that restructures asset loading paths (e.g., adding a cache layer) can inadvertently introduce a new silent failure path where the cache returns a default-initialized entry on a miss.

**Consequences:**
- Blank parallax layer with no error during development
- After adding a caching layer: stale cached-null entries persist, asset appears perpetually missing even after the file is fixed
- Asset load failures never surface to the developer during refactor iteration

**Prevention:**
- Add explicit null/zero checks after every asset load: `CF_ASSERT(cf_sprite_id(sprite) != 0)` in development builds (NDEBUG strips it for release).
- When building a caching layer, distinguish between "not yet loaded", "loaded successfully", and "failed to load" — never cache a failure state as a valid entry.
- Log asset failures at `log_error` level (not just `log_warn`) so they appear prominently during development.

**Detection:** Deliberately rename a parallax PNG and run the game. The system should log an error and skip the draw. If no error appears or the game crashes rather than skipping, the error handling is insufficient.

**Phase:** Asset pipeline refactor phase

---

### Pitfall 10: Lighting Compute Caching Breaks Dynamic Light Correctness

**What goes wrong:** The current absorption map is rebuilt every frame from `state->world.map` and the camera position (see `game.c:game_render()` Pass 2). A refactoring to cache this map "when the tilemap hasn't changed" must account for the camera offset. The absorption map is camera-relative (it covers only what's visible at the current camera position). Caching by tilemap-dirty-flag alone misses camera movement — a cached absorption map from last frame's camera position will misplace occlusion, causing lights to bleed through walls that are now in a different screen position.

**Why it happens:** The comment in `game.c` line 276 explicitly says "Rebuild absorption each frame with current camera so it stays registered with the emissivity pass." A refactor that reads "rebuild if dirty" without understanding this comment will cache incorrectly.

**Consequences:**
- Light bleeds through walls when camera moves
- Static light halos shift visually when scrolling
- Only apparent with camera movement, easy to miss in desk-testing where the player stands still

**Prevention:**
- Cache key must include camera position (or camera delta). Cache is only valid when `camera == cached_camera`.
- Or: cache the absorption map in level-space (not camera-space) and only regenerate when `map->loaded` changes. Then let `lighting_build_absorption()` crop to camera-space at the point of use.
- The safest first pass is to cache only when camera is stationary for N frames — this avoids the correctness issue while still reducing cost in combat.

**Detection:** After any lighting cache is added, walk the player continuously while watching the lighting debug mode. Lights near level edges should not bleed or shift relative to tiles during scrolling.

**Phase:** Lighting optimization sub-phase (within render pipeline refactor)

---

## Minor Pitfalls

---

### Pitfall 11: Projection Matrix Restoration Omitted in Early-Return Paths

**What goes wrong:** `game_render()` has an early return for the minimized window case (when `ensure_composite_canvas()` returns false in debug mode, or when `window_w <= 0`). The current code does not call `cf_draw_projection()` to restore the canvas projection in these paths. For most frames this is irrelevant (minimized windows skip rendering entirely), but if CF's projection state is global and persists across frames, the next non-minimized frame may render with whatever projection was left by a previous partial frame.

**Prevention:** Add an explicit projection restore (`cf_draw_projection(cf_ortho_2d(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT))`) at every early-return path in `game_render()`, or restructure to ensure projection is set at the start of each frame unconditionally.

**Detection:** Minimize then restore window quickly while game runs. Watch for one-frame distortion on restore.

**Phase:** Render pipeline refactor phase

---

### Pitfall 12: snprintf Path Buffer Overflow in ldtk.c

**What goes wrong:** `build_path()` in `ldtk.c` (lines 45-48, 130-131) uses `snprintf` into a fixed-size buffer. The `LDTK_MAX_PATH` limit is not visible in the header. If a refactor extracts `build_path()` to a shared utility, or if future level structures use longer relative paths, the buffer silently truncates the path. `snprintf` returns the number of bytes that would have been written, not an error — and the code does not check the return value.

**Prevention:** Check `snprintf` return value: if it equals or exceeds the buffer size, log an error and abort the load rather than using a truncated path that silently fails to resolve.

**Detection:** Create a level with a long path name (e.g., nested directories) and confirm the load either succeeds or errors explicitly rather than silently returning 0 bytes.

**Phase:** Asset pipeline robustness pass

---

### Pitfall 13: Incremental Refactor Leaves Naming Inconsistency

**What goes wrong:** If a refactor renames concepts (e.g., "canvas" → "render target", or "world" → "scene") in some files but not others, the codebase ends up with aliased terminology. This is a maintenance tax, not a bug — but it actively slows down the next developer (including the author 3 months later) and conflicts with the goal of a clean foundation for the next feature wave.

**Prevention:** Any naming refactor must go all the way in a single commit. Use `grep` to find all instances before declaring the rename done. Use a working-branch approach: rename all, verify build, merge.

**Detection:** After each phase, run a search for the old names. Zero matches = complete.

**Phase:** All phases (hygiene concern throughout)

---

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| GameState struct changes | Layout mismatch on hot-reload (Pitfall 1) | Add size validation in `game_hot_reload()`; treat size change as full reset |
| Hot-reload coroutine handling | Dangling coroutine pointer (Pitfall 2) | Enumerate all entities with live coroutines; destroy all before returning from `world_hot_reload()` |
| Coordinate extraction to coords.h | Divergence window / partial migration (Pitfalls 3, 8) | Atomic commit; no partial states; grep to verify clean cut-over |
| Render pipeline restructure | Projection state left dirty (Pitfall 4) | Document projection state at each pass boundary; test on window resize |
| Composite canvas resize | Use-after-destroy on handle (Pitfall 5) | Set `composite_w = 0` immediately after `cf_destroy_canvas()`; guard every read |
| Entity iteration refactor | Active-list snapshot misses mid-frame spawns (Pitfall 6) | Audit for `world_add_entity()` in system dispatch; document snapshot semantics |
| Entity remove/level reload | Coroutine leak on `world_remove_entity()` (Pitfall 7) | Add `entity_cleanup()` helper; call before zeroing slot |
| Asset caching layer | Silent stale failure cached as valid (Pitfall 9) | Three-state cache: unloaded / ok / failed; never cache failure as hit |
| Lighting caching | Camera-relative absorption map incorrectly cached (Pitfall 10) | Include camera position in cache key, or restructure to level-space map |

---

## Sources

- Community documentation: [Interactive Programming in C — nullprogram.com](https://nullprogram.com/blog/2014/12/23/) — struct layout and function pointer issues with dlopen hot reload (HIGH confidence)
- Community documentation: [Exile: Hot Reloading — thenumb.at](https://thenumb.at/Hot-Reloading-in-Exile/) — struct size comparison as full-reset trigger (HIGH confidence via multiple corroborating sources)
- Library: [cr.h: A Simple C Hot Reload Header-only Library — fungos/cr](https://github.com/fungos/cr) — state migration patterns, size-based invalidation (MEDIUM confidence)
- Community: [Hot Reloading in C — bytesbeneath.com](https://www.bytesbeneath.com/p/hot-reloading-in-c) — function pointer invalidation post-reload (MEDIUM confidence)
- Community issue: [GDScript coroutine resumption crashes after script hot reload #24684 — godotengine/godot](https://github.com/godotengine/godot/issues/24684) — coroutine pointer crash on reload is a documented pattern across engines (MEDIUM confidence, different language but same underlying cause)
- Codebase analysis: `.planning/codebase/CONCERNS.md`, `.planning/codebase/ARCHITECTURE.md`, `src/game/game.c`, `src/game/world.c`, `src/game/world.h`, `src/engine/game_state.h`, `src/game/systems/physics_system.c` — all HIGH confidence (direct inspection)
