# ADR-001: Integrate JUCE via CMake FetchContent with a Pinned Version Tag

**Date:** 2026-03-25
**Status:** Accepted
**Deciders:** Project founder

---

## Context

PraatPlugin needs the JUCE framework for its plugin shell (AudioProcessor, AudioProcessorEditor, VST3/AU format wrappers, JUCE utility classes). JUCE is a large C++ framework (~500 MB git history) and can be integrated into a CMake project in several ways:

1. **Git submodule** — JUCE committed as a subdirectory of the repo
2. **CMake FetchContent** — CMake downloads JUCE at configure time, pinned to a tag
3. **Global installation** — JUCE installed system-wide, discovered via `find_package(JUCE)`
4. **Projucer-generated CMake** — use Projucer IDE to export a CMakeLists.txt

The project is starting from scratch with no existing build infrastructure.

## Decision

We will use **CMake `FetchContent`** with a pinned version tag (`8.0.12`).

```cmake
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)
```

## Rationale

- **Small repository:** No JUCE source files are committed to this repo. The repo stays under 1 MB even as JUCE grows.
- **Reproducible builds:** Pinning `GIT_TAG 8.0.12` means every developer and CI run uses the exact same JUCE version. Floating tags like `master` or `HEAD` introduce silent breaking changes.
- **No global install required:** Contributors don't need to install JUCE separately. `cmake configure` fetches everything.
- **`GIT_SHALLOW TRUE`** downloads only the tagged commit, not the full 5+ year git history (~250 MB vs ~500 MB).
- **Future upgrades are explicit:** Upgrading JUCE means changing one line in `CMakeLists.txt` and writing a new ADR if the upgrade has consequences.

## Alternatives Considered

| Option | Why not chosen |
|--------|---------------|
| Git submodule | Adds ~500 MB to the repo. Requires `git submodule update --init` from every contributor. Easy to forget, causing confusing build failures. |
| `find_package(JUCE)` global install | Requires each developer to install the same JUCE version globally. Fragile across machines. No good package manager path on macOS for exact JUCE versions. |
| Projucer-generated CMake | Projucer is effectively deprecated in favor of native CMake in JUCE 6+. It adds a mandatory Projucer dependency and re-export step whenever CMakeLists.txt needs changes. |

## Consequences

**Positive:**
- Zero-friction onboarding: `cmake configure` is all a new contributor needs
- Repo stays lean
- JUCE version is explicit and auditable

**Negative / Trade-offs:**
- First `cmake configure` requires internet access and ~250 MB download
- Subsequent configures use the CMake cache (fast), but a clean build directory requires a re-download

**Risks and mitigations:**
- Risk: `juce-framework/JUCE` tag `8.0.12` is deleted or the repo moves.
  Mitigation: If this ever happens, mirror the tag in a fork and update `GIT_REPOSITORY`.
- Risk: FetchContent does not work in offline/air-gapped CI environments.
  Mitigation: Pre-populate the CMake fetch cache or switch to a git submodule if offline CI is required in the future.
