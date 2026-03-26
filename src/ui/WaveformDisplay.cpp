#include "ui/WaveformDisplay.h"

WaveformDisplay::WaveformDisplay()
{
    setMouseCursor (juce::MouseCursor::IBeamCursor);
}

//==============================================================================
// Data

void WaveformDisplay::displayAudioBuffer (const juce::AudioBuffer<float>* buffer,
                                           double sampleRate,
                                           bool   clearSelection)
{
    audioBuffer_ = buffer;
    sampleRate_  = sampleRate;
    if (clearSelection)
        selectionSeconds_ = {};
    repaint();
}

void WaveformDisplay::setPlayheadPosition (double positionInSeconds)
{
    if (positionInSeconds != playheadSeconds_)
    {
        playheadSeconds_ = positionInSeconds;
        repaint();
    }
}

void WaveformDisplay::setWaveformColour (juce::Colour colour)
{
    waveformColour_ = colour;
    repaint();
}

void WaveformDisplay::setPlaceholderText (const juce::String& text)
{
    placeholderText_ = text;
    repaint();
}

void WaveformDisplay::setRecordingMode (bool isRecording, float peakLevel)
{
    isRecordingMode_ = isRecording;
    recordingPeak_   = peakLevel;
    repaint();
}

void WaveformDisplay::setSelectedRegionInSeconds (juce::Range<double> regionInSeconds)
{
    selectionSeconds_ = regionInSeconds;
    repaint();
}

juce::Range<double> WaveformDisplay::getSelectedRegionInSeconds() const noexcept
{
    return selectionSeconds_;
}

//==============================================================================
// Coordinate helpers

double WaveformDisplay::totalDurationSeconds() const noexcept
{
    if (audioBuffer_ == nullptr || sampleRate_ <= 0.0)
        return 0.0;

    return static_cast<double> (audioBuffer_->getNumSamples()) / sampleRate_;
}

double WaveformDisplay::pixelXToSeconds (float pixelX) const noexcept
{
    const float w = static_cast<float> (getWidth());
    if (w <= 0.0f) return 0.0;

    const double fraction = static_cast<double> (juce::jlimit (0.0f, w, pixelX)) / w;
    return fraction * totalDurationSeconds();
}

float WaveformDisplay::secondsToPixelX (double seconds) const noexcept
{
    const double duration = totalDurationSeconds();
    if (duration <= 0.0) return 0.0f;

    const double fraction = seconds / duration;
    return static_cast<float> (fraction) * static_cast<float> (getWidth());
}

//==============================================================================
// Paint

void WaveformDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // ── Background ──────────────────────────────────────────────────────────
    g.setColour (juce::Colour (0xff0d0d1a));
    g.fillRoundedRectangle (bounds, 4.0f);

    // ── Border ───────────────────────────────────────────────────────────────
    g.setColour (juce::Colour (0xff2a2a4a));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    if (audioBuffer_ == nullptr || audioBuffer_->getNumSamples() == 0)
    {
        // Empty state placeholder
        g.setColour (juce::Colour (0xff3a3a5a));
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText (placeholderText_,
                    bounds.toNearestInt(), juce::Justification::centred, true);
        return;
    }

    // ── Selection highlight (drawn behind the waveform) ──────────────────────
    paintSelection (g, bounds);

    // ── Zero line ────────────────────────────────────────────────────────────
    const float centreY = bounds.getCentreY();
    g.setColour (juce::Colour (0xff2a2a4a));
    g.fillRect (bounds.getX(), centreY - 0.5f, bounds.getWidth(), 1.0f);

    // ── Waveform ─────────────────────────────────────────────────────────────
    paintWaveform (g, bounds);

    // ── Playhead ──────────────────────────────────────────────────────────────
    if (playheadSeconds_ >= 0.0)
    {
        const float px = secondsToPixelX (playheadSeconds_) + bounds.getX();

        if (px >= bounds.getX() && px <= bounds.getRight())
        {
            // Subtle glow
            g.setGradientFill (juce::ColourGradient (
                juce::Colours::white.withAlpha (0.25f), px, bounds.getY(),
                juce::Colours::transparentWhite,        px + 6.0f, bounds.getY(), false));
            g.fillRect (px, bounds.getY(), 6.0f, bounds.getHeight());

            // Crisp white line
            g.setColour (juce::Colours::white.withAlpha (0.90f));
            g.fillRect (px, bounds.getY(), 1.5f, bounds.getHeight());

            // Triangle cap at top
            juce::Path cap;
            cap.addTriangle (px - 4.5f, bounds.getY(),
                             px + 4.5f, bounds.getY(),
                             px,        bounds.getY() + 9.0f);
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.fillPath (cap);
        }
    }

    // ── Recording VU meter overlay ────────────────────────────────────────────
    if (isRecordingMode_)
    {
        // Semi-transparent dark scrim over the top strip
        constexpr float stripH = 20.0f;
        const float stripY = bounds.getY() + 6.0f;
        const float innerX = bounds.getX() + 6.0f;
        const float innerW = bounds.getWidth() - 12.0f;

        g.setColour (juce::Colour (0xcc0a0a18));
        g.fillRoundedRectangle (innerX - 2.0f, stripY - 2.0f, innerW + 4.0f, stripH, 3.0f);

        // Meter track (dark background)
        constexpr float meterH = 8.0f;
        const float meterY = stripY + (stripH - meterH) * 0.5f;
        g.setColour (juce::Colour (0xff1a0808));
        g.fillRoundedRectangle (innerX, meterY, innerW, meterH, 2.0f);

        // Active level bar — gradient from red through orange
        const float filledW = juce::jlimit (0.0f, innerW,
                                            innerW * juce::jlimit (0.0f, 1.0f, recordingPeak_));
        if (filledW > 1.0f)
        {
            g.setGradientFill (juce::ColourGradient (
                juce::Colour (0xffff2244), innerX,          meterY,
                juce::Colour (0xffff7700), innerX + innerW, meterY, false));
            g.fillRoundedRectangle (innerX, meterY, filledW, meterH, 2.0f);
        }

        // Pulsing "REC" dot + label
        g.setColour (juce::Colour (0xffff2244));
        constexpr float dotR = 3.5f;
        const float dotCX = innerX + innerW + 8.0f + dotR;
        const float dotCY = stripY + stripH * 0.5f;
        g.fillEllipse (dotCX - dotR, dotCY - dotR, dotR * 2.0f, dotR * 2.0f);

        g.setFont (juce::Font (juce::FontOptions (8.5f, juce::Font::bold)));
        g.drawText ("REC",
                    (int)(dotCX + dotR + 3.0f), (int)(stripY),
                    30, (int)stripH,
                    juce::Justification::centredLeft, false);
    }
}

