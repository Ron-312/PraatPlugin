#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "plugin/PraatPluginProcessor.h"
#include "scripts/ScriptParameterParser.h"
#include "ui/WaveformDisplay.h"

// Defined in PraatPluginEditor.cpp before the editor methods.
class PraatLookAndFeel;
class ScriptParameterPanel;

// The plugin's user interface.
//
// Layout (520 x 620, or taller when parameter panel is visible):
//
//   ┌────────────────────────────────────────────────────────┐
//   │ [2px accent stripe]                                    │
//   │  PRAAT PLUGIN            [LOAD FILE]  [REC]           │  50px header
//   ├────────────────────────────────────────────────────────┤  1px divider
//   │ CLEAN ─────────────────────────────────────────────    │  16px label
//   │ [original waveform — drag to select]                   │  90px
//   │ MORPHED ───────────────────────────────────────────    │  16px label (6px gap above)
//   │ [processed waveform — view only]                       │  90px
//   ├────────────────────────────────────────────────────────┤  1px divider
//   │  [PLAY A]  [PLAY B]  [STOP]                            │  36px transport
//   ├────────────────────────────────────────────────────────┤  1px divider
//   │  SCRIPT  [dropdown ─────────────────]  [LOAD DIR]     │  36px
//   ├────────────────────────────────────────────────────────┤  1px divider
//   │  [              ANALYZE                           ]    │  44px
//   │  RESULTS ──────────────────────────────────────────    │  16px label
//   │ ┌──────────────────────────────────────────────────┐   │
//   │ │ mean_pitch: 220.5 Hz                             │   │  results panel
//   │ └──────────────────────────────────────────────────┘   │
//   ├────────────────────────────────────────────────────────┤  1px divider
//   │ ●  Ready                                               │  26px status
//   └────────────────────────────────────────────────────────┘
//
// Polls processor state via juce::Timer at 20fps.
// All state lives in PraatPluginProcessor — this class is pure UI.
class PraatPluginEditor : public juce::AudioProcessorEditor,
                           private juce::Timer,
                           private juce::Button::Listener,
                           private juce::ComboBox::Listener
{
public:
    explicit PraatPluginEditor (PraatPluginProcessor& processor);
    ~PraatPluginEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    void timerCallback () override;
    void buttonClicked (juce::Button*)   override;
    void comboBoxChanged (juce::ComboBox*) override;

    void onLoadAudioButtonClicked ();
    void onRecordButtonClicked ();
    void onPlayOriginalButtonClicked ();
    void onPlayProcessedButtonClicked ();
    void onStopButtonClicked ();
    void onExportButtonClicked ();
    void onAnalyzeButtonClicked ();
    void onLoadScriptsDirButtonClicked ();
    void onScriptSelectionChanged ();
    void onWaveformSelectionChanged (juce::Range<double> selectionInSeconds);

    void refreshWaveformDisplay ();
    void refreshScriptSelectorContents ();
    void refreshResultsDisplay (const AnalysisResult& result);
    void refreshStatusBar ();
    void refreshTransportButtonStates ();
    void refreshAnalyzeButtonEnabledState ();

    // Rebuilds the parameter panel for the currently active script.
    // Called whenever the script selection changes.
    void rebuildParameterPanel ();

    // Draws a section header line: "LABEL ──────────────────"
    void paintSectionLabel (juce::Graphics& g,
                             const juce::String& labelText,
                             juce::Rectangle<int> area) const;

    juce::Colour colourForCurrentStatus () const;

    //──────────────────────────────────────────────────────────────────────────
    // Processor reference
    //──────────────────────────────────────────────────────────────────────────
    PraatPluginProcessor& praatProcessor_;

    // Kept alive across async FileChooser callbacks (JUCE 8 requires this).
    std::unique_ptr<juce::FileChooser> activeFileChooser_;

    //──────────────────────────────────────────────────────────────────────────
    // Components
    //──────────────────────────────────────────────────────────────────────────

    // Header
    juce::Label      pluginTitleLabel_;
    juce::TextButton loadAudioButton_    { "LOAD FILE" };
    juce::TextButton recordButton_       { "REC" };

    // Waveform — original (drag to select)
    WaveformDisplay  waveformDisplay_;
    // Waveform — processed output (view only)
    WaveformDisplay  processedWaveformDisplay_;

    // Transport
    juce::TextButton playOriginalButton_  { "PLAY A" };
    juce::TextButton playProcessedButton_ { "PLAY B" };
    juce::TextButton stopButton_          { "STOP" };
    juce::TextButton exportButton_        { "EXPORT" };

    // Script
    juce::Label      scriptSectionLabel_;
    juce::ComboBox   scriptSelectorDropdown_;
    juce::TextButton loadScriptsDirButton_ { "..." };

    // Script parameters — rebuilt each time the script selection changes
    std::unique_ptr<ScriptParameterPanel> parameterPanel_;

    // Analysis
    juce::TextButton analyzeButton_      { "MORPH" };

    // Results
    juce::TextEditor resultsTextDisplay_;

    // Status bar
    juce::Label      statusBarLabel_;
    juce::Colour     statusDotColour_;

    // Holds a snapshot of the ring buffer during live recording so the waveform
    // display shows the audio growing in real time.  Updated by the 20fps timer.
    juce::AudioBuffer<float> liveSnapshotBuffer_;

    std::unique_ptr<juce::LookAndFeel_V4> lookAndFeel_;

    //──────────────────────────────────────────────────────────────────────────
    // Layout constants
    //──────────────────────────────────────────────────────────────────────────
    static constexpr int kWidth          { 520 };
    static constexpr int kHeight         { 620 };
    static constexpr int kHeaderH        { 50 };
    static constexpr int kWaveformLabelH { 16 };   // "BEFORE" / "AFTER" labels
    static constexpr int kWaveformH      { 90 };   // height of each waveform display
    static constexpr int kWaveformGap    { 6 };    // gap between the two waveform sections
    static constexpr int kTransportH     { 36 };
    static constexpr int kScriptRowH     { 36 };
    static constexpr int kAnalyzeH       { 44 };
    static constexpr int kResultsLabelH  { 16 };
    static constexpr int kStatusH        { 26 };
    static constexpr int kDivider        { 1 };
    static constexpr int kPadH           { 10 };   // horizontal outer margin

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PraatPluginEditor)
};
