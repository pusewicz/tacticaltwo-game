# Architecture

**Analysis Date:** 2026-03-18

## Pattern Overview

**Overall:** Layered + Hot-Reloadable ECS-inspired Architecture

**Key Characteristics:**
- Monolithic "fat struct" entity system (no external ECS library)
- Hot-reloadable game library with persistent state across reloads
- Multi-pass rendering: game canvas → lighting compute → composite to screen
- Platform abstraction layer separating host executable from game logic
- Fixed-capacity entity array (4096 max) with component-enabled flags

## Layers

**Host Application (`src/app/`):**
- Purpose: Bootstraps the game, manages dynamic library loading, hot-reload lifecycle
- Location: `src/app/main.c`
- Contains: Entry point, hot-reload loop, library loading/unloading
- Depends on: Platform layer, game library exports
- Used by: Operating system

**Platform Abstraction (`src/platform/`):**
- Purpose: Encapsulates Cute Framework integration and platform-specific concerns (dynamic library loading, timing, frame cycling)
- Location: `src/platform/platform_cute.h`, `src/platform/platform_cute.c`
- Contains: Cute Framework initialization, game library loading/unloading, frame begin/end hooks
- Depends on: Cute Framework, system APIs (dlopen/dlsym on macOS)
- Used by: Host application (`src/app/main.c`)

**Engine (`src/engine/`):**
- Purpose: Core game state container and utilities that survive hot reloads
- Location: `src/engine/`
- Contains: `GameState` struct, logging system, asset helpers
- Depends on: Cute Framework primitives
- Used by: Game layer

**Game Logic (`src/game/`):**
- Purpose: Main gameplay, entity management, world simulation
- Location: `src/game/`
- Contains: World manager, entity factory, system dispatch, LDtk integration, lighting, rain effects
- Depends on: Engine, platform, systems
- Used by: Host application via exported functions

**Systems (`src/game/systems/`):**
- Purpose: Data-driven processing of entity components (input, physics, rendering, collision, animation, camera)
- Location: `src/game/systems/`
- Contains: Plain `void` functions iterating `World.entities[]` array
- Depends on: World entity data, Cute Framework drawing/input
- Used by: `update_world()` and `render_world()` in `src/game/world.c`

**Configuration (`src/config/`):**
- Purpose: Game constants (canvas size, physics gravity, app metadata)
- Location: `src/config/config.h`
- Contains: Compile-time configuration
- Depends on: Nothing
- Used by: All layers during initialization

## Data Flow

**Initialization Flow:**

1. Host app (`src/app/main.c:main()`) initializes platform
2. Platform loads game library (`libgame.dylib`)
3. Host invokes `game_init(Platform*)` → initializes `GameState`, calls `init_world()`
4. `init_world()` → loads LDtk map, spawns player entity, initializes lighting/rain systems

**Update Loop (each frame):**

1. `platform_begin_frame()` → Cute Framework window/input polling
2. `game_update()` → `update_world(dt)` → system dispatch:
   - `sys_gather_input()` → reads keyboard into PlayerInput components
   - `sys_player_coroutine()` → drives state machine via CF_Coroutine
   - `update_muzzle_flash(dt)` → countdown active muzzle flash
   - `sys_update_player_movement()` → applies walk velocity from controller
   - `sys_apply_velocity()` → Euler integration: position += velocity * dt
   - `sys_collide_tilemap()` → AABB collision vs LDtk IntGrid
   - `sys_camera_follow()` → smooth camera tracking with level bounds
3. Optional ImGui debug overlay (F toggles `debug_mode`)
4. `platform_end_frame()` → frame flip

**Render Pipeline (3-pass system):**

**PASS 1 - Game Scene:**
- Target: `state->canvas` (480×270 pixels, 3× upscaled)
- Clears to sky blue, dispatches render systems:
  - `sys_render_parallax()` → layers with depth-based scroll
  - `sys_render_tilemap()` → LDtk pre-rendered tile layer PNGs
  - `sys_render_sprites()` → entity sprites at transform positions
  - Debug overlays (if `debug_mode`): colliders, tiles, grid

**PASS 2 - HRC Lighting Compute:**
- Computes Holographic Radiance Cascades global illumination
- Inputs: Static lights from LDtk entities, dynamic muzzle flash light
- Outputs: Fluence canvas (computed GI as RGBA8)
- Algorithm: cascade pyramid with absorption/emission per cascade

**PASS 3 - Composite & Display:**
- Debug mode: renders to `composite_canvas`, displayed in ImGui window
- Normal mode: renders directly to screen framebuffer
- Compositing: game canvas + lighting fluence (multiply blend)
- Projection adjusted for window size (maintains aspect ratio with pillarbox/letterbox)

**State Management:**

- Global `state` pointer (extern in `src/game/game.c`)
- Survives hot reloads via `game_state()` / `game_hot_reload(void* game_state)`
- Contains entire `World` struct with fixed entity array
- Platform holds the GameLibrary struct; on reload, state is extracted, library reloaded, then restored

## Key Abstractions

