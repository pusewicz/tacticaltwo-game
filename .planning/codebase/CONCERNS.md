# Codebase Concerns

**Analysis Date:** 2026-03-18

## Tech Debt

**Fixed-size entity array (4096 max entities):**
- Issue: Entity management uses a monolithic fixed array (`Entity entities[MAX_ENTITIES]`) with O(n) linear scan lookup on `world_add_entity()`. No entity pooling or free-list optimization.
- Files: `src/game/world.h`, `src/game/world.c`
- Impact: Adding/removing entities scales linearly; all systems iterate full array regardless of actual entity count. New entity lookups will slow as entity count grows.
- Fix approach: Implement a free-list or bitfield to track occupied slots, or switch to sparse entity ID allocation scheme.

**Fat struct entity components:**
- Issue: All possible component data embedded in single `Entity` struct (184+ bytes per entity) regardless of which components are active. Components marked with `enabled` flags but memory still allocated.
- Files: `src/game/world.h` (lines 94-104)
- Impact: Memory overhead for unused components; cache misses during iteration; difficult to add new component types without growing struct.
- Fix approach: For > 100 active entities, consider splitting into sparse component store or data-oriented layout. Currently acceptable for 4096-entity ceiling but fragile if features multiply components.

**No explicit state versioning for hot-reload:**
- Issue: Hot-reload in `world.c::world_hot_reload()` only destroys stale coroutines; no mechanism to version or migrate entity state across library reloads. Non-coroutine entity state persists through reload.
- Files: `src/game/world.c` (lines 205-216), `src/game/game.c` (line 114)
- Impact: If entity update logic changes between reloads, stale state can produce undefined behavior. No validation that persisted structs match new layout.
- Fix approach: Add generation counter or checksum validation to detect layout changes; provide explicit entity state migration hook.

**Coordinate system duplication in render and physics:**
- Issue: CF↔LDtk coordinate conversion duplicated identically between `physics_system.c` and `render_system.c` (lines 20-60 in each). Three inline functions replicated.
- Files: `src/game/systems/physics_system.c` (lines 38-60), `src/game/systems/render_system.c` (lines 20-40)
- Impact: Coordinate bug fix requires patching two places; maintainability risk.
- Fix approach: Extract `cf_x_to_grid()`, `cf_y_to_grid()`, `grid_cell_aabb()` to shared header or separate coordinate module in `src/game/coords.h`.

**LDtk int_grid CSV parsing lacks robustness:**
- Issue: Custom CSV parser in `ldtk.c::parse_csv()` (lines 50-124) handles trailing commas, missing newlines, but no validation of expected dimensions or bounds checking during parse.
- Files: `src/game/ldtk.c` (lines 50-124)
- Impact: Malformed CSV could allocate wrong grid size and write out-of-bounds; no early bounds check on `grid[idx]` assignment (line 110).
- Fix approach: Add explicit dimension header parsing, pre-allocate with verified size, bounds-check before write.

---

## Performance Bottlenecks

**All-entity iteration on every system:**
- Problem: Every frame, 6+ systems iterate all 4096 entity slots even if only 1-10 entities exist. `sys_apply_velocity()`, `sys_collide_tilemap()`, `sys_render_sprites()`, animation, input, player all scan full array.
- Files: `src/game/world.c`, `src/game/systems/*.c`
- Cause: No active entity list or spatial partitioning; filter only by `exists` and component `enabled` flags.
- Improvement path: Maintain active entity list (simple linked list or generation counter array) to skip 4086 unused slots per system.

**O(n²) tilemap collision checks per entity:**
- Problem: `sys_collide_tilemap()` queries up to 2x2 tile grid cells per entity per entity, iterating each. At 20x20 grid, that's 400 cells per collision entity.
- Files: `src/game/systems/physics_system.c` (lines 67-164)
- Cause: Brute-force AABB against all solid tiles within entity bounds.
- Improvement path: Spatial hash or quadtree for collision queries; or pre-compute collision mesh.

**Lighting compute shader every frame:**
- Problem: HRC compute pipeline runs every frame even with static scene. Involves multiple shader passes + storage buffer reads/writes at LIGHTING_WORLD_SIZE² resolution.
- Files: `src/game/lighting.c`, `src/game/game.c` (lines 270-277)
- Cause: No caching of static light output; absorption map rebuilt from int_grid every frame even if camera/tilemap unchanged.
- Improvement path: Cache absorption map when tilemap unchanged; defer lighting compute to 30fps or on-change; or reduce grid size in non-combat scenes.

**Parallax sprite loads on every level init:**
- Problem: Three parallax layer sprites loaded from PNG each time `init_world()` called (lines 128-154 in `world.c`). No caching between level reloads or across sessions.
- Files: `src/game/world.c` (lines 128-154)
- Cause: Sprites loaded fresh each world init; no persistent asset cache.
- Improvement path: Cache parallax sprites in `GameState` or use LDtk layer sprites directly.

