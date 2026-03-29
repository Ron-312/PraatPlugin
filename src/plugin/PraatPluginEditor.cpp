#include "plugin/PraatPluginEditor.h"
#include <BinaryData.h>

// ─── Constructor / Destructor ─────────────────────────────────────────────────

PraatPluginEditor::PraatPluginEditor (PraatPluginProcessor& processor)
    : juce::AudioProcessorEditor (&processor),
      praatProcessor_ (processor)
{
    auto options = buildBrowserOptions();

    if (juce::WebBrowserComponent::areOptionsSupported (options))
    {
        browser_ = std::make_unique<PraatWebBrowser> (options);
        addAndMakeVisible (*browser_);

        // Push state immediately when the page finishes loading.
        // pageLoaded_ gates the 20fps timer so emitEventIfBrowserIsVisible()
        // is never called before the page is ready — doing so can spin JUCE's
        // WebView2 async-init queue and hang the DAW on Windows.
        browser_->onPageLoaded = [this]
        {
            pageLoaded_ = true;
            pushStateToWebView();
        };

        // Surface any load failure as a visible error message.
        browser_->onLoadError = [this] (const juce::String& error)
        {
            browser_->setVisible (false);
            webViewUnavailableLabel_.setText (
                "Failed to load plugin UI:\n" + error,
                juce::dontSendNotification);
            showWebViewUnavailableMessage();
        };

        browser_->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    }
    else
    {
        // WebView2 is not installed — show a plain fallback on Windows.
        showWebViewUnavailableMessage();
    }

    // setSize is called AFTER addAndMakeVisible so that resized() fires when
    // browser_ already exists and receives its bounds.  Calling setSize before
    // creating browser_ left the WebBrowserComponent with {0,0,0,0} bounds,
    // causing WKWebView to have zero size and render nothing.
    setSize (kWidth, kHeight);

    // Allow the user to drag the window edges to resize.
    // Width: 420–900  Height: 480–1400
    // The React layout is pure flexbox so it adapts automatically.
    setResizable (true, true);
    setResizeLimits (420, 480, 900, 1400);

    startTimerHz (20);
}

PraatPluginEditor::~PraatPluginEditor()
{
    stopTimer();
}

// ─── Layout ───────────────────────────────────────────────────────────────────

void PraatPluginEditor::paint (juce::Graphics& g)
{
    // Background matches --color-bg in tokens.css so there's no flash
    // between plugin load and the first WebView paint.
    g.fillAll (juce::Colour (0xff181818));
}

void PraatPluginEditor::resized()
{
    auto bounds = getLocalBounds();

    if (browser_)
        browser_->setBounds (bounds);
    else
        webViewUnavailableLabel_.setBounds (bounds);
}

// ─── Timer (20fps) ────────────────────────────────────────────────────────────

void PraatPluginEditor::timerCallback()
{
    if (browser_ && pageLoaded_)
        pushStateToWebView();
}

// ─── WebView setup ────────────────────────────────────────────────────────────

