# Community script compatibility map

Generated 2026-10-29 by the `[compat]` test suite (`tests/test_ScriptCompatibility.cpp`)
against [Praat-plugin_AudioTools](https://github.com/ShaiCohen-ops/Praat-plugin_AudioTools)
(459 scripts, `py/` excluded like ScriptManager does) on **Praat 7.0.02**, macOS arm64.
Each script gets a 1.5 s mono 44.1 kHz harmonic tone and its **default** parameters
(boolean `Play…` fields forced off). Per-script results: `script-compatibility.tsv`.

## Result

| | Library (459) | Bundled (9) |
|---|---|---|
| Before (no `--FULL-TRUST`, old parser) | 11 work | 2 work |
| `--FULL-TRUST` only | 345 work | 9 work |
| **This branch** (`--FULL-TRUST` on Praat 7 + parser fixes) | **381 work** (369 audio + 12 text) | **9 work** |

### What was fixed

- **Praat 7 needs `--FULL-TRUST`.** Praat 7 refuses file writes (`Save as WAV file`) from
  `--run` scripts without it, so no script could return audio. `JobDispatcher` now passes it
  when `Praat --version` reports 7 or newer (`PraatRunner::executableNeedsFullTrust`, cached
  per path); Praat 6 rejects the flag, so it is not passed there. The user picks the
  script, but downloaded community scripts do get full file access.
- **Colon form syntax** (`real: "Delay (s)", "0.25"`, `optionMenu:` + `option: "…"`,
  `form: "Title"`) is now parsed. ~110 scripts use it; their arguments were dropped or shifted.
- **Numeric defaults with hints** (`integer Dots 5 (= top line)`) pass only the number.
- **`endform` with trailing whitespace / CRLF** no longer runs the parser into later code.
- **File-path detection** also requires a path-like or empty default, so `word Output_name
  montage` / `sentence Sofa_file kemar` stay ordinary parameters.
- `infile` / `outfile` / `folder` fields are recognised and quoted.

## What still fails (78), by cause

Checked with the `[compat]` options `PRAAT_COMPAT_CHANNELS`, `PRAAT_COMPAT_TIMEOUT_S`, `PRAAT_COMPAT_ONLY`.

| Group | # | Scripts (examples) | Possible fix |
|---|---|---|---|
| Need **two (or more) Sounds** | 23 | Cross_Synthesis, Spectral_Morph, Creative_Convolution (source + IR), Pitch_Contour_Transfer, Neural_Audio_Mosaic, Math_Operations, Kick_detector_and_bass_adder, Segment_Mixer, Stereo_Mosaic, Ambisonic_Bed_Mixer, Universal_Audio_Export… | **Add a second input ("Sound B") to the plugin**; wrapper loads both and selects A + B |
| **Hidden file/folder dialog** (blank folder field → `chooseFolder$`, or `chooseWriteFile$`), so Praat hangs until timeout | 8 | OT_CORPUS_CONCATENATOR, Timbral_Similarity_Browser, Bayesian_Drone_Weaver, KL_Divergence_Corpus_Resynthesis, Sound_atom_composer, Recursive WAV opener, Scala_Scale_Auralizer, Formant_to_MusicXML | Show a folder/file picker for those fields; detect `choose…$` up front instead of waiting 30 s |
| **Interactive**: Demo window (mouse drawing) or `pause` dialogs | 9 | UPIC_Draw_Synthesis, Draw_Intensity_Envelope, Spatial_Trajectory_Painter, Peephole_montage, Chladni_Resonance_Lab, Audiovisual_Sonification, Sidechain_Feedback_VCA, Polyphonic_Improviser, Universal Convolution Generator | Can't run headless. Label as "interactive, open in Praat" or hide |
| **Upstream script bugs** | 14 | Vector Chain Live_*/chain_2/3/Composition_* (stale `runScript` args), Higher-Order_Ambisonic_Encoder/Decoder, Multi-channel Random Slice Time-Stretcher (formula errors), Rhythmic_Fractal_Granulator | Report to the library author |
| **Input-dependent** (needs speech/notes/onsets/longer audio; pass on real material) | 10 | Audio_Quantizer, OT_Grammar_Learning, OM_Score_Transformer, Dramaturgical_Structure_Composer (>20 s), MDS_Space_Navigator, chain_1, 3D_Audio_Room_Simulator, Onset-Based_Oscillator_Bank, Gesture-Based_Hard_Quantization, SPEAR parser (needs a text file) | None needed; show Praat's error message clearly |
| **Need an image (Photo)** | 4 | Photo_sonification, Spectral_Image_Sonification, Percussive_Image_Sonification, Photo_Brightness… | Optional image input |
| **Need stereo input** | 4 | Mid-Side_Matrix, Mix_Multi-Channel_to_Stereo, Spatial trajectory tracker, Stereo Channel Similarity Meter | **None**: all pass with a stereo take (plugin passes the track's channels) |
| TextGrid editor | 2 | Pitch Loop Finder (both copies) | Can't run headless |
| External tool / OS / not a processor | 5 | Media_ffmpeg_tools (FFmpeg), Send_to_Ableton (Windows), Matrix_Chain (.py), Clean_Ableton_Bridge, setup.praat | Hide |
| Just slow | 1 | chain_7 (44 s) | Raise timeout or scale it with clip length |

## Other issues found

- **Case-colliding files** upstream (`Dynamic_Formant_Sweeper` vs `DYNAMIC_FORMANT_SWEEPER`,
  `Stereo_Shimmer` vs `stereo_shimmer`): on macOS only one survives.
- **Audio playback**: many scripts default `Play_result 1`, and Praat plays through the
  speakers during a plugin run. The plugin could force `Play…` booleans off.
- Test suite did not link before this branch (`JobQueue.cpp` missing), and the locator test
  assumed Praat was *not* installed.

## Re-running

```bash
cmake -B build-tests -G Ninja -DBUILD_TESTS=ON && cmake --build build-tests --target PraatPluginTests
PRAAT_EXE=/Applications/Praat.app/Contents/MacOS/Praat PRAAT_SCRIPTS_DIR=<scripts dir> \
PRAAT_COMPAT_SHARD=0/8 PRAAT_COMPAT_REPORT=/tmp/c0.tsv \
  ./build-tests/tests/PraatPluginTests "[compat]"     # run shards 0..7 in parallel
```
