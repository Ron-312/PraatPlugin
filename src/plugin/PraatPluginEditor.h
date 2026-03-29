#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "plugin/PraatPluginProcessor.h"
#include "praat/PraatFormParser.h"

// ─── PraatWebBrowser ──────────────────────────────────────────────────────────
// Thin WebBrowserComponent subclass so PraatPluginEditor can react to page-load
// events without inheriting from WebBrowserComponent itself.
class PraatWebBrowser : public juce::WebBrowserComponent
{
public:
    using juce::WebBrowserComponent::WebBrowserComponent;

    // Called when a page finishes loading — used to push initial state immediately.
    std::function<void()>              onPageLoaded;
    // Called on network error — used to surface a readable error message.
    std::function<void(juce::String)>  onLoadError;

    void pageFinishedLoading (const juce::String&) override
    {
        if (onPageLoaded) onPageLoaded();
    }

    bool pageLoadHadNetworkError (const juce::String& errorInfo) override
    {
        if (onLoadError) onLoadError (errorInfo);
        return false;   // false = don't show JUCE's built-in error page
    }


};


// ─── PraatPluginEditor ────────────────────────────────────────────────────────
//
// WebView-based plugin UI.  Replaces the original JUCE-component editor with
// the React app bundled in ui/dist/index.html (BinaryData::index_html).
//
// ── Communication model ───────────────────────────────────────────────────────
//
//   C++ → JS : timerCallback() calls pushStateToWebView() at 20fps.
//              State is serialised into a juce::var (JSON-like) and delivered
//              via emitEventIfBrowserIsVisible("stateUpdate", stateVar).
//              JS merges the incoming object into its local React state.
//
//   JS → C++ : Each user action in the UI calls sendToPlugin(eventId, data),
//              which fires window.__JUCE__.backend.emitEvent().
//              This is captured by withEventListener registrations built in
//              buildBrowserOptions() and dispatched to handler methods here.
//
// ── Windows fallback ──────────────────────────────────────────────────────────
//
//   If WebView2 Runtime is not installed, a plain JUCE label is shown directing
//   the user to https://microsoft.com/edge/webview2.
//   On macOS, WebKit is always available — no fallback needed.
//
// ── Resource serving ──────────────────────────────────────────────────────────
//
//   The React bundle (BinaryData::index_html) is served via
//   Options::withResourceProvider on the juce://juce.backend/ URL scheme.
//   WKWebView blocks file:// URLs in plugin processes, so this is the only
//   reliable approach on macOS.
//
// See docs/adr/ for design rationale.
// ─────────────────────────────────────────────────────────────────────────────
class PraatPluginEditor  : public juce::AudioProcessorEditor,
                            public  juce::FileDragAndDropTarget,
                            private juce::Timer
{
public:
    explicit PraatPluginEditor (PraatPluginProcessor& processor);
    ~PraatPluginEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    // ── Timer (20fps) ─────────────────────────────────────────────────────
    void timerCallback() override;

    // ── FileDragAndDropTarget (drop audio files onto the plugin) ──────────
    // Covers the full window on macOS (WKWebView participates in the responder
    // chain).  On Windows, WebView2 intercepts its own area — the
    // NativeControlStrip below provides a reliable drop zone for both platforms.
    bool isInterestedInFileDrag (const juce::StringArray& files)               override;
    void fileDragEnter          (const juce::StringArray& files, int x, int y) override;
    void fileDragExit           (const juce::StringArray& files)               override;
    void filesDropped           (const juce::StringArray& files, int x, int y) override;

    // ── WebView setup ─────────────────────────────────────────────────────

    // Builds Options with native integration, resource provider, and all
    // event listeners registered.
    juce::WebBrowserComponent::Options buildBrowserOptions();

    // ── C++ → JS : state serialisation ───────────────────────────────────

    // Serialises the processor's current state and emits "stateUpdate" to JS.
    void pushStateToWebView();

    // Builds a juce::Array<var> of downsampled RMS values from an audio buffer.
    // C++ target is ~512 points — enough for the 520px-wide waveform canvas.
    juce::var buildDownsampledWaveform (const juce::AudioBuffer<float>& buffer) const;

    // Derives a statusType string from the processor's current mode.
    // Maps to the LED colour logic in StatusBar.jsx.
    juce::String deriveStatusType() const;

    // ── JS → C++ : action handlers ────────────────────────────────────────
    // All run on the message thread (dispatched via MessageManager::callAsync).

    void onLoadAudioFile  ();
    void onToggleRecord   ();
    void onPlayOriginal   ();
    void onPlayProcessed  ();
    void onStopPlayback   ();
    void onLoadScriptsDir  ();
    void onRun             ();
    void onSelectScript    (const juce::String& scriptName);
    void onSetRegion       (double startFraction, double endFraction);
    void onExportProcessed ();
    void onStartDragExport ();
    void onSetScriptParam       (const juce::String& name, const juce::String& value);
    void onBrowsePraatExecutable();

    // ── Windows fallback ──────────────────────────────────────────────────
    void showWebViewUnavailableMessage();

    // ── Windows drag-drop fix ─────────────────────────────────────────────
    // WebView2 revokes JUCE's IDropTarget on init.  Called from pageLoaded
    // to re-register our own IDropTarget on the parent HWND.
    void registerWindowsDropTarget();

    // ── macOS drag-drop fix ───────────────────────────────────────────────
    // WKWebView registers for NSPasteboardTypeFileURL drags and eats them.
    // A transparent AudioFileDropView overlay (see PraatPluginEditor_mac.mm)
    // sits above WKWebView and intercepts audio-file drops instead.
    void registerMacDropTarget();
    void updateMacDropTargetBounds();
    void unregisterMacDropTarget();

    // ── Members ───────────────────────────────────────────────────────────

    PraatPluginProcessor& praatProcessor_;

    // Null if WebView is unavailable (Windows without WebView2).
    std::unique_ptr<PraatWebBrowser> browser_;

    // True while an audio file is being dragged over the editor window.
    // Included in stateUpdate so React can show a drop-zone overlay.
    // On Windows this is driven by AudioFileDropTarget; on macOS by the
    // FileDragAndDropTarget callbacks on this class.
    bool isDragOver_ { false };

#if JUCE_WINDOWS
    // Non-owning pointer to the COM drop target (COM manages its lifetime via
    // AddRef/Release).  Null until registerWindowsDropTarget() is called.
    class AudioFileDropTarget* windowsDropTarget_ { nullptr };
#endif

#if JUCE_MAC
    // Owning pointer to the AudioFileDropView NSView overlay (void* to keep
    // ObjC out of the header).  Null until registerMacDropTarget() is called.
    void* macDropOverlay_ { nullptr };
#endif

    // Shown instead of the browser when WebView2 is missing on Windows.
    juce::Label webViewUnavailableLabel_;

    // Kept alive across async FileChooser callbacks (JUCE 8 requires this).
    std::unique_ptr<juce::FileChooser> activeFileChooser_;

    // ── Waveform cache ────────────────────────────────────────────────────
    // Rebuilt only when audio content changes, not every timer tick.
    // Sending 512 floats as JSON at 20fps would be wasteful.
    juce::var  cachedOriginalWaveform_  { juce::Array<juce::var>{} };  // always [], never null
    juce::var  cachedProcessedWaveform_ { juce::Array<juce::var>{} };
    juce::File lastKnownAudioFile_;
    uint32_t   lastKnownAudioVersion_ { 0xFFFFFFFFu }; // intentionally mismatches initial 0
    bool       lastKnownHasProcessed_ { false };

    // ── Script list cache ─────────────────────────────────────────────────
    // scriptFolders JSON is large (400+ scripts) — only rebuild when the list changes.
    int       lastKnownScriptCount_  { -1 };
    int       lastKnownFolderCount_  { -1 };
    juce::var cachedScriptFolders_   { juce::Array<juce::var>{} };

    // True once pageFinishedLoading() fires — guards the timer from emitting
    // events before the WebView2 page is ready, which can deadlock the DAW.
    bool pageLoaded_ { false };

    // ── Script parameter cache ─────────────────────────────────────────────
    // Re-parsed whenever the active script changes.
    juce::File                    lastKnownScriptFile_;
    juce::Array<FormParam>        currentParams_;
    juce::StringPairArray         currentParamValues_;   // name → value, keyed by field name

    static constexpr int kWidth  { 520 };
    static constexpr int kHeight { 620 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PraatPluginEditor)
};
