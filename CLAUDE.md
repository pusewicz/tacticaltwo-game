# TacticalTwo Game

A side-scrolling, tactical platformer game written in C23 using Cute Framework.

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
- Strict warnings: `-Wall -Wextra -Wpedantic -Wconversion`
- Use `nullptr` instead of `NULL`
- Use `[[maybe_unused]]` attribute instead of `(void)variable`
- Use `//` for single-line comments, not `/* */`

## Hot Reloading
- Game library exports: `game_init`, `game_update`, `game_render`, `game_shutdown`, `game_state`, `game_hot_reload`
- `ENABLE_HOT_RELOADING=ON` enables shared library build

## Workflow Orchestration

### 1. Plan Mode Default
- Enter plan mode for ANY non-trivial task (3+ steps or architectural decisions)
- If something goes sideways, STOP and re-plan immediately - don't keep pushing
- Use plan mode for verification steps, not just building
- Write detailed specs upfront to reduce ambiguity

### 2. Subagent Strategy
- Use subagents liberally to keep main context window clean
- Offload research, exploration, and parallel analysis to subagents
- For complex problems, throw more compute at it via subagents
- One task per subagent for focused execution

### 3. Self-Improvement Loop
- After ANY correction from the user: update `tasks/lessons.md` with the pattern
- Write rules for yourself that prevent the same mistake
- Ruthlessly iterate on these lessons until mistake rate drops
- Review lessons at session start for relevant project

### 4. Verification Before Done
- Never mark a task complete without proving it works
- Diff behavior between main and your changes when relevant
- Ask yourself: "Would a staff engineer approve this?"
- Run tests, check logs, demonstrate correctness

### 5. Demand Elegance (Balanced)
- For non-trivial changes: pause and ask "is there a more elegant way?"
- If a fix feels hacky: "Knowing everything I know now, implement the elegant solution"
- Skip this for simple, obvious fixes - don't over-engineer
- Challenge your own work before presenting it

### 6. Autonomous Bug Fixing
- When given a bug report: just fix it. Don't ask for hand-holding
- Point at logs, errors, failing tests - then resolve them
- Zero context switching required from the user
- Go fix failing CI tests without being told how

## Task Management

1. **Plan First**: Write plan to `tasks/todo.md` with checkable items
2. **Verify Plan**: Check in before starting implementation
3. **Track Progress**: Mark items complete as you go
4. **Explain Changes**: High-level summary at each step
5. **Document Results**: Add review section to `tasks/todo.md`
6. **Capture Lessons**: Update `tasks/lessons.md` after corrections

## Core Principles

- **Simplicity First**: Make every change as simple as possible. Impact minimal code.
- **No Laziness**: Find root causes. No temporary fixes. Senior developer standards.
- **Minimal Impact**: Changes should only touch what's necessary. Avoid introducing bugs.