juce::WebBrowserComponent::Options PraatPluginEditor::buildBrowserOptions()
{
    auto options = juce::WebBrowserComponent::Options{}
        .withNativeIntegrationEnabled()

        // Keep the page loaded even when the plugin window is hidden.
        // Without this, JUCE replaces the page with about:blank on hide,
        // then calls goBack() on show — which can race with the initial load.
        .withKeepPageLoadedWhenBrowserIsHidden()

        // ── Resource provider ─────────────────────────────────────────────
        // Serves BinaryData::index_html when the browser requests "/".
        // WKWebView blocks file:// URLs in plugin processes, so this custom
        // URL scheme (juce://juce.backend/) is the only reliable approach.
        .withResourceProvider ([] (const juce::String& url) -> std::optional<juce::WebBrowserComponent::Resource>
        {
            if (url == "/" || url == "/index.html")
            {
                const auto* bytes = reinterpret_cast<const std::byte*> (BinaryData::index_html);
                return juce::WebBrowserComponent::Resource {
                    std::vector<std::byte> (bytes, bytes + BinaryData::index_htmlSize),
                    "text/html"
                };
            }
            return std::nullopt;
        })

        // ── JS → C++ event listeners ──────────────────────────────────────
        // Each eventId matches a sendToPlugin() call in juceBridge.js.
        // Callbacks are always called on the message thread (JUCE guarantee).

        .withEventListener ("loadAudioFile", [this] (const juce::var&)
        {
            onLoadAudioFile();
        })
        .withEventListener ("toggleRecord", [this] (const juce::var&)
        {
            onToggleRecord();
        })
        .withEventListener ("playOriginal", [this] (const juce::var&)
        {
            onPlayOriginal();
        })
        .withEventListener ("playProcessed", [this] (const juce::var&)
        {
            onPlayProcessed();
        })
        .withEventListener ("stopPlayback", [this] (const juce::var&)
        {
            onStopPlayback();
        })
        .withEventListener ("loadScriptsDir", [this] (const juce::var&)
        {
            onLoadScriptsDir();
        })
        .withEventListener ("analyze", [this] (const juce::var&)
        {
            onAnalyze();
        })
        .withEventListener ("selectScript", [this] (const juce::var& data)
        {
            onSelectScript (data["name"].toString());
        })
        .withEventListener ("setRegion", [this] (const juce::var& data)
        {
            onSetRegion (static_cast<double> (data["startFraction"]),
                         static_cast<double> (data["endFraction"]));
        })
        .withEventListener ("exportProcessed", [this] (const juce::var&)
        {
            onExportProcessed();
        })
        .withEventListener ("setScriptParam", [this] (const juce::var& data)
        {
            onSetScriptParam (data["name"].toString(), data["value"].toString());
        })
        .withEventListener ("requestState", [this] (const juce::var&)
        {
            // JS asks for state on mount (UI is definitely visible at this point).
            // Reset the script list version counters so pushStateToWebView() always
            // fires a fresh scriptsUpdate — emitEventIfBrowserIsVisible() may have
            // silently dropped the earlier emit if the window wasn't open yet.
            lastKnownScriptCount_ = -1;
            lastKnownFolderCount_ = -1;
            pushStateToWebView();
        })
        .withEventListener ("downloadScripts", [this] (const juce::var&)
        {
            praatProcessor_.triggerScriptDownload();
        })
        .withEventListener ("cancelAnalysis", [this] (const juce::var&)
        {
            praatProcessor_.cancelCurrentAnalysis();
        })
        .withEventListener ("browsePraatExecutable", [this] (const juce::var&)
        {
            onBrowsePraatExecutable();
        });

#if JUCE_WINDOWS
    // On Windows, request the WebView2 (Chromium) backend.
    // Plugin processes are often denied access to the default WebView2 user-data
    // folder, so we redirect it to the temp directory.
    // If WebView2 Runtime is not installed, areOptionsSupported() will return false
    // and the fallback message is shown instead.
    options = options
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options (
            juce::WebBrowserComponent::Options::WinWebView2{}
                .withUserDataFolder (
                    juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("PraatPlugin").getChildFile ("WebView2")
                )
                .withStatusBarDisabled()
                .withBuiltInErrorPageDisabled()
        );
#endif

    return options;
}

// ─── C++ → JS : state push ────────────────────────────────────────────────────

