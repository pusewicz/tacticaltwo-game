---
phase: 1
slug: audit-toolchain
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-18
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Shell commands (rake, clang-tidy, cppcheck, ASan runtime) |
| **Config file** | `.clang-tidy` (exists), `CMakeLists.txt` (sanitizer config) |
| **Quick run command** | `rake` (build with sanitizers) |
| **Full suite command** | `rake && rake lint && rake analyze` |
| **Estimated runtime** | ~30 seconds |

---

## Sampling Rate

- **After every task commit:** Run `rake` (verify build succeeds with sanitizers)
- **After every plan wave:** Run `rake && rake lint && rake analyze` (full toolchain)
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 30 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 01-01-01 | 01 | 1 | CORR-08 | integration | `rake` (sanitizer build succeeds) | ❌ W0 | ⬜ pending |
| 01-01-02 | 01 | 1 | CORR-08 | integration | `rake run` (game launches under sanitizers) | ❌ W0 | ⬜ pending |
| 01-02-01 | 02 | 1 | CORR-07 | integration | `rake lint` (clang-tidy runs without crashing) | ❌ W0 | ⬜ pending |
| 01-02-02 | 02 | 1 | CORR-07 | integration | `rake analyze` (cppcheck runs and produces report) | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] CMakeLists.txt Sanitize build type — must exist before any verification commands work
- [ ] Rakefile lint/analyze tasks — must exist before verification commands work

*Wave 0 is the implementation itself — there are no pre-existing test stubs to create. Verification is running the tools successfully.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Game renders correctly under Sanitize build | CORR-08 | Visual verification — no automated pixel comparison | Launch game, confirm rendering matches RelWithDebInfo build |
| Hot-reload works under Sanitize build | CORR-08 | Requires live file edit cycle | Edit src/game, save, confirm hot-reload triggers without ASan crash |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 30s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
