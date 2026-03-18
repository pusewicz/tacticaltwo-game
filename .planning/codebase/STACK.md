# Technology Stack

**Analysis Date:** 2026-03-18

## Languages

**Primary:**
- C23 - Game logic (`src/game/`), engine utilities (`src/engine/`), platform layer (`src/platform/`)
- C - Cute Framework (vendored)

**Secondary:**
- C++ 17 - Cute Framework internal components
- CMake - Build configuration
- Ruby - Build orchestration (Rakefile)

## Runtime

**Environment:**
- Native executable (macOS, Linux, Windows target support)
- 64-bit architecture required for hot-reloading (dlopen/dyld)

**Build Architecture:**
- Host platform binary (TacticalTwo) loads hot-reloadable game library
- Game library: `src/game/` compiles as shared library (`libgame.dylib` on macOS)
- Hot-reload strategy: Library detection + file replacement + VM state preservation

## Package Manager

**Build System:**
- CMake 3.25+ - Primary build orchestration
- Ninja - Build backend (configured via Rakefile)
- FetchContent - Vendor dependency management

**Dependency Management:**
- Git submodules for vendored code (`vendor/`)
- No external package managers (npm, pip, cargo) used

## Frameworks

**Core:**
- Cute Framework 1.1.1 - Game development framework
  - Location: `vendor/cute_framework/` (git submodule)
  - Purpose: Graphics, input, windowing, asset loading, JSON parsing, math, memory allocation
  - Configuration: Static/shared library toggle via `CF_FRAMEWORK_STATIC` (shared for hot-reload)
  - Architecture: Wraps SDL3 for platform abstraction

**Graphics Backend:**
- SDL3 - Windowing, input, event loop
  - Fetched via FetchContent from SDL GitHub releases
  - Supports rendering via D3D12, Vulkan, Metal, OpenGL
  - Platform-specific: Metal on macOS, D3D12 on Windows, Vulkan on Linux

**Rendering Pipeline:**
- SDL GPU (SDL3's graphics abstraction)
- SPIR-V shader compilation (runtime or offline via cute-shaderc)

## Key Dependencies

**Critical:**
- SDL3 (latest) - Window management and input events
- Cute Framework 1.1.1 - All game framework utilities
  - Includes: Graphics, sprite system, coroutines, JSON parsing, file system, math library

**Internal Engine:**
- `src/engine/log.h` - Structured logging with tags
- `src/engine/game_state.h` - Global GameState struct (survives hot reloads)
- `src/engine/asset.h` - Asset loading and caching
- `src/engine/platform.h` - Platform abstraction (minimal: system page size)

## Build Configuration

**CMake Variables:**
- `CMAKE_C_STANDARD`: 23
- `CMAKE_CXX_STANDARD`: 17
- `CMAKE_BUILD_TYPE`: RelWithDebInfo (default)
- `ENABLE_HOT_RELOADING`: ON (development), OFF (release - not fully supported yet)
- `CF_FRAMEWORK_STATIC`: OFF (hot-reload), ON (release)

**Compile Flags:**
- Warning level: `-Wall -Wextra -Wpedantic -Wconversion -Wdouble-promotion -Wstrict-prototypes -Wmissing-prototypes`
- Code generation: Position-independent code enabled (PIC) for shared libraries
- Debug info: Always included (RelWithDebInfo default)
- Color diagnostics: Enabled

**Platform Defines:**
- `DEBUG` - RelWithDebInfo and Debug configs
- `RELEASE` - Release config only
- `PLATFORM_MACOS`, `PLATFORM_LINUX`, `PLATFORM_WINDOWS` - Platform detection
- `LOG_SOURCE_DIR` - Logging source path prefix

## Code Style Configuration

**Formatting:**
- Tool: clang-format
- Config: `.clang-format`
- Standard: LLVM-based with C23 adjustments
- Indentation: 2 spaces (no tabs)
- Column limit: 80 characters
- Pointer alignment: Left (`int*` not `int *`)
- Include sorting: Case-sensitive, organized by type (standard → project)

**Editor:**
- `.editorconfig` present for IDE coordination

## Platform Requirements

**Development:**
- macOS 10+ (tested), Linux, or Windows
- CMake 3.25+
- Ninja build system
- Clang or GCC compiler (C23 support required)
- Ruby (for Rakefile orchestration) + `listen` gem for file watching

**macOS Specific:**
- `/usr/bin/ar` required for mruby builds (Homebrew ar conflicts)
- `codesign` used for ad-hoc code signing of reloaded library
- dyld compatibility for hot-reload dlopen

**Production:**
- Target platforms: macOS, Linux, Windows
- Deployment: Single executable + assets directory
- No external runtime dependencies (all linked statically in release build)

## System Architecture

**Multi-target Build:**
- Host executable (TacticalTwo): `src/app/main.c`
- Game library: `src/game/*.c` (hot-reloadable shared library)
- Hot-reload mechanism: dlopen-based library replacement with state preservation

**Memory Management:**
- Cute Framework arena allocator (`CF_Arena`) for scratch memory
- GameState persists across hot reloads (stored in game process memory)
- Entity system: Fixed array of 4096 entities (fat struct, no external ECS)

---

*Stack analysis: 2026-03-18*
