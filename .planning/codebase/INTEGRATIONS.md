# External Integrations

**Analysis Date:** 2026-03-18

## APIs & External Services

**Not detected.**

The codebase contains no HTTP clients, REST API calls, SDK integrations, or external web service dependencies. All game data is loaded locally from files.

## Data Storage

**Databases:**
- None - No persistent database integration

**File Storage:**
- Local filesystem only (Cute Framework VFS)
- Asset loading: `cf_fs_read_entire_file_to_memory_and_nul_terminate()` from `src/engine/asset.h`
- Assets mounted at `/assets` in CF virtual filesystem
- Asset directory: `assets/` (at project root, symlinked or copied to build)

**LDtk Level Format:**
- Format: JSON (LDtk editor output)
- Loader: `src/game/ldtk.c` parses `data.json` and loads pre-rendered layer PNGs
- CSV parsing: IntGrid layer data loaded from CSV files within level directory

**Caching:**
- None persistent - All data loaded on startup or level load
- In-memory only during runtime

## Authentication & Identity

**Auth Provider:**
- None - Single-player offline game, no user authentication

**Access Control:**
- Not applicable

## Monitoring & Observability

**Error Tracking:**
- None - No remote error reporting

**Logs:**
- Local console logging only
- Framework: `src/engine/log.h`
- Tags: Structured logging with event tag support (e.g., "main", "platform")
- Destination: stdout/stderr (platform dependent)
- Log levels: `log_info()`, `log_error()`, `log_debug()`

**Performance Monitoring:**
- Cute Framework provides FPS/framerate control
- Fixed timestep: 60 FPS
- Profiling: Not integrated (only instrumentation available from CF)

## CI/CD & Deployment

**Hosting:**
- Not applicable - Desktop standalone executable

**CI Pipeline:**
- Not detected - No GitHub Actions, GitLab CI, or other CI configuration in repository

**Build Artifacts:**
- Primary: `build/relwithdebinfo/bin/TacticalTwo` (executable)
- Game library: `build/relwithdebinfo/bin/libgame.dylib` (macOS, or .so/.dll on other platforms)

**Version Control:**
- Git repository with submodules
- No automated release/deployment pipeline detected

## Environment Configuration

**Required Environment Variables:**
- `CPATH` - Must NOT include system SDL3 (causes vendored SDL3 to be shadowed) - stripped in Rakefile
- Game paths: Configured at build time via CMake
  - `GAME_LIB_PATH` - Path to loaded game library
  - `ASSETS_PATH` - Path to game assets directory

**Configuration Files:**
- `.clang-format` - Code formatting (development)
- `.editorconfig` - Editor settings (development)
- `CMakeLists.txt` - Build configuration
- `Rakefile` - Build orchestration
- `src/config/config.h` - Game constants (hardcoded):
  - `GAME_NAME = "TacticalTwo"`
  - `GAME_VERSION = "0.1.0"`
  - `GAME_APP_ID = "com.pusewicz.tacticaltwo"`
  - `CANVAS_WIDTH = 480`, `CANVAS_HEIGHT = 270`
  - `CANVAS_SCALE = 3`
  - `GRAVITY = -500.0f`

**Secrets Location:**
- Not applicable - No secrets or credentials in codebase

## Webhooks & Callbacks

**Incoming:**
- None - No network endpoints

**Outgoing:**
- None - No external service calls

## Platform Dependencies

**Cute Framework Components Used:**
- `cute_app.h` - Application lifecycle (init, update, shutdown)
- `cute_input.h` - Input handling (keyboard, mouse, gamepad)
- `cute_graphics.h` - Rendering (canvases, sprites, drawing)
- `cute_draw.h` - Immediate-mode drawing utilities
- `cute_sprite.h` - Sprite and animation system
- `cute_json.h` - JSON parsing for LDtk data
- `cute_file_system.h` - Asset filesystem abstraction
- `cute_color.h` - Color utilities
- `cute_math.h` - Vector math
- `cute_time.h` - Timing and frame management
- `cute_string.h` - String utilities
- `cute_symbol.h` - Symbol interning (string deduplication)
- `cute_c_runtime.h` - C standard library wrappers
- `cute_coroutine.h` - Coroutine system (player animations)
- `cute_alloc.h` - Memory allocation (arena allocator)
- `cute_defines.h` - Framework definitions
- `cute_result.h` - Result/error types

**SDL3 Components:**
- `SDL_timer.h` - Timing
- `SDL_filesystem.h` - Platform filesystem paths
- `SDL_init.h` - SDL initialization
- `SDL_error.h` - Error reporting
- `SDL_stdinc.h` - Standard C types

## Asset Pipeline

**Asset Formats:**
- PNG - Tilemap and sprite textures
- JSON - LDtk level data (`data.json`)
- CSV - IntGrid collision data (within LDtk directories)
- Aseprite (.ase) - Sprite source files (dev tool: `tools/aseprite`)

**Asset Loading:**
- No build-time asset processing
- Runtime loading from `assets/` directory via CF filesystem
- LDtk levels: Directory structure `assets/ldtk/[levelname]/` with:
  - `data.json` - Level metadata
  - `[layername].png` - Pre-rendered tilemap layers
  - `data/[gridname].csv` - IntGrid collision/data

**Sprite System:**
- Cute Framework sprite rendering
- Animation: Frame-based with duration per frame

## No External Dependencies

**Notable absences:**
- No HTTP/REST client
- No database (SQL or NoSQL)
- No cloud services (AWS, GCP, Azure, etc.)
- No analytics or telemetry
- No messaging queues (Kafka, RabbitMQ, etc.)
- No caching service (Redis, Memcached)
- No real-time sync (Firebase, Supabase)
- No third-party SDKs (payment, ads, etc.)

This is a pure standalone game with all logic and assets local to the executable.

---

*Integration audit: 2026-03-18*
