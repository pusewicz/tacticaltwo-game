# Technology Stack: Audit & Refactor Tooling

**Project:** TacticalTwo — Code Audit & Refactor milestone
**Researched:** 2026-03-18
**Scope:** Tools and methodologies for auditing and refactoring a C23 game codebase on macOS

---

## Recommended Audit & Refactor Tooling Stack

### Static Analysis — Primary

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| clang-tidy | 18+ (bundled with Xcode or Homebrew LLVM) | Lint, bug detection, style enforcement | First-party Clang integration; reads `compile_commands.json` natively; C23-aware via `-std=c23`; `bugprone-*`, `performance-*`, `readability-*` check suites directly applicable to the existing codebase issues |
| clang Static Analyzer (scan-build) | Bundled with Clang | Deep inter-procedural bug finding | Finds null pointer dereferences and use-after-free paths that clang-tidy misses; runs as a build wrapper (`scan-build ninja`) with no code changes required; produces HTML reports |
| cppcheck | 2.14+ | Second-pass static analysis | Complementary to clang-tidy; detects different classes of bugs (out-of-bounds array indexing, uninitialized variable reads); explicit `--std=c23` flag supported; catches issues that Clang's AST-based analysis misses |

**Rationale for two static analyzers:** clang-tidy and cppcheck have low overlap in what they detect. Running both maximizes signal with minimal false-positive overlap. Slant community data and CodeChecker project both confirm this complementary relationship. (MEDIUM confidence — multiple community sources agree.)

### Dynamic Analysis — Sanitizers

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| AddressSanitizer (ASan) | Bundled with Clang | Memory errors at runtime: buffer overflows, use-after-free, heap corruption | The hot-reload system (`dlopen`/`dyld`) and arena allocator patterns in this codebase are exactly where these errors hide; `-fsanitize=address` catches them at the call site |
| UndefinedBehaviorSanitizer (UBSan) | Bundled with Clang | Catches signed integer overflow, out-of-bounds shifts, null pointer dereference traps | The coordinate conversion math in `physics_system.c` and `render_system.c` is the primary risk area; `-fsanitize=undefined` confirms no UB in those paths before extraction to a shared module |
| LeakSanitizer (LSan) | Bundled with Clang (part of ASan on macOS) | Memory leak detection | Confirms asset-loading refactoring (parallax sprites, LDtk maps) doesn't introduce leaks; on macOS, LSan is enabled automatically with `-fsanitize=address` |

**macOS note:** Valgrind is NOT recommended. It has poor macOS support (last macOS version with good support is 10.x; ARM is unsupported as of 2025). Use ASan + `leaks` tool instead. The macOS `leaks` command-line tool (`MallocStackLogging=1 leaks -atExit -- ./binary`) is a viable lightweight alternative for post-hoc leak checking without recompile. (HIGH confidence — Clang documentation is authoritative.)

**Sanitizer interaction with hot reload:** ASan and UBSan work with shared libraries loaded via `dlopen`. The shared library itself must be compiled with the sanitizer flags. Do NOT combine ASan with `ENABLE_HOT_RELOADING=ON` without also adding sanitizer flags to `libgame.dylib` — partial instrumentation causes false positives. Dedicate a separate `Sanitize` CMake build type for audit runs.

### CMake Integration

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| `CMAKE_EXPORT_COMPILE_COMMANDS=ON` | CMake 3.5+ | Generate `compile_commands.json` | Required by both clang-tidy and iwyu; already using Ninja + CMake 3.25+, so cost is zero |
| `CMAKE_C_CLANG_TIDY` property | CMake 3.7+ | Run clang-tidy on every compile unit | Integrates tidy into the build; use during audit phase only (slows incremental builds) |
| Custom `Sanitize` build type | CMake | Dedicated sanitizer build | Separate from `RelWithDebInfo` default; `-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1` |

### Header Dependency Audit

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| include-what-you-use (iwyu) | 0.22+ (matches Clang 18) | Find unused `#include` directives, find missing direct includes | The codebase has grown organically; iwyu exposes which headers are included transitively vs. explicitly, reducing compile coupling; `CMAKE_C_INCLUDE_WHAT_YOU_USE` flag |

**Caveat:** iwyu is aggressive and will suggest changes that break code when vendored libraries (Cute Framework) use indirect-include patterns. Run iwyu in report-only mode and apply suggestions selectively. (MEDIUM confidence — documented behavior in iwyu README.)

