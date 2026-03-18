# Project Research Summary

**Project:** TacticalTwo — Code Audit & Refactor Milestone
**Domain:** C23 game codebase audit, static/dynamic analysis, incremental refactoring
**Researched:** 2026-03-18
**Confidence:** HIGH

## Executive Summary

TacticalTwo is a C23 side-scrolling tactical platformer (~750–900 LOC of game logic across 13 source files) built on Cute Framework with a hot-reloadable game library (`libgame.dylib`). This milestone is a code quality milestone, not a feature milestone: the goal is to audit and refactor the existing codebase into a clean foundation before the next wave of feature work (enemies, projectiles, audio). The codebase has a sound overall architecture — fat-struct entity system, explicit system dispatch, 3-pass render pipeline — but has accumulated organic growth artifacts: duplicated coordinate conversion math, unsafe asset load paths, an unguarded CSV parser, and no protection against silent hot-reload layout corruption.

The recommended approach is a strict three-phase discipline: audit without changing behavior, extract and validate incrementally, then fix confirmed issues. The audit toolchain for macOS is clang-tidy + cppcheck for static analysis plus ASan + UBSan for runtime validation (Valgrind is excluded — no Apple Silicon support). Every structural change must be atomic and independently verifiable: no partial migrations, no bundled changes, no deleted build directories. The hot-reload mechanism is the highest-leverage and highest-risk aspect of the architecture: struct layout changes across the `dlopen` boundary silently corrupt state with no compiler warning, so adding a `GameState` version guard is the first structural change to make.

The top risks are: (1) hot-reload layout corruption from any `GameState`/`World`/`Entity` field addition, (2) dangling coroutine function pointers after library reload, (3) coordinate conversion divergence during extraction, and (4) render pipeline projection state left dirty between passes. All four are preventable with well-defined protocols rather than requiring architectural overhaul. The biggest trap is scope creep: full ECS migration, SoA data layout, and unit test suite creation are explicitly anti-features for this milestone — they solve non-problems at the current entity count and would consume the milestone budget without payoff.

---

## Key Findings

### Recommended Stack

The audit and refactor toolchain is entirely macOS-native Clang-based. clang-tidy is the primary linter (reads `compile_commands.json`, C23-aware via `-std=c23`), complemented by cppcheck for a second independent analysis pass — the two tools have low overlap in what they detect. The Clang Static Analyzer (`scan-build`) handles inter-procedural analysis without code changes. ASan + UBSan handle runtime validation and replace Valgrind (which has no Apple Silicon support). The existing `rake cmake:configure` target already supports `CMAKE_EXPORT_COMPILE_COMMANDS=ON`, making tool integration zero-cost.

The key infrastructure addition is a dedicated `Sanitize` CMake build type (`-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1`) that instruments both the host executable and `libgame.dylib`. Partial instrumentation (host without shared library) produces false positives across the `dlopen` boundary — this is a non-obvious requirement specific to the hot-reload architecture.

**Core technologies:**
- clang-tidy 18+: primary lint/bug detection — C23-aware, reads `compile_commands.json` natively
- cppcheck 2.14+: second-pass static analysis — detects different bug classes than clang-tidy
- scan-build (bundled Clang): inter-procedural analysis — no code changes required
- ASan + UBSan (bundled Clang): runtime sanitizers — macOS-native, replaces Valgrind
- LeakSanitizer (bundled with ASan on macOS): automatic with `-fsanitize=address`
- include-what-you-use 0.22+: header dependency audit — run in report-only mode against vendored CF
- clang-format: already configured, no changes needed

### Expected Refactoring Outcomes

This is a refactoring milestone. "Features" are correctness improvements and structural extractions that unblock future work.

**Must have (table stakes — correctness/safety issues):**
- Extract coordinate conversion module (`src/game/coords.h`) — duplicate code in physics and render systems will produce two-place bugs
- Active entity list / high-water mark — O(MAX_ENTITIES) scan on every frame scales linearly with every new system
- Guard parallax null-sprite render path — latent UB from partial load failure
- Fix composite canvas zero-size guard — crash path on minimized window
- Bounds-check LDtk CSV parser — out-of-bounds write on malformed CSV
- Unified error path for asset load failures — silent failures with stale pointers on hot-reload
- Rain arena allocation null check — silent memory corruption on arena exhaustion

**Should have (differentiators — structural improvements):**
- Hot-reload state layout validation (magic number + `sizeof(GameState)` check)
- Decouple PASS 3 composite logic into `pass_composite()` helper
- Lighting dirty flag / static light cache
- Parallax asset caching across world reloads (move to `GameState`)
- Explicit system ordering documentation in `world.c`
- LDtk entity field schema validation warnings
- Muzzle flash default initialization consolidation

