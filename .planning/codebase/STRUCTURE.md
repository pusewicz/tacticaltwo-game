# Codebase Structure

**Analysis Date:** 2026-03-18

## Directory Layout

```
tacticaltwo-game/
├── src/
│   ├── app/                       # Host executable (TacticalTwo app)
│   ├── engine/                    # Game state and engine utilities
│   ├── platform/                  # Platform abstraction (Cute Framework)
│   ├── game/                      # Main game logic (hot-reloadable library)
│   │   ├── systems/               # ECS-like systems (input, physics, render)
│   │   └── [game subsystems]      # LDtk, lighting, rain, etc.
│   └── config/                    # Configuration constants
├── assets/
│   ├── sprites/                   # Sprite sheets (.ase Aseprite files)
│   ├── ldtk/map/                  # LDtk level exports
│   ├── shaders/                   # GLSL compute/fragment shaders
│   └── GandalfHardcore City Tiles/# Parallax background layers
├── vendor/
│   └── cute_framework/            # Submodule: rendering, input, math, etc.
├── tools/                         # Development utilities (aseprite CLI)
├── build/                         # CMake build output (RelWithDebInfo)
├── CMakeLists.txt                 # Root CMake configuration
├── Rakefile                       # Build automation
└── CLAUDE.md                      # Project conventions and build guide
```

## Directory Purposes

**`src/app/`:**
- Purpose: Host executable entry point and hot-reload management
- Contains: `main.c` (game loop, library loading/unloading)
- Key files: `src/app/main.c`

**`src/engine/`:**
- Purpose: Core abstractions and utilities that survive hot reloads
- Contains: GameState struct, logging system, asset loading helpers
- Key files:
  - `src/engine/game_state.h` — GameState struct (global state container)
  - `src/engine/log.h` — Logging macros and interface
  - `src/engine/platform.h` — Platform abstraction interface
  - `src/engine/asset.c/h` — Asset loading utilities

**`src/platform/`:**
- Purpose: Platform-specific implementations (Cute Framework integration, dynamic loading)
- Contains: Cute Framework initialization, game library lifecycle, frame timing
- Key files:
  - `src/platform/platform_cute.h` — GameLibrary struct, function pointers
  - `src/platform/platform_cute.c` — Platform implementation

**`src/game/`:**
- Purpose: Main game logic and world management (hot-reloadable)
- Contains: World entity system, entity factory, system dispatch, asset/level integration
- Key files:
  - `src/game/game.h` — Exported functions (game_init, game_update, etc.)
  - `src/game/game.c` — Game lifecycle, render pipeline, ImGui debug UI
  - `src/game/world.h` — Entity, World, component structs
  - `src/game/world.c` — Entity management, world initialization, system dispatch
  - `src/game/ldtk.h/c` — LDtk level loading and spawning
  - `src/game/lighting.h/c` — HRC 2D global illumination
  - `src/game/rain.h/c` — Procedural rain effect
  - `src/game/systems/systems.h` — System function declarations

**`src/game/systems/`:**
- Purpose: Data-driven entity processing (no state, pure iteration + logic)
- Contains: Input, physics, rendering, collision, camera, animation systems
- Key files:
  - `src/game/systems/input_system.c` — Keyboard/mouse → PlayerInput component
  - `src/game/systems/player_system.c` — Player state machine (coroutine-based)
  - `src/game/systems/physics_system.c` — Velocity integration, AABB-tilemap collision
  - `src/game/systems/render_system.c` — Sprite rendering, debug overlays
  - `src/game/systems/camera_system.c` — Camera follow with bounds clamping
  - `src/game/systems/animation_system.c` — Sprite animation playback
  - `src/game/systems/parallax_system.c` — Parallax background layers
  - `src/game/systems/tilemap_system.c` — LDtk tilemap rendering

**`src/config/`:**
- Purpose: Game constants (build-time configuration)
- Contains: Canvas dimensions, physics tuning, app metadata
- Key files: `src/config/config.h`

**`assets/sprites/`:**
- Purpose: Game sprites (Aseprite .ase files with frame tags and animations)
- Contains: Player character animations (GunAim, GunWalk, GunCrouch, GunFire, GunReload)
- Key files: `assets/sprites/player_combat.ase`

**`assets/ldtk/map/simplified/`:**
- Purpose: LDtk simplified export format for levels
- Contains: `data.json` (metadata, entities), pre-rendered PNG layers, CSV collision grids
- Key files: `assets/ldtk/map/simplified/Level_0/` (first level)
- Loaded by: `ldtk_load()` in `src/game/ldtk.c`

**`assets/shaders/hrc/`:**
- Purpose: HRC lighting compute shaders (GLSL)
- Contains: Cascade radiance passes, absorption computation
- Loaded by: `lighting_init()`, watched for hot reload

**`assets/GandalfHardcore City Tiles/`:**
- Purpose: Parallax background artwork
- Contains: PNG layers (sky, layer1, layer2) for depth-based scrolling
- Loaded by: `init_world()` in `src/game/world.c`

**`vendor/cute_framework/`:**
- Purpose: Rendering, input, math, windowing library (submodule)
- Contains: Headers in `libraries/cute/`, CMakeLists integration
- Provides: CF_* types, drawing API, sprite system, coroutines, filesystem

**`tools/`:**
- Purpose: Development utilities
- Contains: `aseprite` script wrapper for inspecting .ase files (metadata, frame durations, layer structure)

