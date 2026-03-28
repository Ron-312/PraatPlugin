#include "scripts/ScriptDownloader.h"

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

    // ── 2. Download via curl ───────────────────────────────────────────────────
    // curl is pre-installed on macOS. -L follows redirects; -o writes to file.
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

    // ── 3. Extract zip ────────────────────────────────────────────────────────
    const juce::File destDir = communityScriptsRoot().getParentDirectory();  // community_scripts/
    destDir.createDirectory();

    juce::ZipFile zip (tempZip);
    if (zip.getNumEntries() == 0)
    {
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
        juce::MessageManager::callAsync ([cb = onComplete, msg]
        {
            if (cb) cb (false, "Extraction failed: " + msg);
        });
        return;
    }

    // ── 4. Done ────────────────────────────────────────────────────────────────
    juce::MessageManager::callAsync ([cb = onComplete]
    {
        if (cb) cb (true, {});
    });
}
