# Phase 1: Audit Toolchain - Research

**Researched:** 2026-03-18
**Domain:** C static analysis (clang-tidy, cppcheck) and runtime sanitizers (ASan/UBSan) on macOS
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Reports only — clang-tidy and cppcheck produce advisory reports via rake tasks, they do NOT block the build
- Baseline only — capture existing findings as a report, don't fix them in this phase. Later phases fix issues as they touch those files
- Keep the current `.clang-tidy` config as-is (bugprone-*, readability-*, portability-*, misc-include-cleaner) — no expansion
- Analyze `src/` only, not vendor code. The existing `HeaderFilterRegex` in `.clang-tidy` already scopes to `src/`
- ASan/UBSan replaces the default dev build — RelWithDebInfo is no longer the default, Sanitize is
- Both `rake run` and `rake watch` use the sanitizer build by default — hot-reload rebuilds of libgame.dylib are also instrumented
- Crash immediately on violation (default ASan behavior, `halt_on_error=1`) — no log-and-continue
- If ASan + codesigned dylib causes issues on macOS: disable ad-hoc codesigning in the Sanitize build (codesigning is only needed for distribution)

### Claude's Discretion
- cppcheck check configuration and suppression patterns
- Exact CMake configuration for the Sanitize build type (compiler flags, linker flags)
- Rake task naming and structure for lint/analyze commands
- Whether to keep a non-sanitizer build type available as an opt-in alternative

### Deferred Ideas (OUT OF SCOPE)
None — discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CORR-07 | Set up clang-tidy configuration with `compile_commands.json` and cppcheck for static analysis | clang-tidy 22.1.1 installed at `/opt/homebrew/opt/llvm/bin/clang-tidy`. `compile_commands.json` exists and is symlinked at project root. `run-clang-tidy` parallel runner at `/opt/homebrew/opt/llvm/bin/run-clang-tidy`. cppcheck 2.20 needs `brew install`. Source filter pattern verified. |
| CORR-08 | Add ASan/UBSan Sanitize CMake build type with full instrumentation of host and shared library | Apple clang 17 supports `-fsanitize=address,undefined` for both executables and shared dylibs. `CMAKE_C_FLAGS_SANITIZE` pattern verified. ASan+codesign+dlopen tested and confirmed working on this macOS setup. |
</phase_requirements>

## Summary

This phase wires up two classes of analysis tooling: static analysis (clang-tidy and cppcheck) run as advisory Rake tasks, and runtime instrumentation (ASan+UBSan) as a new `Sanitize` CMake build type that replaces the default dev build.

**Static analysis** is nearly ready. clang-tidy 22.1.1 (Homebrew LLVM) and the `run-clang-tidy` parallel runner are already installed. `compile_commands.json` exists (built and symlinked) and covers all 18 `src/` source files. The `.clang-tidy` config is already committed and requires no changes. The only missing piece is a Rake task and `--extra-arg="-isysroot..."` to resolve macOS SDK headers. cppcheck needs `brew install cppcheck` and a `--file-filter` to scope to `src/`.

**Sanitizer build** is straightforward. Apple clang 17 supports `-fsanitize=address,undefined` for both executables and shared dylibs. The combination of ASan+codesign+`dlopen` has been tested and confirmed working on this machine — the copy-based hot-reload mechanism (SDL_CopyFile) preserves codesign, so no codesign suppression is needed. The main CMake work is defining `CMAKE_C_FLAGS_SANITIZE` and updating the `DEBUG` preprocessor definition to include the `Sanitize` config.

**Primary recommendation:** Use Apple clang (not Homebrew LLVM) for the Sanitize build — it ships the correct `libclang_rt.asan_osx_dynamic.dylib` for macOS, and the project already uses Apple clang as the default compiler.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| clang-tidy | 22.1.1 (Homebrew LLVM) | Static analysis using AST-based checks | Already installed; uses `compile_commands.json`; respects existing `.clang-tidy` config |
| run-clang-tidy | 22.1.1 (same package) | Parallel clang-tidy runner over all files | Ships with LLVM; filters by source path pattern |
| cppcheck | 2.20.0 (brew install) | Independent static analysis (different check set) | Complementary to clang-tidy; `--std=c23` supported; `--project=compile_commands.json` mode |
| ASan | (Apple clang 17) | Address sanitizer: heap/stack/use-after-free | Ships with Apple clang; macOS dylib runtime included |
| UBSan | (Apple clang 17) | Undefined behavior sanitizer | Ships with Apple clang; combinable with ASan |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Homebrew LLVM | 22.1.1 | Source of clang-tidy/run-clang-tidy | Already installed; do NOT use for compilation (Apple clang is the build compiler) |

