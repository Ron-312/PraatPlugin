# PraatPlugin — Architecture

## What This Plugin Does

PraatPlugin is a VST3/AU audio plugin that lets you run Praat speech-analysis scripts from inside any compatible DAW (Ableton, Logic, GarageBand). You load an audio file, see its waveform, drag to select a region of interest, press **Analyze**, and the plugin sends that region to Praat and displays the results — all without leaving your DAW session.

You can also **Play** the selected region back through the plugin's audio output so Ableton (or any DAW) can hear or record it on another track.

It is **not** a realtime effect. Praat was not built for sample-accurate, low-latency DSP. The plugin is an analysis and measurement tool: offline, user-triggered, deterministic.

---

## System Boundary Diagram

```
┌──────────────────────────────────────────────────────────────────────────┐
│  DAW (Ableton, Logic, etc.)                                              │
│                                                                          │
│   Message Thread                               Worker Thread             │
│   ──────────────────────────────               ─────────────────────     │
│   User clicks "Load Audio"                     JobDispatcher             │
│       │                                             │                    │
│       ▼  AudioFormatManager reads file              │                    │
│   PraatPluginProcessor                              │                    │
│   loadedAudioBuffer_ (in memory)                    │                    │
│       │                                             │                    │
│       ▼  WaveformDisplay renders                    │                    │
│   PraatPluginEditor ◄────── 50ms Timer poll         │                    │
│       │                                             │                    │
│       │  User drags selection                       │                    │
│       ▼                                             │                    │
│   PraatPluginProcessor                              │                    │
│   selectedRegionSeconds_                            │                    │
│       │                                             │                    │
│       │  User clicks "Analyze"                      │                    │
│       ▼  copies selected region                     │                    │
│   AnalysisJob.audioToAnalyze ──────────────► JobQueue                   │
│                                                     │                    │
│                                                     ▼                    │
│                                               WavFileWriter              │
│                                     (writes audioToAnalyze → input.wav) │
│                                                     │                    │
│                                                     ▼                    │
│                                               PraatRunner                │
│                                       (spawns Praat --run script)        │
│                                                     │                    │
│                                                     ▼                    │
│                                               ResultParser               │
│                                          (KEY: value from stdout)        │
│                                                     │                    │
│   Audio Thread          ◄── callAsync ──────── AnalysisResult           │
│   ─────────────────────                                                  │
│   processBlock()                                                         │
│       │  AudioTransportSource.getNextAudioBlock()                        │
│       ▼  (plays selected region when user clicks Play)                   │
│   DAW output bus  → Ableton can record to another track                  │
└──────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
                          /tmp/PraatPlugin/<uuid>/
                              input.wav   ← selected region only
```

---

## Modules

### `src/plugin/` — Plugin Shell

The JUCE entry point. Two classes:

- **`PraatPluginProcessor`** — the `AudioProcessor` subclass. Owns all other modules. Loads audio files, manages the selected analysis region, drives `AudioTransportSource` for playback, dispatches analysis jobs, and delivers results. Also implements `AsyncUpdater` to safely bridge the worker thread result back onto the message thread.
- **`PraatPluginEditor`** — the `AudioProcessorEditor` UI. Shows the waveform display, Load Audio button, Play/Stop controls, script selector, Analyze button, results panel, and status bar. Polls job state via a `juce::Timer` at 50 ms intervals.

### `src/ui/` — Waveform Display

- **`WaveformDisplay`** — renders an `AudioBuffer<float>` as a min/max waveform. The user drags on the component to select a region of interest; the selection fires `onSelectionChanged` with the range in seconds. See [ADR-008](adr/ADR-008-file-based-audio-workflow.md).

### `src/audio/` — Audio I/O Utilities

- **`AudioCapture`** — lock-free ring buffer from `processBlock`. **Not used in the current file-based workflow** (retained for future use / unit tests).
- **`WavFileWriter`** — writes an `AudioBuffer<float>` to a WAV file on disk using `juce::WavAudioFormat`. Called only from the worker thread to materialise the selected audio region before passing it to Praat.

### `src/praat/` — Praat Integration

- **`PraatInstallationLocator`** — searches known macOS locations (`/Applications/Praat.app`, `~/Applications`, PATH) for the Praat binary. Supports a user-overridden path stored in plugin state.
- **`PraatRunner`** — launches `Praat --run <script> [args]` as a child process via `juce::ChildProcess`. Captures stdout and stderr. Enforces a 30-second timeout.
- **`PraatScriptInvoker`** — composes the correct CLI argument list from a script file and parameter map.

**How Praat is called:**
```
/Applications/Praat.app/Contents/MacOS/Praat --run /path/to/script.praat [arg1 arg2 ...]
```
- `--run` runs the script in batch mode (no GUI, no windows)
- stdout from `writeInfoLine:` goes to the plugin's result parser
- stderr from Praat errors goes to the error log

### `src/jobs/` — Async Job System

- **`AnalysisJob`** — plain value type holding: input WAV path, script path, current state (`Pending → Running → CompletedSuccessfully | FailedWithError`), and result.
- **`JobQueue`** — thread-safe FIFO. Producer: message thread. Consumer: `JobDispatcher`.
- **`JobDispatcher`** — single `juce::Thread`. Dequeues jobs, calls `WavFileWriter`, calls `PraatRunner`, calls `ResultParser`, then fires `AsyncUpdater` to deliver the result on the message thread.

### `src/scripts/` — Script Management

- **`ScriptManager`** — loads `.praat` files from a user-selected directory. Also exposes bundled example scripts. Validates that a file has a `.praat` extension and is readable.

