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
#if JUCE_WINDOWS
    const auto programFiles = juce::File (juce::SystemStats::getEnvironmentVariable ("ProgramFiles", "C:\\Program Files"));
    return programFiles.getChildFile ("Praat\\Praat.exe");
#else
    return juce::File ("/Applications/Praat.app/Contents/MacOS/Praat");
#endif
}

juce::File PraatInstallationLocator::tryFindingPraatInUserApplicationsFolder() const
{
#if JUCE_WINDOWS
    // 1. Program Files (x86)
    const auto programFilesX86 = juce::File (juce::SystemStats::getEnvironmentVariable ("ProgramFiles(x86)", "C:\\Program Files (x86)"));
    auto candidate = programFilesX86.getChildFile ("Praat\\Praat.exe");
    if (candidate.existsAsFile()) return candidate;

    // 2. Per-user install location (winget / manual drops)
    const auto localAppData = juce::File (juce::SystemStats::getEnvironmentVariable ("LOCALAPPDATA", ""));
    if (localAppData != juce::File{})
    {
        candidate = localAppData.getChildFile ("Programs\\Praat\\Praat.exe");
        if (candidate.existsAsFile()) return candidate;
    }

    // 3. Desktop (users sometimes just drop Praat.exe there)
    const auto desktop = juce::File::getSpecialLocation (juce::File::userDesktopDirectory);
    candidate = desktop.getChildFile ("Praat.exe");
    if (candidate.existsAsFile()) return candidate;

    // 4. "Desktop Shortcuts" subfolder (common on some setups)
    candidate = desktop.getChildFile ("Desktop Shortcuts\\Praat.exe");
    if (candidate.existsAsFile()) return candidate;

    // 5. User home directory
    candidate = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Praat.exe");
    return candidate;
#else
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
               .getChildFile ("Applications/Praat.app/Contents/MacOS/Praat");
#endif
}

juce::File PraatInstallationLocator::tryFindingPraatOnSystemPath() const
{
    // Use `which` (macOS/Linux) or `where` (Windows) to locate Praat if it's on PATH.
    juce::ChildProcess whichProcess;
#if JUCE_WINDOWS
    if (whichProcess.start ("where praat"))
#else
    if (whichProcess.start ("which praat"))
#endif
    {
        whichProcess.waitForProcessToFinish (3000);
        const auto result = whichProcess.readAllProcessOutput().trim();
        if (result.isNotEmpty())
            return juce::File (result);
    }
    return {};
}