**Installation:**
```bash
brew install cppcheck
```

**Version verification:**
```bash
clang-tidy --version        # 22.1.1 confirmed
cppcheck --version          # after brew install, expect 2.20.0
/usr/bin/cc --version       # Apple clang 17.0.0 (build compiler)
```

## Architecture Patterns

### Recommended Project Structure (new files only)
```
CMakeLists.txt              # Add Sanitize build type flags + DEBUG define for Sanitize
Rakefile                    # Add rake analyze:clang_tidy, rake analyze:cppcheck, update default + run + watch tasks
.clang-tidy                 # No changes (keep as-is per locked decision)
compile_commands.json       # Symlink at root — update to point to build/sanitize/ after Sanitize becomes default
build/
├── relwithdebinfo/         # Keep — non-default opt-in alternative
│   └── compile_commands.json
└── sanitize/               # New default build dir
    ├── compile_commands.json
    └── bin/TacticalTwo
```

### Pattern 1: CMake Sanitize Build Type

**What:** A custom CMake build type with ASan+UBSan compiler and linker flags, defined via `CMAKE_C_FLAGS_SANITIZE` cache variables. CMake uses these automatically when `-DCMAKE_BUILD_TYPE=Sanitize`.

**When to use:** This is the only correct way to create a custom build type in CMake that works with Ninja and generator expressions.

**Example:**
```cmake
# In root CMakeLists.txt, before add_subdirectory() calls

set(CMAKE_C_FLAGS_SANITIZE
    "-fsanitize=address,undefined -fno-omit-frame-pointer -fno-optimize-sibling-calls -g -O1"
    CACHE STRING "Flags used for Sanitize build" FORCE)
set(CMAKE_CXX_FLAGS_SANITIZE
    "-fsanitize=address,undefined -fno-omit-frame-pointer -fno-optimize-sibling-calls -g -O1"
    CACHE STRING "Flags used for Sanitize build" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_SANITIZE
    "-fsanitize=address,undefined"
    CACHE STRING "Linker flags for Sanitize build" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_SANITIZE
    "-fsanitize=address,undefined"
    CACHE STRING "Shared linker flags for Sanitize build" FORCE)

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "RelWithDebInfo" "MinSizeRel" "Sanitize")
```

**Also required:** Add `Sanitize` to the `DEBUG` preprocessor define so ImGui and debug overlay work:
```cmake
# In the foreach(target ${TARGETS}) block, replace:
$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:DEBUG>
# With:
$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>,$<CONFIG:Sanitize>>:DEBUG>
```

### Pattern 2: Rake Analyze Tasks

**What:** Non-blocking advisory analysis tasks that write reports to a file. Based on existing Rake task patterns.

**Example:**
```ruby
namespace :analyze do
  desc "Run clang-tidy on all src/ files (advisory, non-blocking)"
  task clang_tidy: "cmake:sanitize:configure" do
    sdk = `xcrun --show-sdk-path`.strip
    sh "/opt/homebrew/opt/llvm/bin/run-clang-tidy " \
       "-p build/sanitize " \
       "-source-filter '.*/tacticaltwo-game/src/.*' " \
       "-extra-arg=-isysroot#{sdk} " \
       "-quiet 2>&1 | tee reports/clang-tidy.txt || true"
    puts "clang-tidy report written to reports/clang-tidy.txt"
  end

  desc "Run cppcheck on src/ (advisory, non-blocking)"
  task :cppcheck do
    sh "cppcheck " \
       "--project=compile_commands.json " \
       "--std=c23 " \
       "--enable=all " \
       "--suppress=missingIncludeSystem " \
       "--file-filter='*/src/*' " \
       "--output-file=reports/cppcheck.txt " \
       "|| true"
    puts "cppcheck report written to reports/cppcheck.txt"
  end
end
```

### Pattern 3: Rake Sanitize Build Tasks

