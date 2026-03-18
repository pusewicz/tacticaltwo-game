---
phase: 01-audit-toolchain
plan: 01
subsystem: infra
tags: [cmake, asan, ubsan, sanitizers, rakefile, build-system]

# Dependency graph
requires: []
provides:
  - "CMake Sanitize build type with ASan + UBSan instrumentation for both TacticalTwo and libgame.dylib"
  - "Sanitize build as default development build (rake, rake run, rake watch all use build/sanitize/)"
  - "compile_commands.json symlink retargeted to build/sanitize for clangd/tooling"
  - "DEBUG preprocessor symbol active in Sanitize builds (enables ImGui debug overlay)"
affects:
  - "all subsequent phases (every dev build now runs under sanitizers)"
  - "02-coordinate-extraction"
  - "hot-reload workflow"

# Tech tracking
tech-stack:
  added: ["ASan (AddressSanitizer)", "UBSan (UndefinedBehaviorSanitizer)"]
  patterns:
    - "Sanitize CMake build type pattern: cmake -B build/sanitize -G Ninja -DCMAKE_BUILD_TYPE=Sanitize"
    - "Shader tools disabled in Sanitize build to avoid C++ static archive ASan link failures"

key-files:
  created: []
  modified:
    - "CMakeLists.txt"
    - "Rakefile"

key-decisions:
  - "Make Sanitize the default development build rather than RelWithDebInfo — every dev session now catches memory errors automatically"
  - "Disable CF_CUTE_SHADERC and CF_RUNTIME_SHADER_COMPILATION in Sanitize build — pre-compiled C++ static archives (libglslang, etc.) cause typeinfo linker failures with ASan linker flags; offline shader tools not needed for dev iteration"
  - "Keep RelWithDebInfo build as opt-in via rake cmake:build — preserves escape hatch if sanitizer overhead is prohibitive"
  - "Pass ASAN_OPTIONS=halt_on_error=1 in rake run — crash immediately on first violation rather than accumulating reports"

patterns-established:
  - "Sanitize build pattern: CMAKE_C_FLAGS_SANITIZE / CMAKE_EXE_LINKER_FLAGS_SANITIZE / CMAKE_SHARED_LINKER_FLAGS_SANITIZE cache variables"
  - "Generator expression for DEBUG define: $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>,$<CONFIG:Sanitize>>:DEBUG>"

requirements-completed:
  - CORR-08

# Metrics
duration: 8min
completed: 2026-03-18
---

# Phase 01 Plan 01: Sanitize Build Type Summary

**ASan + UBSan Sanitize CMake build type established as default development build, instrumenting both TacticalTwo executable and libgame.dylib hot-reload library**

## Performance

- **Duration:** ~8 min
- **Started:** 2026-03-18T18:12:00Z
- **Completed:** 2026-03-18T18:21:00Z
- **Tasks:** 1/2 complete (Task 2 awaiting human verification)
- **Files modified:** 2

## Accomplishments

- Added Sanitize CMake build type with `-fsanitize=address,undefined -fno-omit-frame-pointer -fno-optimize-sibling-calls -g -O1` flags
- Both `TacticalTwo` executable and `libgame.dylib` compile and link with ASan/UBSan
- Default `rake` task now builds the Sanitize configuration; `rake run` launches with `ASAN_OPTIONS=halt_on_error=1`
- `rake watch` monitors src/ and rebuilds `game` target in `build/sanitize/`
- `compile_commands.json` symlink retargeted to `build/sanitize/compile_commands.json` on configure
- `DEBUG` preprocessor symbol included in Sanitize builds (ImGui debug overlay functional)

## Task Commits

Each task was committed atomically:

1. **Task 1: Add Sanitize CMake build type and update Rakefile defaults** - `4895803` (feat)

**Plan metadata:** pending (will be committed after Task 2 checkpoint)

## Files Created/Modified

- `CMakeLists.txt` - Added Sanitize build type cache variables, updated DEBUG generator expression
- `Rakefile` - Added cmake:sanitize namespace, updated default/build/run/watch tasks

## Decisions Made

- Disable `CF_CUTE_SHADERC=OFF` and `CF_RUNTIME_SHADER_COMPILATION=OFF` for Sanitize build: pre-compiled C++ static archives (libglslang, SPIRV) in vendor don't carry matching RTTI symbols when linked with ASan, causing typeinfo link failures. Offline shader tools are not needed during development iteration.
- Keep RelWithDebInfo as opt-in (via `rake cmake:build`) — not removed, preserved as fallback.
- `halt_on_error=1` chosen over accumulate mode: cleaner crash reports, stops at first violation.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Added CF_CUTE_SHADERC=OFF and CF_RUNTIME_SHADER_COMPILATION=OFF to Sanitize configure**
- **Found during:** Task 1 (build verification step)
- **Issue:** `cmake --build build/sanitize` failed with linker error: `ld: symbol(s) not found for architecture arm64` — `typeinfo for glslang::TShader` and `typeinfo for glslang::TProgram` undefined in `libcute-shader.a`. The pre-compiled C++ archives in vendor were not built with ASan, causing RTTI symbol mismatches when ASan linker flags were applied to `cute-shaderc` executable.
- **Fix:** Added `-DCF_CUTE_SHADERC=OFF -DCF_RUNTIME_SHADER_COMPILATION=OFF` to the Rakefile `cmake:sanitize:configure` task and reconfigured. These are offline build tools, not runtime requirements.
- **Files modified:** Rakefile
- **Verification:** `cmake --build build/sanitize` succeeds (70 targets built, `ninja: no work to do` on re-run), `build/sanitize/bin/TacticalTwo` and `build/sanitize/bin/libgame.dylib` both exist
- **Committed in:** `4895803` (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (Rule 1 - bug in build configuration)
**Impact on plan:** Auto-fix necessary for build to succeed. Shader tools are only needed for asset pipeline, not dev iteration. No scope creep.

## Issues Encountered

- Vendor C++ shader compiler tools (`cute-shaderc`) inherit CMake's global EXE_LINKER_FLAGS_SANITIZE and cannot link against pre-compiled glslang/SPIRV static archives built without ASan. Resolved by disabling those tools in the Sanitize configuration. This is the expected behavior documented in the research phase (ASan + codesigned dylib confirmed working; shader tool linker issue was unresearched but straightforward to resolve).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Sanitize build is configured and compiles successfully
- Human verification (Task 2) needed: run `rake run` and confirm game launches without ASan crashes
- If verification passes: CORR-08 requirement satisfied, Phase 01 can proceed to Plan 02
- If ASan violations appear: debug output will be in terminal after game exit; use `ASAN_SYMBOLIZER_PATH=$(xcrun --find llvm-symbolizer)` for better stack traces

---
*Phase: 01-audit-toolchain*
*Completed: 2026-03-18*
