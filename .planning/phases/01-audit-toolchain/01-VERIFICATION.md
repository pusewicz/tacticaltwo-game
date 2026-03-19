---
phase: 01-audit-toolchain
verified: 2026-03-19T14:30:00Z
status: human_needed
score: 9/10 must-haves verified
re_verification: false
human_verification:
  - test: "Run `rake run` and exercise the game briefly"
    expected: "Game launches, player can move, no immediate ASan/UBSan terminal output (red text about memory violations). ImGui debug overlay appears when pressing G (confirms DEBUG define is active). On quit, no sanitizer violation reports printed."
    why_human: "Cannot execute a graphical game binary to observe runtime crash absence or ImGui overlay visibility"
---

# Phase 01: Audit Toolchain Verification Report

**Phase Goal:** Establish runtime sanitizer baseline and static analysis tooling so every subsequent phase automatically catches memory errors and undefined behavior during development.
**Verified:** 2026-03-19T14:30:00Z
**Status:** human_needed
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths (from ROADMAP.md Success Criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | `clang-tidy` runs against all src/ files using `compile_commands.json` and produces a report without crashing or skipping files | VERIFIED | `reports/clang-tidy.txt` exists at 725 lines; first lines show 17 files processed across 16 threads |
| 2 | `cppcheck` runs with `--std=c23` and produces a second independent report | VERIFIED | `reports/cppcheck.txt` exists at 21134 lines; Rakefile uses `--std=c23 --enable=all` |
| 3 | A `Sanitize` CMake build type exists that instruments both the host executable and `libgame.dylib` with ASan and UBSan | VERIFIED | `CMAKE_C_FLAGS_SANITIZE`, `CMAKE_EXE_LINKER_FLAGS_SANITIZE`, `CMAKE_SHARED_LINKER_FLAGS_SANITIZE` all defined in CMakeLists.txt lines 15–26; `build/sanitize/bin/TacticalTwo` and `build/sanitize/bin/libgame.dylib` both exist |
| 4 | The game launches and runs under the `Sanitize` build without immediate false-positive crashes | ? NEEDS HUMAN | Cannot verify runtime behavior of graphical application programmatically |
| 5 | `.clang-tidy` config file is committed alongside `compile_commands.json` generation in the build system | VERIFIED | `.clang-tidy` is git-tracked; `CMAKE_EXPORT_COMPILE_COMMANDS ON` is in CMakeLists.txt line 8; `compile_commands.json` symlink points to `build/sanitize/compile_commands.json` |

**Score:** 4/5 truths verified automated, 1 requires human confirmation

---

### Must-Haves: Plan 01 (CORR-08)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Sanitize CMake build type compiles both TacticalTwo and libgame.dylib with ASan/UBSan | VERIFIED | Both binaries exist under `build/sanitize/bin/`; flags `-fsanitize=address,undefined` in both C and linker flag variables |
| 2 | `rake` (default) builds using the Sanitize configuration | VERIFIED | Rakefile line 49: `task default: "cmake:sanitize:build"` |
| 3 | `rake run` launches the game under Sanitize build with `ASAN_OPTIONS=halt_on_error=1` | VERIFIED | Rakefile lines 55–58: `ninja -C build/sanitize` then `exec({ "ASAN_OPTIONS" => "halt_on_error=1" }, "build/sanitize/bin/TacticalTwo")` |
| 4 | `rake watch` rebuilds libgame.dylib in the Sanitize build directory | VERIFIED | Rakefile lines 74–90: `task watch: "cmake:sanitize:configure"` with `ninja -C build/sanitize game` |
| 5 | `compile_commands.json` symlink at project root points to `build/sanitize/` | VERIFIED | `readlink compile_commands.json` returns `build/sanitize/compile_commands.json` |
| 6 | `DEBUG` preprocessor symbol is defined in Sanitize builds so ImGui and debug overlay work | VERIFIED | CMakeLists.txt line 60: `$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>,$<CONFIG:Sanitize>>:DEBUG>` |

### Must-Haves: Plan 02 (CORR-07)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | `clang-tidy` runs against all src/ files using compile_commands.json and produces a report without crashing or skipping files | VERIFIED | `reports/clang-tidy.txt` at 725 lines, processing confirmed 17/17 src/ files |
| 2 | `cppcheck` runs with `--std=c23` and produces a second independent report | VERIFIED | `reports/cppcheck.txt` at 21134 lines; `--std=c23` in Rakefile line 110 |
| 3 | Static analysis tasks are advisory only — they never block the build or return non-zero exit codes | VERIFIED | Rakefile: clang_tidy uses `; true` shell suffix (line 101); cppcheck uses `--error-exitcode=0` (line 115) |
| 4 | Analysis reports are written to the `reports/` directory | VERIFIED | Both `reports/clang-tidy.txt` and `reports/cppcheck.txt` exist on disk |

---

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `CMakeLists.txt` | Sanitize build type with ASan/UBSan flags | VERIFIED | Lines 14–29: all four flag cache variables defined, `set_property STRINGS` lists "Sanitize", `CONFIG:Sanitize` in DEBUG generator expression |
| `Rakefile` | Sanitize build/run/watch tasks as defaults; analyze namespace | VERIFIED | Lines 18–45: `cmake:sanitize` namespace. Lines 49/52: default and build depend on `cmake:sanitize:build`. Lines 92–121: full `analyze` namespace with clang_tidy, cppcheck, all tasks |
| `sanitizer-ignorelist.txt` | Excludes vendor/, system headers from UBSan | VERIFIED | File exists, git-tracked. Excludes `*/vendor/*`, Xcode SDK, `/opt/homebrew/*` |
| `.clang-tidy` | Committed clang-tidy config with check selection | VERIFIED | Git-tracked. Defines bugprone-*, readability-*, portability-* checks; `WarningsAsErrors: ""` (advisory) |
| `.gitignore` | `reports/` excluded from git | VERIFIED | Line 60: `/reports/` |
| `reports/clang-tidy.txt` | Baseline clang-tidy report (non-empty) | VERIFIED | 725 lines, contains actual clang-tidy findings |
| `reports/cppcheck.txt` | Baseline cppcheck report (non-empty) | VERIFIED | 21134 lines, contains cppcheck findings (file is .gitignored, not committed) |
| `build/sanitize/bin/TacticalTwo` | Host executable built with sanitizers | VERIFIED | File exists |
| `build/sanitize/bin/libgame.dylib` | Hot-reload shared library built with sanitizers | VERIFIED | File exists |
| `build/sanitize/compile_commands.json` | Compile commands for sanitize build | VERIFIED | File exists (symlink target resolves) |

---

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| Rakefile default task | `cmake:sanitize:build` | task dependency | WIRED | `task default: "cmake:sanitize:build"` (line 49) |
| Rakefile `cmake:sanitize:configure` | CMakeLists.txt | `cmake -DCMAKE_BUILD_TYPE=Sanitize` | WIRED | `sh "cmake -B build/sanitize -G Ninja -DCMAKE_BUILD_TYPE=Sanitize -DENABLE_HOT_RELOADING=ON"` (line 24) |
| Rakefile `analyze:clang_tidy` | `compile_commands.json` | `-p build/sanitize` | WIRED | Line 98: `-p build/sanitize` |
| Rakefile `analyze:cppcheck` | `compile_commands.json` | `--project=compile_commands.json` | WIRED | Line 109: `--project=compile_commands.json` |
| `compile_commands.json` symlink | `build/sanitize/compile_commands.json` | filesystem symlink | WIRED | `readlink` confirms target |

---

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| CORR-07 | 01-02-PLAN.md | Set up clang-tidy configuration with `compile_commands.json` and cppcheck for static analysis | SATISFIED | `.clang-tidy` committed; `analyze:clang_tidy` and `analyze:cppcheck` Rake tasks exist; baseline reports generated |
| CORR-08 | 01-01-PLAN.md | Add ASan/UBSan Sanitize CMake build type with full instrumentation of host and shared library | SATISFIED | `CMAKE_C_FLAGS_SANITIZE` with `-fsanitize=address,undefined`; both binaries built; default rake task uses Sanitize |

**Note:** REQUIREMENTS.md marks CORR-08 as `[ ]` (not ticked) and its traceability table entry shows "Pending". This is a documentation discrepancy — the implementation is fully present in the codebase. ROADMAP.md plan checkboxes (lines 40–41) are also unchecked. These tracking fields need updating to reflect completion.

---

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| REQUIREMENTS.md | 19, 84 | CORR-08 marked `[ ]` / "Pending" despite full implementation | Info | Misleading state tracking only; no code impact |
| ROADMAP.md | 40–41 | Plan checkboxes `[ ]` for both 01-01 and 01-02 despite plans being complete | Info | Misleading state tracking only; no code impact |

No code anti-patterns (TODO/FIXME/placeholder/stub returns) found in modified files.

---

### Human Verification Required

#### 1. Game Runs Under Sanitize Build Without Crashes

**Test:** `cd /path/to/tacticaltwo-game && rake run`
**Expected:** Game window opens, player character is visible and responds to input. No red ASan/UBSan error text in terminal. ImGui debug overlay appears when pressing G (confirms `DEBUG` preprocessor define is active in Sanitize builds). On quit (Cmd+Q or window close), no sanitizer violation reports are printed to terminal.
**Why human:** Runtime crash absence and graphical overlay visibility cannot be verified programmatically without executing the GUI application.

#### 2. (Optional) Hot-Reload Under Sanitizers

**Test:** In one terminal run `rake watch`; in another make a trivial change to `src/game/game.c` (add/remove a blank line) and save.
**Expected:** Watch terminal shows rebuild succeeding and hot-reload triggering without an ASan crash.
**Why human:** Requires live process observation across two terminals.

---

### Gaps Summary

No functional gaps found. All automated checks pass. The phase goal is substantively achieved:

- The Sanitize CMake build type exists with correct ASan/UBSan flags for both C and C++ targets
- Both the host executable (TacticalTwo) and hot-reload shared library (libgame.dylib) are built and present
- The sanitizer ignorelist correctly excludes vendor code and system headers from UBSan instrumentation
- `rake` (default), `rake run`, and `rake watch` all use the Sanitize build
- `compile_commands.json` symlink is retargeted to `build/sanitize/`
- Both `analyze:clang_tidy` and `analyze:cppcheck` tasks exist, are advisory-only, and have produced non-empty baseline reports
- `.clang-tidy` config is committed

One truth requires human confirmation: that the game actually runs under ASan/UBSan without false-positive crashes (runtime behavior that cannot be verified statically).

Documentation housekeeping: REQUIREMENTS.md and ROADMAP.md still show CORR-08 as incomplete and both plan checkboxes as unchecked. These should be updated to reflect the actual state.

---

_Verified: 2026-03-19T14:30:00Z_
_Verifier: Claude (gsd-verifier)_
