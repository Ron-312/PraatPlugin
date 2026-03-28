#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <functional>

// Downloads the community Praat script library from GitHub as a ZIP archive
// and extracts it to the app-support directory.
//
// The download runs on a background thread. On completion (success or failure),
// onComplete is called on the message thread.
//
// Download happens only once — use isAlreadyDownloaded() to check before calling
// startDownload(). Use startDownload() again to force a re-download/update.
class ScriptDownloader : public juce::Thread
{
public:
    // Called on the message thread when the download finishes.
    // success=true means scripts are ready at communityScriptsRoot().
    std::function<void (bool success, juce::String errorMessage)> onComplete;

    ScriptDownloader();
    ~ScriptDownloader() override;

    // Starts the background download. Safe to call from the message thread.
    void startDownload();

    // Returns the root directory of the extracted community scripts.
    // This is ~/Library/Application Support/PraatPlugin/community_scripts/
    //            Praat-plugin_AudioTools-main/
    static juce::File communityScriptsRoot();

    // Returns true if communityScriptsRoot() already exists on disk (previously downloaded).
    static bool isAlreadyDownloaded();

private:
    void run() override;

    static constexpr const char* kZipUrl {
        "https://github.com/ShaiCohen-ops/Praat-plugin_AudioTools"
        "/archive/refs/heads/main.zip"
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScriptDownloader)
};
