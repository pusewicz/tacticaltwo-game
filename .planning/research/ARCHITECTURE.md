# Architecture Patterns

**Domain:** C23 side-scrolling tactical platformer with hot-reloading
**Researched:** 2026-03-18
**Confidence:** HIGH (based on direct codebase analysis + verified patterns)

---

## Current Architecture Assessment

The codebase has a sound layered structure with a genuine hot-reload split between host executable and game library. The core design decisions (fat struct entities, fixed array, system dispatch) are appropriate for a solo-dev game at this scale. The issues are not structural misdesign — they are organic growth artifacts: duplicated code, missing module boundaries within `src/game/`, and a few lifecycle sequencing problems in the render pipeline.

The goal for refactoring is **extraction and clarification**, not replacement.

---

## Recommended Architecture

```
src/
├── app/               Host executable — unchanged
│   └── main.c
├── platform/          CF integration — unchanged
│   └── platform_cute.{h,c}
├── engine/            Survives hot reload — expand slightly
│   ├── game_state.h   GameState struct (version-tagged)
│   ├── log.{h,c}
│   ├── asset.{h,c}
│   └── coords.{h,c}   [NEW] Coordinate conversion utilities
├── config/
│   └── config.h
└── game/              Hot-reloadable library
    ├── game.{h,c}     Lifecycle + render pipeline coordinator
    ├── world.{h,c}    Entity storage + system dispatch
    ├── entity.h       [EXTRACT] Entity/component type definitions
    ├── ldtk.{h,c}     Level loader — unchanged interface
    ├── lighting.{h,c} HRC GI — unchanged interface
    ├── rain.{h,c}     Rain effect — unchanged interface
    └── systems/
        ├── systems.h
        ├── input_system.c
        ├── player_system.c
        ├── physics_system.c   (drops coordinate helpers after extraction)
        ├── render_system.c    (drops coordinate helpers after extraction)
        ├── collision_system.c
        ├── camera_system.c
        ├── animation_system.c
        ├── parallax_system.c
        └── tilemap_system.c
```

---

## Component Boundaries

| Component | Responsibility | Communicates With | Notes |
|-----------|---------------|-------------------|-------|
| `app/main.c` | Bootstrap, hot-reload loop | `platform/` | Never include game headers |
| `platform/` | CF init, dlopen/dlsym, frame timing | `engine/` | Owns `GameLibrary` struct |
| `engine/game_state.h` | `GameState` definition with version tag | Everything | Must be layout-stable across hot reloads |
| `engine/coords.{h,c}` | CF↔LDtk coordinate conversion | Any consumer | Single source of truth for conversions |
| `game/game.c` | Lifecycle entry points, render pipeline | `world.h`, subsystems | Pass structs explicitly to render passes |
| `game/world.{h,c}` | Entity storage, system dispatch | `systems/`, subsystems | No render logic here |
| `game/entity.h` | `Entity`, component structs, `PlayerState` enum | `world.h`, systems | No function bodies |
| `game/systems/*.c` | Pure iteration — no global init/shutdown | `entity.h`, `coords.h` | Each file = one concern |
| `game/ldtk.{h,c}` | Level loading, entity spawning | `coords.h` (optional) | Self-contained |
| `game/lighting.{h,c}` | HRC compute pipeline | `ldtk.h` for absorption | Self-contained |
| `game/rain.{h,c}` | Rain shader + splash | World state via `state->` | Self-contained |

---

## Pattern 1: Coordinate Module Extraction

**What:** Move `cf_x_to_grid`, `cf_y_to_grid`, `grid_cell_aabb` into a shared `engine/coords.{h,c}` (or `game/coords.{h,c}`) module with a clean public interface.

**Why:** These three functions are currently copy-pasted verbatim between `physics_system.c` and `render_system.c` with only a `debug_` prefix difference. Any future consumer (AI pathfinding, enemy spawning, HUD) would need to copy them again.