**What:** Mirror of the existing `cmake:configure` / `cmake:build` namespace, scoped to `build/sanitize/`.

**Example:**
```ruby
namespace :cmake do
  namespace :sanitize do
    desc "Configure CMake Sanitize build"
    task :configure do
      if File.exist?("build/sanitize/build.ninja")
        puts "Sanitize build already configured"
      else
        sh "cmake -B build/sanitize -G Ninja -DCMAKE_BUILD_TYPE=Sanitize -DENABLE_HOT_RELOADING=ON"
      end
    end

    desc "Build Sanitize configuration"
    task build: :configure do
      sh "cmake --build build/sanitize"
    end
  end
end

# New defaults (Sanitize replaces RelWithDebInfo)
task default: "cmake:sanitize:build"

task :run do
  sh "ninja -C build/sanitize"
  exec "build/sanitize/bin/TacticalTwo"
end
```

### Pattern 4: run-clang-tidy Source Filter

**What:** The `-source-filter` flag on `run-clang-tidy` restricts which files from `compile_commands.json` are analyzed. Must use the full project path prefix to exclude SDL3 and other fetched deps.

**Critical detail:** `".*\/src\/.*"` matches SDL3's `_deps/sdl3-src/src/` path too. Use the project name as a discriminator:

```bash
# WRONG — matches vendor SDL3:
-source-filter ".*\/src\/.*"

# CORRECT — matches only project src/:
-source-filter ".*/tacticaltwo-game/src/.*"
```

**Verified:** Running `run-clang-tidy` with the correct filter on this project's `compile_commands.json` processes exactly 17 source files (18 entries, minus one duplicate `log.c`) from `src/` and produces clang-tidy warnings scoped to project code only.

### Pattern 5: cppcheck --file-filter for src/ only

**What:** cppcheck's `--file-filter` glob restricts which files from the `compile_commands.json` project are checked.

```bash
cppcheck \
  --project=compile_commands.json \
  --std=c23 \
  --enable=all \
  --suppress=missingIncludeSystem \
  --file-filter="*/src/*" \
  --output-file=reports/cppcheck.txt
```

**Note:** `--suppress=missingIncludeSystem` silences "can't find header" warnings for system includes — essential when running outside Xcode's build environment.

### Anti-Patterns to Avoid

- **Blocking the build on static analysis warnings:** The locked decision is advisory-only. Never use `--error-exitcode` in the Rake analyze tasks.
- **Analyzing vendor code:** Using `-source-filter ".*\/src\/.*"` without the project name discriminator will pick up `_deps/sdl3-src/src/*.c` files from the CMake fetch. Always use `.*/tacticaltwo-game/src/.*`.
- **Using Homebrew LLVM clang for compilation:** The project uses Apple clang (`/usr/bin/cc`). Do not change `CC` or `CXX` for the Sanitize build — Apple clang 17 fully supports ASan/UBSan and ships the correct macOS runtime dylibs.
- **Omitting `-isysroot` from clang-tidy invocations:** Without `--extra-arg="-isysroot$(xcrun --show-sdk-path)"`, clang-tidy (Homebrew LLVM) cannot find `<AvailabilityMacros.h>` and emits hard errors for every SDL3 header.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Running clang-tidy on many files | Shell loop over `find src/ -name *.c` | `run-clang-tidy` | Parallel execution, respects `compile_commands.json`, handles compile flags automatically |
| ASan runtime for dylibs on macOS | Custom `__attribute__` instrumentation | Apple clang `-fsanitize=address` + standard `-shared` | Apple ships `libclang_rt.asan_osx_dynamic.dylib`; automatic interposition handles malloc/free/new/delete |
| cppcheck vendor exclusion | Manual path filtering | `--file-filter` glob | cppcheck's built-in filter avoids needing a separate file list |

**Key insight:** Both static analysis tools and sanitizer runtimes are mature, well-maintained infrastructure. The only project-specific work is wiring them into CMake and Rake.

## Common Pitfalls

