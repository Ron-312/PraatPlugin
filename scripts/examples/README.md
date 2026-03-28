# Example Scripts

These nine scripts ship bundled inside PraatPlugin and are extracted to
`~/Library/Application Support/PraatPlugin/scripts/` on first run.

They are adapted from [Shay Cohen's Praat-plugin_AudioTools](https://github.com/ShaiCohen-ops/Praat-plugin_AudioTools)
library for use with PraatPlugin's headless CLI calling convention.
For 300+ more scripts across every category, see that repo.

---

## Scripts in this folder

| Script | What it does | Key parameters |
|---|---|---|
| `pitch_analysis.praat` | Measures mean / min / max pitch and std deviation | — |
| `formant_analysis.praat` | Extracts F1–F4 formant frequencies at the file midpoint | — |
| `pitch_shift.praat` | Transposes pitch by N semitones using PSOLA | `semitones` |
| `robot_voice.praat` | Flattens pitch to a constant frequency (monotone effect) | `target_pitch` (Hz) |
| `fuzz_distortion.praat` | Soft-clipping saturation via tanh curve | `drive` (0–10) |
| `stereo_phaser.praat` | LFO-modulated comb filter with stereo width | `rate_Hz`, `depth_ms`, `dry_wet` |
| `spectral_reverb.praat` | Stochastic convolution reverb with synthesized IR | `tail_s`, `wet_mix`, `decay_speed` |
| `wavefolder.praat` | Buchla-style wavefolding distortion | `threshold`, `fold_depth`, `input_gain_dB` |
| `paulstretch.praat` | Extreme time-stretch via FFT phase randomisation | `stretch_factor`, `window_s` |

---

## Writing your own script

Any `.praat` file that follows this calling convention will work in PraatPlugin.
Drop it in `~/Library/Application Support/PraatPlugin/scripts/` (or any folder
you load via **BROWSE**) and it appears in the script selector immediately.

### Minimal template

```praat
form My Script
    sentence inputFile /tmp/input.wav
    sentence outputFile /tmp/output.wav
    # Add your parameters here — each becomes a UI control
    real myParam 1.0
endform

# Read the input
Read from file: inputFile$

# ... your processing ...

# Write the result (the plugin shows this as the morphed waveform)
Write to WAV file: outputFile$

# Print results — these appear in the Results panel
appendInfoLine: "myParam: ", myParam
```

### Form field types → UI controls

| Praat type | Plugin UI |
|---|---|
| `real` / `positive` / `integer` / `natural` | Slider + number input |
| `boolean` | ON / OFF toggle |
| `choice` / `optionmenu` | Dropdown menu |
| `sentence` / `word` / `text` | Text input |

### Rules

1. `sentence inputFile` and `sentence outputFile` must be the first two fields — the plugin fills them in automatically, they don't appear in the UI
2. Script runs in headless mode (`Praat --run`) — no GUI dialogs
3. Write processed audio to `outputFile$` for the morphed waveform to appear
4. Print `KEY: value` lines to show results in the Results panel
5. Scripts that only analyse (no audio output) still work — just skip writing `outputFile$`

### Analysis-only example

```praat
form Pitch Stats
    sentence inputFile /tmp/input.wav
    sentence outputFile /tmp/output.wav
endform

Read from file: inputFile$
To Pitch (ac)... 0 75 600 15 no 0.03 0.45 0.01 0.35 0.14

mean_pitch = Get mean... 0 0 Hertz
min_pitch  = Get minimum... 0 0 Hertz Parabolic
max_pitch  = Get maximum... 0 0 Hertz Parabolic

appendInfoLine: "mean_pitch: ", mean_pitch
appendInfoLine: "min_pitch: ",  min_pitch
appendInfoLine: "max_pitch: ",  max_pitch
```

### Processing example (with audio output)

```praat
form Pitch Shift
    sentence inputFile /tmp/input.wav
    sentence outputFile /tmp/output.wav
    real semitones 7.0
endform

Read from file: inputFile$
To Pitch (ac)... 0 75 600 15 no 0.03 0.45 0.01 0.35 0.14
plus Sound untitled
To PointProcess (peaks)... 0.9 1 yes no

plus Sound untitled
plus Pitch untitled

Overlap-add... 1 semitones 1 1 0.01 50

Write to WAV file: outputFile$

appendInfoLine: "semitones: ", semitones
```

---

## More scripts

**[Praat-plugin_AudioTools](https://github.com/ShaiCohen-ops/Praat-plugin_AudioTools)** by Shay Cohen —
300+ scripts across Analysis, Pitch, Distortion, Reverb, Spectral, Spatial,
Time & Granular, Modulation, Filter, Dynamics, Generative, and AI categories.

PraatPlugin downloads this library automatically on first run. Hit the **↓** button
in the plugin at any time to update to the latest version.
