# Contributing to PraatPlugin

## Prerequisites

| Tool | Version | Install |
|------|---------|---------|
| macOS | 15+ (Apple Silicon) | — |
| Xcode Command Line Tools | 17+ | `xcode-select --install` |
| CMake | 3.22+ | `brew install cmake` |
| Praat | 6.x | [Download from fon.hum.uva.nl](https://www.fon.hum.uva.nl/praat/) or `brew install --cask praat` |
| Git | 2.x | Included with CLT |

JUCE is fetched automatically by CMake at configure time (~250 MB, cached after first run).

---

## Building

```bash
# 1 — Clone the repo
git clone <repo-url> PraatPlugin
cd PraatPlugin

# 2 — Configure (first run downloads JUCE ~250 MB)
cmake -B build/debug \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_OSX_ARCHITECTURES=arm64 \
      -G "Unix Makefiles"

# 3 — Build VST3
cmake --build build/debug --target PraatPlugin_VST3 -- -j$(sysctl -n hw.logicalcpu)

# 4 — Build AU
cmake --build build/debug --target PraatPlugin_AU -- -j$(sysctl -n hw.logicalcpu)

# 5 — Install to system plugin folders
cp -r build/debug/PraatPlugin_artefacts/Debug/VST3/PraatPlugin.vst3 \
      ~/Library/Audio/Plug-Ins/VST3/
cp -r build/debug/PraatPlugin_artefacts/Debug/AU/PraatPlugin.component \
      ~/Library/Audio/Plug-Ins/Components/

# 6 — Validate AU
auval -v aufx Prt1 Prat
```

---

## Running Tests

```bash
cmake -B build/tests \
      -DCMAKE_BUILD_TYPE=Debug \
      -DBUILD_TESTS=ON \
      -G "Unix Makefiles"

cmake --build build/tests
cd build/tests && ctest --output-on-failure
```

---

## Code Style

This project uses self-documenting names — code should read like a sentence.

**Do:**
```cpp
void captureAudioFromProcessBlock(const juce::AudioBuffer<float>& block);
RunOutcome launchPraatWithScript(const juce::File& script, const juce::StringArray& args);
bool isPraatExecutableReachable() const;
```

**Don't:**
```cpp
void cap(const juce::AudioBuffer<float>& b);
Result run(juce::File f, juce::StringArray a);
bool check() const;
```

Rules:
- No abbreviations except universally accepted ones (`wav`, `uuid`, `id`, `url`)
- Verb-first method names that state what they do
- Boolean methods start with `is`, `has`, or `can`
- No single-letter variables except loop counters (`i`, `j`)
- `const` on every method that doesn't mutate state

---

## Thread Safety Contract

**The audio thread (`processBlock`) must never:**
- Acquire a mutex
- Allocate or free memory
- Perform file I/O
- Call system APIs (e.g. `open`, `write`, `spawn`)

Anything that needs to happen in response to audio must go through the lock-free `AudioCapture` ring buffer. Everything else happens on the message thread or the `JobDispatcher` worker thread.

If you add a new code path that touches `processBlock`, label it with a comment:
```cpp
// AUDIO-THREAD SAFE: lock-free, no alloc
```

---

## Architecture Decisions

When you make a non-obvious architectural choice, write an ADR (Architecture Decision Record) in `docs/adr/`. Use the template at `docs/adr/template.md`.

ADRs are not just for big decisions. They are for any decision that a future reader might question and wonder "why did they do it this way?"

---

## Writing Praat Scripts for the Plugin

Scripts must be compatible with Praat's `--run` (batch) mode:

- No GUI commands (`View & Edit`, `Edit`, `Draw...`)
- No commands that open windows
- Read audio from the input file passed as argument `$1` (or from a fixed path for testing)
- Print results using `writeInfoLine:` — one line per metric, in `KEY: value` format:

```praat
# pitch_analysis.praat
# Usage: Praat --run pitch_analysis.praat /path/to/input.wav

form Pitch Analysis
    sentence inputFile
endform

Read from file: inputFile$
To Pitch: 0, 75, 600

mean = Get mean: 0, 0, "Hertz"
min  = Get minimum: 0, 0, "Hertz", "Parabolic"
max  = Get maximum: 0, 0, "Hertz", "Parabolic"

writeInfoLine: "mean_pitch: " + string$(mean) + " Hz"
appendInfoLine: "min_pitch: "  + string$(min)  + " Hz"
appendInfoLine: "max_pitch: "  + string$(max)  + " Hz"
```

Lines not matching `KEY: value` are shown verbatim in the results panel and do not cause errors.

---

## Adding a New Module

1. Create `.h` and `.cpp` files under the appropriate `src/` subdirectory.
2. Add the `.cpp` to `target_sources` in `CMakeLists.txt`.
3. Add unit tests in `tests/`.
4. If the module introduces a new architectural boundary (new thread, new external process, new file format), write an ADR.

---

## Reporting Issues

Document decisions that didn't work out in a new ADR with status `Superseded`. Don't delete old ADRs — the history of why we tried something and moved on is valuable.
