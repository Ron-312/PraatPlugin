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
};
