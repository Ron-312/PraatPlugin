#pragma once

#include <juce_core/juce_core.h>

// Loads and manages a collection of .praat script files available for selection.
//
// Sources:
//   - Bundled example scripts shipped with the plugin (scripts/examples/)
//   - Scripts from a user-chosen directory (persisted in plugin state)
//
// The ScriptManager does not execute scripts — it only finds and validates them.
// The active script is persisted across DAW sessions via the plugin state.
class ScriptManager
{
public:
    ScriptManager()  = default;
    ~ScriptManager() = default;

    // Scans the given directory for files ending in .praat and adds them
    // to the available scripts list. Does not clear previously loaded scripts.
    void loadScriptsFromDirectory (const juce::File& directory);

    // Clears all loaded scripts and resets the active script.
    void clearAllLoadedScripts();

    juce::Array<juce::File> availableScripts() const;
    juce::File              scriptAtIndex (int index) const;
    int                     numberOfAvailableScripts() const noexcept;

    // Returns true if the file has a .praat extension and exists on disk.
    bool fileAppearsToBeAValidPraatScript (const juce::File& file) const;

    // Sets the script that will be used for the next analysis job.
    // Ignored if the file is not a valid Praat script.
    void setActiveScript (const juce::File& script);

    // Returns the currently selected script, or an invalid File if none is selected.
    juce::File activeScript() const;
    bool       hasActiveScriptSelected() const noexcept;

    // Returns the index of the active script in availableScripts(), or -1.
    int indexOfActiveScript() const;

private:
    juce::Array<juce::File>  loadedScripts_;
    juce::File               activeScript_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScriptManager)
};