void PraatPluginEditor::pushStateToWebView()
{
    if (browser_ == nullptr)
        return;

    auto& sm  = praatProcessor_.scriptManager();
    auto& loc = praatProcessor_.praatLocator();

    auto state = std::make_unique<juce::DynamicObject>();

    // ── Script list — sent as a SEPARATE event, not part of stateUpdate ──────
    // The script list can be 400+ items.  Bundling it in stateUpdate at 20fps
    // overwhelms WKWebView's message queue.  Instead emit "scriptsUpdate" as
    // a one-shot event only when the list actually changes.
    {
        const int scriptCount = sm.numberOfAvailableScripts();
        const int folderCount = sm.numberOfFolders();

        if (scriptCount != lastKnownScriptCount_ || folderCount != lastKnownFolderCount_)
        {
            juce::Array<juce::var> foldersArray;
            for (int fi = 0; fi < folderCount; ++fi)
            {
                auto folderObj = std::make_unique<juce::DynamicObject>();
                folderObj->setProperty ("name", sm.folderNameAtIndex (fi));

                juce::Array<juce::var> scriptNames;
                for (int si = 0; si < sm.numberOfScriptsInFolder (fi); ++si)
                    scriptNames.add (sm.scriptInFolder (fi, si).getFileNameWithoutExtension());
                folderObj->setProperty ("scripts", juce::var (scriptNames));

                foldersArray.add (juce::var (folderObj.release()));
            }

            cachedScriptFolders_  = juce::var (foldersArray);
            lastKnownScriptCount_ = scriptCount;
            lastKnownFolderCount_ = folderCount;

            // Fire the dedicated heavy event.
            browser_->emitEventIfBrowserIsVisible ("scriptsUpdate", cachedScriptFolders_);
        }
    }

    const auto activeScript = sm.activeScript();
    if (activeScript.existsAsFile())
    {
        const auto folderName = sm.folderNameForScript (activeScript);
        const auto qualifiedName = folderName.isNotEmpty()
            ? folderName + "/" + activeScript.getFileNameWithoutExtension()
            : activeScript.getFileNameWithoutExtension();
        state->setProperty ("selectedScript", juce::var (qualifiedName));
    }
    else
    {
        state->setProperty ("selectedScript", juce::var (juce::String{}));
    }

    // ── Script parameters ─────────────────────────────────────────────────
    // Re-parse the form block when the script changes; keep current values
    // when it stays the same.
    if (activeScript != lastKnownScriptFile_)
    {
        lastKnownScriptFile_ = activeScript;
        currentParams_       = PraatFormParser::parseExtraParams (activeScript);
        currentParamValues_.clear();
        for (const auto& p : currentParams_)
            currentParamValues_.set (p.name, p.defaultValue);
    }

    {
        juce::Array<juce::var> paramsArray;
        for (int i = 0; i < currentParams_.size(); ++i)
        {
            const auto& p   = currentParams_[i];
            const auto  val = currentParamValues_.getValue (p.name, p.defaultValue);

            auto obj = std::make_unique<juce::DynamicObject>();
            obj->setProperty ("name",    p.name);
            obj->setProperty ("type",    p.type);
            obj->setProperty ("default", p.defaultValue);
            obj->setProperty ("value",   val);

            juce::Array<juce::var> opts;
            for (const auto& opt : p.options)
                opts.add (opt);
            obj->setProperty ("options", juce::var (opts));

            paramsArray.add (juce::var (obj.release()));
        }
        state->setProperty ("scriptParams", juce::var (paramsArray));
    }

    // ── Audio ─────────────────────────────────────────────────────────────
    state->setProperty ("hasAudio",          praatProcessor_.hasAudioLoaded());
    state->setProperty ("hasProcessedAudio", praatProcessor_.hasProcessedAudio());
    state->setProperty ("fileName",
        praatProcessor_.loadedAudioFile().getFileName());

    // ── Recording ─────────────────────────────────────────────────────────
    state->setProperty ("isRecording", praatProcessor_.isCapturing());

    // ── Playback ──────────────────────────────────────────────────────────
    state->setProperty ("isPlaying",     praatProcessor_.isPlayingBack());
    state->setProperty ("playingSource", praatProcessor_.isPlayingOriginal()
                                             ? juce::var ("original")
                                             : juce::var ("processed"));

    // ── Analysis ──────────────────────────────────────────────────────────
    state->setProperty ("isAnalyzing",         praatProcessor_.isAnalysisInProgress());
    state->setProperty ("isDownloadingScripts", praatProcessor_.isDownloadingScripts());
    state->setProperty ("praatFound",           loc.isPraatInstalled());

    // ── Status ────────────────────────────────────────────────────────────
    state->setProperty ("status",     praatProcessor_.currentStatusMessage());
    state->setProperty ("statusType", deriveStatusType());

    // ── Results ───────────────────────────────────────────────────────────
    // Send whenever there's something to show: parsed output OR an error.
    const auto result = praatProcessor_.mostRecentAnalysisResult();

    if (result.parsedSuccessfully || result.failureReason.isNotEmpty()
            || result.rawConsoleOutput.isNotEmpty())
    {
        auto resultsObj = std::make_unique<juce::DynamicObject>();

        juce::Array<juce::var> pairs;
        if (result.parsedSuccessfully)
        {
            const auto& kvp = result.keyValuePairs;
            for (int i = 0; i < kvp.size(); ++i)
            {
                juce::Array<juce::var> pair;
                pair.add (kvp.getAllKeys()[i]);
                pair.add (kvp.getAllValues()[i]);
                pairs.add (juce::var (pair));
            }
        }

        resultsObj->setProperty ("pairs",         juce::var (pairs));
        resultsObj->setProperty ("raw",           result.rawConsoleOutput);
        resultsObj->setProperty ("error",         result.failureReason);
        resultsObj->setProperty ("parsedOk",      result.parsedSuccessfully);
        state->setProperty ("results", juce::var (resultsObj.release()));
    }

    // ── Waveforms ─────────────────────────────────────────────────────────
    // Rebuilt only when audio content changes — not every timer tick.
    // We check BOTH the file path (covers load-from-file) AND loadedAudioVersion()
    // (covers re-recordings that reuse the same UUID path edge case, and same-file
    // reloads from the file picker).
    const auto     currentFile   = praatProcessor_.loadedAudioFile();
    const uint32_t audioVersion  = praatProcessor_.loadedAudioVersion();
    const bool     audioChanged  = (currentFile  != lastKnownAudioFile_)
                                || (audioVersion != lastKnownAudioVersion_);
    if (audioChanged && praatProcessor_.hasAudioLoaded())
    {
        cachedOriginalWaveform_ = buildDownsampledWaveform (praatProcessor_.loadedAudioBuffer());
        lastKnownAudioFile_    = currentFile;
        lastKnownAudioVersion_ = audioVersion;
    }

    // Rebuild processed waveform every time a new morph produces audio output.
    // consumeAudioOutputNotification() resets the flag, so it fires once per job.
    if (praatProcessor_.consumeAudioOutputNotification())
    {
        cachedProcessedWaveform_ = buildDownsampledWaveform (praatProcessor_.processedAudioBuffer());
        lastKnownHasProcessed_   = true;
    }

    // Always include the cached arrays (they may be empty []).
    state->setProperty ("waveformSamples",  cachedOriginalWaveform_);
    state->setProperty ("processedSamples", cachedProcessedWaveform_);

    // ── Playback cursor ───────────────────────────────────────────────────
    // Fraction 0–1 of the way through the currently-playing clip.
    // 0 when stopped.  The UI draws a vertical cursor line on the right waveform.
    {
        double fraction = 0.0;

        if (praatProcessor_.isPlayingBack())
        {
            const double pos = praatProcessor_.currentPlaybackPosition();

            if (praatProcessor_.isPlayingOriginal())
            {
                const double sr = praatProcessor_.loadedAudioSampleRate();
                const int    n  = praatProcessor_.loadedAudioBuffer().getNumSamples();
                if (sr > 0.0 && n > 0)
                    fraction = pos / (static_cast<double> (n) / sr);
            }
            else
            {
                const double sr = praatProcessor_.processedAudioSampleRate();
                const int    n  = praatProcessor_.processedAudioBuffer().getNumSamples();
                if (sr > 0.0 && n > 0)
                    fraction = pos / (static_cast<double> (n) / sr);
            }

            fraction = juce::jlimit (0.0, 1.0, fraction);
        }

        state->setProperty ("playbackFraction", fraction);
    }

    browser_->emitEventIfBrowserIsVisible ("stateUpdate", juce::var (state.release()));
}