---

## Known Bugs

**SDL_GetSystemPageSize() not available pre-SDL 3.4.0:**
- Symptoms: Hard-coded `4096` page size; will fail on systems with different page size (rare but possible on exotic platforms).
- Files: `src/platform/platform_cute.c` (line 77, TODO comment)
- Trigger: Run on non-4096-pagesize system or use newer SDL 3.4.0 that has the function.
- Workaround: Current hard-code works on x86/ARM (Linux/macOS/Windows).

**Parallax sprite load can fail silently:**
- Symptoms: If any parallax PNG fails to load, `px->loaded` is marked false, but render code doesn't guard against null sprites.
- Files: `src/game/world.c` (lines 128-154), `src/game/systems/parallax_system.c`
- Trigger: Missing or corrupted asset PNG.
- Workaround: None; renders null sprite. Should skip render if load failed.

**Entity coroutine lifecycle during level reload:**
- Symptoms: If level changes mid-coroutine, `world_hot_reload()` destroys coroutine, but `ldtk_spawn_entities()` creates new entities without migrating state. Player coroutine state lost.
- Files: `src/game/world.c` (lines 205-216), `src/game/ldtk.c` (lines 410-550)
- Trigger: Rapid level transitions or hot-reload during player animation.
- Workaround: Coroutine re-initialized on next frame, but state (animation frame, etc.) lost.

---

## Fragile Areas

**Rain texture generation uses scratch arena without bounds checking:**
- Files: `src/game/rain.c` (lines 24-80)
- Why fragile: Two large buffers allocated from scratch arena (`w * h * 4` bytes each). If scratch arena is small or fragmented, allocation could fail silently or return nullptr.
- Safe modification: Check returned pointer from `cf_arena_alloc()` before use; or validate arena has enough space before allocating.
- Test coverage: No test for rain init with various grid sizes.

**Lighting state canvases can be zero-sized if window minimized:**
- Files: `src/game/game.c` (lines 68-89, composite canvas creation)
- Why fragile: `ensure_composite_canvas()` creates canvas with `w, h` from `cf_app_get_width/height()`. If window minimized (w/h = 0), canvas creation may fail or behave undefined.
- Safe modification: Return false early if w/h ≤ 0; caller should skip render pass.
- Test coverage: No test for minimized window render path.

**LDtk entity spawn assumes fixed custom fields:**
- Files: `src/game/ldtk.c` (lines 410-550, `ldtk_spawn_entities()`)
- Why fragile: Reads Color, Intensity, Radius, Direction, ConeAngle from LDtk custom fields with no schema validation. If LDtk export changes field names or types, defaults silently apply.
- Safe modification: Add field schema validation or warn when expected fields missing.
- Test coverage: Only tested with known LDtk export structure.

**Physics coordinate conversion assumes level dimensions are known:**
- Files: `src/game/systems/physics_system.c` (lines 38-61)
- Why fragile: `cf_x_to_grid()` and friends assume `level_width`, `level_height` passed are valid. No bounds checking; invalid dimensions cause wrong cell lookups.
- Safe modification: Assert or validate level dimensions before collision checks.
- Test coverage: No unit test for coordinate conversion with edge cases.

---

## Security Considerations

**Dynamic lighting shader compilation every frame (potential DoS):**
- Risk: If shader directory is writable by untrusted code, shader hot-reload could compile malicious compute shaders.
- Files: `src/game/lighting.c` (line 69-75, `lighting_load_shader()`), `src/game/game.c` (line 105)
- Current mitigation: Shader files are read-only in release build; only hot-reloaded in development.
- Recommendations: In development, restrict shader directory to signed/validated files. Disable hot-reload in shipping builds.

**File path construction uses fixed buffers with snprintf:**
- Risk: If LDtk level names or paths contain very long strings, `snprintf()` in `build_path()` could overflow.
- Files: `src/game/ldtk.c` (lines 45-48, 130-131)
- Current mitigation: Uses `LDTK_MAX_PATH` (assumed 256 or 512), but not exposed in header. Paths hardcoded to reasonable lengths.
- Recommendations: Define `LDTK_MAX_PATH` clearly; validate asset paths against whitelist in production.

**Interned strings without deduplication limit:**
- Risk: `cf_sintern()` interns level identifiers (in `ldtk.c` line 143). If LDtk export has unbounded unique strings, string pool could bloat.
- Files: `src/game/ldtk.c` (line 143)
- Current mitigation: Only interns level identifiers (bounded by level count). Cute Framework's string intern likely has a reasonable pool.
- Recommendations: Monitor string pool size in debug mode; cap number of levels.