void WaveformDisplay::paintWaveform (juce::Graphics& g,
                                      const juce::Rectangle<float>& bounds) const
{
    const int totalSamples = audioBuffer_->getNumSamples();
    const int numChannels  = juce::jmin (audioBuffer_->getNumChannels(), 2);
    const int w            = static_cast<int> (bounds.getWidth());
    const int h            = static_cast<int> (bounds.getHeight());
    const float centreY    = bounds.getCentreY();
    const float halfHeight = bounds.getHeight() * 0.42f;  // use 84% of height
    const float originX    = bounds.getX();

    // Waveform colour: set per-instance via setWaveformColour()
    g.setColour (waveformColour_.withAlpha (0.80f));

    for (int x = 0; x < w; ++x)
    {
        // Map this pixel column to a contiguous block of samples.
        const int startSample = static_cast<int> (
            (static_cast<float> (x)     / static_cast<float> (w)) * static_cast<float> (totalSamples));
        const int endSample   = juce::jmin (
            static_cast<int> ((static_cast<float> (x + 1) / static_cast<float> (w)) * static_cast<float> (totalSamples)),
            totalSamples - 1);

        if (startSample > endSample)
            continue;

        float peakMin =  1.0f;
        float peakMax = -1.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* samples = audioBuffer_->getReadPointer (ch);

            for (int s = startSample; s <= endSample; ++s)
            {
                const float v = samples[s];
                peakMin = juce::jmin (peakMin, v);
                peakMax = juce::jmax (peakMax, v);
            }
        }

        // Clamp to [-1, 1] to handle files with slight clipping.
        peakMin = juce::jlimit (-1.0f, 1.0f, peakMin);
        peakMax = juce::jlimit (-1.0f, 1.0f, peakMax);

        const float top    = centreY - peakMax * halfHeight;
        const float bottom = centreY - peakMin * halfHeight;
        const float height = juce::jmax (1.0f, bottom - top);

        g.fillRect (originX + static_cast<float> (x), top, 1.0f, height);
    }

    juce::ignoreUnused (h);
}

void WaveformDisplay::paintSelection (juce::Graphics& g,
                                       const juce::Rectangle<float>& bounds) const
{
    if (selectionSeconds_.getLength() <= 0.0)
        return;

    const float selStartX = secondsToPixelX (selectionSeconds_.getStart()) + bounds.getX();
    const float selEndX   = secondsToPixelX (selectionSeconds_.getEnd())   + bounds.getX();
    const float selWidth  = selEndX - selStartX;

    if (selWidth < 1.0f)
        return;

    // Semi-transparent fill
    g.setColour (juce::Colour (0xff00b4cc).withAlpha (0.18f));
    g.fillRect (selStartX, bounds.getY(), selWidth, bounds.getHeight());

    // Left handle
    g.setColour (juce::Colour (0xff00b4cc).withAlpha (0.85f));
    g.fillRect (selStartX, bounds.getY(), 2.0f, bounds.getHeight());

    // Right handle
    g.fillRect (selEndX - 2.0f, bounds.getY(), 2.0f, bounds.getHeight());
}

//==============================================================================
// Mouse

void WaveformDisplay::mouseDown (const juce::MouseEvent& event)
{
    if (audioBuffer_ == nullptr)
        return;

    isDragging_   = true;
    dragStartX_   = static_cast<float> (event.x);

    // Start with a zero-width selection at the click point.
    const double t      = pixelXToSeconds (dragStartX_);
    selectionSeconds_   = juce::Range<double> (t, t);
    repaint();
}

void WaveformDisplay::mouseDrag (const juce::MouseEvent& event)
{
    if (! isDragging_ || audioBuffer_ == nullptr)
        return;

    const double t0 = pixelXToSeconds (dragStartX_);
    const double t1 = pixelXToSeconds (static_cast<float> (event.x));

    // Allow dragging left or right.
    selectionSeconds_ = juce::Range<double>::between (t0, t1);
    repaint();
}

void WaveformDisplay::mouseUp (const juce::MouseEvent& event)
{
    if (! isDragging_)
        return;

    isDragging_ = false;

    // Clamp the final selection to the valid duration.
    const double duration = totalDurationSeconds();
    selectionSeconds_     = selectionSeconds_.getIntersectionWith ({ 0.0, duration });

    // Notify the editor.
    if (onSelectionChanged)
        onSelectionChanged (selectionSeconds_);

    repaint();
    juce::ignoreUnused (event);
}
