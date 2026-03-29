#include "scripts/ScriptDownloader.h"
#include "debug/DebugWatchdog.h"

ScriptDownloader::ScriptDownloader()
    : juce::Thread ("ScriptDownloader")
{
}

ScriptDownloader::~ScriptDownloader()
{
    // Give the thread up to 5 seconds to finish, then force-stop.
    stopThread (5000);
}

void ScriptDownloader::startDownload()
{
    if (isThreadRunning())
        return;
    startThread (juce::Thread::Priority::background);
}

juce::File ScriptDownloader::communityScriptsRoot()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("PraatPlugin")
               .getChildFile ("community_scripts")
               .getChildFile ("Praat-plugin_AudioTools-main");
}

bool ScriptDownloader::isAlreadyDownloaded()
{
    return communityScriptsRoot().isDirectory();
}

// ─── Background thread ────────────────────────────────────────────────────────

void ScriptDownloader::run()
{
    // ── 1. Determine temp zip path ─────────────────────────────────────────────
    const juce::File tempDir  = juce::File::getSpecialLocation (juce::File::tempDirectory);
    const juce::File tempZip  = tempDir.getChildFile ("PraatPlugin_community_scripts.zip");

    // Clean up any leftover zip from a previous failed attempt.
    tempZip.deleteFile();

    // ── 2. Download ────────────────────────────────────────────────────────────
#if JUCE_WINDOWS
    // On Windows 10 1803+ and all of Windows 11, curl.exe ships in System32.
    // We use the full path to bypass Git's bundled curl, which has SSL issues
    // with GitHub on some machines.
    {
        const auto systemRoot = juce::File (
            juce::SystemStats::getEnvironmentVariable ("SystemRoot", "C:\\Windows"));
        const auto curlExe = systemRoot.getChildFile ("System32\\curl.exe");

        juce::ChildProcess curl;
        const juce::StringArray curlArgs {
            curlExe.getFullPathName(),
            "-L", "-s", "--max-time", "120",
            "--ssl-no-revoke",   // Schannel can't always reach the CRL endpoint
            "-o", tempZip.getFullPathName(),
            kZipUrl
        };

        if (! curl.start (curlArgs))
        {
            PRAAT_LOG_ERR ("ScriptDownloader: failed to launch curl.exe — Windows 10 1803+ required");
            juce::MessageManager::callAsync ([cb = onComplete]
            {
                if (cb) cb (false, "Could not launch curl.exe — Windows 10 1803+ required.");
            });
            return;
        }

        curl.waitForProcessToFinish (130000);

        if (threadShouldExit())
        {
            tempZip.deleteFile();
            return;
        }

        if (curl.getExitCode() != 0 || ! tempZip.existsAsFile() || tempZip.getSize() == 0)
        {
            PRAAT_LOG_ERR ("ScriptDownloader: curl failed (exit "
                           + juce::String (curl.getExitCode()) + ")");
            tempZip.deleteFile();
            juce::MessageManager::callAsync ([cb = onComplete]
            {
                if (cb) cb (false, "Download failed — check your internet connection.");
            });
            return;
        }
    }
#else
    // On macOS, curl is always available and handles GitHub redirects reliably.
    juce::ChildProcess curl;
    const juce::StringArray curlArgs {
        "curl", "-L", "-s", "--max-time", "120",
        "-o", tempZip.getFullPathName(),
        kZipUrl
    };

    if (! curl.start (curlArgs))
    {
        juce::MessageManager::callAsync ([cb = onComplete]
        {
            if (cb) cb (false, "Failed to launch curl — is curl installed?");
        });
        return;
    }

    curl.waitForProcessToFinish (130000);   // 130s timeout (matches --max-time + margin)

    if (threadShouldExit())
    {
        tempZip.deleteFile();
        return;
    }

    if (curl.getExitCode() != 0 || ! tempZip.existsAsFile() || tempZip.getSize() == 0)
    {
        tempZip.deleteFile();
        juce::MessageManager::callAsync ([cb = onComplete]
        {
            if (cb) cb (false, "Download failed — check your internet connection.");
        });
        return;
    }
#endif

    // ── 3. Extract zip ────────────────────────────────────────────────────────
    const juce::File destDir = communityScriptsRoot().getParentDirectory();  // community_scripts/
    destDir.createDirectory();

    juce::ZipFile zip (tempZip);
    if (zip.getNumEntries() == 0)
    {
        PRAAT_LOG_ERR ("ScriptDownloader: downloaded archive is empty or corrupt");
        tempZip.deleteFile();
        juce::MessageManager::callAsync ([cb = onComplete]
        {
            if (cb) cb (false, "Downloaded archive appears empty or corrupt.");
        });
        return;
    }

    const auto result = zip.uncompressTo (destDir);
    tempZip.deleteFile();

    if (result.failed())
    {
        const juce::String msg = result.getErrorMessage();
        PRAAT_LOG_ERR ("ScriptDownloader: extraction failed — " + msg);
        juce::MessageManager::callAsync ([cb = onComplete, msg]
        {
            if (cb) cb (false, "Extraction failed: " + msg);
        });
        return;
    }

    // ── 4. Done ────────────────────────────────────────────────────────────────
    PRAAT_LOG ("ScriptDownloader: community scripts downloaded successfully");
    juce::MessageManager::callAsync ([cb = onComplete]
    {
        if (cb) cb (true, {});
    });
}