### Pitfall 1: clang-tidy "AvailabilityMacros.h not found" error
**What goes wrong:** When Homebrew LLVM's clang-tidy (version 22) processes files that transitively include SDL3 headers, it cannot find `<AvailabilityMacros.h>` — an Apple SDK header not in LLVM's sysroot.
**Why it happens:** Homebrew LLVM does not bundle macOS SDK headers. The build compiler (Apple clang) uses the Xcode SDK implicitly, but clang-tidy re-parses without that sysroot.
**How to avoid:** Always pass `--extra-arg="-isysroot$(xcrun --show-sdk-path)"` to `run-clang-tidy` and direct `clang-tidy` invocations.
**Warning signs:** Output contains `error: 'AvailabilityMacros.h' file not found [clang-diagnostic-error]` and "Error while processing file" messages.

### Pitfall 2: Sanitize build does not define DEBUG
**What goes wrong:** The existing `target_compile_definitions` generator expression only defines `DEBUG` for `Debug` and `RelWithDebInfo` configs. The `Sanitize` config gets no `DEBUG`, so ImGui debug overlay, `CF_APP_OPTIONS_GFX_DEBUG_BIT`, and `log_debug()` calls are compiled out.
**Why it happens:** Custom CMake build types are not automatically included in generator expression lists.
**How to avoid:** Extend the expression to `$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>,$<CONFIG:Sanitize>>:DEBUG>`.
**Warning signs:** Game starts without ImGui, debug overlay does not appear when pressing G.

