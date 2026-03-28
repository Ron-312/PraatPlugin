# PraatPlugin

A VST3 / AU audio plugin that runs [Praat](https://www.praat.org) scripts on audio from inside your DAW. Record a take, drag-select the region you care about, tweak the script parameters, and hit **Morph** — the processed audio appears as a second waveform ready to play or export.

---

## What it does

- **Load or record audio** directly inside the plugin window
- **Drag a region** on the waveform to process only a subset
- **Select a Praat script** from any folder on your machine
- **Adjust script parameters** live with sliders, toggles, and dropdowns — values are read directly from the script's `form` block
- **Morph** — runs Praat in headless mode and shows the result waveform
- **Compare** original and processed audio with the built-in transport
- **Export** the morphed audio as a WAV file

---

## Requirements

| Dependency | Notes |
|---|---|
| **macOS 12+** | Windows supported (requires WebView2 Runtime) |
| **Praat** | Download free from [praat.org](https://www.praat.org). The plugin searches standard macOS locations automatically. |
| **DAW** | Any DAW that loads VST3 or AU plugins (Ableton Live, Logic Pro, Reaper, etc.) |

### Build requirements

| Tool | Version |
|---|---|
| CMake | 3.22 or later |
| Xcode Command Line Tools | Any recent version |
| Node.js + npm | For rebuilding the UI only |

---

## Building from source

```bash
# 1. Clone
git clone https://github.com/Ron-312/PraatPlugin.git
cd PraatPlugin

# 2. Build the React UI (only needed when ui/src changes)
cd ui && npm install && npm run build && cd ..

# 3. Configure and build the plugin
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# 4. Deploy to system plugin folders
cp -R build/PraatPlugin_artefacts/Release/VST3/PraatPlugin.vst3 \
      ~/Library/Audio/Plug-Ins/VST3/
cp -R build/PraatPlugin_artefacts/Release/AU/PraatPlugin.component \
      ~/Library/Audio/Plug-Ins/Components/
```

After deploying, rescan plugins in your DAW.

---

## Writing Praat scripts

Scripts must follow a specific calling convention so the plugin can invoke them:

```praat
form My Effect
    sentence inputFile /tmp/input.wav
    sentence outputFile /tmp/output.wav
    real stretchFactor 2.0
    positive windowSize 0.25
endform

# ... your Praat code here ...
# Read input from inputFile$, write result to outputFile$
```

**Rules:**
1. The script must have a `form`/`endform` block
2. The first two fields must be `sentence inputFile` and `sentence outputFile` — the plugin fills these automatically
3. Any additional fields become interactive controls in the plugin UI:
   - `real` / `positive` / `natural` / `integer` → slider + number input
   - `boolean` → ON/OFF toggle
   - `choice` / `optionmenu` → dropdown
   - `sentence` / `word` / `text` → text input
4. Write processed audio to `outputFile$` — the plugin reads it back and displays the result waveform

### Example scripts

The `scripts/examples/` folder contains ready-to-use scripts:

| Script | Description |
|---|---|
| `pitch_analysis.praat` | Measures mean/max pitch and voiced fraction |
| `formant_analysis.praat` | Extracts F1–F4 formant frequencies |
| `pitch_shift.praat` | Transposes pitch by N semitones (PSOLA) |
| `robot_voice.praat` | Flattens pitch to a constant frequency |
| `fuzz_distortion.praat` | Adds harmonic distortion / fuzz effect |

---

## UI overview

```
┌─────────────────────────────────────────────┐
│  PRAAT PLUGIN                    ● Ready    │  ← header + status
├─────────────────────────────────────────────┤
│  [Load File]  [● Record]  [■ Stop]          │  ← audio controls
│  ▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░░░░░▓▓▓▓▓▓▓▓▓▓  │  ← original waveform (drag to select)
│  ░░░░░░░░░░░░░▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░  │  ← morphed waveform
├─────────────────────────────────────────────┤
│  [▶ Original] [▶ Morphed] [■ Stop] [↓ Export]│
├─────────────────────────────────────────────┤
│  Script  [robot_voice          ▼] [Load…]  │
│                                             │
│  PARAMETERS                                 │
│  target_pitch  ●────────────────  [120   ] │
├─────────────────────────────────────────────┤
│  [              ▶  MORPH                  ] │
├─────────────────────────────────────────────┤
│  RESULTS                                    │
│  mean_pitch   220.5 Hz                      │
│  voiced_frac  0.82                          │
└─────────────────────────────────────────────┘
```

The window is resizable — drag any edge to make it taller or wider (420–900 × 480–1400 px).

---

## Project structure

```
PraatPlugin/
├── src/
│   ├── plugin/          # JUCE AudioProcessor + WebView editor
│   ├── audio/           # Live capture (ring buffer) + WAV writer
│   ├── praat/           # Praat locator, runner, form parser
│   ├── jobs/            # Analysis job queue + dispatcher
│   ├── scripts/         # Script manager (file scanning)
│   ├── results/         # Result parser (key-value extraction)
│   └── ui/              # WaveformDisplay (legacy C++ component)
├── ui/                  # React app (Vite, single-file bundle)
│   └── src/
│       ├── components/  # Header, AudioSection, Transport, etc.
│       ├── hooks/       # usePluginState (state + actions)
│       ├── bridge/      # juceBridge.js (JS ↔ C++ events)
│       └── styles/      # Design tokens + global CSS
├── legacy/              # Original JUCE-component editor (preserved)
├── scripts/examples/    # Ready-to-use Praat scripts
├── docs/adr/            # Architecture decision records
└── CMakeLists.txt
```

---

## Architecture

The UI is a React app embedded via JUCE's `WebBrowserComponent`. State flows in one direction:

```
C++ (20fps timer) → emitEventIfBrowserIsVisible("stateUpdate") → React setState → re-render
User action → sendToPlugin(eventId) → withEventListener callback → C++ handler
```

Audio processing is fully file-based: the plugin writes a temp WAV, Praat reads it, writes an output WAV, and the plugin reads that back. This keeps the audio thread completely isolated from the analysis work.

See `docs/adr/` for detailed architecture decision records.

---

## License

MIT