**Defer (explicitly out of scope):**
- Full ECS migration (flecs, etc.) — unjustified at 1–20 entity scale; breaks hot-reload invariant
- SoA data layout — cache gains unmeasurable below ~1000 active entities
- Unit test suite — separate milestone; requires non-trivial headless CF setup
- Audio, save/load, entity-entity collision — feature work, not refactoring scope

### Architecture Approach

The refactoring goal is extraction and clarification, not replacement. The layered structure (host `app/` → platform `platform/` → engine `engine/` → game library `game/`) is sound. The two concrete additions are `src/game/coords.{h,c}` (coordinate conversion shared module) and `src/game/entity.h` (extracted entity/component type definitions). `src/engine/` should remain lean — only `game_state.h`, `log.h`, `platform.h`, `asset.{h,c}`, and the new `coords.{h,c}`. Lighting, rain, and LDtk belong in `src/game/` because they hold GPU resources and game-domain state.

**Major components:**
1. `src/game/coords.{h,c}` [NEW] — single source of truth for CF↔LDtk coordinate conversion; eliminates `physics_system.c`/`render_system.c` duplication
2. `src/game/entity.h` [EXTRACT] — entity/component struct definitions separated from world management
3. `GameState.version` field [NEW] — compile-time version constant validated in `game_hot_reload()`; prevents silent layout corruption
4. `World.entity_high_water` [NEW] — bounds system iteration to highest occupied index instead of scanning all 4096 slots every frame
5. `pass_composite()` helper [EXTRACT] — collapses duplicate debug/normal render paths into a single parameterized function

### Critical Pitfalls

1. **GameState/World layout change during hot reload** — adding any field to `GameState`, `World`, or `Entity` silently corrupts the live state pointer; add `uint32_t version` as the first `GameState` field, export `game_state_size()`, and treat size mismatch as a full reset trigger in `game_hot_reload()`

2. **Dangling coroutine pointer after library reload** — `CF_Coroutine` stores function pointers into the old `.dylib` text segment; `world_hot_reload()` must enumerate all entities (not just player) and call `cf_coroutine_destroy()` on every live coroutine before returning; verify by enabling ASan and triggering a reload mid-animation

3. **Partial coordinate module migration** — extracting `coords.h` while leaving one system on the old formula makes the debug collider overlay disagree with actual collision; the cut-over must be a single atomic commit: add header, remove both duplicate definitions, update all call sites; grep for zero remaining `cf_x_to_grid` definitions in `.c` files to verify

4. **Render pipeline projection state left dirty** — `cf_draw_projection()` is global mutable state with no stack-based save/restore; any early-return path in `game_render()` (minimized window, ensure-canvas failure) must explicitly restore the canvas ortho projection; test by resizing window rapidly during all render refactor phases

5. **CF_Canvas use-after-destroy** — `state->composite_canvas` is read by `game_update()` (ImGui) and destroyed/recreated by `ensure_composite_canvas()` in `game_render()`; set `composite_w = 0` immediately after `cf_destroy_canvas()` and guard every handle read with `composite_w > 0`

---

## Implications for Roadmap

Based on combined research, the refactoring milestone naturally structures into three phases, matching the audit-discipline methodology. Phase ordering is constrained by two rules: (a) hot-reload validation infrastructure must precede any struct-layout changes, and (b) coordinate extraction must be atomic and precede any system-level changes that touch the iteration loop.

### Phase 1: Correctness Fixes (Audit & Quick Wins)

**Rationale:** These are the lowest-risk, highest-payoff changes — each is a small, self-contained fix with no behavior change. They close active crash paths and UB before any structural work begins. Running audit tools (clang-tidy, cppcheck, ASan+UBSan) in this phase establishes the baseline before any changes corrupt the signal.

**Delivers:** A codebase with no confirmed crash paths, no known UB in asset loading, and a clean audit report as the baseline for Phase 2.

**Addresses:** All table-stakes items — coordinate extraction, parallax null guard, composite canvas zero-size, rain arena null check, LDtk CSV bounds check, unified asset error paths.

**Avoids:**
- Pitfall 3/8 (coordinate divergence): extraction is the very first change, done atomically
- Pitfall 9 (silent asset load failures): error paths hardened before caching layer is added
- Pitfall 12 (snprintf path truncation): surface during audit pass

**Tools needed:** clang-tidy + cppcheck (run first, report only), scan-build, then ASan+UBSan `Sanitize` build.

### Phase 2: Architecture — Hot-Reload Safety & Entity System

