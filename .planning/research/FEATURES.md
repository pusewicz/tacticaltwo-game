# Feature Landscape

**Domain:** C game engine codebase audit and refactoring
**Researched:** 2026-03-18
**Confidence:** HIGH (based on direct source code analysis, not external research)

## Context

This is a refactoring milestone, not a feature-building one. "Features" here are
refactoring outcomes — what must be improved (table stakes), what would provide
meaningful leverage (differentiators), and what traps to avoid (anti-features).

The codebase is a C23 side-scrolling platformer (~750–900 LOC of game logic across
13 source files) built on Cute Framework with hot-reloading. It is currently at an
early-feature stage: one player entity, one level, no enemies. The entity system
supports 4096 slots; at present 1–2 entities are active at any time.

---

## Table Stakes

Fixes that **block future development** or introduce active risk of corruption,
undefined behavior, or silent failures. These are not optional cleanup — they are
correctness and maintainability issues that compound if left in place.

| Refactoring | Why Required | Complexity | Evidence |
|-------------|--------------|------------|----------|
| Extract coordinate conversion module | `cf_x_to_grid`, `cf_y_to_grid`, `grid_cell_aabb` are identically copy-pasted into `physics_system.c` (lines 38–61) and `render_system.c` (lines 20–41) with a `debug_` prefix. Any coordinate bug requires a two-place fix. | Low — extract 3 functions to `src/game/coords.h` | Direct source read: identical bodies confirmed |
| Add active entity list / generation counter | Every system iterates all 4096 slots on every frame. With 1–2 active entities this wastes ~4094 loop iterations per system call (~6 systems = ~24K wasted checks/frame). This scales linearly with every new system added. Adding enemies or projectiles compounds the cost. | Medium — maintain a compact active-index array alongside the fat-struct array | `world.c:world_add_entity`, all `systems/*.c` iterate `MAX_ENTITIES` |
| Guard parallax null-sprite render path | `parallax.loaded` is only set to the result of the *last* load, not the conjunction of all three. If `layer1` or `sky` fails silently, `draw_layer()` calls `cf_draw_sprite()` with a zero-initialized sprite struct. This is a latent null-pointer/UB. | Low — check individual load results; set `px->loaded` per-sprite or add null guard in `draw_layer()` | `world.c:128–153`, `parallax_system.c:draw_layer` |
| Fix composite canvas zero-size guard | `ensure_composite_canvas()` already returns false on `w/h <= 0`, but the caller path in `game_render()` (`PASS 3`) calls `cf_app_set_canvas_size(window_w, window_h)` before the guard check, and proceeds to `cf_draw_canvas()` in the else branch unconditionally even with a zero-size window. Minimized window will call CF draw functions with 0×0 framebuffer. | Low — early return in `game_render` pass 3 when window dimensions are zero | `game.c:309–356` |
| Bounds-check LDtk CSV parser | `parse_csv()` does a two-pass count, then allocates `rows * cols * sizeof(int)`, then writes to `grid[idx]` without checking that `idx < rows * cols`. Trailing comma handling in the first pass can inflate col/row count mismatch, putting writes one cell past allocation. | Medium — add `idx < total` assertion before write; separately validate parsed dimensions match JSON-reported grid size | `ldtk.c:50–124` |
| Unified error path for asset load failures | Individual asset load failures log a warning but continue, leaving partially initialized structs active. Hot-reload can re-enter `init_world()` with stale pointers. Need explicit per-asset validity checks and a fail-fast mechanism during init. | Low — add `CF_ASSERT` or early return + log_error on critical asset load paths | `world.c:128–154`, `rain.c:24–80` |
| Rain arena allocation null check | `rain_init()` allocates two `w*h*4` byte buffers from a 4MB scratch arena without checking the returned pointer for null. If the arena is exhausted or the grid is large, the allocation fails silently and subsequent writes corrupt adjacent memory or crash. | Low — check pointer after `cf_arena_alloc()`; log error and disable rain if allocation fails | `rain.c:24–80` |

---

## Differentiators

Improvements that **meaningfully accelerate future feature development** — not bugs,
but structural improvements that pay off when adding enemies, projectiles, audio
triggers, cutscenes, etc.