// Downsamples an AudioBuffer to ~512 RMS values for the waveform canvas.
// RMS per window gives a smoother, more readable waveform than peak-picking.
juce::var PraatPluginEditor::buildDownsampledWaveform (const juce::AudioBuffer<float>& buffer) const
{
    constexpr int kTargetPoints = 512;

    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
        return juce::var (juce::Array<juce::var>{});

    const int   totalSamples = buffer.getNumSamples();
    const int   channels     = buffer.getNumChannels();
    const float windowSize   = static_cast<float> (totalSamples) / kTargetPoints;

    juce::Array<juce::var> points;
    points.ensureStorageAllocated (kTargetPoints);

    for (int i = 0; i < kTargetPoints; ++i)
    {
        const int start = static_cast<int> (i * windowSize);
        const int end   = std::min (static_cast<int> ((i + 1) * windowSize), totalSamples);

        float sumSq = 0.0f;
        int   count = 0;

        for (int ch = 0; ch < channels; ++ch)
        {
            const float* samples = buffer.getReadPointer (ch);
            for (int s = start; s < end; ++s)
            {
                sumSq += samples[s] * samples[s];
                ++count;
            }
        }

        const float rms = (count > 0) ? std::sqrt (sumSq / static_cast<float> (count)) : 0.0f;
        points.add (juce::var (rms));
    }

    return juce::var (points);
}

