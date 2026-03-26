# ADR-005: v1 Targets Non-Sandboxed Hosts Only

**Date:** 2026-03-25
**Status:** Accepted
**Deciders:** Project founder

---

## Context

On macOS, audio plugins run inside the host DAW's process. Plugin code inherits the host's sandbox entitlements. Specifically:

- **Non-sandboxed hosts** (Ableton Live, Reaper, Cubase, REAPER): the plugin process has full access to the filesystem and can spawn child processes via `NSTask` / `posix_spawn`. This is what `juce::ChildProcess` uses internally.
- **Sandboxed hosts** (Logic Pro, GarageBand with strict sandbox, App Store AU validators): the plugin process cannot spawn arbitrary child processes. Attempting to do so will either silently fail or crash the process.

PraatPlugin's core feature (`PraatRunner`) depends on spawning `Praat` as a child process. This is incompatible with strict sandboxing.

The primary v1 target host is **Ableton Live 12**, which is not sandboxed on macOS.

## Decision

v1 explicitly targets **non-sandboxed hosts** only.

When the plugin detects that it is running inside a sandboxed host, it will:
1. Disable the **Analyze** button
2. Display a clear message in the status bar: *"Analysis unavailable: this host restricts process spawning"*

It will **not** crash, silently fail, or attempt to spawn Praat anyway.

**How sandbox is detected at runtime:**
```cpp
bool isRunningInSandboxedHost()
{
    // macOS sets APP_SANDBOX_CONTAINER_ID for sandboxed processes
    return juce::SystemStats::getEnvironmentVariable(
               "APP_SANDBOX_CONTAINER_ID", "").isNotEmpty();
}
```

## Rationale

Making PraatPlugin work in sandboxed hosts would require:
1. An **XPC service** — a separately signed, notarized helper process that the plugin communicates with via Apple's XPC framework. The XPC service would be the entity that spawns Praat, not the plugin itself.
2. **Apple Developer Program membership** — required for signing and notarization.
3. A **plugin installer** that places the XPC service in the right location and registers it with launchd.

This is weeks of additional complexity for a feature that the primary v1 use case (Ableton Live) does not require. It makes sense to defer this until there is confirmed demand for Logic Pro or GarageBand support.

## Alternatives Considered

| Option | Why not chosen |
|--------|---------------|
| XPC service from day one | Adds weeks of complexity; premature for a prototype targeting Ableton |
| Crash or silently corrupt when sandbox is detected | Terrible user experience; may damage the host DAW session |
| Skip sandbox detection and rely on users reading documentation | Users in sandboxed hosts would experience mysterious failures with no explanation |
| Hardcoded host allowlist | Fragile; host sandboxing can change between versions |

## Consequences

**Positive:**
- v1 is significantly simpler to build and ship
- Users in Ableton, Reaper, Cubase get full functionality
- The plugin never crashes in sandboxed hosts — it degrades gracefully

**Negative / Trade-offs:**
- Logic Pro and GarageBand users cannot use the Analyze feature
- If a user loads the plugin in Logic, they will see the disabled state with no obvious path forward (other than switching hosts)

**Risks and mitigations:**
- Risk: `APP_SANDBOX_CONTAINER_ID` detection is not reliable across all future macOS versions.
  Mitigation: Also catch `juce::ChildProcess` launch failures and surface them as errors rather than crashes.
- Risk: Future JUCE versions may change how child processes are spawned.
  Mitigation: Sandbox detection and child process launch are isolated in `PraatRunner` and `PraatInstallationLocator`, making it easy to replace the strategy when sandboxed support is added.

## Future Work

A future ADR (ADR-007 or similar) will supersede this one if XPC-based sandbox support is added. That ADR should address:
- XPC service architecture
- How Praat is located from the XPC service context
- Code signing and notarization workflow
- Installer design