| Refactoring | Value Proposition | Complexity | Notes |
|-------------|-------------------|------------|-------|
| Hot-reload state layout validation | Adding a magic number + `sizeof(GameState)` check in `game_hot_reload()` would catch struct layout changes before they produce silent corruption. Currently a new field added to `Entity` causes silent stale reads. Required before `Entity` grows new components. | Low — add version constant + size assert to `game_hot_reload` and `world_hot_reload` | `game.c:374–377`, `world.c:205–216` |
| Decouple PASS 3 composite logic into a helper | `game_render()` has two near-identical 15-line blocks (debug path and normal path) that both call `cf_draw_canvas` + `multiply_blend_state` + `cf_draw_canvas(lighting_fluence)`. Extracting `composite_scene_to(CF_Canvas target, int w, int h)` removes the duplication and makes adding a post-process pass (e.g., vignette, screen-space effects) a single-site change. | Low — pure refactor, no behavior change | `game.c:316–356` |
| Lighting dirty flag / static light cache | `lighting_build_absorption()` and `lighting_compute()` run every frame unconditionally. With static geometry and no dynamic lights, the fluence output is identical frame-to-frame. A dirty flag (`lighting_dirty`) set on camera move, light change, or tilemap reload would skip the compute pipeline on static frames. This directly unblocks turning HRC on in more scenes without frame budget concern. | Medium — add dirty tracking to `LightingState`; condition `lighting_build_absorption` + `lighting_compute` on the flag | `game.c:270–307`, `lighting.c` |
| Parallax asset caching across world reloads | `init_world()` calls `cf_make_easy_sprite_from_png()` for all three parallax assets on every world reload. Because `init_world()` is called on hot-reload and level transitions, the same PNG is decoded from disk repeatedly. Moving parallax sprites into `GameState` (outside `World`) would persist them across hot-reloads, eliminating disk reads on reload. | Low — move `Parallax` from `World` to `GameState`, guard load behind a `loaded` flag | `game_state.h`, `world.c:128–154` |
| Explicit system ordering documentation | `update_world()` dispatches 6 systems in a fixed order (`input → player_coroutine → muzzle_flash → movement → velocity → collision → camera`). This order is load-bearing (velocity must precede collision; input must precede movement). Adding a new system requires knowing the correct insertion point. A comment block in `world.c` documenting the ordering contract would prevent integration errors when adding AI or projectile systems. | Trivial — comment only | `world.c:166–184` |
| LDtk entity field schema validation | `ldtk_spawn_entities()` reads `Color`, `Intensity`, `Radius`, `Direction`, `ConeAngle` from custom fields with fallback-to-default silently. When a level designer renames a field in LDtk, lights silently spawn at default intensity. A `log_warn` on field-not-found would surface the mismatch during development. | Low — add presence check before field read; warn on missing expected fields | `ldtk.c:410–550` |
| `muzzle_flash` default initialization moved to `world_trigger_muzzle_flash` | Currently `game_update()` (ImGui block) guards `if (mf->duration == 0.0f)` to set defaults, and `world_trigger_muzzle_flash()` also guards `if (mf->duration == 0.0f)`. Defaults are split across two call sites. Consolidate initialization into `world_trigger_muzzle_flash()` or into a `muzzle_flash_init()` called from `init_world()`. | Trivial — single consolidation | `game.c:232–237`, `world.c:94–99` |

---

## Anti-Features

Refactoring traps to **deliberately avoid** — changes that look tempting but
introduce complexity, destabilize the codebase, or solve non-problems at this scale.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| Full ECS library migration (flecs, etc.) | The fat-struct system is intentionally simple and matches the game's scale (1–20 entities). A full ECS migration would break the `GameState` hot-reload invariant, require rewriting all 9 systems, and add an external dependency. The CONCERNS audit flags fat-struct memory overhead but with 4096 × 184 bytes = ~750 KB it is well within budget. | Maintain fat-struct; add active entity tracking for iteration efficiency only |
| Data-oriented component splitting into parallel arrays (SoA) | Cache locality gains from SoA become measurable only above ~1000 active entities. With a maximum realistic entity count of <50 in this game style, the iteration improvement is unmeasurable and the refactor would require rewriting all system iteration patterns and the hot-reload state extraction. | Add an active-index array (see Table Stakes) to skip empty slots; leave struct layout unchanged |
| Runtime entity type dispatch (vtable / function pointer components) | Adding virtual dispatch to entity systems trades simple flat iteration for pointer indirection. C vtables in game entities are notoriously cache-hostile and add debugging friction. The coroutine-based behavior system already provides behavioral polymorphism cleanly. | Use coroutines for behavioral variation; keep system dispatch as typed functions |
| Replacing the scratch arena with a general allocator | The scratch arena provides deterministic frame-memory behavior. Replacing it with `malloc`/`free` pairs or a pool allocator would introduce fragmentation, leak risk, and complicate the hot-reload memory picture. CF's arena is purpose-fit for this pattern. | Keep scratch arena; fix the rain allocation null-check instead |
| Breaking `GameState` across a DLL boundary with versioned structs | Adding explicit struct versioning with a migration table is attractive for long-term safety but excessive for a single-developer project where the DLL and host are always rebuilt together. The lightweight approach (size assert + magic number) is sufficient. | Use magic number + sizeof assert in `game_hot_reload` (see Differentiators) |
| Extracting a "pure" render module from `game.c` into a separate compilation unit | `game_render()` is 100 lines. The 3-pass structure is already well-commented. Splitting it into a separate file adds include complexity without reducing comprehension burden at this size. | Extract the composite helper function; leave `game_render` in `game.c` |
| Adding a unit test suite this milestone | The project explicitly scopes out test suite creation. C game engine testing requires custom harnesses or headless CF mode — non-trivial setup that would consume most of the milestone budget. The table-stakes fixes directly address the highest-risk paths without tests. | Document invariants in comments; defer test suite to its own milestone |