**Interface:**

```c
// engine/coords.h
#pragma once
#include <cute_math.h>

// CF world X -> LDtk grid column
int coords_cf_x_to_grid(float cf_x, int level_width);

// CF world Y -> LDtk grid row
int coords_cf_y_to_grid(float cf_y, int level_height);

// LDtk grid cell -> CF world AABB (center + half-extents)
CF_Aabb coords_grid_cell_aabb(int gx, int gy, int level_width, int level_height);
```

**Placement decision:** Put in `engine/` if you anticipate any non-game consumer (unlikely) or in `game/` as `game/coords.{h,c}` if it will only ever be used inside the game library. For hot-reload reasons, prefer `engine/` so the coordinate logic is in the host-linked layer — but in practice, since these are pure math with no state, either location works. Recommendation: `src/game/coords.{h,c}` because the coordinate system is entirely game-domain knowledge (LDtk grid size is a game constant, not platform knowledge).

**Confidence:** HIGH

---

## Pattern 2: Entity High-Water Mark for Active Scan

**What:** Track `world.entity_count` (highest occupied index + 1) to bound system iteration instead of scanning all 4096 slots.

**Current state:** Every system iterates `for (int i = 0; i < MAX_ENTITIES; i++)`. With 4096 slots and ~10 active entities, this is 409× wasted iteration per system call per frame.

**How to do it safely:**

```c
// In World struct — add:
int entity_high_water; // One past the highest-ever-used index

// In world_add_entity():
int world_add_entity(Entity e) {
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (!state->world.entities[i].exists) {
            state->world.entities[i] = e;
            if (i + 1 > state->world.entity_high_water) {
                state->world.entity_high_water = i + 1;
            }
            return i;
        }
    }
    return ENTITY_NONE;
}

// In all systems — change loop bound:
for (int i = 0; i < state->world.entity_high_water; i++) { ... }
```

**Why not a packed active list?** A packed list requires maintaining a separate array and updating it on every `world_add_entity`/`world_remove_entity`. The high-water mark gives ~90% of the benefit with almost no code change and zero risk of index invalidation bugs during system iteration. With ≤100 entities (the scale of this game), the high-water approach is fine.

**Hot-reload safety:** `entity_high_water` is a plain `int` field added to `World`. Adding fields to `World` or `GameState` changes the struct layout, which means the game must be fully restarted (not just hot-reloaded) when this field is added. Document this explicitly in a `// HOT-RELOAD: restart required on layout change` comment in `game_state.h`.

**Confidence:** HIGH

---

## Pattern 3: Entity Type Field

**What:** Add an `EntityType` enum to the `Entity` struct to distinguish player, enemy, projectile, pickup, etc. without checking which component set is enabled.

**Current state:** There is no way to ask "is this entity a player?" without checking `e->player_input.enabled`. This works today with one entity type but becomes unmaintainable when enemies arrive.

```c
// In entity.h
typedef enum EntityType {
    ENTITY_TYPE_NONE = 0,
    ENTITY_TYPE_PLAYER,
    ENTITY_TYPE_ENEMY,        // future
    ENTITY_TYPE_PROJECTILE,   // future
} EntityType;

typedef struct Entity {
    bool exists;
    EntityType type;           // [ADD]
    // ...components...
} Entity;
```

**Why now:** This is a one-field addition that prevents a category of "which entities does this system apply to?" confusion in every future system. The cost is minimal; the benefit compounds with every new entity type.

**Confidence:** HIGH

---

## Pattern 4: Render Pipeline — Explicit Pass Structs

**What:** Extract the 3-pass render pipeline in `game_render()` into named functions with explicit parameter structs, so each pass has a clear contract.

**Current state:** `game_render()` is 100 lines of inline code with duplicated normal-mode/debug-mode blocks that share most logic. The debug branch and normal branch both call `calculate_dest_size()`, set up projection, draw the canvas, draw the fluence with multiply blend, and restore projection — with copy-paste divergence.

