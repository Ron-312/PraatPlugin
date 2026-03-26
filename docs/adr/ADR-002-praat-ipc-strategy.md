# ADR-002: Invoke Praat via Direct CLI Process Spawn (Not a Persistent Daemon)

**Date:** 2026-03-25
**Status:** Accepted
**Deciders:** Project founder

---

## Context

PraatPlugin needs to invoke Praat to analyze audio. The plugin runs inside a DAW process. Praat is a separate standalone application installed on the user's machine.

There are two primary strategies for connecting a plugin to an external process:

1. **Direct spawn per job** — for each analysis request, launch `Praat --run script.praat` as a short-lived child process via `juce::ChildProcess`. The process runs and exits.
2. **Persistent daemon** — launch a long-lived background helper process once at plugin startup. The plugin communicates with it via a local socket, named pipe, or XPC connection. Jobs are sent to the daemon; the daemon runs Praat internally or spawns it.
3. **Embedded libpraat** — compile Praat's source as a static library, link it directly into the plugin.

The v1 use case is strictly **offline analysis** — the user triggers a single job at a time by pressing "Analyze." There is no streaming or near-realtime requirement.

## Decision

We will use **direct CLI spawn per job** via `juce::ChildProcess`.

The invocation is:
```
/Applications/Praat.app/Contents/MacOS/Praat --run /path/to/script.praat [arg1 arg2 ...]
```

- `--run` activates batch mode: no GUI, stdout replaces the Info window, stderr carries errors
- Arguments after the script path are passed to the script as `$1`, `$2`, etc. (or via Praat's `form` mechanism)
- A 30-second timeout kills the process if Praat hangs

## Rationale

**Why not a daemon:**
- A daemon requires lifecycle management: detecting if the daemon is already running, restarting it after a crash, defining a communication protocol, handling version mismatches between plugin and daemon, and ensuring the daemon is cleaned up when the DAW exits.
- For a single-job, user-triggered workflow, this complexity provides no benefit.
- A daemon would also complicate macOS notarization: helper daemons must be separately signed, notarized, and either bundled with the plugin installer or installed as a separate app.

**Why not embedded libpraat:**
- Praat is licensed under GPL v2+. Linking it into a closed-source plugin would require the plugin to also be GPL v2+.
- Praat's build system is not designed to be used as a library; integrating it would require significant maintenance work every time Praat releases a new version.

**Why direct spawn works for v1:**
- `juce::ChildProcess` provides a clean, cross-platform API for spawning processes and capturing stdout/stderr.
- Praat's `--run` flag was designed exactly for this use case (scripted/batch invocation from other programs).
- Startup overhead (~0.5–2 seconds per invocation) is acceptable for offline analysis where the user has already pressed a button and expects a short wait.

## Alternatives Considered

| Option | Why not chosen |
|--------|---------------|
| Persistent daemon (local HTTP/socket) | Over-engineered for v1; complicates notarization and lifecycle management |
| NSXPCConnection (macOS XPC) | Requires Apple Developer Program, signed helper, and significant boilerplate; appropriate for v2+ if near-realtime analysis is needed |
| Embedded libpraat | GPL licensing conflict; build system complexity |
| sendpraat protocol | An older Praat IPC mechanism that communicates with a running Praat GUI instance — inappropriate for batch/analysis use |

## Consequences

**Positive:**
- Simple: one `juce::ChildProcess` call per job
- No daemon lifecycle to manage
- No IPC protocol to define or version
- Testable in isolation (unit tests can call PraatRunner directly without a DAW)

**Negative / Trade-offs:**
- Each analysis incurs Praat startup time (~0.5–2 seconds). Acceptable for offline mode.
- No job pipelining: jobs run one at a time. For v1 with single-user-triggered jobs, this is fine.
- If the user closes the plugin while a job is running, the child process must be killed cleanly — `JobDispatcher` handles this in its destructor.

**Risks and mitigations:**
- Risk: Praat startup time varies and may feel slow on older hardware.
  Mitigation: Show a spinner/status bar message immediately when Analyze is clicked so the user knows work is in progress.
- Risk: If v2 needs near-realtime analysis (e.g. continuous pitch tracking), direct per-job spawn will be too slow.
  Mitigation: Document this as a known limitation. A future ADR would supersede this one and introduce a persistent `--pipe` or daemon strategy.
- Risk: macOS sandboxed hosts block `NSTask`/`posix_spawn` from plugin processes.
  Mitigation: See ADR-005. v1 explicitly targets non-sandboxed hosts.
