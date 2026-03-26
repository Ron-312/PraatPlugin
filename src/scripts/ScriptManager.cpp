#include "scripts/ScriptManager.h"

void ScriptManager::loadScriptsFromDirectory (const juce::File& directory)
{
    if (! directory.isDirectory())
        return;

    for (const auto& entry : juce::RangedDirectoryIterator (directory, false, "*.praat"))
    {
        const auto scriptFile = entry.getFile();
        if (fileAppearsToBeAValidPraatScript (scriptFile) && ! loadedScripts_.contains (scriptFile))
            loadedScripts_.add (scriptFile);
    }
}

void ScriptManager::clearAllLoadedScripts()
{
    loadedScripts_.clear();
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