**Recommended refactoring:**

```c
// Internal to game.c

typedef struct RenderPassParams {
    CF_Canvas game_canvas;
    CF_Canvas fluence_canvas;
    CF_Canvas dest_canvas;   // null = screen framebuffer
    int dest_w;
    int dest_h;
} RenderPassParams;

static void pass_game_scene(CF_Canvas canvas);
static void pass_lighting_compute(LightingState* lt, World* world);
static void pass_composite(RenderPassParams p);
```

This collapses the debug/normal duplication: both modes call `pass_composite()` with a different `dest_canvas`. The `ensure_composite_canvas()` call stays in `game_render()` as setup before compositing.

**Key insight:** The duplication exists because debug mode renders to `composite_canvas` while normal mode renders to screen, but the compositing math is identical. Parameterize the destination, not the logic.

**Confidence:** HIGH

---

## Pattern 5: Render Pass 2 — Decouple Light Collection from Compute

**What:** Separate "collect lights from world state" from "run HRC compute" in `game_render()`.

**Current state:** Pass 2 does both: it reads `lights_static`, reads `muzzle_flash`, calls `lighting_begin_frame()`, calls `lighting_add_light()` for each source, then calls `lighting_compute()`. The muzzle flash intensity curve math (lines 290-304 in `game.c`) is inline in the render function.

**Recommended refactoring:**

```c
// In world.h or lighting.h — new function:
void world_collect_lights(World* world, LightingState* lt);
```

This function handles:
- Calling `lighting_begin_frame()`
- Adding static lights
- Evaluating the muzzle flash curve and adding its dynamic light if active

`game_render()` then calls:
```c
world_collect_lights(&state->world, &state->world.lighting);
lighting_compute(&state->world.lighting, cam.x, cam.y);
```

**Why:** The muzzle flash light-curve math is game logic (what color/intensity at what timer value), not render coordination. Putting it in `game.c`'s render function mixes concerns. It belongs alongside `update_muzzle_flash()` in `world.c`.

**Confidence:** HIGH

---

## Pattern 6: GameState Version Tag for Hot-Reload Safety

**What:** Add a compile-time version constant to `GameState` that the host checks after reload to detect layout mismatches.

**Current state:** Hot reload restores `state` by passing the saved pointer back to the new library via `game_hot_reload(void* game_state)`. If `GameState` layout changed between builds, the pointer is silently used with the wrong layout, causing crashes or corrupted data with no diagnostic.

**Recommended implementation:**

```c
// In engine/game_state.h

// Increment this whenever GameState (or any embedded struct) layout changes.
// Host checks this after hot reload; mismatch forces full restart.
#define GAME_STATE_VERSION 1

typedef struct GameState {
    uint32_t version;          // [ADD] Must be first field. Set in game_init().
    Platform* platform;
    CF_Arena* scratch_arena;
    // ...
} GameState;
```

In `game_init()`:
```c
state->version = GAME_STATE_VERSION;
```

In the platform hot-reload code (or `game_hot_reload`):
```c
GameState* s = (GameState*)saved_state;
if (s->version != GAME_STATE_VERSION) {
    log_error("platform", "GameState layout changed — restart required");
    // Trigger graceful shutdown instead of reload
}
```

**Trade-off:** This requires the host executable to know `GAME_STATE_VERSION`, so `engine/game_state.h` must be included in both host and library. It already is (via `platform_cute.h` or direct include), so this is feasible. The version check only catches the case where you remembered to bump the version — it does not auto-detect layout changes. For a solo dev, this is sufficient; the pattern is discipline, not automation.

**Confidence:** MEDIUM (pattern is sound; exact integration point with platform code needs verification against `platform_cute.h` structure)

---

## Pattern 7: LDtk Subsystem — Surface Error Propagation

**What:** Change `ldtk_load()` failures from silent-default-position fallback to explicit, logged errors with `CF_Result`-style returns throughout.