**Entity (Fat Struct):**
- Purpose: Monolithic representation of all game objects
- Location: `src/game/world.h` (lines 94–104)
- Pattern: Single `Entity` struct contains all possible component data with `enabled` flags
- Components: PlayerInput, PlayerController, PlayerStateComp, Transform, Velocity, Collider, Sprite
- Rationale: Simple cache locality, easy debugging, no external dependency

**World:**
- Purpose: Container for all entity data and subsystem state
- Location: `src/game/world.h` (lines 136–149)
- Contains: Fixed array `entities[MAX_ENTITIES]`, player index, camera, LDtk map, lighting, rain
- Accessed globally via `state->world`

**System:**
- Purpose: Pure data-processing function iterating entity array
- Location: `src/game/systems/`
- Pattern: `void sys_name(void)` → loop entities → filter by `exists` + component `enabled` → apply logic
- Example: `sys_gather_input()` reads CF keyboard into PlayerInput components

**LDtk Map:**
- Purpose: Level data: tile layers (PNG sprites), collision grid (CSV → IntGrid), entities with custom fields
- Location: `src/game/ldtk.h`, `src/game/ldtk.c`
- Integration: Loaded in `init_world()`, checked for reload each frame, entities spawned via `ldtk_spawn_entities()`
- Coordinate system: LDtk uses top-left origin, grid-snapped; internally converted to CF Y-up center-origin

**HRC Lighting (Holographic Radiance Cascades):**
- Purpose: Real-time 2D global illumination with cone-shaped spotlights and occlusion
- Location: `src/game/lighting.h`, `src/game/lighting.c`
- Input: Light sources (static from LDtk, dynamic from code), absorption map (LDtk solid tiles)
- Output: Fluence canvas composited over game with multiply blend
- Cascade pyramid: level 0 (coarse) to level N (fine detail), virtual rays encode indirect light

**Muzzle Flash:**
- Purpose: Dynamic light effect triggered by player firing
- Location: `src/game/world.h` (lines 121–130), managed in `src/game/world.c`
- Integration: Injected into HRC each frame, timer countdown
- Tunable: duration, peak intensity, radius arc (tweakable via ImGui)

**Rain:**
- Purpose: Procedural fullscreen rain overlay with ground splashes using tilemap collision
- Location: `src/game/rain.h`, `src/game/rain.c`
- Shader: GLSL compute shader at `/assets/shaders/`
- Interaction: Reads LDtk IntGrid as collision mask, renders splashes at tile surface

## Entry Points

**`game_init(Platform* platform)` → `src/game/game.c:91`**
- Triggers: Called once by host after library load
- Responsibilities:
  - Allocate and initialize `GameState`
  - Create main game canvas (480×270)
  - Set up Cute Framework projection matrix
  - Call `init_world()` to bootstrap level/entities
  - Register shader hot-reload callback

**`game_update()` → `src/game/game.c:114`**
- Triggers: Called once per frame by host
- Responsibilities:
  - Reset scratch arena
  - Toggle debug mode (G key)
  - Dispatch `update_world(dt)` systems
  - Render ImGui debug overlay (if debug mode)
  - Return false to signal shutdown

**`game_render()` → `src/game/game.c:259`**
- Triggers: Called once per frame after update
- Responsibilities:
  - Execute 3-pass rendering pipeline
  - PASS 1: Render to game canvas
  - PASS 2: Compute HRC lighting
  - PASS 3: Composite and display (debug or normal mode)

**`game_shutdown()` → `src/game/game.c:361`**
- Triggers: Called once before library unload
- Responsibilities:
  - Shutdown world subsystems
  - Destroy canvases
  - Free allocated memory

**`game_hot_reload(void* game_state)` → `src/game/game.c:374`**
- Triggers: Called after new library loads
- Responsibilities:
  - Restore global `state` pointer from saved state
  - Call `world_hot_reload()` to re-register coroutine callbacks

## Error Handling

**Strategy:** Silent fallback with warnings logged

**Patterns:**
- LDtk load failure → spawn player at default position (0, 0)
- Asset load failure → log warning, continue (e.g., parallax layer missing)
- Collision detection → clamp position within level bounds if math fails
- Shader compilation → defer to Cute Framework's shader system (auto-recompiles on file change)

## Cross-Cutting Concerns

**Logging:**
- Macros in `src/engine/log.h`: `log_debug()`, `log_info()`, `log_warn()`, `log_error()`
- Implementation: Wraps CF logger with file/line capture
- Used in: Asset loading, LDtk parsing, hot-reload lifecycle

**Validation:**
- Entity existence checked in all systems via `if (!e->exists) continue`
- Component enabled flags checked before processing (e.g., `if (!e->sprite.enabled) continue`)
- Level bounds clamping in camera system

**Coordinate System Conversion:**
- LDtk uses top-left origin with Y-down, 16×16 grid cells
- CF uses center origin with Y-up
- Conversion functions in `src/game/systems/physics_system.c`: `cf_x_to_grid()`, `cf_y_to_grid()`, `grid_cell_aabb()`
- Same conversions duplicated in `src/game/systems/render_system.c` for debug rendering

---

*Architecture analysis: 2026-03-18*
