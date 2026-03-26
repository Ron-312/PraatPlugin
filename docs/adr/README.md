# Architecture Decision Records

This directory documents the significant decisions made during the design and development of PraatPlugin. Each ADR captures the context, the decision made, and the reasoning — so future contributors understand not just *what* was decided, but *why*.

## Index

| ADR | Title | Status |
|-----|-------|--------|
| [ADR-001](ADR-001-juce-integration-strategy.md) | JUCE Integration via CMake FetchContent | Accepted |
| [ADR-002](ADR-002-praat-ipc-strategy.md) | Praat IPC via Direct CLI Spawn | Accepted |
| [ADR-003](ADR-003-audio-thread-isolation.md) | Audio Thread Isolation via Lock-Free Ring Buffer | Accepted |
| [ADR-004](ADR-004-temp-file-lifecycle.md) | Temp Files in TMPDIR/PraatPlugin/UUID/ | Accepted |
| [ADR-005](ADR-005-sandbox-compatibility-scope.md) | v1 Targets Non-Sandboxed Hosts Only | Accepted |
| [ADR-006](ADR-006-result-output-contract.md) | Praat Output Contract: KEY: value Lines | Accepted |
| [ADR-007](ADR-007-cmake4-juce8-compatibility.md) | CMake 4.x / JUCE 8 Compatibility Fixes | Accepted |
| [ADR-008](ADR-008-file-based-audio-workflow.md) | File-Based Audio Workflow with Waveform Display | Accepted |

## How to Add a New ADR

1. Copy `template.md` to `ADR-NNN-short-title.md`
2. Fill in all sections
3. Add a row to the index table above
4. Reference the ADR from relevant source files with a comment, e.g.:
   ```cpp
   // See ADR-003: audio-thread isolation
   ```

## ADR Statuses

- **Proposed** — under discussion, not yet implemented
- **Accepted** — decided and in effect
- **Superseded by ADR-NNN** — replaced by a later decision (keep the file, update status)
- **Deprecated** — no longer relevant but preserved for history