**Current state:** `init_world()` handles `ldtk_load()` failure by silently spawning at `(0,0)`. Individual layer/entity parse failures inside `ldtk.c` are not surfaced. Rain's `rain_init()` can fail silently if no `LdtkMap` is loaded. The `Parallax` loading sets `px->loaded` only if the *last* layer loaded successfully — if `sky` loads but `layer2` fails, `loaded` is false and all parallax is skipped.

**Recommended fix:** Each subsystem that can fail should return a result code, and `init_world()` should log a clear diagnostic for each failure:

```c
// ldtk.h — already returns bool from ldtk_load(), good
// rain.h — add return value:
bool rain_init(void);  // returns false if height map build failed

// init_world() — add per-asset diagnostic:
if (!ldtk_load(...)) {
    log_error("world", "Fatal: LDtk load failed. Check assets/ldtk/map/simplified/");
    make_player_at(0.0f, 0.0f);
}

// Parallax — track per-layer load status:
px->sky_loaded    = !cf_is_error(sky_result);
px->layer1_loaded = !cf_is_error(l1_result);
px->layer2_loaded = !cf_is_error(l2_result);
px->loaded        = true;  // struct is initialized, use per-layer flags in render
```

**Confidence:** HIGH

---

## Refactoring Order

The following order respects dependencies and keeps the game compilable and runnable after each step.

### Step 1: Extract Coordinate Module (no behavior change)
**Files:** Create `src/game/coords.{h,c}`. Update `physics_system.c` and `render_system.c` to use it.
**Risk:** Zero — pure extraction. No logic changes.
**Hot-reload impact:** None — new .c file in game library, no state changes.
**Validates:** Duplication eliminated, all systems share same conversion math.

### Step 2: Add Entity Type Field
**Files:** `src/game/entity.h` (or `world.h`), `src/game/world.c` (player factory).
**Risk:** Low — adding a field to `Entity` struct.
**Hot-reload impact:** `Entity` is inside `World` which is inside `GameState`. Adding a field changes `GameState` layout. This step requires a full restart after the build — document in commit message.
**Validates:** Foundation for future entity type discrimination in systems.

### Step 3: Add GameState Version Tag
**Files:** `src/engine/game_state.h`, `src/game/game.c`, `src/platform/platform_cute.c`.
**Risk:** Low — additive. New `version` field must be first field in struct (forces restart).
**Hot-reload impact:** Layout change — requires full restart once. After that, version checking prevents silent corrupt-state scenarios.
**Validates:** Hot-reload safety mechanism in place before further GameState changes.

### Step 4: Entity High-Water Mark
**Files:** `src/game/world.h` (add `entity_high_water` to `World`), `src/game/world.c` (`world_add_entity`), all `systems/*.c` (change loop bound).
**Risk:** Low — change loop bound is mechanical. `entity_high_water` starts at 0 (via `memset` in `init_world`), so systems correctly iterate nothing until entities are added.
**Hot-reload impact:** `World` struct layout changes (field added) → GameState layout changes → full restart required. Bump `GAME_STATE_VERSION`.

### Step 5: Extract `world_collect_lights()`
**Files:** `src/game/world.{h,c}` (new function), `src/game/game.c` (call it).
**Risk:** Low — move muzzle flash math out of `game_render()` into `world.c`.
**Hot-reload impact:** None — no struct changes.
**Validates:** Pass 2 coordination logic is game-domain code, not render coordinator code.

### Step 6: Refactor Pass 3 Composite (de-duplicate debug/normal paths)
**Files:** `src/game/game.c` only.
**Risk:** Medium — visual regression risk. Test both debug mode (G key) and normal mode after change.
**Hot-reload impact:** None — no struct changes.
**Validates:** Pass 3 logic is unified; debug mode is a parameter, not a fork.

