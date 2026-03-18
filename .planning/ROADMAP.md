# Roadmap: TacticalTwo — Code Audit & Refactor

## Overview

This milestone transforms TacticalTwo's organically-grown codebase into a clean, safe foundation for future feature work. The journey runs in four stages: establish the audit toolchain first (phases 1), fix the correctness issues the audit reveals (phases 2-4), harden the hot-reload and entity-system architecture (phases 5-6), then refactor the render pipeline and lock in developer-velocity wins (phases 7-10). Every phase is a single focused session that compiles, runs, and leaves the game behaviorally identical to before.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Audit Toolchain** - Wire up clang-tidy, cppcheck, and ASan/UBSan so every subsequent change has a static and runtime validation baseline
- [ ] **Phase 2: Coordinate Extraction** - Atomically extract duplicated coordinate conversion math into `src/game/coords.h`, removing both copies from physics and render systems
- [ ] **Phase 3: Null and Zero Guards** - Harden parallax render, composite canvas, and rain arena against null/zero-dimension inputs
- [ ] **Phase 4: Asset and LDtk Error Paths** - Add bounds check to CSV parser and unified fail-fast error handling for critical asset loads
- [ ] **Phase 5: Hot-Reload Version Guard** - Add `GameState.version` magic + size check in `game_hot_reload()` so layout mismatches trigger a full reset instead of silent corruption
- [ ] **Phase 6: Entity System Hardening** - Add entity high-water mark to bound iteration and `entity_cleanup()` helper to fix coroutine leaks on removal and level reload
- [ ] **Phase 7: Render Pass Extraction** - Extract PASS 3 composite logic into `pass_composite()`, collapsing duplicate debug/normal render paths
- [ ] **Phase 8: Lighting Dirty Flag** - Add dirty-flag mechanism to `LightingState` so static scenes skip absorption and compute each frame
- [ ] **Phase 9: Parallax Asset Caching** - Move parallax sprites from `World` into `GameState` so they survive hot-reloads and level transitions without repeated PNG decodes
- [ ] **Phase 10: Developer Velocity** - Document system ordering, add LDtk field schema warnings, and consolidate muzzle flash initialization

## Phase Details

### Phase 1: Audit Toolchain
**Goal**: The codebase has a working static analysis and runtime sanitizer baseline before any changes are made
**Depends on**: Nothing (first phase)
**Requirements**: CORR-07, CORR-08
**Success Criteria** (what must be TRUE):
  1. `clang-tidy` runs against all source files using `compile_commands.json` and produces a report without crashing or skipping files
  2. `cppcheck` runs with `--std=c23` and produces a second independent report
  3. A `Sanitize` CMake build type exists that instruments both the host executable and `libgame.dylib` with ASan and UBSan
  4. The game launches and runs under the `Sanitize` build without immediate false-positive crashes
  5. `.clang-tidy` config file is committed alongside `compile_commands.json` generation in the build system
**Plans:** 2 plans
Plans:
- [ ] 01-01-PLAN.md — Add Sanitize CMake build type with ASan/UBSan and make it the default dev build
- [ ] 01-02-PLAN.md — Add clang-tidy and cppcheck static analysis Rake tasks with baseline reports

### Phase 2: Coordinate Extraction
**Goal**: Coordinate conversion lives in exactly one place (`src/game/coords.h`) with zero duplicate definitions remaining in the codebase
**Depends on**: Phase 1
**Requirements**: CORR-01
**Success Criteria** (what must be TRUE):
  1. `src/game/coords.h` and `src/game/coords.c` exist with `coords_cf_x_to_grid`, `coords_cf_y_to_grid`, and `coords_grid_cell_aabb` as the canonical implementations
  2. `grep` for the old inline conversion formulas in `physics_system.c` and `render_system.c` returns zero matches
  3. The game compiles, runs, and the debug collision overlay visually aligns with actual collision geometry (no divergence)
  4. ASan/UBSan Sanitize build passes cleanly after extraction
**Plans**: TBD

### Phase 3: Null and Zero Guards
**Goal**: Three latent crash/UB paths are closed — parallax null sprite, composite canvas zero dimensions, and rain arena allocation failure
**Depends on**: Phase 2
**Requirements**: CORR-02, CORR-03, CORR-06
**Success Criteria** (what must be TRUE):
  1. Minimizing the game window to zero dimensions no longer crashes or leaves projection state dirty; the frame skips cleanly and rendering resumes when the window is restored
  2. Removing a parallax PNG from disk and reloading the game logs an error per failed sprite and renders remaining layers without undefined behavior
  3. Rain initialization with a forced allocation failure (or exhausted arena) logs an error and disables rain rather than corrupting memory
  4. ASan/UBSan Sanitize build reports no new violations after these guards are added
**Plans**: TBD

### Phase 4: Asset and LDtk Error Paths
**Goal**: The LDtk CSV parser cannot write out of bounds, and critical asset load failures in `init_world` are explicit and fail-fast rather than silent
**Depends on**: Phase 3
**Requirements**: CORR-04, CORR-05
**Success Criteria** (what must be TRUE):
  1. Feeding the CSV parser a malformed level file (wrong dimensions) logs an error and returns without writing past the grid boundary
  2. A missing critical asset (player sprite, tileset) causes `init_world` to log an explicit error and return early rather than continuing with a null pointer
  3. The game handles a corrupted `.ldtk` file without a segfault — graceful error message, no crash
  4. ASan/UBSan Sanitize build shows no out-of-bounds writes during level load
