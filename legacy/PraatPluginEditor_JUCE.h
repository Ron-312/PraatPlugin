#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "plugin/PraatPluginProcessor.h"
#include "ui/WaveformDisplay.h"

// Defined in PraatPluginEditor.cpp before the editor methods.
class PraatLookAndFeel;

// The plugin's user interface.
//
// Layout (520 x 580):
//
//   ┌────────────────────────────────────────────────────────┐
//   │ [2px accent stripe]                                    │
//   │  PRAAT PLUGIN            [LOAD FILE]  [REC]           │  50px header
//   ├────────────────────────────────────────────────────────┤  1px divider
//   │ waveform (drag to select)                              │  120px
//   ├────────────────────────────────────────────────────────┤  1px divider
//   │  [PLAY]  [STOP]                                        │  36px transport
//   ├────────────────────────────────────────────────────────┤  1px divider
//   │  SCRIPT  [dropdown ─────────────────]  [LOAD DIR]     │  36px
//   ├────────────────────────────────────────────────────────┤  1px divider
//   │  [              ANALYZE                           ]    │  44px
//   │  RESULTS ──────────────────────────────────────────    │  18px label
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
    void onPlayButtonClicked ();
    void onStopButtonClicked ();
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

    // Waveform
    WaveformDisplay  waveformDisplay_;

    // Transport
    juce::TextButton playButton_         { "PLAY" };
    juce::TextButton stopButton_         { "STOP" };

    // Script
    juce::Label      scriptSectionLabel_;
    juce::ComboBox   scriptSelectorDropdown_;
    juce::TextButton loadScriptsDirButton_ { "..." };

    // Analysis
    juce::TextButton analyzeButton_      { "ANALYZE" };

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
    static constexpr int kWidth         { 520 };
    static constexpr int kHeight        { 580 };
    static constexpr int kHeaderH       { 50 };
    static constexpr int kWaveformH     { 120 };
    static constexpr int kTransportH    { 36 };
    static constexpr int kScriptRowH    { 36 };
    static constexpr int kAnalyzeH      { 44 };
    static constexpr int kResultsLabelH { 18 };
    static constexpr int kStatusH       { 26 };
    static constexpr int kDivider       { 1 };
    static constexpr int kPadH          { 10 };   // horizontal outer margin

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PraatPluginEditor)
};
