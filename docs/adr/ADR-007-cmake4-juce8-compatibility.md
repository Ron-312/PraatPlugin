# ADR-007: CMake 4.x / JUCE 8 Compatibility Fixes

**Date:** 2026-03-26
**Status:** Accepted
**Deciders:** Project founder

---

## Context

When the project was first configured with CMake 4.3.0 (Homebrew default), two immediate
failures occurred that required concrete fixes before the build would proceed.

## Decision

Apply three targeted compatibility fixes to `CMakeLists.txt` and source files.

---

### Fix 1 — CMake version range in `cmake_minimum_required`

**Problem:** `cmake_minimum_required(VERSION 3.22)` caused CMake 4.x to enforce its
strictest new policies. JUCE 8.0.12 was written before CMake 4.0 and leaves the C language
variable `CMAKE_C_COMPILE_OBJECT` unset under those policies, failing at the Generate step.

**Fix:**
```cmake
cmake_minimum_required(VERSION 3.22...4.3)
```
The range syntax tells CMake 4.x to use the policies from 3.22 through the current version,
which enables backward-compatible behaviour for dependencies like JUCE that haven't yet
adopted CMake 4 idioms.

---

### Fix 2 — Explicit `LANGUAGES C CXX` in `project()`

**Problem:** `LANGUAGES CXX` alone caused CMake 4.x to not fully initialise the C compiler
toolchain variables. JUCE internally uses C for some object-library targets (graphics,
audio), so the missing variables caused generation to fail.

**Fix:**
```cmake
project(PraatPlugin ... LANGUAGES C CXX)
```

---

### Fix 3 — JUCE 8 removed `FileChooser::browseForDirectory()`

**Problem:** The synchronous `FileChooser::browse*()` family was removed in JUCE 7/8 in
favour of the async `launchAsync()` API (to support iOS and other platforms where blocking
the UI thread for a dialog is not allowed).

**Fix:** Replaced the synchronous call in `PraatPluginEditor::onLoadScriptsButtonClicked()`
with `launchAsync()`. A `std::unique_ptr<juce::FileChooser> activeFileChooser_` member
keeps the chooser alive for the duration of the async callback, then resets it.

---

## Other Build-Time Type Fixes

| Issue | Fix |
|-------|-----|
| `juce::StringPair` doesn't exist | Use `std::pair<juce::String, juce::String>` |
| `StringPairArray::getValue(int, ...)` overload ambiguous | Use `getAllValues()` which returns a `StringArray` |
| `juce::File::operator=({})` ambiguous | Use explicit `juce::File()` default constructor |
| Constructor parameter `processor` shadowed JUCE's inherited `processor` member | Renamed parameter to `ownerProcessor`, stored as `praatProcessor_` |

## Consequences

**Positive:** Build succeeds. No functional behaviour changed — these are all API surface
adjustments, not logic changes.

**Negative:** If JUCE releases 9.x or CMake releases 5.x, similar compatibility checks
will be needed. The version range `3.22...4.3` should be updated to include the new
maximum version when that happens.
