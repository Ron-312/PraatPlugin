#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Renders an audio buffer as a waveform and lets the user drag to select a
// region of interest.
//
// The component does NOT own the audio buffer — it holds a const pointer set
// by the caller (typically PraatPluginEditor on the message thread).  The
// pointer must remain valid for the lifetime of any paint() call, so replace
// it only from the message thread (same thread that triggers repaints).
//
// INTERACTIONS
//   mouseDown  — begins a new selection at the clicked position
//   mouseDrag  — extends the selection in real time
//   mouseUp    — finalises the selection and fires onSelectionChanged
//
// CALLBACKS
//   onSelectionChanged  — called with the new selection in seconds after mouseUp
//
// See docs/adr/ADR-008 for the design rationale.
class WaveformDisplay : public juce::Component
{
public:
    WaveformDisplay();
    ~WaveformDisplay() override = default;

    //──────────────────────────────────────────────────────────────────────────
    // Data
    //──────────────────────────────────────────────────────────────────────────

    // Points the display at a loaded audio buffer.
    // Pass nullptr to clear (shows an empty-state placeholder).
    // clearSelection controls whether the current drag selection is preserved
    // (pass false during live recording so the display updates without losing state).
    // Must be called from the message thread.
    void displayAudioBuffer (const juce::AudioBuffer<float>* buffer,
                             double sampleRate,
                             bool   clearSelection = true);

    // Switches the display into recording mode.  While active, a VU meter bar
    // is drawn on top of the waveform showing the live input level.
    // peakLevel must be in [0, 1].  Call with isRecording = false to dismiss.
    void setRecordingMode (bool isRecording, float peakLevel = 0.0f);

    // Moves the playhead to positionInSeconds.  Pass a negative value to hide.
    // Triggers a repaint only when the position actually changes.
    void setPlayheadPosition (double positionInSeconds);

    // Overwrites the visible selection without firing onSelectionChanged.
    // Used by the processor to restore persisted state.
    void setSelectedRegionInSeconds (juce::Range<double> regionInSeconds);

    // Returns the currently selected region (start, end) in seconds.
    // Both values are 0.0 when nothing is selected (full-file implied).
    juce::Range<double> getSelectedRegionInSeconds() const noexcept;

    // Returns true if any audio buffer is currently set.
    bool hasAudioLoaded() const noexcept { return audioBuffer_ != nullptr; }

    // Overrides the waveform peak colour (default: cyan 0xff00b4cc).
    void setWaveformColour (juce::Colour colour);

    // Overrides the selection highlight colour (default: cyan 0xff00b4cc).
    void setSelectionColour (juce::Colour colour);

    // Overrides the placeholder text shown when no audio is loaded.
    void setPlaceholderText (const juce::String& text);

    //──────────────────────────────────────────────────────────────────────────
    // Callback
    //──────────────────────────────────────────────────────────────────────────

    // Set this before display; called on the message thread after mouseUp.
    std::function<void (juce::Range<double> selectionInSeconds)> onSelectionChanged;

    //──────────────────────────────────────────────────────────────────────────
    // Component overrides
    //──────────────────────────────────────────────────────────────────────────

    void paint   (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;

private:
    // Coordinate helpers (pixel ↔ seconds).
    double totalDurationSeconds() const noexcept;
    double pixelXToSeconds (float pixelX) const noexcept;
    float  secondsToPixelX (double seconds) const noexcept;

    // Draws subtle amplitude guide lines (±50 % of full scale).
    void paintGrid      (juce::Graphics& g, const juce::Rectangle<float>& bounds) const;

    // Draws the waveform peaks into the current Graphics context.
    void paintWaveform  (juce::Graphics& g, const juce::Rectangle<float>& bounds) const;

    // Draws the selection highlight and handles.
    void paintSelection (juce::Graphics& g, const juce::Rectangle<float>& bounds) const;

    const juce::AudioBuffer<float>* audioBuffer_ { nullptr };
    double                          sampleRate_  { 44100.0 };

    juce::Range<double>             selectionSeconds_;   // (0,0) means full-file
    bool                            isDragging_      { false };
    float                           dragStartX_      { 0.0f };

    bool                            isRecordingMode_ { false };
    float                           recordingPeak_   { 0.0f };
    juce::Colour                    waveformColour_  { 0xff00b4cc };
    juce::Colour                    selectionColour_ { 0xff00b4cc };
    juce::String                    placeholderText_ { "Load an audio file to see the waveform" };
    double                          playheadSeconds_ { -1.0 };   // < 0 = hidden

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
};
