#include "scripts/ScriptManager.h"

void ScriptManager::loadScriptsFromDirectory (const juce::File& directory)
{
    if (! directory.isDirectory())
        return;

    ScriptFolder bundled;
    bundled.folderName = "BUNDLED";

    for (const auto& entry : juce::RangedDirectoryIterator (directory, false, "*.praat"))
    {
        const auto scriptFile = entry.getFile();
        if (fileAppearsToBeAValidPraatScript (scriptFile) && ! loadedScripts_.contains (scriptFile))
        {
            loadedScripts_.add (scriptFile);
            bundled.scripts.add (scriptFile);
        }
    }

    if (bundled.scripts.size() > 0)
        loadedFolders_.add (bundled);
}

void ScriptManager::loadScriptsFromDirectoryRecursive (const juce::File& rootDirectory)
{
    clearAllLoadedScripts();

    if (! rootDirectory.isDirectory())
        return;

    // Collect immediate subdirectories, sorted by name.
    // Must pass File::findDirectories — the default (findFiles) skips directories.
    juce::Array<juce::File> subdirs;
    for (const auto& entry : juce::RangedDirectoryIterator (rootDirectory, false, "*",
                                                             juce::File::findDirectories))
        subdirs.add (entry.getFile());

    subdirs.sort();

    for (const auto& subdir : subdirs)
    {
        // The "py" folder contains hybrid Praat-Python scripts that require
        // an external Python environment and a Praat plugin installation.
        // They are not compatible with the plugin's headless runner model.
        if (subdir.getFileName().equalsIgnoreCase ("py"))
            continue;

        ScriptFolder folder;
        folder.folderName = subdir.getFileName().toUpperCase();

        for (const auto& entry : juce::RangedDirectoryIterator (subdir, false, "*.praat"))
        {
            const auto scriptFile = entry.getFile();
            if (fileAppearsToBeAValidPraatScript (scriptFile) && ! loadedScripts_.contains (scriptFile))
            {
                loadedScripts_.add (scriptFile);
                folder.scripts.add (scriptFile);
            }
        }

        if (folder.scripts.size() > 0)
            loadedFolders_.add (folder);
    }
}

void ScriptManager::clearAllLoadedScripts()
{
    loadedScripts_.clear();
    loadedFolders_.clear();
    activeScript_ = juce::File();
}

juce::Array<juce::File> ScriptManager::availableScripts() const
{
    return loadedScripts_;
}

juce::File ScriptManager::scriptAtIndex (int index) const
{
    if (juce::isPositiveAndBelow (index, loadedScripts_.size()))
        return loadedScripts_[index];
    return {};
}

int ScriptManager::numberOfAvailableScripts() const noexcept
{
    return loadedScripts_.size();
}

bool ScriptManager::fileAppearsToBeAValidPraatScript (const juce::File& file) const
{
    return file.hasFileExtension (".praat") && file.existsAsFile();
}

void ScriptManager::setActiveScript (const juce::File& script)
{
    if (fileAppearsToBeAValidPraatScript (script))
        activeScript_ = script;
}

juce::File ScriptManager::activeScript() const
{
    return activeScript_;
}

bool ScriptManager::hasActiveScriptSelected() const noexcept
{
    return activeScript_.existsAsFile();
}

int ScriptManager::indexOfActiveScript() const
{
    return loadedScripts_.indexOf (activeScript_);
}

int ScriptManager::numberOfFolders() const noexcept
{
    return loadedFolders_.size();
}

juce::String ScriptManager::folderNameAtIndex (int folderIndex) const
{
    if (juce::isPositiveAndBelow (folderIndex, loadedFolders_.size()))
        return loadedFolders_[folderIndex].folderName;
    return {};
}

int ScriptManager::numberOfScriptsInFolder (int folderIndex) const
{
    if (juce::isPositiveAndBelow (folderIndex, loadedFolders_.size()))
        return loadedFolders_[folderIndex].scripts.size();
    return 0;
}

juce::File ScriptManager::scriptInFolder (int folderIndex, int scriptIndex) const
{
    if (juce::isPositiveAndBelow (folderIndex, loadedFolders_.size()))
    {
        const auto& scripts = loadedFolders_[folderIndex].scripts;
        if (juce::isPositiveAndBelow (scriptIndex, scripts.size()))
            return scripts[scriptIndex];
    }
    return {};
}

juce::String ScriptManager::folderNameForScript (const juce::File& script) const
{
    for (const auto& folder : loadedFolders_)
        if (folder.scripts.contains (script))
            return folder.folderName;
    return {};
}