// Maps processor state to a statusType string that drives the LED colour in StatusBar.jsx.
juce::String PraatPluginEditor::deriveStatusType() const
{
    if (praatProcessor_.isCapturing())           return "recording";
    if (praatProcessor_.isAnalysisInProgress())  return "analyzing";
    if (praatProcessor_.isPlayingBack())         return "playing";
    if (! praatProcessor_.isPraatAvailable())    return "error";
    return "idle";
}

// ─── JS → C++ : action handlers ──────────────────────────────────────────────

void PraatPluginEditor::onLoadAudioFile()
{
    activeFileChooser_ = std::make_unique<juce::FileChooser> (
        "Load Audio File",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.aif;*.aiff;*.mp3;*.flac");

    activeFileChooser_->launchAsync (
        juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            const auto selected = chooser.getResult();
            if (selected.existsAsFile())
                praatProcessor_.loadAudioFromFile (selected);
        });
}

void PraatPluginEditor::onToggleRecord()
{
    if (praatProcessor_.isCapturing())
        praatProcessor_.stopLiveCapture();
    else
        praatProcessor_.startLiveCapture();
}

void PraatPluginEditor::onPlayOriginal()
{
    praatProcessor_.startPlaybackOfOriginalRegion();
}

void PraatPluginEditor::onPlayProcessed()
{
    praatProcessor_.startPlaybackOfProcessedOutput();
}

void PraatPluginEditor::onStopPlayback()
{
    praatProcessor_.stopPlayback();
}

void PraatPluginEditor::onLoadScriptsDir()
{
    activeFileChooser_ = std::make_unique<juce::FileChooser> (
        "Select Scripts Directory",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));

    activeFileChooser_->launchAsync (
        juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& chooser)
        {
            const auto dir = chooser.getResult();
            if (dir.isDirectory())
                praatProcessor_.scriptManager().loadScriptsFromDirectoryRecursive (dir);
        });
}

void PraatPluginEditor::onAnalyze()
{
    praatProcessor_.stopPlayback();
    praatProcessor_.beginAnalysisOfSelectedRegion (currentParamValues_);
}

