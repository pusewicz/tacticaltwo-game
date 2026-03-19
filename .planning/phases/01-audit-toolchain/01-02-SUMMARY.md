---
phase: 01-audit-toolchain
plan: 02
subsystem: toolchain
tags: [clang-tidy, cppcheck, static-analysis, rake, advisory-reports]

# Dependency graph
requires:
  - phase: 01-audit-toolchain/01-01
    provides: cmake:sanitize:configure task and compile_commands.json symlink pointing to build/sanitize/
provides:
  - analyze:clang_tidy Rake task — runs run-clang-tidy against all src/ files with correct source filter
  - analyze:cppcheck Rake task — runs cppcheck with --std=c23 on src/ files only
  - analyze:all Rake task — runs both analysis tools in sequence
  - reports/clang-tidy.txt — baseline static analysis report (725 lines, .gitignored)
  - reports/cppcheck.txt — baseline cppcheck report (21134 lines, .gitignored)
  - /reports/ in .gitignore — prevents baseline reports from entering git history
affects:
  - all future phases (baseline reports measure regressions/improvements)

# Tech tracking
tech-stack:
  added:
    - cppcheck 2.20.0 (brew install cppcheck)
  patterns:
    - Advisory-only analysis: tasks use --error-exitcode=0 and "; true" to never block the build
    - Source filter discrimination: -source-filter '.*/tacticaltwo-game/src/.*' excludes SDL3 _deps/ files
    - macOS SDK resolution: -extra-arg=-isysroot$(xcrun --show-sdk-path) for Homebrew LLVM clang-tidy

key-files:
  created:
    - reports/clang-tidy.txt (baseline, .gitignored, 725 lines)
    - reports/cppcheck.txt (baseline, .gitignored, 21134 lines)
  modified:
    - Rakefile (added analyze:clang_tidy, analyze:cppcheck, analyze:all tasks — 32 lines added)
    - .gitignore (added /reports/ exclusion)

key-decisions:
  - "Reports are advisory only — analyze tasks always exit 0 (--error-exitcode=0, ; true) — never block the build"
  - "Source filter uses project name discriminator: .*/tacticaltwo-game/src/.* (NOT .*\\/src\\/.*) to exclude SDL3 deps"
  - "clang_tidy task depends on cmake:sanitize:configure to ensure compile_commands.json is up-to-date"
  - "cppcheck task does NOT depend on cmake:sanitize:configure — uses root symlink directly (already correct from Plan 01)"

patterns-established:
  - "Pattern: Advisory Rake tasks — sh '... || true' or --error-exitcode=0 ensures Rake always sees exit 0"
  - "Pattern: Source discrimination — always include project dir name in source filter paths"
  - "Pattern: macOS SDK headers — Homebrew LLVM tools require -isysroot xcrun --show-sdk-path"

requirements-completed: [CORR-07]

# Metrics
duration: 20min
completed: 2026-03-19
---

# Phase 01 Plan 02: Static Analysis Tasks Summary

**Two advisory Rake tasks (analyze:clang_tidy, analyze:cppcheck) producing baseline reports from 17 src/ files using run-clang-tidy 22.1.1 and cppcheck 2.20.0**

## Performance

- **Duration:** ~20 min
- **Started:** 2026-03-18T18:40:06Z
- **Completed:** 2026-03-19T13:39:53Z
- **Tasks:** 1 of 1
- **Files modified:** 2

## Accomplishments

- Installed cppcheck 2.20.0 via Homebrew (system dependency)
- Added `analyze:clang_tidy` Rake task that runs run-clang-tidy against 17 src/ files with correct macOS SDK resolution and source filter excluding SDL3 deps
- Added `analyze:cppcheck` Rake task with --std=c23, --enable=all, scoped to src/ via --file-filter
- Added `analyze:all` convenience task running both tools in sequence
- Generated baseline reports: reports/clang-tidy.txt (725 lines) and reports/cppcheck.txt (21134 lines)
- Added /reports/ to .gitignore so baseline reports never pollute git history

## Task Commits

Each task was committed atomically:

1. **Task 1: Install cppcheck and add static analysis Rake tasks** - `32e820e` (feat)

**Plan metadata:** `[pending]` (docs: complete plan)

## Files Created/Modified

- `Rakefile` - Added analyze:clang_tidy, analyze:cppcheck, analyze:all tasks (32 lines)
- `.gitignore` - Added /reports/ exclusion

## Decisions Made

- Reports are advisory only — both tasks use exit-0 patterns (--error-exitcode=0 for cppcheck, "; true" shell suffix for clang-tidy via tee pipe) — never block the build
- Source filter uses project name discriminator: `.*/tacticaltwo-game/src/.*` (NOT `.*\/src\/.*`) to correctly exclude SDL3 `_deps/sdl3-src/src/*.c` files from compile_commands.json
- `analyze:clang_tidy` depends on `cmake:sanitize:configure` to ensure compile_commands.json exists and is current before running analysis
- `analyze:cppcheck` does NOT depend on `cmake:sanitize:configure` — uses the root compile_commands.json symlink directly, which was already correctly retargeted by Plan 01

## Deviations from Plan

None — plan executed exactly as written.

## Issues Encountered

None — cppcheck 2.20.0 installed cleanly. Both tools ran against 17 src/ files without issues. The clang-tidy -isysroot extra-arg correctly resolved macOS SDK headers (no AvailabilityMacros.h errors).

## User Setup Required

None — no external service configuration required. cppcheck was installed automatically via `brew install cppcheck` as part of task execution.

## Next Phase Readiness

- Baseline static analysis reports established in reports/ directory (.gitignored)
- Both analyze: tasks are advisory-only (always exit 0) — will not block CI or builds
- Future phases can compare analysis output to measure improvement/regression
- reports/clang-tidy.txt and reports/cppcheck.txt document the current state of findings for prioritization

## Self-Check: PASSED

- FOUND: Rakefile (contains analyze:clang_tidy, analyze:cppcheck, analyze:all)
- FOUND: .gitignore (contains /reports/)
- FOUND: 01-02-SUMMARY.md
- FOUND: commit 32e820e (feat(01-02): add clang-tidy and cppcheck static analysis Rake tasks)
- FOUND: reports/clang-tidy.txt (725 lines, non-empty, .gitignored)
- FOUND: reports/cppcheck.txt (21134 lines, non-empty, .gitignored)

---
*Phase: 01-audit-toolchain*
*Completed: 2026-03-19*
