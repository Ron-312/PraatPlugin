# Contributing to PraatPlugin

## Prerequisites

| Tool | Version | Install |
|------|---------|---------|
| macOS | 12+ | — |
| Xcode Command Line Tools | Any recent version | `xcode-select --install` |
| **Windows** | 10+ | — |
| Visual Studio | 2022 (MSVC 19.3+) | [visualstudio.com](https://visualstudio.microsoft.com/) |
| WebView2 Runtime | Any | Pre-installed on Windows 10/11; JUCE fetches the SDK headers automatically |
| CMake | 3.22+ | `brew install cmake` |
| Node.js + npm | 18+ | `brew install node` |
| Praat | 6.x | [Download from fon.hum.uva.nl](https://www.fon.hum.uva.nl/praat/) or `brew install --cask praat` |
| Git | 2.x | Included with CLT |

JUCE is fetched automatically by CMake at configure time (~250 MB, cached after first run).

---

## Building

```bash
# 1 — Clone the repo
git clone https://github.com/Ron-312/PraatPlugin.git PraatPlugin
cd PraatPlugin

# 2 — Build the React UI (only needed when ui/src changes)
cd ui && npm install && npm run build && cd ..

# 3 — Configure (first run downloads JUCE ~250 MB)
cmake -B build/debug \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_OSX_ARCHITECTURES=arm64 \
      -G "Unix Makefiles"

# 4 — Build VST3
cmake --build build/debug --target PraatPlugin_VST3 -- -j$(sysctl -n hw.logicalcpu)

# 5 — Build AU
cmake --build build/debug --target PraatPlugin_AU -- -j$(sysctl -n hw.logicalcpu)

# 6 — Install to system plugin folders
cp -r build/debug/PraatPlugin_artefacts/Debug/VST3/PraatPlugin.vst3 \
      ~/Library/Audio/Plug-Ins/VST3/
cp -r build/debug/PraatPlugin_artefacts/Debug/AU/PraatPlugin.component \
      ~/Library/Audio/Plug-Ins/Components/

# 7 — Validate AU
auval -v aufx Prt1 Prat
```

> **Note on `auval` warnings:** Debug builds may emit warnings about missing Info.plist keys or entitlements. These are expected and do not indicate a failure. The test passes as long as the final line reads `PASS`.

### Windows

```bat
git clone https://github.com/Ron-312/PraatPlugin.git PraatPlugin
cd PraatPlugin

cd ui && npm ci && npm run build && cd ..

cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build --target PraatPlugin_VST3 --config Debug --parallel
```

Copy `build\PraatPlugin_artefacts\Debug\VST3\PraatPlugin.vst3` to `%CommonProgramFiles%\VST3\`.
AU is macOS-only; VST3 is the Windows format.

---

## UI Development

The plugin UI is a React app bundled by Vite into a single HTML file that JUCE serves via `WebBrowserComponent`. You only need to touch this when modifying files under `ui/src/`.

```bash
cd ui

# Install dependencies (first time or after package.json changes)
npm install

# Hot-reload dev server — open in a browser for fast iteration
# Note: the bridge to C++ won't work here; use stub data in juceBridge.js
npm run dev

# Produce the final bundle (required before building the plugin)
npm run build
```

After `npm run build`, the output is written to `ui/dist/`. CMake copies this into the plugin binary at build time — you must rebuild the C++ target after any UI change.

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

## Contributing a Change

1. **Fork** the repo on GitHub and clone your fork locally.
2. **Create a branch** from `master`:
   - `feature/<short-name>` for new functionality
   - `fix/<short-name>` for bug fixes
   - `docs/<short-name>` for documentation-only changes
3. Make your changes — keep each PR focused on one concern.
4. **Run the tests** before opening a PR (see [Running Tests](#running-tests)).
5. **Open a PR** against `master` on `Ron-312/PraatPlugin`.

**What reviewers check:**
- New code paths in `processBlock` are labeled `// AUDIO-THREAD SAFE: lock-free, no alloc`
- New modules have unit tests in `tests/`
- Non-obvious architectural choices have a corresponding ADR in `docs/adr/`
- CI passes (build + ctest on both macOS and Windows — see `.github/workflows/ci.yml`)

---

## Commit Messages

Use the format `<type>: <short description>` (imperative mood, ≤ 72 chars):

```
feat: add spectral freeze script to bundled library
fix: prevent double-free in JobDispatcher on early cancel
refactor: extract WAV path logic into AudioFileManager
test: add ring-buffer overflow coverage
docs: clarify auval validation step in CONTRIBUTING
chore: bump JUCE to 7.0.12
```

A commit body is optional but encouraged for non-obvious changes — explain *why*, not *what*.

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

## Superseding Architecture Decisions

Document decisions that didn't work out in a new ADR with status `Superseded`. Don't delete old ADRs — the history of why we tried something and moved on is valuable.

For bugs or feature requests, open a GitHub issue at [Ron-312/PraatPlugin](https://github.com/Ron-312/PraatPlugin/issues).

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
