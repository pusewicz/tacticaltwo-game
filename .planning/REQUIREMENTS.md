# Requirements: TacticalTwo — Code Audit & Refactor

**Defined:** 2026-03-18
**Core Value:** Clean, safe foundation for future feature development without breaking hot-reload workflow

## v1 Requirements

Requirements for this refactoring milestone. Each maps to roadmap phases.

### Correctness

- [ ] **CORR-01**: Extract coordinate conversion functions (`cf_x_to_grid`, `cf_y_to_grid`, `grid_cell_aabb`) into shared `src/game/coords.h` module, removing duplicates from physics_system.c and render_system.c
- [ ] **CORR-02**: Guard parallax render path against null/failed sprite loads — check individual load results per-sprite, skip render if load failed
- [ ] **CORR-03**: Fix composite canvas zero-size guard — early return in game_render PASS 3 when window dimensions are zero, restore projection state
- [ ] **CORR-04**: Add bounds check to LDtk CSV parser (`parse_csv`) before `grid[idx]` write — validate parsed dimensions match expected grid size
- [ ] **CORR-05**: Add unified error paths for asset load failures in `init_world` — explicit validity checks and fail-fast on critical assets
- [ ] **CORR-06**: Add null check after rain arena allocation in `rain_init` — log error and disable rain if allocation fails
- [ ] **CORR-07**: Set up clang-tidy configuration with `compile_commands.json` and cppcheck for static analysis
- [ ] **CORR-08**: Add ASan/UBSan Sanitize CMake build type with full instrumentation of host and shared library

### Entity System

- [ ] **ENTS-01**: Add entity high-water mark (`World.entity_high_water`) to bound system iteration to highest occupied index instead of scanning all 4096 slots
- [ ] **ENTS-02**: Add hot-reload state layout validation — `GameState.version` field with magic number + `sizeof(GameState)` check in `game_hot_reload()`, treat mismatch as full reset
- [ ] **ENTS-03**: Add `entity_cleanup()` helper that destroys coroutines before zeroing entity slot — fix coroutine leak in `world_remove_entity()` and level reload paths

### Render Pipeline

- [ ] **REND-01**: Extract PASS 3 composite logic into `pass_composite()` helper — collapse duplicate debug/normal render paths into single parameterized function
- [ ] **REND-02**: Add lighting dirty flag to `LightingState` — skip `lighting_build_absorption()` and `lighting_compute()` on frames where camera, lights, and tilemap are unchanged
- [ ] **REND-03**: Cache parallax sprites in `GameState` (outside `World`) — persist across hot-reloads and level transitions, eliminating repeated PNG decode from disk

### Developer Velocity

- [ ] **DEVX-01**: Document system ordering contract in `world.c` — comment block explaining dispatch order dependencies (input before movement, velocity before collision, etc.)
- [ ] **DEVX-02**: Add LDtk entity field schema validation — `log_warn` when expected custom fields (Color, Intensity, Radius, Direction, ConeAngle) are missing from level data
- [ ] **DEVX-03**: Consolidate muzzle flash default initialization — move split defaults from `game_update()` ImGui block and `world_trigger_muzzle_flash()` into single init site

## v2 Requirements

Deferred to future milestones. Tracked but not in current roadmap.

### Testing

- **TEST-01**: Unit tests for coordinate conversion edge cases
- **TEST-02**: Integration tests for LDtk level loading pipeline
- **TEST-03**: Tests for hot-reload entity lifecycle
- **TEST-04**: Tests for physics integration edge cases

### Features

- **FEAT-01**: Entity-entity collision system
- **FEAT-02**: Audio system integration
- **FEAT-03**: Save/load game state
- **FEAT-04**: Input rebinding and gamepad support
- **FEAT-05**: Pause menu / game state management

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Full ECS library migration (flecs, etc.) | Unjustified at 1-20 entity scale; breaks hot-reload invariant |
| SoA data layout | Cache gains unmeasurable below ~1000 active entities |
| Runtime entity type dispatch (vtable) | Coroutines already provide behavioral polymorphism |
| Replacing scratch arena with general allocator | Arena provides deterministic frame-memory behavior |
| Breaking GameState across DLL with versioned structs | Excessive for single-developer project; lightweight approach sufficient |
| Extracting separate render module from game.c | game_render is ~100 lines, already well-commented; composite helper is enough |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| CORR-01 | — | Pending |
| CORR-02 | — | Pending |
| CORR-03 | — | Pending |
| CORR-04 | — | Pending |
| CORR-05 | — | Pending |
| CORR-06 | — | Pending |
| CORR-07 | — | Pending |
| CORR-08 | — | Pending |
| ENTS-01 | — | Pending |
| ENTS-02 | — | Pending |
| ENTS-03 | — | Pending |
| REND-01 | — | Pending |
| REND-02 | — | Pending |
| REND-03 | — | Pending |
| DEVX-01 | — | Pending |
| DEVX-02 | — | Pending |
| DEVX-03 | — | Pending |

**Coverage:**
- v1 requirements: 17 total
- Mapped to phases: 0
- Unmapped: 17

---
*Requirements defined: 2026-03-18*
*Last updated: 2026-03-18 after initial definition*
