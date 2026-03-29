#pragma once

#include <juce_core/juce_core.h>

// Searches well-known macOS locations for a usable Praat executable.
//
// Search order (also documented in CONTRIBUTING.md):
//   1. User-overridden path (stored in plugin state)
//   2. /Applications/Praat.app/Contents/MacOS/Praat
//   3. ~/Applications/Praat.app/Contents/MacOS/Praat
//   4. `which praat` on the system PATH
//
// If Praat is not found, all methods return safe defaults and
// isPraatInstalled() returns false — the plugin UI responds by
// disabling the Analyze button and showing an install hint.
//
// See docs/adr/ADR-005 for the sandbox compatibility scope.
class PraatInstallationLocator
{
public:
    PraatInstallationLocator();

    // Returns a valid File pointing to the Praat executable if found,
    // or an invalid (non-existent) File if Praat cannot be located.
    juce::File locatePraatExecutable() const;

    // Returns true if locatePraatExecutable() would return a valid file.
    bool isPraatInstalled() const;

    // Allows the user to manually specify the Praat executable path.
    // Stored in plugin state and takes priority over automatic discovery.
    void overrideExecutablePathWithUserChoice (const juce::File& userChosenFile);
    juce::File userOverriddenExecutablePath() const;
    bool userHasOverriddenExecutablePath() const noexcept;

    // Returns a sentence explaining what was or wasn't found, for the status bar.
    // Example: "Found Praat at /Applications/Praat.app"
    //          "Praat not found — install from fon.hum.uva.nl/praat"
    juce::String describeSearchResult() const;

private:
    juce::File tryFindingPraatInSystemApplicationsFolder() const;
    juce::File tryFindingPraatInUserApplicationsFolder() const;
    juce::File tryFindingPraatOnSystemPath() const;

    juce::File userOverriddenPath_;

    // Cached result of the last full executable search.
    //
    // Finding Praat can be expensive: tryFindingPraatOnSystemPath() spawns a
    // `where` / `which` subprocess and waits up to 3 s for it to complete.
    // Because locatePraatExecutable() is called on the JUCE message thread
    // (e.g. from the 20 Hz editor timer via isPraatAvailable()), running the
    // search on every call would stall the DAW host repeatedly.
    //
    // Caching strategy:
    //   • Positive result (Praat found): cached permanently for the session.
    //     The executable won't move while the DAW is running.
    //   • Negative result (Praat not found): cached for kNotFoundRetryIntervalMs,
    //     then retried automatically.  This lets the plugin recover if the user
    //     installs Praat mid-session without having to manually set the path.
    //
    // The cache is also cleared immediately when the user overrides the path via
    // overrideExecutablePathWithUserChoice(), so the new path takes effect at once.
    //
    // Thread-safety: all fields are only ever read/written from the JUCE
    // message thread, so no mutex is required.
    static constexpr int kNotFoundRetryIntervalMs { 30'000 };  // retry every 30 s

    mutable bool       executableCacheValid_ { false };
    mutable juce::File cachedExecutable_;
    mutable juce::int64 lastSearchTimeMs_    { 0 };

    void clearExecutableCache() noexcept;
};