### `src/results/` — Output Parsing

- **`ResultParser`** — parses the stdout produced by Praat. Looks for lines matching `KEY: value`. Everything else is preserved as raw text. See [ADR-006](adr/ADR-006-result-output-contract.md) for the contract.
- **`AnalysisResult`** — value type holding: parsed key-value pairs, raw output, parse error message, timestamp.

---

## Thread Safety Rules

These rules are non-negotiable. Violating them causes audio glitches, crashes, or data races:

| Thread | What it may do | What it must never do |
|--------|---------------|----------------------|
| Audio thread (`processBlock`) | Append to ring buffer via `AbstractFifo` | File I/O, mutexes, system calls, allocations |
| Message thread | Read/write plugin state, update UI, enqueue jobs | Long-running work, blocking calls |
| Worker thread (`JobDispatcher`) | File I/O, spawn Praat process, parse results | Touch JUCE UI components, call `MessageManager` directly |

The handoff points:
- **Audio → Message**: lock-free `AbstractFifo` in `AudioCapture`
- **Message → Worker**: mutex-protected `std::queue` in `JobQueue`
- **Worker → Message**: `juce::AsyncUpdater::triggerAsyncUpdate()` (safe from any thread)

---

## Data Flow: One Analysis Pass  (v2 — file-based workflow, ADR-008)

1. User clicks **Load Audio** → `FileChooser` opens
2. User selects an audio file → `PraatPluginProcessor::loadAudioFromFile()`:
   - `AudioFormatManager` reads the file into `loadedAudioBuffer_` (in memory)
   - `AudioFormatReaderSource` + `AudioTransportSource` are set up for playback
   - Editor's `WaveformDisplay` is pointed at the buffer → waveform renders
3. User drags on the waveform to select a region:
   - `WaveformDisplay::onSelectionChanged` fires → editor calls `setAnalysisRegionInSeconds()`
4. *(Optional)* User clicks **Play**:
   - `AudioTransportSource` starts → `processBlock` calls `getNextAudioBlock()` → DAW output carries the audio
5. User clicks **Analyze**:
   - Message thread calls `PraatPluginProcessor::beginAnalysisOfSelectedRegion()`
   - Selected samples are **copied** into `AnalysisJob.audioToAnalyze`
   - Job is pushed onto `JobQueue`
6. `JobDispatcher` wakes, dequeues the job:
   - `WavFileWriter` writes `job.audioToAnalyze` → `/tmp/PraatPlugin/<uuid>/input.wav`
   - `PraatRunner` launches `Praat --run script.praat`
   - Praat reads `input.wav`, prints analysis to stdout, exits
   - `ResultParser` parses stdout → `AnalysisResult`
7. `JobDispatcher` calls `MessageManager::callAsync` → `receiveCompletedJob()` runs on message thread:
   - Stores `AnalysisResult`, clears `analysisIsInProgress_`, fires `triggerAsyncUpdate()`
8. Editor timer picks up the result → updates results panel

---

## Temp File Lifecycle

All temp files live under:
```
$TMPDIR/PraatPlugin/<job-uuid>/
    input.wav       ← audio written before Praat runs
    (Praat may write additional files here if the script produces output audio)
```

- Created by `WavFileWriter` on the worker thread
- Praat reads `input.wav` and may write output files (script-dependent)
- Deleted by `JobDispatcher` after the job completes or fails
- macOS also cleans `$TMPDIR` on reboot as a fallback

---

## Praat Output Convention

Scripts communicate results back to the plugin via `writeInfoLine:` statements printed to stdout. The expected format is:

```
mean_pitch: 220.5 Hz
voiced_frames: 82%
duration: 1.432 s
```

Lines not matching `KEY: value` are stored as raw text and shown verbatim. See [ADR-006](adr/ADR-006-result-output-contract.md).

---

## v1 Scope and Known Limitations

| Feature | v1 status |
|---------|-----------|
| Offline analysis (user-triggered) | Supported |
| VST3 format | Supported |
| AU format | Supported |
| macOS arm64 | Supported |
| Non-sandboxed hosts (Ableton Live) | Supported |
| Sandboxed hosts (Logic Pro, sandboxed AU validators) | Not supported — see ADR-005 |
| Realtime processing | Not in scope |
| AAX (Pro Tools) | Not in scope for v1 |
| Windows / Linux | Not in scope for v1 |
| Bundled Praat binary | Not in scope — user installs Praat separately |

---

## Key ADRs

| ADR | Summary |
|-----|---------|
| [ADR-001](adr/ADR-001-juce-integration-strategy.md) | JUCE via CMake FetchContent (not submodule) |
| [ADR-002](adr/ADR-002-praat-ipc-strategy.md) | Direct CLI spawn per job (not a daemon) |
| [ADR-003](adr/ADR-003-audio-thread-isolation.md) | processBlock uses only lock-free AbstractFifo |
| [ADR-004](adr/ADR-004-temp-file-lifecycle.md) | Temp files in TMPDIR/PraatPlugin/UUID/ |
| [ADR-005](adr/ADR-005-sandbox-compatibility-scope.md) | v1 targets non-sandboxed hosts only |
| [ADR-006](adr/ADR-006-result-output-contract.md) | Script output: KEY: value line convention |
| [ADR-007](adr/ADR-007-cmake4-juce8-compatibility.md) | CMake 4.x / JUCE 8 build fixes |
| [ADR-008](adr/ADR-008-file-based-audio-workflow.md) | File-based audio loading + waveform display (replaces ring-buffer capture) |
