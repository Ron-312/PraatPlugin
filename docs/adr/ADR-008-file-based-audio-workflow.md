# ADR-008 — File-Based Audio Workflow with Waveform Display

**Status:** Accepted

---

## Context

The original v1 design (ADR-003) captured audio from the DAW's `processBlock` using a lock-free ring buffer. The user would play audio into the plugin live, then press **Analyze**. This turned out to be the wrong user experience for a speech analysis tool:

1. **The plugin is an offline analyzer, not a live effect.** Users want to analyse a specific recording, not a fleeting live moment.
2. **The ring buffer captured silence.** When the plugin sits on a DAW track without active playback, `processBlock` receives silence. The user had no way to know whether audio had actually been captured.
3. **No way to target a region.** Speech analysis (pitch, formants) is meaningful only on a defined segment — a vowel, a word, a sentence. The ring buffer approach captured an arbitrary 30-second window with no region control.
4. **No visual feedback.** Users could not see what audio was captured, which made the tool feel like a black box.

The intended workflow — analogous to how users work in Praat itself — is:

> Open a file → see the waveform → select a region → run a script on that region.

---

## Decision

Replace the ring-buffer live-capture model with a **file-based audio loading workflow**:

1. **Load Audio** button opens a file picker (`juce::FileChooser` with `launchAsync`).
2. The selected audio file is read into an in-memory `juce::AudioBuffer<float>` by the processor.
3. A new `WaveformDisplay` component renders the waveform. The user drags on it to select a region of interest.
4. **Play** button plays the selected region through the plugin's audio output, so Ableton (or any DAW) can hear and optionally record it on another track.
5. **Analyze** button extracts the selected audio samples, writes them to a temp WAV file, and runs the selected Praat script — identical to the existing job pipeline from that point onward.

---

## Consequences

### Changes to source files

| File | Change |
|------|--------|
| `src/ui/WaveformDisplay.{h,cpp}` | **New.** Waveform rendering, drag-to-select, selection highlight. |
| `src/plugin/PraatPluginProcessor.{h,cpp}` | Remove `AudioCapture` member. Add `loadAudioFromFile()`, `setAnalysisRegion()`, playback API, `beginAnalysisOfSelectedRegion()`. |
| `src/jobs/AnalysisJob.h` | Add `audioToAnalyze` (`AudioBuffer<float>`) + `audioSampleRate` fields. |
| `src/jobs/JobDispatcher.{h,cpp}` | Remove `AudioCapture` dependency. Write temp WAV from `job.audioToAnalyze` buffer. |
| `src/plugin/PraatPluginEditor.{h,cpp}` | New layout: header Load Audio button, waveform area, Play/Stop, script row, Analyze, results. |
| `CMakeLists.txt` | Add `juce::juce_audio_utils` (AudioFormatManager, AudioTransportSource). Add `WaveformDisplay.cpp` to sources. |

### What is preserved

- `AudioCapture.h/.cpp` — kept intact (referenced by unit tests; may serve future realtime features).
- All Praat IPC code (`PraatRunner`, `PraatScriptInvoker`, `ResultParser`).
- The temp-file lifecycle contract (ADR-004) — UUID dirs, cleanup after job.
- `PraatLookAndFeel` and all custom UI styling.

### Thread safety notes

- `loadedAudioBuffer_` is read and written exclusively on the **message thread** (editor paint calls happen on the message thread, `loadAudioFromFile` is called by editor button handler).
- Before creating an `AnalysisJob`, the selected region is extracted into `job.audioToAnalyze` (a copy) on the **message thread**. The worker thread operates only on the copy.
- `AudioTransportSource` handles its own internal thread safety for `start()`/`stop()` (message thread) vs `getNextAudioBlock()` (audio thread).
- `playbackRegionEndSeconds_` is an `std::atomic<double>` so `processBlock` can read the end boundary without a mutex.

### Alternatives considered

| Option | Reason rejected |
|--------|----------------|
| Keep ring-buffer, add waveform preview of ring buffer | Ring buffer wraps around, making region selection confusing. Still doesn't solve the "silence captured" problem. |
| AudioThumbnail (juce_audio_utils) for waveform | Requires background thumbnail generation thread. Simpler to draw min/max directly from the in-memory buffer for the file sizes we expect. |
| ReaderSource only (never load to memory) | Would need a second reader for the waveform pass. Loading once into memory is simpler and allows instant region extraction. |

---

## References

- ADR-002: Praat IPC via Direct CLI Spawn
- ADR-003: Audio Thread Isolation (ring buffer; now used only by AudioCapture unit tests)
- ADR-004: Temp File Lifecycle
- `src/ui/WaveformDisplay.h` — waveform component implementation notes