**Rationale:** All changes in this phase modify struct layout (`GameState`, `World`, `Entity`). The hot-reload version guard must be added first within this phase — it is the safety net for everything else. Entity high-water mark and `EntityType` field come after the guard is in place.

**Delivers:** A safe, validated hot-reload mechanism and an entity iteration loop that scales to 100+ entities without per-frame waste. Foundation for adding enemies and projectiles.

**Addresses:** Hot-reload layout validation, active entity high-water mark, `EntityType` field, `entity_cleanup()` helper, `world_remove_entity()` coroutine leak fix.

**Avoids:**
- Pitfall 1 (layout corruption): version guard is first change; treat mismatch as full reset
- Pitfall 2 (dangling coroutine): `entity_cleanup()` handles coroutine destroy before slot zeroing
- Pitfall 6 (active-list snapshot misses mid-frame spawns): audit all `world_add_entity()` calls in systems before choosing snapshot vs. dynamic semantics
- Pitfall 7 (coroutine leak on level reload): prerequisite `entity_cleanup()` fix applied before iteration refactor

**Tools needed:** ASan+UBSan `Sanitize` build after each struct change to confirm no new violations.

**Research flag:** This phase needs verification of the `platform_cute.h` integration point for the version check — the exact location where the host calls `game_hot_reload()` needs inspection before the size-comparison guard is added.

### Phase 3: Render Pipeline & Developer Velocity

**Rationale:** Render refactoring carries the highest visual regression risk and should come after correctness and entity-system work is stable. Developer velocity improvements (lighting dirty flag, parallax caching, documentation) have no dependencies and are the lowest-risk changes in the milestone.

**Delivers:** A deduplicated render pipeline with explicit pass contracts, reduced per-reload disk reads for parallax, and a lighting system that can run in more scenes without frame budget concern. Documentation that makes system ordering explicit for future contributors.

**Addresses:** PASS 3 composite helper extraction, `world_collect_lights()` decoupling, lighting dirty flag, parallax asset caching in `GameState`, system ordering comment block, LDtk field schema warnings, muzzle flash initialization consolidation.

**Avoids:**
- Pitfall 4 (projection state dirty): test on window resize after every sub-step
- Pitfall 5 (canvas use-after-destroy): set `composite_w = 0` immediately after destroy
- Pitfall 10 (lighting cache breaks dynamic light correctness): cache key must include camera position, or restructure to level-space map
- Pitfall 11 (projection omitted in early-return paths): add explicit restore at every early-return in `game_render()`

**Research flag:** Parallax asset caching moves `Parallax` from `World` to `GameState` — this is a layout change that triggers Phase 2's version bump requirement. Sequence this after the version guard is in place.

### Phase Ordering Rationale

- Correctness before architecture: fixing active crash paths gives a stable baseline before struct modifications multiply the blast radius of any bug
- Version guard before layout changes: every Phase 2 and Phase 3 change that touches struct layout requires the guard to already exist; without it, the hot-reload mechanism is flying blind
- Coordinate extraction first in Phase 1: it's the change most likely to introduce subtle divergence if done incorrectly; doing it first while the codebase is freshest reduces risk
- Render last: visual regressions are hardest to catch programmatically; stable entity system makes regression comparison easier
- Anti-features excluded throughout: ECS migration, SoA layout, and test suite are explicitly deferred; the milestone is bounded

### Research Flags

Phases likely needing deeper research during planning:

- **Phase 2 (hot-reload version guard):** The integration point in `platform_cute.c` or `app/main.c` needs inspection to determine where `game_hot_reload()` is called and what the exact handshake looks like. STACK.md has medium confidence on this integration (source: cr.h reference, TacticalTwo-specific wiring unverified).
- **Phase 2 (coroutine enumeration in `world_hot_reload()`):** The current `world_hot_reload()` code needs to be read in full to confirm it correctly enumerates all entity coroutines, not just the player. PITFALLS.md flags this as a race condition risk.
- **Phase 3 (lighting absorption camera-relative semantics):** PITFALLS.md Pitfall 10 identifies a correctness constraint on any lighting caching: the absorption map is camera-relative, not level-relative. The dirty-flag implementation in FEATURES.md (differentiators) may be incomplete as written; needs design review before implementation.

Phases with standard patterns (can proceed without additional research):