### Code Formatting (already in place)

| Technology | Purpose | Status |
|------------|---------|--------|
| clang-format | Consistent formatting via `.clang-format` | Already configured — no change needed |

---

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| Memory analysis | ASan (compile-time) | Valgrind | Valgrind has no Apple Silicon support; ASan is faster and natively macOS-compatible |
| Static analysis | clang-tidy + cppcheck | PVS-Studio | PVS-Studio is commercial; clang-tidy + cppcheck covers the same ground for free |
| Static analysis | clang-tidy + cppcheck | SonarQube C | Server infrastructure overkill for a solo project; same checks available locally |
| Deep analysis | scan-build (inter-procedural) | Infer (Facebook) | Infer is powerful but requires Java runtime, has poor macOS ARM support as of 2025, and complex setup for a game project |
| Heap profiling | Xcode Instruments (Leaks, Allocations) | Massif (Valgrind) | Massif requires Valgrind; Instruments integrates natively on macOS with GUI visualization |

---

## Refactoring Methodology

The standard approach for incremental C game codebase refactoring (2025) follows a three-phase discipline:

### Phase A: Audit Without Changing Behavior

1. Generate `compile_commands.json` with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
2. Run clang-tidy across all `src/` files with `bugprone-*`, `performance-*`, `readability-*`, `clang-analyzer-*` checks enabled; capture all warnings to a report
3. Run cppcheck with `--std=c23 --enable=all` across `src/`; capture warnings
4. Run scan-build against a full build; inspect HTML report
5. Build a `Sanitize` target; run the game under ASan+UBSan for a representative play session; capture any runtime violations
6. Annotate findings against `.planning/codebase/CONCERNS.md` — categorize as: confirmed bug, refactor target, or false positive
7. Do NOT fix anything during audit phase — pure observation

**Rationale:** Mixing audit and fix creates confusion about which baseline the findings came from. Auditing cold gives a clean picture before changes corrupt the signal.

### Phase B: Extract Then Validate (incremental)

For each identified extract/refactor (e.g., coordinate module, active entity list):

1. Write the target interface before implementation — what should the module expose?
2. Extract with zero behavioral change — move code, don't rewrite logic
3. Compile immediately; verify the build is green
4. Re-run ASan+UBSan session to confirm no new violations introduced
5. Manually verify hot-reload still works after each structural change

**Key discipline:** Each extract step should be independently reviewable. One module extraction per commit. Avoid bundling unrelated changes.

### Phase C: Fix Confirmed Issues

Address confirmed bugs and fragile areas identified in Phases A and B:

1. Fix one issue at a time
2. Verify behavioral equivalence (game runs, renders identically)
3. Re-run relevant sanitizer checks after each fix
4. Mark items resolved in `.planning/codebase/CONCERNS.md`

**What NOT to do:**
- Do not run a "big bang" reformatting pass — it destroys diff readability
- Do not combine style fixes with logic fixes in the same change
- Do not delete the `build/` directory to "fix" build errors — per CLAUDE.md

---

## CMake Sanitizer Build Type Setup

Add to `CMakeLists.txt` or a separate `cmake/SanitizerBuild.cmake` module:

```cmake
# Usage: cmake -DCMAKE_BUILD_TYPE=Sanitize ..
set(CMAKE_C_FLAGS_SANITIZE
    "-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1"
    CACHE STRING "Flags for sanitizer build" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_SANITIZE
    "-fsanitize=address,undefined"
    CACHE STRING "Linker flags for sanitizer build" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_SANITIZE
    "-fsanitize=address,undefined"
    CACHE STRING "Shared library linker flags for sanitizer build" FORCE)
```

Both the host executable and `libgame.dylib` must be compiled with sanitizer flags — partial instrumentation produces false positives from the dlopen boundary.

---

## clang-tidy Check Configuration