---

## Feature Dependencies

Refactoring ordering constraints — some fixes enable or require others.

```
Coordinate module extraction (coords.h)
  → Required before: active entity list (systems must update to use it too)
  → Enables: coordinate logic appears in one place when expanding to multi-level

Active entity tracking
  → Depends on: none (self-contained world.c change)
  → Enables: performance headroom for projectile/enemy systems
  → Must not break: world_add_entity / world_remove_entity API

Hot-reload layout validation
  → Depends on: none
  → Must be added BEFORE any Entity struct expansion (new components for enemies etc.)

Parallax asset caching (move to GameState)
  → Depends on: none
  → Requires: GameState struct change → triggers hot-reload layout validation

Lighting dirty flag
  → Depends on: none
  → Enables: lighting-on-by-default without frame budget concern

Fix composite canvas zero-size guard
  → Depends on: none
  → Should be done before: any work touching PASS 3 path

CSV parser bounds check
  → Depends on: none
  → Should be done before: any multi-level work
```

---

## MVP Recommendation

For this refactoring milestone, prioritize in order:

**Phase A — Correctness (do first, lowest risk, highest payoff):**
1. Extract coordinate module (`src/game/coords.h`) — 1 file, removes duplication
2. Parallax null-sprite guard — 3-line fix, removes latent UB
3. Composite canvas zero-size guard — 2-line fix, closes crash path
4. Rain arena null check — 3-line fix, closes corruption path
5. LDtk CSV parser bounds check — 10-line fix, closes OOB write

**Phase B — Architecture (medium effort, enables feature development):**
6. Active entity list — adds compact index array to `World`, updates all systems
7. Hot-reload layout validation — adds magic number + sizeof check
8. Decouple PASS 3 composite helper — extract `composite_scene_to()` function

**Phase C — Developer Velocity (low-risk polish):**
9. Parallax asset caching (move to GameState)
10. Lighting dirty flag
11. Muzzle flash default consolidation
12. System ordering comment block
13. LDtk field schema warnings

**Defer:**
- Full ECS migration: not justified at this entity count
- Unit test suite: separate milestone per project scope
- Audio, save/load, entity-entity collision: explicitly out of scope

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Table stakes identification | HIGH | Directly confirmed from source code reads — no inference required |
| Complexity estimates | HIGH | All are contained refactors within files already read in full |
| Differentiator value | MEDIUM | Value claims depend on future feature trajectory; plausible given scope in PROJECT.md |
| Anti-feature exclusions | HIGH | Based on entity count, codebase size, and explicit PROJECT.md out-of-scope statements |

---

## Sources

All findings are based on direct source analysis (HIGH confidence):

- `/Users/piotr/Work/GitHub/pusewicz/tacticaltwo-game/src/game/game.c` — render pipeline, hot-reload entry points
- `/Users/piotr/Work/GitHub/pusewicz/tacticaltwo-game/src/game/world.c` — entity lifecycle, system dispatch order, init_world
- `/Users/piotr/Work/GitHub/pusewicz/tacticaltwo-game/src/game/world.h` — Entity fat struct layout, World struct, MAX_ENTITIES
- `/Users/piotr/Work/GitHub/pusewicz/tacticaltwo-game/src/game/systems/physics_system.c` — coordinate helpers, full-array iteration
- `/Users/piotr/Work/GitHub/pusewicz/tacticaltwo-game/src/game/systems/render_system.c` — duplicated coordinate helpers confirmed
- `/Users/piotr/Work/GitHub/pusewicz/tacticaltwo-game/src/game/systems/animation_system.c` — coroutine behavior pattern
- `/Users/piotr/Work/GitHub/pusewicz/tacticaltwo-game/src/game/systems/parallax_system.c` — hardcoded image dimensions
- `/Users/piotr/Work/GitHub/pusewicz/tacticaltwo-game/src/game/systems/player_system.c` — MAX_ENTITIES iteration pattern
- `/Users/piotr/Work/GitHub/pusewicz/tacticaltwo-game/src/game/systems/camera_system.c` — player index access pattern
- `/Users/piotr/Work/GitHub/pusewicz/tacticaltwo-game/src/game/ldtk.c` (lines 1–124) — CSV parser structure
- `/Users/piotr/Work/GitHub/pusewicz/tacticaltwo-game/src/engine/game_state.h` — GameState struct layout
- `/Users/piotr/Work/GitHub/pusewicz/tacticaltwo-game/.planning/codebase/CONCERNS.md` — prior audit findings
- `/Users/piotr/Work/GitHub/pusewicz/tacticaltwo-game/.planning/codebase/ARCHITECTURE.md` — layering analysis
- `/Users/piotr/Work/GitHub/pusewicz/tacticaltwo-game/.planning/PROJECT.md` — scope boundaries and constraints