**`build/relwithdebinfo/`:**
- Purpose: CMake build output directory
- Contains: Compiled binaries, CMake cache, dependency builds
- Files:
  - `game/src/libgame.dylib` — Hot-reloadable game library
  - `app/TacticalTwo` — Host executable

## Key File Locations

**Entry Points:**
- `src/app/main.c` — Host executable entry, event loop
- `src/game/game.c:game_init()` — Game initialization
- `src/game/game.c:game_update()` — Per-frame update
- `src/game/game.c:game_render()` — Per-frame rendering

**Configuration:**
- `src/config/config.h` — Game constants (canvas size, gravity, app ID)
- `CMakeLists.txt` — Build configuration
- `Rakefile` — Build automation (rake, rake run, rake watch, rake format)

**Core Logic:**
- `src/game/world.h` — Entity and World data structures
- `src/game/world.c` — Entity lifecycle, world initialization, system dispatch
- `src/engine/game_state.h` — GameState struct (survives hot reloads)

**Systems (Pure Iteration):**
- `src/game/systems/input_system.c` — Input polling
- `src/game/systems/player_system.c` — Player state machine
- `src/game/systems/physics_system.c` — Physics and collision
- `src/game/systems/render_system.c` — Sprite and debug rendering

**Subsystems:**
- `src/game/ldtk.h/c` — Level loading (entities, collision, layers)
- `src/game/lighting.h/c` — HRC 2D GI
- `src/game/rain.h/c` — Procedural rain

**Testing:**
- No test files (no automated test framework in place)

## Naming Conventions

**Files:**
- `src/game/systems/[name]_system.c` — System implementations
- `src/game/[subsystem].h/c` — Subsystem header/implementation pairs
- Asset files: snake_case (e.g., `player_combat.ase`, `height_map`)

**Directories:**
- lowercase: `src/app`, `src/game/systems`, `assets/sprites`
- Mixed case only for asset providers: `GandalfHardcore City Tiles`

**Functions:**
- Global public: `game_init()`, `update_world()`, `sys_gather_input()`
- Static/private: `ensure_composite_canvas()`, `multiply_blend_state()`
- Pattern: `verb_noun()` (e.g., `world_add_entity()`, `ldtk_load()`)

**Variables:**
- Globals: `state` (GameState*)
- Local: `e` (Entity*), `map` (LdtkMap*), `dt` (float delta-time)
- Structs: PascalCase (GameState, Entity, World)
- Fields: snake_case (player_input, position, velocity)

**Types:**
- Structs: PascalCase (Entity, World, LdtkMap)
- Enums: UPPER_CASE (PLAYER_STATE_IDLE, LOG_LEVEL_ERROR)
- Typedefs for CF types: CF_V2, CF_Canvas, CF_Coroutine

## Where to Add New Code

**New Feature (e.g., weapon system):**
- Primary code: `src/game/weapon.h/c` (new subsystem) or extend `src/game/world.h` (new component)
- System logic: `src/game/systems/weapon_system.c`
- Tests: No automated tests; use ImGui debug controls or manual testing

**New Entity Component:**
1. Add struct to `src/game/world.h` (e.g., `typedef struct { bool enabled; /* fields */ } ComponentName;`)
2. Add field to `Entity` struct (line 94+)
3. Initialize in entity factory (e.g., `make_player_at()`)
4. Create system to process it: `src/game/systems/[component]_system.c`
5. Dispatch system in `update_world()` or `render_world()` as appropriate

**New System:**
1. Create `src/game/systems/[name]_system.c`
2. Add declaration to `src/game/systems/systems.h`
3. Implement as `void sys_[name](void)` that iterates `state->world.entities[]`
4. Call from `update_world()` in `src/game/world.c:166` (update phase) or `render_world()` in `src/game/world.c:186` (render phase)

**New Subsystem (e.g., damage system):**
1. Create `src/game/[subsystem].h` (public interface, types)
2. Create `src/game/[subsystem].c` (implementation, using `src/engine/game_state.h` for access to `state`)
3. Initialize in `init_world()` in `src/game/world.c:117`
4. Update/render in `update_world()` or `render_world()` as needed
5. Shutdown in `shutdown_world()` (if resources to free)

**Configuration:**
- Add constants to `src/config/config.h` (compile-time) or as fields in `World`/`GameState` (runtime tweakable via ImGui)

**Utilities:**
- Shared helpers: `src/engine/` (survives hot reload) or `src/game/[subsystem].h` (scoped)

## Special Directories

**`build/`:**
- Purpose: CMake build output
- Generated: Yes (by `rake cmake:configure`)
- Committed: No (in .gitignore)
- Artifacts: `libgame.dylib` (hot-reloadable game library), `TacticalTwo` (host executable)

**`vendor/cute_framework/`:**
- Purpose: Cute Framework library (git submodule)
- Generated: No (checked in)
- Committed: Yes (submodule pointer in .gitmodules)
- Usage: CMake `add_subdirectory()` in root CMakeLists.txt

**`assets/ldtk/map/simplified/`:**
- Purpose: LDtk level data (generated by LDtk editor export)
- Generated: Yes (by LDtk simplified export)
- Committed: Yes (levels checked in)
- Loader: `ldtk_load()` in `src/game/ldtk.c`

**`assets/shaders/hrc/`:**
- Purpose: HRC lighting compute shaders
- Generated: No (hand-written GLSL)
- Committed: Yes
- Watched: Yes (hot reload on change detected)

---

*Structure analysis: 2026-03-18*