void PraatPluginEditor::onSelectScript (const juce::String& qualifiedName)
{
    auto& sm = praatProcessor_.scriptManager();

    // Try folder-qualified format first: "FOLDER/scriptName"
    const int slash = qualifiedName.indexOfChar ('/');
    if (slash >= 0)
    {
        const auto folder = qualifiedName.substring (0, slash);
        const auto name   = qualifiedName.substring (slash + 1);

        for (int fi = 0; fi < sm.numberOfFolders(); ++fi)
        {
            if (sm.folderNameAtIndex (fi) == folder)
            {
                for (int si = 0; si < sm.numberOfScriptsInFolder (fi); ++si)
                {
                    const auto f = sm.scriptInFolder (fi, si);
                    if (f.getFileNameWithoutExtension() == name)
                    {
                        sm.setActiveScript (f);
                        return;
                    }
                }
            }
        }
    }

    // Fallback: flat name search (backward compat with bundled scripts)
    for (const auto& scriptFile : sm.availableScripts())
    {
        if (scriptFile.getFileNameWithoutExtension() == qualifiedName)
        {
            sm.setActiveScript (scriptFile);
            return;
        }
    }
}

void PraatPluginEditor::onSetRegion (double startFraction, double endFraction)
{
    // {0, 1} from the UI means "reset to full file" — clear the selection.
    if (startFraction <= 0.0 && endFraction >= 1.0)
    {
        praatProcessor_.setAnalysisRegionInSeconds ({});
        return;
    }

    const double sampleRate = praatProcessor_.loadedAudioSampleRate();
    const int    numSamples = praatProcessor_.loadedAudioBuffer().getNumSamples();

    if (sampleRate <= 0.0 || numSamples == 0)
        return;

    const double duration = static_cast<double> (numSamples) / sampleRate;
    praatProcessor_.setAnalysisRegionInSeconds ({
        startFraction * duration,
        endFraction   * duration
    });
}

void PraatPluginEditor::onExportProcessed()
{
    const auto processedFile = praatProcessor_.processedAudioFile();
    if (! processedFile.existsAsFile())
        return;

    activeFileChooser_ = std::make_unique<juce::FileChooser> (
        "Export Processed Audio",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory)
            .getChildFile ("PraatPlugin_output.wav"),
        "*.wav");

    activeFileChooser_->launchAsync (
        juce::FileBrowserComponent::saveMode |
        juce::FileBrowserComponent::canSelectFiles |
        juce::FileBrowserComponent::warnAboutOverwriting,
        [processedFile] (const juce::FileChooser& chooser)
        {
            const auto dest = chooser.getResult();
            if (dest.getFullPathName().isNotEmpty())
                processedFile.copyFileTo (dest);
        });
}

void PraatPluginEditor::onSetScriptParam (const juce::String& name, const juce::String& value)
{
    currentParamValues_.set (name, value);
}

void PraatPluginEditor::onBrowsePraatExecutable()
{
    activeFileChooser_ = std::make_unique<juce::FileChooser> (
        "Locate Praat Executable",
        juce::File::getSpecialLocation (juce::File::userDesktopDirectory),
#if JUCE_WINDOWS
        "Praat.exe"
#else
        "Praat"
#endif
    );

    activeFileChooser_->launchAsync (
        juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            const auto chosen = chooser.getResult();
            if (chosen.existsAsFile())
                praatProcessor_.praatLocator().overrideExecutablePathWithUserChoice (chosen);
        });
}

// ─── Fallback UI ──────────────────────────────────────────────────────────────

void PraatPluginEditor::showWebViewUnavailableMessage()
{
    webViewUnavailableLabel_.setText (
        "PraatPlugin requires the Microsoft WebView2 Runtime.\n\n"
        "Download it free from:\nmicrosoft.com/edge/webview2",
        juce::dontSendNotification);
    webViewUnavailableLabel_.setJustificationType (juce::Justification::centred);
    webViewUnavailableLabel_.setFont (juce::Font (13.0f));
    webViewUnavailableLabel_.setColour (juce::Label::textColourId,       juce::Colour (0xffe8e8e8));
    webViewUnavailableLabel_.setColour (juce::Label::backgroundColourId, juce::Colour (0xff181818));
    addAndMakeVisible (webViewUnavailableLabel_);
}
