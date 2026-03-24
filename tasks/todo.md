# Zig Game Code Migration Plan

## Overview

Replace `src/game/` (C) with `src/game_zig/` (Zig) while keeping:
- The app/platform layer in C (`src/app/`, `src/platform/`)
- Cute Framework as the graphics/input/audio backend
- Hot-reloading via the existing shared library mechanism

## Phase 1: Proof of Concept ✅

- [x] Create `src/game_zig/build.zig` — Zig build config for shared library
- [x] Create `src/game_zig/cf.zig` — Thin wrapper over CF's C API via `@cImport`
- [x] Create `src/game_zig/game.zig` — Minimal game exporting 6 C-ABI functions
- [ ] Install Zig toolchain (0.13+ recommended)
- [ ] Build CF with CMake first: `rake cmake:configure && rake`
- [ ] Build Zig game lib: `cd src/game_zig && zig build -Dcf-lib-path=../../build/relwithdebinfo/bin`
- [ ] Copy `zig-out/lib/libgame.so` → `build/relwithdebinfo/bin/libgame.so`
- [ ] Run TacticalTwo executable — verify bouncing white box renders

## Phase 2: Incremental Port (system by system)

Port each system one at a time, testing hot-reload after each:

- [ ] Port data structures: `Entity`, `World`, component types
- [ ] Port `input_system` (simplest — just reads keyboard state)
- [ ] Port `physics_system` (velocity, gravity)
- [ ] Port `camera_system` (smooth follow)
- [ ] Port `player_system` (coroutine state machine — may need rethinking)
- [ ] Port `animation_system`
- [ ] Port `tilemap_system`
- [ ] Port `render_system`
- [ ] Port `parallax_system`

## Phase 3: Complex Subsystems

- [ ] Port `ldtk.c` (LDtk level loader — JSON parsing, asset loading)
- [ ] Port `rain.c` (particle system + shader)
- [ ] Port `lighting.c` (HRC compute shader pipeline)
- [ ] Port ImGui debug UI

## Phase 4: Build System Unification

- [ ] Evaluate: keep dual build (CMake + Zig) vs. all-Zig
- [ ] If all-Zig: port CF compilation to `build.zig` (CF is C/C++, Zig can compile it)
- [ ] Update `Rakefile` for Zig workflow
- [ ] Update `rake watch` for Zig file watching

## Architecture Decisions

### `@cImport` vs Manual Bindings
Start with `@cImport` — it handles CF headers automatically. Only create manual
bindings if `@cImport` can't handle specific macros or if we want more
Zig-idiomatic APIs.

### GameState Compatibility
Phase 1 uses a Zig-native `GameState`. This means hot-reload between C and Zig
versions won't work — you must restart when switching. Phase 2+ can use
`extern struct` to match the C layout if cross-language hot-reload is desired.

### Coroutines
CF's coroutine API (used for player state machine) is C-based. Options:
1. Continue using CF coroutines via C interop
2. Rewrite as a Zig state machine (tagged union + switch)
3. Use Zig's async (if/when it stabilizes)

Recommendation: Option 2 — a tagged union state machine is more idiomatic Zig
and easier to debug than C coroutines.

### Entity System
The current fat-struct `Entity` pattern works fine in Zig. Later, Zig's comptime
could enable a more flexible ECS (generate component arrays at compile time).

## How to Test

```bash
# 1. Build CF and the C app (only needed once, or when CF changes)
rake cmake:configure
rake

# 2. Build the Zig game library
cd src/game_zig
zig build -Doptimize=ReleaseSafe -Dcf-lib-path=../../build/relwithdebinfo/bin

# 3. Copy into place (or use `zig build deploy`)
cp zig-out/lib/libgame.so ../../build/relwithdebinfo/bin/libgame.so

# 4. Run
../../build/relwithdebinfo/bin/TacticalTwo
```

## Risks & Mitigations

| Risk | Mitigation |
|------|------------|
| `@cImport` fails on CF macros | Fall back to manual `extern fn` declarations |
| CF struct layout mismatch | Use `@sizeOf`/`@offsetOf` assertions in tests |
| Zig compile times slow for CF headers | Cache with `zig build` incremental compilation |
| Hot-reload breaks | State is a simple pointer — same mechanism works for any .so |
