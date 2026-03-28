#pragma once

#include <juce_core/juce_core.h>
#include <utility>

// Loads and manages a collection of .praat script files available for selection.
//
// Sources:
//   - Bundled example scripts shipped with the plugin (scripts/examples/)
//   - Scripts from a user-chosen directory (persisted in plugin state)
//   - Community scripts downloaded from GitHub (recursive folder structure)
//
// The ScriptManager does not execute scripts — it only finds and validates them.
// The active script is persisted across DAW sessions via the plugin state.
class ScriptManager
{
public:
    // Represents one named folder of scripts (e.g. "PITCH", "REVERB").
    struct ScriptFolder
    {
        juce::String            folderName;
        juce::Array<juce::File> scripts;
    };

    ScriptManager()  = default;
    ~ScriptManager() = default;

    // Scans the given directory for *.praat files (flat, non-recursive) and adds
    // them to the available scripts list under a synthetic "BUNDLED" folder.
    // Does not clear previously loaded scripts.
    void loadScriptsFromDirectory (const juce::File& directory);

    // Scans immediate subdirectories of rootDirectory, creating one ScriptFolder
    // per subdirectory. Each folder name is the subdirectory name uppercased.
    // Clears all existing scripts first.
    void loadScriptsFromDirectoryRecursive (const juce::File& rootDirectory);

    // Clears all loaded scripts, folders, and resets the active script.
    void clearAllLoadedScripts();

    // ── Flat (legacy) accessors ────────────────────────────────────────────────
    juce::Array<juce::File> availableScripts() const;
    juce::File              scriptAtIndex (int index) const;
    int                     numberOfAvailableScripts() const noexcept;

    // ── Folder-aware accessors ─────────────────────────────────────────────────
    int          numberOfFolders()                          const noexcept;
    juce::String folderNameAtIndex (int folderIndex)        const;
    int          numberOfScriptsInFolder (int folderIndex)  const;
    juce::File   scriptInFolder (int folderIndex, int scriptIndex) const;

    // Returns the folder name for the given script file, or "" if not found.
    juce::String folderNameForScript (const juce::File& script) const;

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
    juce::Array<juce::File>    loadedScripts_;
    juce::Array<ScriptFolder>  loadedFolders_;
    juce::File                 activeScript_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScriptManager)
};
