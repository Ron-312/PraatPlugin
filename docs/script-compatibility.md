# Community script compatibility map

Generated 2026-10-29 by the `[compat]` test suite (`tests/test_ScriptCompatibility.cpp`)
against [Praat-plugin_AudioTools](https://github.com/ShaiCohen-ops/Praat-plugin_AudioTools)
(459 scripts, `py/` excluded like ScriptManager does) on **Praat 7.0.02**, macOS arm64.
Each script gets a 1.5 s mono 44.1 kHz harmonic tone and its **default** parameters
(boolean `Play…` fields forced off). Per-script results: `script-compatibility.tsv`.

## Headline: Praat 7 needs `--FULL-TRUST`

Praat 7 refuses file writes (`Save as WAV file`) from `--run` scripts unless launched
with `--FULL-TRUST`. The plugin does not pass it, so **no script can return audio**:

| Run mode | Works | Fails |
|---|---|---|
| As shipped (`--run`) | 11 / 459 (analysis text only) | 448 |
| With `--FULL-TRUST` | 345 / 459 (334 audio + 11 text) | 114 |
| Bundled examples, as shipped | 2 / 9 | 7 |
| Bundled examples, `--FULL-TRUST` | 9 / 9 | 0 |

`PraatRunner` now takes an opt-in `fullTrust` flag, but it stays off in the plugin:
Praat 6 rejects the unknown flag, so turning it on needs Praat version detection, and
it is a trust decision, because downloaded scripts then get full file access.

## With `--FULL-TRUST`: what still fails (114)

| Cause | Count | Fixable in plugin? |
|---|---|---|
| **Plugin bug: form parsing / argument mismatch** (`Found N arguments but expected more`, `Unknown value "1" for option menu`) | 47 | Yes. Parser ignores colon syntax (`real: "Name", "1"`, `optionMenu: …`), so args are dropped or shifted |
| Input-dependent (no pitch found, too short, no onsets, needs a score/text file) | 29 | Partly. Many would pass on real speech/music |
| Needs other objects (2 Sounds, TextGrid, Photo, TextGrid editor) | 14 | No, single-input model. Could hide these |
| Timeout (>30 s on a 1.5 s clip) | 8 | Raise timeout / scale with length |
| Runs but leaves no Sound selected / no output | 7 | Wrapper could save the newest Sound instead of the selection |
| Needs stereo input | 4 | Yes, if the DAW input is stereo |
| Silent output (`Vector Chain/Live_4..6`) | 3 | Investigate |
| External tool / OS (FFmpeg, Windows-only Ableton bridge) | 2 | No. Hide |

## Other issues found

- **Parser misreads names**: `word Output_name …` / `sentence Sofa_file …` match the
  file-path heuristic and switch the script to the file-args model (see KNOWN GAP tests).
- **Case-colliding files** in the upstream repo (`Dynamic_Formant_Sweeper` vs
  `DYNAMIC_FORMANT_SWEEPER`, `Stereo_Shimmer` vs `stereo_shimmer`): on macOS only one survives.
- **Audio playback**: scripts default `Play_result 1`; colon-syntax `boolean: "Play result", 1`
  fields are not parsed, so the plugin cannot turn them off, and Praat plays through the speakers.
- Test suite didn't link before this change (`JobQueue.cpp` missing), and the locator test
  assumed Praat was *not* installed.

## Re-running

```bash
cmake -B build-tests -G Ninja -DBUILD_TESTS=ON && cmake --build build-tests --target PraatPluginTests
PRAAT_EXE=/Applications/Praat.app/Contents/MacOS/Praat PRAAT_SCRIPTS_DIR=<scripts dir> \
PRAAT_FULL_TRUST=1 PRAAT_COMPAT_SHARD=0/8 PRAAT_COMPAT_REPORT=/tmp/c0.tsv \
  ./build-tests/tests/PraatPluginTests "[compat]"     # run shards 0..7 in parallel
```