Recommended `.clang-tidy` additions for the audit phase (extend, don't replace, the existing config):

```yaml
Checks: >
  bugprone-*,
  clang-analyzer-*,
  performance-*,
  readability-identifier-naming,
  readability-function-size,
  readability-non-const-parameter,
  -bugprone-easily-swappable-parameters,
  -clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling
```

Disable `bugprone-easily-swappable-parameters` — produces extreme noise on game math functions with multiple `float` parameters. Disable the deprecated-buffer-handling check — Cute Framework uses `snprintf` patterns extensively in vendored code.

Set `HeaderFilterRegex: '^src/'` to exclude vendor headers from analysis.

---

## Hot-Reload State Versioning Approach

The `cr.h` open-source library demonstrates three safety modes for state validation during library reload:

- **CR_SAFEST**: Validates both address and size of persisted state sections — rollback on layout change
- **CR_SAFE**: Validates only section size — catches struct growth/shrinkage
- **CR_UNSAFE**: No validation — current TacticalTwo behavior

The recommended refactoring path is not to adopt `cr.h` wholesale (TacticalTwo has its own dlopen mechanism), but to apply its principle: add a `uint32_t state_version` field to `GameState` and check it in `game_hot_reload()`. If the version doesn't match the compiled constant, log a warning and re-initialize entity state rather than silently continuing with stale layout. This is a LOW-risk, HIGH-value change. (MEDIUM confidence — cr.h is a well-maintained reference implementation; the principle is sound.)

---

## Installation

```bash
# Static analysis tools (macOS, Homebrew)
brew install llvm          # provides clang-tidy, scan-build, clang-format
brew install cppcheck
brew install include-what-you-use

# Verify versions
clang-tidy --version       # expect 18+
cppcheck --version         # expect 2.14+
iwyu --version             # expect 0.22+

# Generate compile_commands.json (one-time, or after CMake reconfigure)
rake cmake:configure       # existing Rakefile target
# compile_commands.json appears in build/relwithdebinfo/
```

---

## Confidence Summary

| Area | Confidence | Basis |
|------|------------|-------|
| clang-tidy as primary linter | HIGH | Official Clang documentation; C23 via `-std=c23` confirmed |
| cppcheck C23 support | MEDIUM | Source code inspection shows C23 in standards enum; full feature coverage unverified |
| ASan/UBSan on macOS | HIGH | Official Clang documentation; macOS support explicitly listed |
| Valgrind exclusion | HIGH | No Apple Silicon support; ASan is the recommended replacement per LLVM team |
| iwyu + CMake integration | MEDIUM | CMake 3.3+ native support confirmed; behavior with CF vendor headers needs validation |
| scan-build inter-procedural | MEDIUM | Well-documented; HTML report generation confirmed; C23 compatibility not explicitly verified but uses same Clang frontend |
| Hot-reload state versioning | MEDIUM | cr.h reference implementation; principle is sound but TacticalTwo-specific integration needs design |
| Sanitizer + dlopen interaction | MEDIUM | General principle confirmed; specific interaction with codesign'd dylibs on macOS needs test run to verify |

---

## Sources

- [Clang-Tidy Documentation](https://clang.llvm.org/extra/clang-tidy/)
- [clang-tidy Checks List](https://clang.llvm.org/extra/clang-tidy/checks/list.html)
- [AddressSanitizer — Clang Documentation](https://clang.llvm.org/docs/AddressSanitizer.html)
- [UndefinedBehaviorSanitizer — Clang Documentation](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)
- [JSON Compilation Database Specification](https://clang.llvm.org/docs/JSONCompilationDatabase.html)
- [scan-build Command Line Usage](https://clang.llvm.org/docs/analyzer/user-docs/CommandLineUsage.html)
- [include-what-you-use GitHub](https://github.com/include-what-you-use/include-what-you-use)
- [cppcheck GitHub — Standards header (C23 enum)](https://github.com/danmar/cppcheck/blob/main/lib/standards.h)
- [Cppcheck 2.14 release notes](https://sourceforge.net/p/cppcheck/news/2024/04/cppcheck-2140/)
- [cr.h: A Simple C Hot Reload Header-only Library](https://fungos.github.io/cr-simple-c-hot-reload/)
- [Sanitizers as Valgrind alternative — LinuxJedi](https://linuxjedi.co.uk/sanitizers-the-alternative-to-valgrind/)
- [macOS leaks command-line tool — Apple](https://developer.apple.com/library/archive/documentation/Performance/Conceptual/ManagingMemory/Articles/FindingLeaks.html)
- [Using ASan in a CMake project](https://felsoci.sk/blog/using-address-sanitizer-asan-in-a-cmake-project.html)
- [sanitizers-cmake module](https://github.com/arsenm/sanitizers-cmake)
- [Slant: Clang Static Analyzer vs Cppcheck](https://www.slant.co/versus/838/839/~clang-static-analyzer_vs_cppcheck)
- [A gentle introduction to static analyzers for C](https://nrk.neocities.org/articles/c-static-analyzers)
