#include "praat/PraatInstallationLocator.h"

PraatInstallationLocator::PraatInstallationLocator() = default;

juce::File PraatInstallationLocator::locatePraatExecutable() const
{
    // 1. User override takes priority.
    if (userHasOverriddenExecutablePath() && userOverriddenPath_.existsAsFile())
        return userOverriddenPath_;

    // 2. Standard system Applications folder.
    auto inSystemApps = tryFindingPraatInSystemApplicationsFolder();
    if (inSystemApps.existsAsFile()) return inSystemApps;

    // 3. User Applications folder.
    auto inUserApps = tryFindingPraatInUserApplicationsFolder();
    if (inUserApps.existsAsFile()) return inUserApps;

    // 4. PATH.
    auto onPath = tryFindingPraatOnSystemPath();
    if (onPath.existsAsFile()) return onPath;

    return {};   // Not found — returns invalid File
}

bool PraatInstallationLocator::isPraatInstalled() const
{
    return locatePraatExecutable().existsAsFile();
}

void PraatInstallationLocator::overrideExecutablePathWithUserChoice (
    const juce::File& userChosenFile)
{
    userOverriddenPath_ = userChosenFile;
}

juce::File PraatInstallationLocator::userOverriddenExecutablePath() const
{
    return userOverriddenPath_;
}

bool PraatInstallationLocator::userHasOverriddenExecutablePath() const noexcept
{
    return userOverriddenPath_.getFullPathName().isNotEmpty();
}

juce::String PraatInstallationLocator::describeSearchResult() const
{
    const auto praatFile = locatePraatExecutable();

    if (praatFile.existsAsFile())
        return "Praat found at: " + praatFile.getParentDirectory().getParentDirectory().getFullPathName();

    return "Praat not found — install from fon.hum.uva.nl/praat or brew install --cask praat";
}

juce::File PraatInstallationLocator::tryFindingPraatInSystemApplicationsFolder() const
{
    return juce::File ("/Applications/Praat.app/Contents/MacOS/Praat");
}

juce::File PraatInstallationLocator::tryFindingPraatInUserApplicationsFolder() const
{
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
               .getChildFile ("Applications/Praat.app/Contents/MacOS/Praat");
}

juce::File PraatInstallationLocator::tryFindingPraatOnSystemPath() const
{
    // Use `which praat` to locate Praat if it's on PATH.
    juce::ChildProcess whichProcess;
    if (whichProcess.start ("which praat"))
    {
        whichProcess.waitForProcessToFinish (3000);
        const auto result = whichProcess.readAllProcessOutput().trim();
        if (result.isNotEmpty())
            return juce::File (result);
    }
    return {};
}