---

## Scaling Limits

**Entity limit of 4096 is architectural ceiling:**
- Current capacity: 4096 entities max
- Limit: At ~184 bytes per entity (fat struct), that's ~750 KB for entity array alone. 6+ systems iterating all 4096 every frame = cache thrashing.
- Scaling path: (1) Sparse entity store (entities only where used), (2) Spatial partitioning for systems (only iterate nearby entities), or (3) Split into multiple smaller arrays by component type.

**Lighting HRC grid resolution is tunable but expensive:**
- Current capacity: Grid sizes 64, 128, 256, 512 configurable; default 512
- Limit: At 512x512 grid with compute shaders, lighting compute time dominates frame budget. Multiple shader passes × 4 frustums = GPU load.
- Scaling path: (1) Cap grid to 128-256 for real-time play, (2) Offload to background thread, or (3) Cache static lighting output.

**LDtk level count limited to `LDTK_MAX_LEVELS` (hardcoded):**
- Current capacity: Appears to be 16 or 32 (not found in header, inferred from array allocation).
- Limit: Fixed array in `LdtkMap` struct.
- Scaling path: Dynamically allocate level array based on export; or stream levels on-demand.

**Rain particle simulation uses 2D collision grid:**
- Current capacity: O(grid_width) height map + O(grid_width * grid_height) collision mask
- Limit: Large level grid (e.g., 512x512) = 2 MB collision mask. Multiple splash particles per frame = CPU cost for visibility checks.
- Scaling path: Use sparse occlusion culling or coarser collision grid.

---

## Missing Critical Features

**No save/load game state:**
- Problem: No mechanism to persist player position, level progress, or dynamic state between sessions.
- Blocks: Cannot create checkpoint/progress system; no continue functionality.

**No pauseable game loop:**
- Problem: Game updates unconditionally; no pause state.
- Blocks: Cannot implement pause menu or cutscenes.

**No audio system:**
- Problem: No sound engine or mixing; all audio placeholders.
- Blocks: Cannot add music, SFX, or audio feedback.

**No input rebinding:**
- Problem: Input hardcoded to keyboard; no gamepad support, no rebinding UI.
- Blocks: Console/gamepad ports; accessibility for alternative input devices.

**No collision resolution for entities (only vs. tilemap):**
- Problem: No entity-entity collision; only collider vs. solid tiles.
- Blocks: Enemy AI, multi-player, projectile interactions.

---

## Test Coverage Gaps

**No unit tests for coordinate system:**
- What's not tested: CF ↔ LDtk coordinate conversion edge cases (negative coords, level boundaries, etc.)
- Files: `src/game/systems/physics_system.c` (lines 38-61)
- Risk: Coordinate bugs silently cause collision misses or soft-clipping issues.
- Priority: **High** — affects both rendering and physics correctness.

**No integration tests for LDtk level loading:**
- What's not tested: Full load pipeline (JSON parse → PNG load → CSV parse → entity spawn) with real LDtk export; error paths (missing PNG, malformed CSV, invalid entity fields).
- Files: `src/game/ldtk.c` (full file)
- Risk: Silent failures on malformed exports; missing assets cause null-pointer dereferences.
- Priority: **High** — blocks level authoring confidence.

**No tests for rain system initialization:**
- What's not tested: Grid resize logic (line 49-59), arena allocation failure, or collision mask correctness.
- Files: `src/game/rain.c` (lines 24-80)
- Risk: Arena allocation could fail without detection; collision mask could be wrong size.
- Priority: **Medium** — rarely changes but fragile.

**No tests for hot-reload entity lifecycle:**
- What's not tested: Entity coroutine destruction during reload, entity state persistence, dynamic light re-add.
- Files: `src/game/world.c` (lines 205-216), `src/game/game.c` (render pass).
- Risk: Stale state could cause crashes or undefined behavior on hot-reload during gameplay.
- Priority: **Medium** — development-only but dangerous during iteration.

**No tests for physics integration edge cases:**
- What's not tested: Multiple simultaneous collisions (multi-tile overlap), collision with level boundaries, grounded flag correctness on ceiling/wall hits.
- Files: `src/game/systems/physics_system.c` (lines 117-163)
- Risk: Physics could behave unexpectedly at level edges or in tight spaces.
- Priority: **Medium** — affects gameplay feel.

**No tests for composite canvas lifecycle:**
- What's not tested: Window resize during render, minimized window (zero size), composite canvas destruction on shutdown.
- Files: `src/game/game.c` (lines 68-89, 259-320)
- Risk: Null dereference or resource leak if window resized while rendering.
- Priority: **Low** — rare in testing but should be verified.

---

*Concerns audit: 2026-03-18*
