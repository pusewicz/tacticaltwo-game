# Phase 1: Audit Toolchain - Context

**Gathered:** 2026-03-18
**Status:** Ready for planning

<domain>
## Phase Boundary

Wire up clang-tidy, cppcheck, and ASan/UBSan so every subsequent phase has a static and runtime validation baseline. This phase sets up tooling only — no code fixes beyond what's needed to make the tools run.

</domain>

<decisions>
## Implementation Decisions

### Strictness policy
- Reports only — clang-tidy and cppcheck produce advisory reports via rake tasks, they do NOT block the build
- Baseline only — capture existing findings as a report, don't fix them in this phase. Later phases fix issues as they touch those files
- Keep the current `.clang-tidy` config as-is (bugprone-*, readability-*, portability-*, misc-include-cleaner) — no expansion
- Analyze `src/` only, not vendor code. The existing `HeaderFilterRegex` in `.clang-tidy` already scopes to `src/`

### Sanitizer usage
- ASan/UBSan replaces the default dev build — RelWithDebInfo is no longer the default, Sanitize is
- Both `rake run` and `rake watch` use the sanitizer build by default — hot-reload rebuilds of libgame.dylib are also instrumented
- Crash immediately on violation (default ASan behavior, `halt_on_error=1`) — no log-and-continue
- If ASan + codesigned dylib causes issues on macOS: disable ad-hoc codesigning in the Sanitize build (codesigning is only needed for distribution)

### Claude's Discretion
- cppcheck check configuration and suppression patterns
- Exact CMake configuration for the Sanitize build type (compiler flags, linker flags)
- Rake task naming and structure for lint/analyze commands
- Whether to keep a non-sanitizer build type available as an opt-in alternative

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Build system
- `.planning/REQUIREMENTS.md` — CORR-07 (clang-tidy + cppcheck setup) and CORR-08 (ASan/UBSan Sanitize build type) define the requirements
- `CMakeLists.txt` — Root build configuration; already has `CMAKE_EXPORT_COMPILE_COMMANDS ON` and warning flags
- `Rakefile` — Existing developer workflow tasks (build, run, format, watch); new lint/sanitize tasks integrate here
- `.clang-tidy` — Existing clang-tidy configuration; keep as-is per user decision

### Codebase analysis
- `.planning/codebase/` — Detailed codebase maps (if they exist) with architecture and conventions

### Known concerns
- `.planning/STATE.md` §Blockers — "ASan + codesigned dylib interaction on macOS is unverified — smoke test as first step"

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `.clang-tidy` — Already configured with curated C-appropriate checks, `HeaderFilterRegex` scoped to `src/`
- `CMAKE_EXPORT_COMPILE_COMMANDS ON` — Already in root CMakeLists.txt, `compile_commands.json` is generated in build dir
- `Rakefile` — Established task patterns (namespace for cmake, desc+task for developer commands, `notify()` helper for macOS notifications)

### Established Patterns
- Build dir: `build/relwithdebinfo/` — new sanitize build would follow same pattern (e.g., `build/sanitize/`)
- Targets: `game` (shared lib) and `TacticalTwo` (executable) — both need sanitizer instrumentation
- Compile options use generator expressions for cross-platform support (`$<$<C_COMPILER_ID:MSVC>:...>`)

### Integration Points
- `Rakefile` `cmake:configure` task — needs updating or new task for Sanitize build type
- `CMakeLists.txt` foreach loop over `TARGETS` — sanitizer flags go here
- `ENABLE_HOT_RELOADING=ON` — must remain ON for sanitizer builds to instrument libgame.dylib

</code_context>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 01-audit-toolchain*
*Context gathered: 2026-03-18*
