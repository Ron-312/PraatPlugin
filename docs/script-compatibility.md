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
| + `--FULL-TRUST` on Praat 7 | 345 | 9 |
| + form parser fixes | 381 | 9 |
| **+ Sound B, folder pickers, headless defaults** (current) | **404** (393 audio + 11 text) | **9** |

The test fills in what the plugin UI asks the user for: a second Sound for `2 SOUNDS`
scripts (only if the run fails without one, since it's optional for some), and a folder
of three clips for `FOLDER` fields.

### What was fixed

- **Praat 7 needs `--FULL-TRUST`.** Praat 7 refuses file writes (`Save as WAV file`) from
  `--run` scripts without it. `JobDispatcher` passes it when `Praat --version` reports 7+
  (`PraatRunner::executableNeedsFullTrust`); Praat 6 rejects the flag, so it isn't passed there.
- **Form parser:** colon syntax (`real: "Delay (s)", "0.25"`, `optionMenu:` + `option: "…"`,
  `form: "Title"`), numeric defaults with hints, padded `endform`, `infile`/`outfile`/`folder`,
  and file-path detection that needs a path-like or empty default.
- **Sound B** (`ScriptCapabilities::usesSecondSound`): the wrapper loads both Sounds and
  selects A then B.
- **Folder fields** get a BROWSE button; a run with a blank folder field that would open
  `chooseFolder$` is refused up front instead of hanging for 30 s.
- **Headless defaults:** `Play…` and `…editor` booleans start off.
- **No-selection output:** when a script leaves nothing selected, the newest non-input Sound
  is saved.
- **Script tags** in the browser: `2 SOUNDS`, `FOLDER`, `DIALOG`, `WINDOW`, `UNSUPPORTED`.

## What still fails (55)

| Cause | # | Scripts | Plugin-side? |
|---|---|---|---|
| Upstream script bugs | 14 | Vector Chain Live_1–3(+Random), Composition_1/2, chain_2/3 (stale `runScript` args); Higher-Order Ambisonic Encoder/Decoder, Multi-channel Random Slice Time-Stretcher (formula errors); Rhythmic_Fractal_Granulator | No. Report upstream |
| Interactive Demo windows / pause dialogs (tagged `WINDOW`) | 9 | UPIC_Draw_Synthesis, Draw_Intensity_Envelope, Spatial_Trajectory_Painter, Peephole_montage, Chladni_Resonance_Lab, Audiovisual_Sonification, Polyphonic_Improviser, Universal Convolution Generator, Creative_Convolution | No, needs the Praat app |
| Input-dependent (too short, no notes/onsets, needs a score/text file) | 9 | Dramaturgical_Structure_Composer (>20 s), OT_Grammar_Learning, OM_Score_Transformer, MDS_Space_Navigator, chain_1, 3D_Audio_Room_Simulator, Onset-Based_Oscillator_Bank, Gesture-Based_Hard_Quantization, SPEAR parser | Mostly fine on real material |
| Need stereo / mono-only / multi-Sound beds | 7 | Mid-Side_Matrix, Mix_Multi-Channel_to_Stereo, Spatial trajectory tracker, Stereo Channel Similarity Meter (all pass with stereo input); Audio_Quantizer, Sidechain_Feedback_VCA (want mono / exactly one); Ambisonic_Bed_Mixer (B-format) | Stereo ones work on stereo tracks |
| Need an image (Photo), tagged `UNSUPPORTED` | 4 | Photo_sonification, Spectral_Image_Sonification, Percussive_Image_Sonification, Photo_Brightness… | Could add image input |
| Unconditional file dialog (tagged `DIALOG`) | 3 | Formant_to_MusicXML, Scala_Scale_Auralizer, Recursive WAV opener | No |
| External tool / OS / helper (tagged `UNSUPPORTED`) | 7 | Media_ffmpeg_tools, Batch_Channel_Format_Exporter (FFmpeg), Send_to_Ableton (Windows), Matrix_Chain (.py), Clean_Ableton_Bridge, Universal_Audio_Export / Universal_Audio_IO (batch exporters), setup.praat | No |
| Slow (44 s) | 1 | chain_7 | Raise the timeout |

## Other issues found

- **Case-colliding files** upstream (`Dynamic_Formant_Sweeper` vs `DYNAMIC_FORMANT_SWEEPER`,
  `Stereo_Shimmer` vs `stereo_shimmer`): on macOS only one survives.
- Test suite did not link before this branch (`JobQueue.cpp` missing), and the locator test
  assumed Praat was *not* installed.

## Re-running

```bash
cmake -B build-tests -G Ninja -DBUILD_TESTS=ON && cmake --build build-tests --target PraatPluginTests
PRAAT_EXE=/Applications/Praat.app/Contents/MacOS/Praat PRAAT_SCRIPTS_DIR=<scripts dir> \
PRAAT_COMPAT_SHARD=0/8 PRAAT_COMPAT_REPORT=/tmp/c0.tsv \
  ./build-tests/tests/PraatPluginTests "[compat]"     # run shards 0..7 in parallel
```
