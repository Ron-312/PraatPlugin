#include "praat/PraatInstallationLocator.h"
#include "debug/DebugWatchdog.h"

PraatInstallationLocator::PraatInstallationLocator() = default;

juce::File PraatInstallationLocator::locatePraatExecutable() const
{
    // Return the cached result immediately when:
    //   a) Praat was found — its location won't change during a DAW session, or
    //   b) Praat was not found, but the retry interval has not yet elapsed.
    //
    // The search (especially tryFindingPraatOnSystemPath) can block for up to
    // 3 s on the message thread, so we must avoid running it on every timer tick.
    // Retrying the negative result every 30 s lets the plugin recover automatically
    // if the user installs Praat mid-session without setting a manual override.
    if (executableCacheValid_)
    {
        const bool praatFound        = cachedExecutable_.existsAsFile();
        const juce::int64 nowMs      = juce::Time::currentTimeMillis();
        const bool retryIntervalOver = (nowMs - lastSearchTimeMs_) >= kNotFoundRetryIntervalMs;

        if (praatFound || ! retryIntervalOver)
            return cachedExecutable_;
    }

    // ── Run the search once ───────────────────────────────────────────────────

    juce::File result;

    // 1. User override takes priority.
    if (userHasOverriddenExecutablePath() && userOverriddenPath_.existsAsFile())
    {
        result = userOverriddenPath_;
    }
    // 2. Standard system Applications folder.
    else if (auto inSystemApps = tryFindingPraatInSystemApplicationsFolder(); inSystemApps.existsAsFile())
    {
        result = inSystemApps;
    }
    // 3. User Applications folder (and other per-user locations).
    else if (auto inUserApps = tryFindingPraatInUserApplicationsFolder(); inUserApps.existsAsFile())
    {
        result = inUserApps;
    }
    // 4. PATH — slow: spawns a `where` / `which` subprocess.
    else
    {
        result = tryFindingPraatOnSystemPath();
    }

    cachedExecutable_    = result;
    executableCacheValid_ = true;
    lastSearchTimeMs_    = juce::Time::currentTimeMillis();
    return cachedExecutable_;
}

bool PraatInstallationLocator::isPraatInstalled() const
{
    return locatePraatExecutable().existsAsFile();
}

void PraatInstallationLocator::overrideExecutablePathWithUserChoice (
    const juce::File& userChosenFile)
{
    userOverriddenPath_ = userChosenFile;
    // The override changes what locatePraatExecutable() should return, so
    // the previously cached result is no longer valid.
    clearExecutableCache();
}

void PraatInstallationLocator::clearExecutableCache() noexcept
{
    executableCacheValid_ = false;
    cachedExecutable_     = juce::File{};
    lastSearchTimeMs_     = 0;   // force an immediate re-search on next call
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
    PRAAT_TIME_SCOPE ("tryFindingPraatOnSystemPath");
    PRAAT_LOG ("PraatInstallationLocator: searching PATH (spawning 'where'/'which' — may take up to 3 s)");

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