**Plans**: TBD

### Phase 5: Hot-Reload Version Guard
**Goal**: Struct layout mismatches between old and new `libgame.dylib` trigger an explicit full reset in `game_hot_reload()` instead of silently corrupting live state
**Depends on**: Phase 4
**Requirements**: ENTS-02
**Success Criteria** (what must be TRUE):
  1. `GameState` has a `version` field (magic constant + sizeof combination) as its first member
  2. `game_hot_reload()` compares the stored version against the newly loaded library's version and logs a warning plus triggers full reset on mismatch
  3. A deliberate struct layout change (add a dummy field, rebuild) triggers the version mismatch path visibly in the log rather than crashing or producing corrupted behavior
  4. Normal hot-reload (no layout change) continues to work correctly with state preserved
**Plans**: TBD

### Phase 6: Entity System Hardening
**Goal**: System iteration is bounded by `entity_high_water` rather than scanning all 4096 slots, and entity removal correctly destroys coroutines before zeroing the slot
**Depends on**: Phase 5
**Requirements**: ENTS-01, ENTS-03
**Success Criteria** (what must be TRUE):
  1. `World.entity_high_water` tracks the highest occupied entity index and all systems iterate `[0, entity_high_water]` instead of `[0, MAX_ENTITIES)`
  2. `entity_cleanup()` exists and calls `cf_coroutine_destroy()` on any live coroutine before zeroing the entity slot
  3. `world_remove_entity()` and level-reload paths call `entity_cleanup()` — ASan/UBSan Sanitize build detects no use-after-free on hot-reload mid-animation
  4. A hot-reload triggered while the player is mid-animation completes without crashing or leaving dangling coroutine pointers
**Plans**: TBD

### Phase 7: Render Pass Extraction
**Goal**: PASS 3 composite logic lives in a single `pass_composite()` function, eliminating the duplicated debug/normal render paths
**Depends on**: Phase 6
**Requirements**: REND-01
**Success Criteria** (what must be TRUE):
  1. `pass_composite()` function exists and is the sole implementation of PASS 3 compositing, parameterized for both debug and normal modes
  2. The duplicate render paths that previously existed in `game_render()` are removed
  3. The game renders visually identically in both normal and debug (ImGui docked viewport) modes after extraction
  4. Rapid window resizing during play does not leave projection state dirty or cause visual artifacts
**Plans**: TBD

### Phase 8: Lighting Dirty Flag
**Goal**: The lighting system skips `lighting_build_absorption()` and `lighting_compute()` on frames where camera position, light list, and tilemap are all unchanged
**Depends on**: Phase 7
**Requirements**: REND-02
**Success Criteria** (what must be TRUE):
  1. `LightingState` has a dirty flag that is set when camera position changes, when any light is added/removed/modified, or when the tilemap changes
  2. Static scenes (camera stationary, no dynamic lights changing) skip the full lighting compute for the majority of frames
  3. Dynamic light effects (muzzle flash) still correctly trigger recomputation — the muzzle flash visual is unchanged
  4. Moving the camera correctly marks lighting dirty and updates the absorption map without visual lag or stale shadows
**Plans**: TBD

### Phase 9: Parallax Asset Caching
**Goal**: Parallax sprites live in `GameState` (outside `World`) so they survive hot-reloads and level transitions without re-decoding PNGs from disk each time
**Depends on**: Phase 8
**Requirements**: REND-03
**Success Criteria** (what must be TRUE):
  1. Parallax sprite handles are stored in `GameState` (not `World`) and survive a hot-reload without being destroyed and recreated
  2. A level transition does not trigger parallax PNG re-decode — sprites loaded once persist until game shutdown
  3. The hot-reload version guard (Phase 5) is bumped to account for the `GameState` layout change from moving `Parallax` out of `World`
  4. Parallax rendering is visually identical before and after the move — no artifacts, no missing layers
**Plans**: TBD

### Phase 10: Developer Velocity
**Goal**: System ordering is documented, LDtk field schema mismatches surface as warnings, and muzzle flash has a single initialization site
**Depends on**: Phase 9
**Requirements**: DEVX-01, DEVX-02, DEVX-03
**Success Criteria** (what must be TRUE):
  1. `world.c` has a comment block before the system dispatch calls that explicitly states the ordering contract and which systems depend on prior systems having run
  2. Loading a level whose entity definitions are missing expected custom fields (Color, Intensity, Radius, Direction, ConeAngle) produces `log_warn` output identifying each missing field by name
  3. Muzzle flash default values are defined in exactly one place — the ImGui block and `world_trigger_muzzle_flash()` no longer each hold a separate copy
  4. The game compiles cleanly with no new warnings after all three changes
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9 → 10

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Audit Toolchain | 0/2 | Planning complete | - |
| 2. Coordinate Extraction | 0/? | Not started | - |
| 3. Null and Zero Guards | 0/? | Not started | - |
| 4. Asset and LDtk Error Paths | 0/? | Not started | - |
| 5. Hot-Reload Version Guard | 0/? | Not started | - |
| 6. Entity System Hardening | 0/? | Not started | - |
| 7. Render Pass Extraction | 0/? | Not started | - |
| 8. Lighting Dirty Flag | 0/? | Not started | - |
| 9. Parallax Asset Caching | 0/? | Not started | - |
| 10. Developer Velocity | 0/? | Not started | - |