### Pitfall 3: compile_commands.json symlink pointing to wrong build
**What goes wrong:** After adding the Sanitize build, the root `compile_commands.json` symlink still points to `build/relwithdebinfo/compile_commands.json`. clang-tidy then uses `relwithdebinfo` flags (no sanitizer flags) — which is fine for analysis — but the symlink should follow the canonical build for editor tooling.
**Why it happens:** The symlink was created manually (exists: `compile_commands.json -> build/relwithdebinfo/compile_commands.json`). Rake tasks do not update it.
**How to avoid:** Retarget the symlink to `build/sanitize/compile_commands.json` when the Sanitize configure task runs (only if it doesn't already exist pointing correctly).
**Warning signs:** Editor/LSP reports different diagnostics than clang-tidy in CI.

### Pitfall 4: MallocNanoZone false positives on macOS Apple Silicon
**What goes wrong:** On some Apple Silicon + macOS combinations, ASan reports a false positive abort in the system malloc zone during startup.
**Why it happens:** Apple's MallocNanoZone and ASan both intercept malloc — with Homebrew LLVM ASan this causes a conflict, but with Apple clang's runtime it is typically absent.
**How to avoid:** Use Apple clang for the Sanitize build (project default). If false positives appear, run with `MallocNanoZone=0 ASAN_OPTIONS=halt_on_error=1`.
**Warning signs:** Crash at process startup before `game_init()` with malloc-related ASan report.

### Pitfall 5: Sanitize Rake watch task rebuilding wrong target
**What goes wrong:** `rake watch` currently calls `ninja -C build/relwithdebinfo game`. If the watch task is updated to use `build/sanitize` but the game is running from the old build, hot-reload does not trigger.
**Why it happens:** Platform loads the dylib from `GAME_LIB_PATH` which is baked in at CMake configure time. Must use the Sanitize build's binary.
**How to avoid:** Both `rake run` and `rake watch` must reference `build/sanitize` consistently after the change.
**Warning signs:** Game runs from `build/relwithdebinfo/bin/TacticalTwo` while watch rebuilds into `build/sanitize/bin/`.

### Pitfall 6: codesign step in src/game/CMakeLists.txt and ASan
**What goes wrong:** The concern from STATE.md is that "ASan + codesigned dylib interaction on macOS is unverified."
**What was verified:** TESTED on this machine — `codesign --force --sign -` applied to an ASan-instrumented dylib does NOT break dlopen or ASan interposition. `SDL_CopyFile` preserves the codesign signature on the copy, so the hot-reload copy path also works. No suppression of the codesign step is needed.
**Confidence:** HIGH (verified by direct test on this macOS/Apple Silicon/clang 17 setup).

## Code Examples

Verified patterns from direct testing on this machine:

### CMake Sanitize build type definition
```cmake
# Source: verified locally — Apple clang 17 + CMake 3.25+
# Place in root CMakeLists.txt before add_subdirectory() calls

set(CMAKE_C_FLAGS_SANITIZE
    "-fsanitize=address,undefined -fno-omit-frame-pointer -fno-optimize-sibling-calls -g -O1"
    CACHE STRING "Flags used for the Sanitize build type" FORCE)
set(CMAKE_CXX_FLAGS_SANITIZE
    "-fsanitize=address,undefined -fno-omit-frame-pointer -fno-optimize-sibling-calls -g -O1"
    CACHE STRING "Flags used for the Sanitize build type" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_SANITIZE
    "-fsanitize=address,undefined"
    CACHE STRING "Linker flags for the Sanitize build type" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_SANITIZE
    "-fsanitize=address,undefined"
    CACHE STRING "Shared linker flags for the Sanitize build type" FORCE)

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "RelWithDebInfo" "MinSizeRel" "Sanitize")
```

### CMake configure command for Sanitize
```bash
# Source: follows existing cmake:configure pattern in Rakefile
cmake -B build/sanitize -G Ninja -DCMAKE_BUILD_TYPE=Sanitize -DENABLE_HOT_RELOADING=ON
```

### run-clang-tidy invocation (verified working)
```bash
# Source: verified locally — correctly processes 17 src/ files, excludes SDL3 deps
SDK=$(xcrun --show-sdk-path)
/opt/homebrew/opt/llvm/bin/run-clang-tidy \
  -p build/sanitize \
  -source-filter ".*/tacticaltwo-game/src/.*" \
  -extra-arg="-isysroot${SDK}" \
  -quiet 2>&1 | tee reports/clang-tidy.txt
```

### cppcheck invocation pattern
```bash
# Source: cppcheck 2.20 manual + verified flag syntax
# Note: requires `brew install cppcheck` first
cppcheck \
  --project=compile_commands.json \
  --std=c23 \
  --enable=all \
  --suppress=missingIncludeSystem \
  --file-filter="*/src/*" \
  --output-file=reports/cppcheck.txt \
  --error-exitcode=0
```

### Running game with ASAN_OPTIONS
```bash
# Source: ASan docs + user decision (halt_on_error=1)
ASAN_OPTIONS=halt_on_error=1 build/sanitize/bin/TacticalTwo
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Manual clang-tidy per-file | `run-clang-tidy` with `-source-filter` | LLVM 3.9+ | Parallel, automatic, covers entire project |
| `CMAKE_C_FLAGS` edits for sanitizers | `CMAKE_C_FLAGS_SANITIZE` cache variable | CMake 2.8+ | Clean per-config flags, no interference |
| Separate cppcheck build dir | `--project=compile_commands.json` | cppcheck 1.85+ | Reuses CMake flags, accurate preprocessing |

**Deprecated/outdated:**
- Direct `clang-tidy file.c` invocations: Works but misses project-wide flag context from `compile_commands.json`.
- Setting `CMAKE_C_FLAGS` globally: Pollutes all build types; use `CMAKE_C_FLAGS_<CONFIG>` instead.

## Open Questions

1. **cppcheck --enable=all produces many warnings — which to suppress?**
   - What we know: `missingIncludeSystem` will fire for every system header include. `unusedFunction` may fire for exported game library functions.
   - What's unclear: Whether `--suppress=unusedFunction` is needed to keep the report actionable.
   - Recommendation: Run baseline report first; add suppressions to `.cppcheck.supp` file as needed (at Claude's discretion).

2. **compile_commands.json symlink: retarget to sanitize or leave pointing to relwithdebinfo?**
   - What we know: The symlink exists (`compile_commands.json -> build/relwithdebinfo/compile_commands.json`). clang-tidy is invoked with explicit `-p build/sanitize` in the Rake task, so analysis is unaffected either way.
   - What's unclear: Editor tooling (clangd) uses the root symlink. Sanitize flags include `-g -O1 -fsanitize=...` which may affect clangd diagnostics.
   - Recommendation: Retarget symlink to `build/sanitize/` (the new canonical build). Simple `ln -sf` in the configure task.

3. **Keep RelWithDebInfo as opt-in? (Claude's Discretion)**
   - What we know: User locked Sanitize as default. The existing Rakefile has `cmake:build` (relwithdebinfo).
   - Recommendation: Keep `cmake:configure` / `cmake:build` (relwithdebinfo) as explicit opt-in tasks. Just change the `default` task to `cmake:sanitize:build`.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | None — no automated test framework exists (see TESTING.md) |
| Config file | N/A |
| Quick run command | `ninja -C build/sanitize game` (build smoke test) |
| Full suite command | `build/sanitize/bin/TacticalTwo` (launch and visually verify no immediate ASan crash) |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| CORR-07 | clang-tidy runs without crashing on all src/ files | smoke | `rake analyze:clang_tidy` (non-zero exit means crash, not findings) | ❌ Wave 0 (Rake task) |
| CORR-07 | cppcheck runs with `--std=c23` and produces report | smoke | `rake analyze:cppcheck` | ❌ Wave 0 (Rake task) |
| CORR-08 | Sanitize build type compiles both targets | smoke | `cmake --build build/sanitize` | ❌ Wave 0 (CMake config) |
| CORR-08 | Game launches under Sanitize build without immediate ASan crash | manual-only | `ASAN_OPTIONS=halt_on_error=1 build/sanitize/bin/TacticalTwo` then `quit` | ❌ Wave 0 (binary) |

**Note:** There is no automated test framework in this project. Validation is via build success + manual smoke launch. The "tests" for this phase are the rake tasks themselves and a brief manual run.

### Sampling Rate
- **Per task commit:** `ninja -C build/sanitize game` — confirms shared library compiles clean
- **Per wave merge:** Full Sanitize build (`cmake --build build/sanitize`) + `rake analyze:clang_tidy`
- **Phase gate:** Both analysis rake tasks produce reports (exit 0), Sanitize build produces `TacticalTwo` binary, manual launch exits cleanly

### Wave 0 Gaps
- [ ] `reports/` directory — must exist for tee/output-file targets
- [ ] `brew install cppcheck` — system dependency, must be installed before `rake analyze:cppcheck`
- [ ] Rake tasks: `analyze:clang_tidy`, `analyze:cppcheck`, `cmake:sanitize:configure`, `cmake:sanitize:build` — none exist yet
- [ ] CMake Sanitize flags — not in `CMakeLists.txt` yet
- [ ] `DEBUG` define for Sanitize config — not in `CMakeLists.txt` yet

## Sources

### Primary (HIGH confidence)
- Direct testing on this machine (macOS arm64, Apple clang 17.0.0, Xcode 17)
  - Verified: clang-tidy 22.1.1 runs against `compile_commands.json` with `-source-filter`
  - Verified: `-isysroot` extra-arg resolves macOS SDK headers
  - Verified: ASan+UBSan compilation works for both executable and shared dylib
  - Verified: `codesign --sign -` on ASan dylib + `dlopen` of copy = success
  - Verified: `MallocNanoZone` / `halt_on_error=1` env var behavior
- `CMakeLists.txt` (project file) — current flag structure, generator expressions, target list
- `Rakefile` (project file) — existing task patterns, notify() helper, watch mechanism
- `.clang-tidy` (project file) — current checks, HeaderFilterRegex, CheckOptions
- `src/game/CMakeLists.txt` — codesign POST_BUILD step for libgame.dylib
- `src/platform/platform_cute.c` — hot-reload copy mechanism (SDL_CopyFile pattern)
- `build/relwithdebinfo/compile_commands.json` — 709 total entries, 18 in src/

### Secondary (MEDIUM confidence)
- [Clang-Tidy documentation](https://clang.llvm.org/extra/clang-tidy/) — `-p` flag, source filter
- [UndefinedBehaviorSanitizer — Clang docs](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html) — flags
- [AddressSanitizer — Clang docs](https://clang.llvm.org/docs/AddressSanitizer.html) — macOS dylib notes
- [Cppcheck manual](http://cppcheck.net/manual.html) — `--project`, `--file-filter`, `--std=c23`
- [Integrating sanitizer tools to CMake builds](http://www.stablecoder.ca/2018/02/01/analyzer-build-types.html) — `CMAKE_C_FLAGS_SANITIZE` pattern

### Tertiary (LOW confidence)
- WebSearch results on sanitizers-cmake, CMake discourse — corroborate `CMAKE_C_FLAGS_SANITIZE` approach but older sources

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all tools verified installed/available or one `brew install` away
- Architecture: HIGH — patterns verified by running the tools against actual project files
- Pitfalls: HIGH — directly reproduced (AvailabilityMacros.h error) or verified resolved (codesign+ASan test)

**Research date:** 2026-03-18
**Valid until:** 2026-06-18 (stable tooling, 90-day estimate)