- **Phase 1 (coordinate extraction):** Pure code extraction with a well-defined interface. ARCHITECTURE.md provides the exact target interface (`coords_cf_x_to_grid`, `coords_cf_y_to_grid`, `coords_grid_cell_aabb`). Pattern is textbook.
- **Phase 1 (asset error path hardening):** additive null checks and `log_error` calls. No novel patterns.
- **Phase 3 (PASS 3 composite extraction):** ARCHITECTURE.md Pattern 4 provides the exact `RenderPassParams` struct and function signatures. Standard C refactor.

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack (audit toolchain) | HIGH | Clang documentation is authoritative; ASan/UBSan on macOS explicitly documented; Valgrind exclusion confirmed (no ARM support) |
| Features (table stakes) | HIGH | Directly derived from source code analysis — no inference required; all issues confirmed by line-number citations |
| Features (differentiators) | MEDIUM | Value claims depend on future feature trajectory; plausible but unverified against actual frame budget |
| Architecture | HIGH | Patterns derived from direct codebase analysis + established hot-reload references; two patterns (GameState versioning, render pass struct) have MEDIUM confidence on exact integration point |
| Pitfalls | HIGH | All critical pitfalls grounded in codebase-specific evidence; community sources corroborate hot-reload coroutine and struct-layout risks |

**Overall confidence:** HIGH

### Gaps to Address

- **`platform_cute.c` hot-reload handshake:** The exact location of `game_hot_reload()` invocation in the platform layer needs inspection before implementing the version size check. Read `src/platform/platform_cute.c` and `src/app/main.c` at the start of Phase 2 planning.
- **cppcheck C23 feature coverage:** Source code inspection shows C23 in cppcheck's standards enum, but full C23 feature coverage (e.g., `typeof`, `_BitInt`) is unverified. Run cppcheck and flag any false positives on C23-specific constructs during the audit phase.
- **ASan + codesigned dylib interaction on macOS:** The interaction of ASan with codesign'd shared libraries on macOS (especially if the dylib is ad-hoc signed) has not been tested in this specific environment. Run a smoke test (`-fsanitize=address` build + launch) before committing to the `Sanitize` build type for the full audit.
- **Lighting dirty flag design:** The camera-relative absorption semantics identified in PITFALLS.md Pitfall 10 create a constraint the FEATURES.md dirty-flag proposal does not fully address. Needs design resolution (camera-keyed cache vs. level-space map crop) before implementation.

---

## Sources

### Primary (HIGH confidence — official documentation)

- [Clang-Tidy Documentation](https://clang.llvm.org/extra/clang-tidy/) — check suites, C23 via `-std=c23`
- [AddressSanitizer — Clang Documentation](https://clang.llvm.org/docs/AddressSanitizer.html) — macOS support, shared library instrumentation
- [UndefinedBehaviorSanitizer — Clang Documentation](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)
- [JSON Compilation Database Specification](https://clang.llvm.org/docs/JSONCompilationDatabase.html) — `compile_commands.json` for clang-tidy
- Direct codebase analysis: `src/game/game.c`, `src/game/world.{h,c}`, `src/game/systems/physics_system.c`, `src/game/systems/render_system.c`, `src/engine/game_state.h`, `src/game/ldtk.c`, `.planning/codebase/CONCERNS.md`, `.planning/codebase/ARCHITECTURE.md`

### Secondary (MEDIUM confidence — community consensus)

- [cr.h: A Simple C Hot Reload Header-only Library](https://fungos.github.io/cr-simple-c-hot-reload/) — struct versioning, size-based invalidation, state migration patterns
- [Exile: Hot Reloading — thenumb.at](https://thenumb.at/Hot-Reloading-in-Exile/) — struct size comparison as full-reset trigger
- [Object Pool Pattern — Game Programming Patterns](https://gameprogrammingpatterns.com/object-pool.html) — entity pool management
- [Interactive Programming in C — nullprogram.com](https://nullprogram.com/blog/2014/12/23/) — struct layout and function pointer issues with dlopen hot reload
- [cppcheck GitHub — Standards header (C23 enum)](https://github.com/danmar/cppcheck/blob/main/lib/standards.h) — C23 flag confirmed in source
- [include-what-you-use GitHub](https://github.com/include-what-you-use/include-what-you-use) — behavior with vendored headers
- [A gentle introduction to static analyzers for C — nrk.neocities.org](https://nrk.neocities.org/articles/c-static-analyzers) — clang-tidy + cppcheck complementarity

### Tertiary (LOW confidence — secondary or inferred)

- [Sanitizers as Valgrind alternative — LinuxJedi](https://linuxjedi.co.uk/sanitizers-the-alternative-to-valgrind/) — Valgrind exclusion rationale
- [Decoupling the Graphics System — GameDev.net](https://www.gamedev.net/forums/topic/652771-decoupling-the-graphics-system-render-engine/) — render pass decoupling patterns
- [GDScript coroutine crash on hot reload #24684 — godotengine/godot](https://github.com/godotengine/godot/issues/24684) — coroutine pointer crash pattern (different language, same root cause)

---
*Research completed: 2026-03-18*
*Ready for roadmap: yes*