### Step 7: Improve Error Propagation in Asset Loading
**Files:** `src/game/world.c` (`init_world`), `src/game/rain.{h,c}` (add return value), `src/game/ldtk.c` (surface inner errors).
**Risk:** Low — additive error logging, no logic changes.
**Hot-reload impact:** None.
**Validates:** Silent failures become diagnosable.

---

## Anti-Patterns to Avoid

### Anti-Pattern 1: Splitting `World` into Multiple Subsystem Structs
**What:** Breaking `World` into `PhysicsWorld`, `RenderWorld`, etc. to "reduce coupling."
**Why bad:** `World` is already the right granularity. Splitting it would require either passing many structs to every system function or creating a container struct that points to all of them — which is just `World` again with extra indirection. The fat struct approach is intentional and correct for a solo-dev game.
**Instead:** Keep `World` monolithic. Improve the *types* within it (extract `entity.h`), not the struct itself.

### Anti-Pattern 2: Adding a System Registration Table
**What:** A `systems[]` array of function pointers that `update_world()` iterates.
**Why bad:** Function pointers in `GameState` break hot-reload — the pointers from the old library become stale after reload, requiring careful re-registration. The current approach (explicit calls in `update_world()`) is simpler, more debuggable, and safe for hot-reload. The cost is that adding a system requires editing `world.c` — this is a feature, not a bug. It makes the system dispatch visible.
**Instead:** Keep explicit dispatch in `update_world()` and `render_world()`.

### Anti-Pattern 3: Coroutine Pointer Stored in `GameState`
**What:** Keeping `CF_Coroutine` inside `Entity.player_state` with `co.id` checked on hot-reload.
**Why bad:** This is exactly how it already works and it's correct — but the implicit contract needs documentation. `CF_Coroutine.id != 0` means "live coroutine from the old library" and `world_hot_reload()` destroys it. Any new coroutine-using component must follow the same destroy-on-reload pattern.
**Instead:** Document the pattern explicitly in `world.h` with a comment: `// Components with CF_Coroutine fields must be destroyed in world_hot_reload()`.

### Anti-Pattern 4: Moving Subsystems Out of `src/game/` Into `src/engine/`
**What:** Moving `lighting`, `rain`, or `ldtk` to `src/engine/` because they "feel like engine features."
**Why bad:** `src/engine/` contains code that is compiled into *both* the host executable and the game library — or at minimum, types that both share. LDtk, lighting, and rain are game library code that lives inside `GameState`. Moving them to `engine/` would either create a circular dependency or require duplicating the types in the hot-reload interface. They belong in `src/game/`.
**Instead:** `src/engine/` should only contain: `game_state.h`, `log.h`, `platform.h`, `asset.{h,c}`, and the new `coords.{h,c}`. Nothing that holds GPU resources or large state.

### Anti-Pattern 5: Eager Static Light Dirty-Flagging
**What:** Adding a `lights_dirty` bool to skip `lighting_build_absorption()` when the camera hasn't moved.
**Why bad:** The absorption texture is camera-relative. The camera moves every frame when the player walks. A dirty flag would only help for completely static scenes. The more impactful optimization is the lighting grid resolution (already tunable via ImGui) and moving to `trace_levels < n` for static lights. Don't add complexity for a case that almost never occurs.
**Instead:** Trust the HRC cascade configuration (grid, trace_levels) to control compute cost. Leave absorption as unconditional per-frame.

---

## Render Pipeline — Clarified Pass Contract

After refactoring, each pass should have a clear documented contract:

