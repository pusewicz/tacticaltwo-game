# TacticalTwo — Code Audit & Refactor

## What This Is

A comprehensive code audit and refactoring pass across the TacticalTwo game codebase — a C23 side-scrolling tactical platformer built on Cute Framework with hot-reloading. The goal is to identify architectural weaknesses, clean up tech debt, and establish a solid foundation before the next wave of feature development.

## Core Value

Every refactoring change must leave the codebase easier to extend with new features (enemies, levels, audio, multiplayer) without breaking existing behavior or hot-reload workflow.

## Requirements

### Validated

<!-- Existing capabilities inferred from codebase map -->

- ✓ Hot-reloadable game library with state preservation — existing
- ✓ Fat-struct entity system with component-enabled flags — existing
- ✓ 3-pass rendering pipeline (game scene → HRC lighting → composite) — existing
- ✓ LDtk level loading with tile layers, collision grid, entity spawning — existing
- ✓ Player input, physics, coroutine-driven state machine — existing
- ✓ HRC global illumination with static and dynamic lights — existing
- ✓ Procedural rain with compute shader and tilemap collision — existing
- ✓ Parallax scrolling background layers — existing
- ✓ Camera follow with level bounds clamping — existing
- ✓ ImGui debug overlay with docked viewport — existing
- ✓ Muzzle flash dynamic light effect — existing

### Active

<!-- Refactoring goals for this milestone -->

- [ ] Audit game loop and system dispatch for clarity and separation of concerns
- [ ] Audit render pipeline (3-pass compositing, shader setup, blend states)
- [ ] Audit asset pipeline (LDtk loading, sprite loading, parallax assets)
- [ ] Audit overall architecture (module boundaries, state management, hot-reload design)
- [ ] Extract duplicated coordinate conversion code into shared module
- [ ] Refactor entity system iteration (active entity tracking vs full-array scan)
- [ ] Refactor render pipeline for clearer pass separation and error handling
- [ ] Refactor asset loading with proper caching and error paths
- [ ] Improve hot-reload robustness (state versioning, layout validation)
- [ ] Address fragile areas identified in codebase concerns audit

### Out of Scope

- New gameplay features (enemies, weapons, new mechanics) — this is foundation work
- Audio system — separate milestone
- Save/load system — separate milestone
- Entity-entity collision — separate milestone
- Test suite creation — may be a follow-up milestone, but not the primary goal here
- Performance optimization beyond what falls out of structural improvements — not chasing benchmarks

## Context

The codebase has grown organically through feature additions (lighting, rain, muzzle flash, debug overlay). The concerns audit identified:
- **Coordinate conversion duplication** across physics and render systems
- **Full-array entity iteration** in all systems (4096 slots scanned even with 1-10 entities)
- **No state versioning** for hot-reload safety
- **Silent failures** in asset loading and LDtk parsing
- **Fragile areas** in rain texture generation, composite canvas lifecycle, and LDtk entity spawning
- **Lighting compute runs every frame** even with static scenes

The codebase map (`.planning/codebase/`) has detailed analysis of all these areas.

## Constraints

- **Hot-reload compatibility**: All refactoring must preserve the hot-reload workflow — `GameState` struct changes require careful migration
- **Cute Framework API**: Stay within CF's API surface; don't fight the framework
- **Behavioral equivalence**: Game must look and play identically after refactoring — no visual or gameplay regressions
- **Incremental**: Each refactoring step should compile and run; no big-bang rewrites

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Bold refactoring allowed | User wants clean foundation, not cosmetic cleanup | — Pending |
| Analyze then fix | Audit all areas first, then refactor based on findings | — Pending |
| All areas in scope | Game loop, shaders, assets, architecture all included | — Pending |

---
*Last updated: 2026-03-18 after initialization*
