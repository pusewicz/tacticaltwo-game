# TacticalTwo Game

C23 game using Cute Framework with hot-reloading support.

## Build Commands
- `rake` - Build RelWithDebInfo (default)
- `rake run` - Build and run game
- `rake format` - Format C files with clang-format
- `rake cmake:configure` - Configure CMake (Ninja, RelWithDebInfo)

## Project Structure
- `src/app/` - Main executable (platform loader)
- `src/game/` - Game logic (hot-reloadable shared library)
- `src/engine/` - Engine utilities (logging, state)
- `src/config/` - Configuration constants
- `src/platform/` - Platform abstraction (Cute Framework)
- `vendor/` - Third-party dependencies
- `assets/` - Game assets (mounted at `/assets` in CF filesystem)

## Code Style
- C23 standard (`-std=c23`)
- Use `.clang-format` for formatting
- Strict warnings: `-Wall -Wextra -Wpedantic -Wconversion`

## Hot Reloading
- Game library exports: `game_init`, `game_update`, `game_render`, `game_shutdown`, `game_state`, `game_hot_reload`
- `ENABLE_HOT_RELOADING=ON` enables shared library build

## Cute Framework Notes
- `cf_make_app()` - Create app window
- `cf_fs_mount()` - Mount directories to virtual paths
- Canvas size: 720x405 scaled 2x