```
PASS 1 — Game Scene
  Input:  World state (entities, map, parallax, camera)
  Output: state->canvas (480×270 RGBA)
  Owner:  render_world() + system dispatch
  Notes:  Camera transform is pushed/popped around world-space draws.
          Rain renders after pop (screen-space effect, no camera transform).

PASS 2 — HRC Lighting Compute
  Input:  World lights (static + dynamic), LdtkMap for absorption, camera position
  Output: state->world.lighting.fluence (LIGHTING_WORLD_SIZE² RGBA8)
  Owner:  world_collect_lights() + lighting_compute()
  Notes:  Absorption rebuilt each frame (camera-relative). Light list reset each frame.

PASS 3 — Composite & Display
  Input:  state->canvas (Pass 1), lighting.fluence (Pass 2), window dimensions
  Output: state->composite_canvas (debug) OR screen framebuffer (normal)
  Owner:  pass_composite() with dest parameter
  Notes:  Multiply blend: fluence × canvas → output.
          Projection restored to game canvas projection after each composite call.
```

---

## Scalability Considerations

| Concern | Current (10 entities) | Future (100 entities) | Future (500+ entities) |
|---------|----------------------|----------------------|----------------------|
| Entity scan per system | 4096 iterations | ~100 (with high-water) | ~500 (with high-water) |
| Entity creation | O(MAX_ENTITIES) scan | same | same — add free-list if needed |
| Lighting per frame | O(LIGHTING_WORLD_SIZE²) | same | same — bounded by grid, not entities |
| LDtk spawn | One-time on level load | same | same |
| Full-array scan (physics) | wasteful but fine | fine | still fine at 500 |

At the scale of a solo-dev platformer (realistically 10-50 active entities at a time), no architecture changes are needed for performance beyond the high-water mark optimization. Do not introduce sparse sets, component pools, or archetype storage until profiling shows a real bottleneck.

---

## Hot-Reload Compatibility Matrix

| Change Type | Hot-Reload Safe? | Action Required |
|-------------|-----------------|-----------------|
| Function body change | YES | Rebuild, game reloads automatically |
| New function added | YES | Rebuild |
| New field added to `Entity` or `World` | NO | Bump `GAME_STATE_VERSION`, restart game |
| New field added to `GameState` | NO | Bump `GAME_STATE_VERSION`, restart game |
| Field removed from any state struct | NO | Bump `GAME_STATE_VERSION`, restart game |
| Field reordered in any state struct | NO | Bump `GAME_STATE_VERSION`, restart game |
| Shader file changed | YES | CF auto-reloads via `on_shader_changed` callback |
| New `.c` file added to game library | YES | Rebuild |
| New `CF_Coroutine` field in `Entity` | PARTIAL | Add destroy call in `world_hot_reload()` |
| LDtk assets changed | YES | `ldtk_check_reload()` detects file modification |

**Rule of thumb:** If you touched `game_state.h`, `world.h`, or `entity.h` — restart the game, don't hot-reload.

---

## Sources

- Direct codebase analysis: `src/game/game.c`, `src/game/world.{h,c}`, `src/game/systems/physics_system.c`, `src/game/systems/render_system.c` (2026-03-18)
- [cr.h: A Simple C Hot Reload Header-only Library](https://fungos.github.io/cr-simple-c-hot-reload/) — struct versioning and state validation patterns (MEDIUM confidence)
- [Exile: Hot Reloading](https://thenumb.at/Hot-Reloading-in-Exile/) — C hot reload architecture patterns and limitations (MEDIUM confidence)
- [Object Pool Pattern — Game Programming Patterns](https://gameprogrammingpatterns.com/object-pool.html) — entity pool management patterns (HIGH confidence, established reference)
- [Implementing Component-Entity-Systems — GameDev.net](https://www.gamedev.net/articles/programming/general-and-gameplay-programming/implementing-component-entity-systems-r3382/) — active entity tracking patterns (MEDIUM confidence)
- [Decoupling the Graphics System — GameDev.net](https://www.gamedev.net/forums/topic/652771-decoupling-the-graphics-system-render-engine/) — render pass decoupling patterns (MEDIUM confidence)
- [Decoupling Input, Game Logic, And Rendering](https://gamedevfaqs.com/decoupling-input-game-logic-and-rendering-in-the-game-loop-for-maintainability/) — system boundary rationale (LOW confidence, secondary source)
