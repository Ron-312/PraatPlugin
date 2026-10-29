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

## What still fails (78)

| Cause | Count | Plugin-side? |
|---|---|---|
| Input-dependent (no pitch/onsets, too short, needs a score/text file) | ~29 | Mostly no. Many pass on real speech/music |
| Needs other objects (2 Sounds, TextGrid, Photo, TextGrid editor) | ~14 | No, single-input model. Could hide these |
| Upstream bug: `Vector Chain` scripts call sub-scripts with stale argument lists | 10 | No. Report upstream |
| Runs but leaves no Sound selected / no output | 10 | Maybe: wrapper could save the newest Sound |
| Timeout (>30 s on a 1.5 s clip) | 9 | Raise timeout / scale with length |
| Needs stereo input | 4 | Only with stereo input |
| Formula errors in Ambisonic/multichannel scripts (`Unknown symbol «Point»`) | 3 | No. Upstream |
| External tool / OS (FFmpeg, Windows-only Ableton bridge) | 2 | No. Hide |

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
