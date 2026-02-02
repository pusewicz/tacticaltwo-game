# TacticalTwo Game

C23 game using Cute Framework with hot-reloading support.

## Setup
```bash
git submodule update --init --recursive
```

## Build Commands
- `rake` - Build RelWithDebInfo (default)
- `rake run` - Build and run game
- `rake format` - Format C files with clang-format
- `rake cmake:configure` - Configure CMake (Ninja, RelWithDebInfo)

## Development Workflow
- `rake watch` - Watch src/ for changes and auto-rebuild game library (for hot-reloading)
  - When running, builds happen automatically on file save
  - Other sessions should NOT manually build while watch is active

## Build Notes
- `ninja: no work to do.` means the build succeeded - all targets are up-to-date
  - Do NOT retry or attempt to "fix" this message
  - This is normal when `rake watch` has already rebuilt, or when no source files changed
- **NEVER remove `build/` directories** unless the user explicitly asks
  - Deleting build directories causes lengthy full rebuilds
  - Build errors do NOT require deleting the build directory

## Project Structure
- `src/app/` - Main executable (platform loader)
- `src/game/` - Game logic (hot-reloadable shared library)
- `src/engine/` - Engine utilities (logging, state)
- `src/config/` - Configuration constants
- `src/platform/` - Platform abstraction (Cute Framework)
- `vendor/` - Third-party dependencies (includes `empyreanx/pico_ecs.h`)
- `assets/` - Game assets (mounted at `/assets` in CF filesystem)
- `tools/` - Development tools
  - `aseprite` - Inspect .ase files (tags, layers, durations). Use `--json` or `--c-header` for output formats.

## Code Style
- C23 standard (`-std=c23`)
- Use `.clang-format` for formatting
- Strict warnings: `-Wall -Wextra -Wpedantic -Wconversion`
- Use `nullptr` instead of `NULL`
- Use `[[maybe_unused]]` attribute instead of `(void)variable`
- Use `//` for single-line comments, not `/* */`

## Hot Reloading
- Game library exports: `game_init`, `game_update`, `game_render`, `game_shutdown`, `game_state`, `game_hot_reload`
- `ENABLE_HOT_RELOADING=ON` enables shared library build

## Cute Framework Notes
- `cf_make_app()` - Create app window
- `cf_fs_mount()` - Mount directories to virtual paths
- Canvas size: 720x405 scaled 2x
