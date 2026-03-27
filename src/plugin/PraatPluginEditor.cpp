#include "plugin/PraatPluginEditor.h"

//==============================================================================
// Colour palette — defined once here so every part of the UI stays consistent.
namespace PraatColours
{
    // Near-black with a very slight cold-blue cast (matches FabFilter display bg).
    static const juce::Colour background    { 0xff0d0f14 };
    // Panel strips: header + controls area — just one shade lighter.
    static const juce::Colour surface       { 0xff131620 };
    static const juce::Colour elevated      { 0xff191d28 };
    static const juce::Colour border        { 0xff252830 };
    static const juce::Colour borderSubtle  { 0xff161920 };

    static const juce::Colour textPrimary   { 0xfff0f0f0 };
    // Medium grey — not near-white, keeps label text from competing with waveforms.
    static const juce::Colour textSecondary { 0xff8a8f99 };
    static const juce::Colour textMuted     { 0xff404550 };

    static const juce::Colour buttonBg      { 0xff161920 };
    static const juce::Colour buttonBorder  { 0xff252830 };

    // FabFilter's signature saturated yellow (not pure #FFD700).
    static const juce::Colour accentGold    { 0xffe8c840 };

    static const juce::Colour statusGreen   { 0xff4caf50 };
    static const juce::Colour statusAmber   { 0xffe8c840 };
    static const juce::Colour statusRed     { 0xffe53935 };
    static const juce::Colour statusBlue    { 0xff5588ff };
    static const juce::Colour recordRed     { 0xffe53935 };
}

//==============================================================================
// PraatLookAndFeel
//==============================================================================
class PraatLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PraatLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId,
                   PraatColours::surface);
        setColour (juce::PopupMenu::textColourId,
                   PraatColours::textPrimary);
        setColour (juce::PopupMenu::highlightedBackgroundColourId,
                   PraatColours::elevated);
        setColour (juce::PopupMenu::highlightedTextColourId,
                   PraatColours::textPrimary);
    }

    // Buttons are classified by their registered buttonColourId saturation + hue.
    //   Achromatic (sat < 0.1) → Secondary (flat dark)
    //   Gold hue (0.08–0.20)   → Primary   (gold fill, only MORPH uses this)
    //   Red hue  (sat >= 0.1, outside gold) → Record
    enum class ButtonRole { Primary, Record, Secondary };

    static ButtonRole roleOf (juce::Button& btn)
    {
        const auto c = btn.findColour (juce::TextButton::buttonColourId);
        if (c.getSaturation() < 0.10f) return ButtonRole::Secondary;
        const float h = c.getHue();
        if (h > 0.08f && h < 0.20f)   return ButtonRole::Primary;
        return ButtonRole::Record;
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& btn,
                               const juce::Colour&, bool isMouseOver, bool isDown) override
    {
        const auto  b   = btn.getLocalBounds().toFloat().reduced (0.5f);
        const float r   = b.getHeight() * 0.30f;
        const bool  on  = btn.getClickingTogglesState() && btn.getToggleState();
        const bool  ena = btn.isEnabled();

        switch (roleOf (btn))
        {
            case ButtonRole::Primary:
            {
                auto fill = PraatColours::accentGold;
                if (! ena)          fill = fill.withAlpha (0.25f);
                else if (isDown)    fill = fill.darker (0.15f);
                else if (isMouseOver) fill = fill.brighter (0.08f);

                g.setColour (fill);
                g.fillRoundedRectangle (b, r);
                break;
            }

            case ButtonRole::Record:
            {
                auto fill = on ? PraatColours::recordRed
                               : PraatColours::buttonBg;
                if (! ena)          fill = fill.withAlpha (0.35f);
                else if (isDown)    fill = fill.darker (0.15f);
                else if (isMouseOver) fill = fill.brighter (0.10f);

                g.setColour (fill);
                g.fillRoundedRectangle (b, r);

                g.setColour (on ? PraatColours::recordRed.brighter (0.3f)
                                : PraatColours::buttonBorder);
                g.drawRoundedRectangle (b, r, 1.0f);

                if (on)
                {
                    constexpr float d = 5.0f;
                    g.setColour (juce::Colours::white.withAlpha (0.9f));
                    g.fillEllipse (b.getX() + 6.0f, b.getCentreY() - d * 0.5f, d, d);
                }
                break;
            }

            case ButtonRole::Secondary:
            {
                auto fill = PraatColours::buttonBg;
                if (! ena)          fill = fill.withAlpha (0.35f);
                else if (isDown)    fill = fill.darker (0.12f);
                else if (isMouseOver) fill = fill.brighter (0.10f);

                g.setColour (fill);
                g.fillRoundedRectangle (b, r);
                g.setColour (PraatColours::buttonBorder);
                g.drawRoundedRectangle (b, r, 1.0f);
                break;
            }
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& btn,
                         bool, bool) override
    {
        const float alpha = btn.isEnabled() ? 1.0f : 0.30f;
        const bool  on    = btn.getClickingTogglesState() && btn.getToggleState();

        switch (roleOf (btn))
        {
            case ButtonRole::Primary:
                g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
                g.setColour (juce::Colour (0xff1a1200).withAlpha (alpha));  // dark on gold
                break;
            case ButtonRole::Record:
                g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
                g.setColour ((on ? juce::Colours::white
                                 : PraatColours::textSecondary).withAlpha (alpha));
                break;
            case ButtonRole::Secondary:
                g.setFont (juce::Font (juce::FontOptions (10.0f)));
                g.setColour (PraatColours::textSecondary.withAlpha (alpha));
                break;
        }

        g.drawFittedText (btn.getButtonText(),
                          btn.getLocalBounds(), juce::Justification::centred, 1);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool,
                       int arrowX, int arrowY, int arrowW, int arrowH,
                       juce::ComboBox& box) override
    {
        const auto b = juce::Rectangle<float> (0.f, 0.f, (float)w, (float)h);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (PraatColours::buttonBorder);
        g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);

        const float cx = arrowX + arrowW * 0.5f;
        const float cy = arrowY + arrowH * 0.5f;
        juce::Path chevron;
        chevron.startNewSubPath (cx - 4.0f, cy - 1.5f);
        chevron.lineTo (cx, cy + 2.5f);
        chevron.lineTo (cx + 4.0f, cy - 1.5f);
        g.setColour (PraatColours::textSecondary);
        g.strokePath (chevron, juce::PathStrokeType (1.4f,
                      juce::PathStrokeType::curved,
                      juce::PathStrokeType::rounded));
    }

    int  getScrollbarButtonSize (juce::ScrollBar&) override { return 0; }

    void drawScrollbar (juce::Graphics& g, juce::ScrollBar& sb,
                        int x, int y, int w, int h, bool isVertical,
                        int thumbStart, int thumbSize, bool, bool) override
    {
        juce::Rectangle<float> thumb;
        constexpr float t = 3.0f;
        if (isVertical)
            thumb = { x + (w - t) * 0.5f, (float)(y + thumbStart), t, (float)thumbSize };
        else
            thumb = { (float)(x + thumbStart), y + (h - t) * 0.5f, (float)thumbSize, t };
        g.setColour (sb.findColour (juce::ScrollBar::thumbColourId));
        g.fillRoundedRectangle (thumb, t * 0.5f);
    }
};

//==============================================================================
// PraatPluginEditor
//==============================================================================

PraatPluginEditor::PraatPluginEditor (PraatPluginProcessor& ownerProcessor)
    : AudioProcessorEditor (&ownerProcessor),
      praatProcessor_ (ownerProcessor),
      lookAndFeel_ (std::make_unique<PraatLookAndFeel>())
{
    setLookAndFeel (lookAndFeel_.get());

    //──────────────────────────────────────────────────────────────────────────
    // Add all components
    //──────────────────────────────────────────────────────────────────────────
    for (juce::Component* c : std::initializer_list<juce::Component*>{
                     &pluginTitleLabel_,
                     &loadAudioButton_,      &recordButton_,
                     &waveformDisplay_,
                     &processedWaveformDisplay_,
                     &playOriginalButton_,   &playProcessedButton_,  &stopButton_,
                     &scriptSectionLabel_,
                     &scriptSelectorDropdown_,
                     &loadScriptsDirButton_,
                     &analyzeButton_,
                     &resultsTextDisplay_,
                     &statusBarLabel_ })
        addAndMakeVisible (c);

    //──────────────────────────────────────────────────────────────────────────
    // Listeners
    //──────────────────────────────────────────────────────────────────────────
    scriptSelectorDropdown_.addListener (this);
    for (auto* b : { &loadAudioButton_, &recordButton_,
                     &playOriginalButton_, &playProcessedButton_, &stopButton_,
                     &loadScriptsDirButton_, &analyzeButton_ })
        b->addListener (this);

    waveformDisplay_.onSelectionChanged = [this] (juce::Range<double> sel)
    {
        onWaveformSelectionChanged (sel);
    };

    //──────────────────────────────────────────────────────────────────────────
    // Button colours — drive PraatLookAndFeel role detection
    //──────────────────────────────────────────────────────────────────────────

    // MORPH: gold hue → Primary role
    analyzeButton_.setColour (juce::TextButton::buttonColourId,  PraatColours::accentGold);
    analyzeButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff1a1200));

    // REC: red hue → Record role; toggleable
    recordButton_.setClickingTogglesState (true);
    recordButton_.setColour (juce::TextButton::buttonColourId,  PraatColours::recordRed);
    recordButton_.setColour (juce::TextButton::textColourOffId, PraatColours::textSecondary);
    recordButton_.setColour (juce::TextButton::textColourOnId,  juce::Colours::white);

    // All other buttons: achromatic → Secondary role
    for (auto* b : { &loadAudioButton_, &playOriginalButton_,
                     &playProcessedButton_, &stopButton_, &loadScriptsDirButton_ })
    {
        b->setColour (juce::TextButton::buttonColourId,  PraatColours::buttonBg);
        b->setColour (juce::TextButton::textColourOffId, PraatColours::textSecondary);
    }

    // Waveform colours — FabFilter-style: CLEAN = bright white edges,
    // MORPHED = saturated gold edges (matches the accent colour).
    waveformDisplay_.setWaveformColour  (juce::Colours::white.withAlpha (0.80f));
    waveformDisplay_.setSelectionColour (PraatColours::accentGold);

    processedWaveformDisplay_.setWaveformColour  (PraatColours::accentGold.withAlpha (0.88f));
    processedWaveformDisplay_.setSelectionColour (juce::Colours::white);
    processedWaveformDisplay_.setPlaceholderText ("Morph audio to see the result here");

    //──────────────────────────────────────────────────────────────────────────
    // Dropdown colours
    //──────────────────────────────────────────────────────────────────────────
    scriptSelectorDropdown_.setColour (juce::ComboBox::backgroundColourId, PraatColours::buttonBg);
    scriptSelectorDropdown_.setColour (juce::ComboBox::textColourId,       PraatColours::textPrimary);
    scriptSelectorDropdown_.setColour (juce::ComboBox::outlineColourId,    PraatColours::buttonBorder);
    scriptSelectorDropdown_.setColour (juce::ComboBox::arrowColourId,      PraatColours::textSecondary);

    //──────────────────────────────────────────────────────────────────────────
    // Results text display
    //──────────────────────────────────────────────────────────────────────────
    resultsTextDisplay_.setMultiLine (true);
    resultsTextDisplay_.setReadOnly (true);
    resultsTextDisplay_.setScrollbarsShown (true);
    resultsTextDisplay_.setFont (juce::Font (juce::FontOptions (
        juce::Font::getDefaultMonospacedFontName(), 12.5f, juce::Font::plain)));

    resultsTextDisplay_.setColour (juce::TextEditor::backgroundColourId,     juce::Colour (0xff0d0f14));
    resultsTextDisplay_.setColour (juce::TextEditor::textColourId,           PraatColours::textPrimary);
    resultsTextDisplay_.setColour (juce::TextEditor::outlineColourId,        juce::Colours::transparentBlack);
    resultsTextDisplay_.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    resultsTextDisplay_.setColour (juce::TextEditor::highlightColourId,      PraatColours::accentGold.withAlpha (0.20f));
    resultsTextDisplay_.setColour (juce::ScrollBar::thumbColourId,           juce::Colour (0xff333333));

    //──────────────────────────────────────────────────────────────────────────
    // Labels
    //──────────────────────────────────────────────────────────────────────────
    pluginTitleLabel_.setText ("PRAAT PLUGIN", juce::dontSendNotification);
    pluginTitleLabel_.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    pluginTitleLabel_.setColour (juce::Label::textColourId, PraatColours::textPrimary);
    pluginTitleLabel_.setJustificationType (juce::Justification::centredLeft);

    scriptSectionLabel_.setText ("SCRIPT", juce::dontSendNotification);
    scriptSectionLabel_.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
    scriptSectionLabel_.setColour (juce::Label::textColourId, PraatColours::textMuted);
    scriptSectionLabel_.setJustificationType (juce::Justification::centredLeft);

    statusBarLabel_.setFont (juce::Font (juce::FontOptions (10.5f)));
    statusBarLabel_.setColour (juce::Label::textColourId, PraatColours::textSecondary);
    statusBarLabel_.setJustificationType (juce::Justification::centredLeft);

    //──────────────────────────────────────────────────────────────────────────
    // Initial state
    //──────────────────────────────────────────────────────────────────────────
    refreshScriptSelectorContents();
    refreshWaveformDisplay();
    refreshTransportButtonStates();
    refreshAnalyzeButtonEnabledState();
    refreshStatusBar();
    statusDotColour_ = colourForCurrentStatus();

    setSize (kWidth, kHeight);
    startTimerHz (20);
}

PraatPluginEditor::~PraatPluginEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
    scriptSelectorDropdown_.removeListener (this);
    for (auto* b : { &loadAudioButton_, &recordButton_,
                     &playOriginalButton_, &playProcessedButton_, &stopButton_,
                     &loadScriptsDirButton_, &analyzeButton_ })
        b->removeListener (this);
}

//==============================================================================
// Paint

void PraatPluginEditor::paint (juce::Graphics& g)
{
    const int w = getWidth();
    const int h = getHeight();

    // ── Main background (the display "viewport" area) ─────────────────────────
    g.fillAll (PraatColours::background);

    // ── Header panel strip — slightly lighter than the display area ───────────
    g.setColour (PraatColours::surface);
    g.fillRect (0, 0, w, kHeaderH);

    // ── 2px gold accent stripe — the only decorative element ─────────────────
    g.setColour (PraatColours::accentGold);
    g.fillRect (0, 0, w, 2);

    // ── Hair-line divider between header and waveform area ────────────────────
    g.setColour (PraatColours::border);
    g.fillRect (0, kHeaderH, w, 1);

    // ── Controls panel strip (transport + script + morph + results + status) ──
    // Starts right after the two waveforms.
    {
        const int controlsY = kHeaderH + 1 + 4 + kWaveformH + kWaveformGap + kWaveformH + 8;
        g.setColour (PraatColours::surface);
        g.fillRect (0, controlsY, w, h - controlsY);

        // Hair-line divider between display area and controls
        g.setColour (PraatColours::border);
        g.fillRect (0, controlsY, w, 1);
    }

    // ── "CLEAN" label — top-left corner of the original waveform ─────────────
    {
        const int waveAY = kHeaderH + 1 + 4;
        g.setFont (juce::Font (juce::FontOptions (8.0f, juce::Font::bold)));
        g.setColour (PraatColours::textMuted.brighter (0.5f));
        g.drawText ("CLEAN",
                    kPadH + 6, waveAY + 5, 60, 12,
                    juce::Justification::centredLeft, false);
    }

    // ── "MORPHED" label — top-left corner of the processed waveform ──────────
    {
        const int waveBY = kHeaderH + 1 + 4 + kWaveformH + kWaveformGap;
        g.setFont (juce::Font (juce::FontOptions (8.0f, juce::Font::bold)));
        g.setColour (PraatColours::accentGold.withAlpha (0.65f));
        g.drawText ("MORPHED",
                    kPadH + 6, waveBY + 5, 70, 12,
                    juce::Justification::centredLeft, false);
    }

    // ── Status bar (bottom strip of the controls panel) ───────────────────────
    g.setColour (PraatColours::background);
    g.fillRect (0, h - kStatusH, w, kStatusH);
    g.setColour (PraatColours::borderSubtle);
    g.fillRect (0, h - kStatusH, w, 1);

    // ── Glowing status dot ────────────────────────────────────────────────────
    {
        constexpr float dotD  = 7.0f;
        constexpr float glowD = 16.0f;
        const float dotX = (float)kPadH;
        const float dotY = (float)(h - kStatusH) + ((float)kStatusH - dotD) * 0.5f;
        const float cx   = dotX + dotD * 0.5f;
        const float cy   = dotY + dotD * 0.5f;

        g.setGradientFill (juce::ColourGradient (
            statusDotColour_.withAlpha (0.35f), cx, cy,
            statusDotColour_.withAlpha (0.0f),  cx + glowD * 0.5f, cy, true));
        const float go = (glowD - dotD) * 0.5f;
        g.fillEllipse (dotX - go, dotY - go, glowD, glowD);
        g.setColour (statusDotColour_);
        g.fillEllipse (dotX, dotY, dotD, dotD);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.fillEllipse (dotX + 1.5f, dotY + 1.0f, 2.5f, 2.0f);
    }

    // ── Results area: subtle card border ─────────────────────────────────────
    {
        const auto panel = resultsTextDisplay_.getBounds().toFloat();
        g.setColour (PraatColours::border.withAlpha (0.5f));
        g.drawRoundedRectangle (panel.expanded (1.0f), 3.0f, 1.0f);
    }
}

//==============================================================================
// Layout

void PraatPluginEditor::resized()
{
    int y = 0;

    // ── Header: title + LOAD + REC, inset vertically ─────────────────────────
    {
        juce::Rectangle<int> row = juce::Rectangle<int> (0, y, getWidth(), kHeaderH)
                                       .reduced (kPadH, 7);
        recordButton_.setBounds    (row.removeFromRight (38));
        row.removeFromRight (5);
        loadAudioButton_.setBounds (row.removeFromRight (66));
        row.removeFromRight (8);
        pluginTitleLabel_.setBounds (row);
    }
    y += kHeaderH + 1;  // 1px subtle separator

    // ── CLEAN waveform — full width, no side padding ──────────────────────────
    y += 4;   // small breathing gap after header
    waveformDisplay_.setBounds (0, y, getWidth(), kWaveformH);
    y += kWaveformH + kWaveformGap;

    // ── MORPHED waveform — full width, no side padding ───────────────────────
    processedWaveformDisplay_.setBounds (0, y, getWidth(), kWaveformH);
    y += kWaveformH + 8;

    // ── Subtle separator (drawn in paint) ─────────────────────────────────────
    y += 1;   // separator itself

    // ── Transport: PLAY A  PLAY B  STOP ─────────────────────────────────────
    {
        juce::Rectangle<int> row (kPadH, y, getWidth() - kPadH * 2, kTransportH);
        playOriginalButton_.setBounds  (row.removeFromLeft (76));
        row.removeFromLeft (5);
        playProcessedButton_.setBounds (row.removeFromLeft (76));
        row.removeFromLeft (5);
        stopButton_.setBounds          (row.removeFromLeft (60));
    }
    y += kTransportH + 8;

    // ── Script row: SCRIPT label + dropdown + ... ────────────────────────────
    {
        juce::Rectangle<int> row (kPadH, y, getWidth() - kPadH * 2, kScriptRowH);
        loadScriptsDirButton_.setBounds (row.removeFromRight (32));
        row.removeFromRight (5);
        scriptSelectorDropdown_.setBounds (row.removeFromRight (row.getWidth() - 52));
        row.removeFromRight (5);
        scriptSectionLabel_.setBounds (row);
    }
    y += kScriptRowH + 6;

    // ── MORPH button — full available width ──────────────────────────────────
    analyzeButton_.setBounds (kPadH, y, getWidth() - kPadH * 2, kMorphH);
    y += kMorphH + 8;

    // ── Results panel — fills remaining space above status bar ───────────────
    const int resultsBottom = getHeight() - kStatusH - 1;
    if (resultsBottom > y)
        resultsTextDisplay_.setBounds (kPadH, y, getWidth() - kPadH * 2,
                                        resultsBottom - y);

    // ── Status bar: dot is drawn in paint(), label positioned here ───────────
    {
        const int dotSpace = 20;
        statusBarLabel_.setBounds (kPadH + dotSpace, getHeight() - kStatusH,
                                    getWidth() - kPadH * 2 - dotSpace, kStatusH);
    }
}

void PraatPluginEditor::paintSectionLabel (juce::Graphics& g,
                                            const juce::String& labelText,
                                            juce::Rectangle<int> area) const
{
    if (area.isEmpty()) return;

    juce::GlyphArrangement ga;
    ga.addLineOfText (juce::Font (juce::FontOptions (8.5f, juce::Font::bold)),
                      labelText, 0.0f, 0.0f);
    const float labelW = ga.getBoundingBox (0, -1, true).getWidth() + 6.0f;
    const float lineY  = area.getCentreY();

    g.setFont (juce::Font (juce::FontOptions (8.5f, juce::Font::bold)));
    g.setColour (PraatColours::textMuted.brighter (0.3f));
    g.drawText (labelText,
                area.getX(), area.getY(), (int)labelW, area.getHeight(),
                juce::Justification::centredLeft);

    g.setColour (PraatColours::borderSubtle.brighter (0.1f));
    g.fillRect ((float)area.getX() + labelW + 3.0f, lineY,
                (float)area.getWidth() - labelW - 3.0f, 1.0f);
}

//==============================================================================
// Timer

void PraatPluginEditor::timerCallback()
{
    refreshTransportButtonStates();
    refreshAnalyzeButtonEnabledState();
    refreshStatusBar();

    // ── Live recording: grow the ORIGINAL waveform + VU meter ───────────────
    if (praatProcessor_.isCapturing())
    {
        praatProcessor_.copyLiveCaptureSnapshotTo (liveSnapshotBuffer_);

        if (liveSnapshotBuffer_.getNumSamples() > 0)
        {
            // clearSelection=false so any existing selection is preserved.
            waveformDisplay_.displayAudioBuffer (&liveSnapshotBuffer_,
                                                  praatProcessor_.captureSampleRate(),
                                                  false /* clearSelection */);
        }

        waveformDisplay_.setRecordingMode (true,
                                           praatProcessor_.captureInputPeakLevel());
    }
    else
    {
        // Ensure recording overlay is always dismissed when not capturing.
        waveformDisplay_.setRecordingMode (false);
    }

    // ── Playhead position ─────────────────────────────────────────────────────
    if (praatProcessor_.isPlayingBack())
    {
        const double pos = praatProcessor_.currentPlaybackPosition();
        if (praatProcessor_.isPlayingOriginal())
        {
            waveformDisplay_.setPlayheadPosition (pos);
            processedWaveformDisplay_.setPlayheadPosition (-1.0);
        }
        else
        {
            waveformDisplay_.setPlayheadPosition (-1.0);
            processedWaveformDisplay_.setPlayheadPosition (pos);
        }
    }
    else
    {
        waveformDisplay_.setPlayheadPosition (-1.0);
        processedWaveformDisplay_.setPlayheadPosition (-1.0);
    }

    // ── Waveform refresh after a script produced audio output ─────────────────
    if (praatProcessor_.consumeAudioOutputNotification())
    {
        refreshWaveformDisplay();
        refreshTransportButtonStates();
    }

    if (! praatProcessor_.isAnalysisInProgress())
    {
        const auto result = praatProcessor_.mostRecentAnalysisResult();
        if (result.parsedSuccessfully || result.failureReason.isNotEmpty())
            refreshResultsDisplay (result);
    }
}

//==============================================================================
// Listeners

void PraatPluginEditor::buttonClicked (juce::Button* b)
{
    if (b == &loadAudioButton_)      onLoadAudioButtonClicked();
    if (b == &recordButton_)         onRecordButtonClicked();
    if (b == &playOriginalButton_)   onPlayOriginalButtonClicked();
    if (b == &playProcessedButton_)  onPlayProcessedButtonClicked();
    if (b == &stopButton_)           onStopButtonClicked();
    if (b == &analyzeButton_)        onAnalyzeButtonClicked();
    if (b == &loadScriptsDirButton_) onLoadScriptsDirButtonClicked();
}

void PraatPluginEditor::comboBoxChanged (juce::ComboBox*)
{
    onScriptSelectionChanged();
}

//==============================================================================
// Handlers

void PraatPluginEditor::onLoadAudioButtonClicked()
{
    activeFileChooser_ = std::make_unique<juce::FileChooser> (
        "Select an audio file to analyse",
        juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");

    activeFileChooser_->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto chosen = fc.getResult();
            if (chosen.existsAsFile())
            {
                if (praatProcessor_.loadAudioFromFile (chosen))
                {
                    refreshWaveformDisplay();
                    refreshTransportButtonStates();
                    refreshAnalyzeButtonEnabledState();
                    refreshStatusBar();
                }
            }
            activeFileChooser_.reset();
        });
}

void PraatPluginEditor::onRecordButtonClicked()
{
    // recordButton_ is a toggle — its new state is already set by JUCE.
    if (recordButton_.getToggleState())
    {
        praatProcessor_.startLiveCapture();
    }
    else
    {
        praatProcessor_.stopLiveCapture();
        // Waveform refresh happens via consumeAudioOutputNotification() in timer.
    }

    refreshTransportButtonStates();
    refreshStatusBar();
}

void PraatPluginEditor::onPlayOriginalButtonClicked()
{
    praatProcessor_.startPlaybackOfOriginalRegion();
    refreshTransportButtonStates();
    refreshStatusBar();
}

void PraatPluginEditor::onPlayProcessedButtonClicked()
{
    praatProcessor_.startPlaybackOfProcessedOutput();
    refreshTransportButtonStates();
    refreshStatusBar();
}

void PraatPluginEditor::onStopButtonClicked()
{
    // Stop whichever is active — playback or recording.
    praatProcessor_.stopPlayback();

    if (praatProcessor_.isCapturing())
    {
        recordButton_.setToggleState (false, juce::dontSendNotification);
        praatProcessor_.stopLiveCapture();
    }

    refreshTransportButtonStates();
    refreshStatusBar();
}

void PraatPluginEditor::onAnalyzeButtonClicked()
{
    praatProcessor_.beginAnalysisOfSelectedRegion();
    refreshAnalyzeButtonEnabledState();
    refreshStatusBar();
}

void PraatPluginEditor::onLoadScriptsDirButtonClicked()
{
    activeFileChooser_ = std::make_unique<juce::FileChooser> (
        "Select a folder of .praat scripts",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));

    activeFileChooser_->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& fc)
        {
            const auto chosen = fc.getResult();
            if (chosen.isDirectory())
            {
                praatProcessor_.scriptManager().loadScriptsFromDirectory (chosen);
                refreshScriptSelectorContents();
            }
            activeFileChooser_.reset();
        });
}

void PraatPluginEditor::onScriptSelectionChanged()
{
    const int idx  = scriptSelectorDropdown_.getSelectedItemIndex();
    const auto scr = praatProcessor_.scriptManager().scriptAtIndex (idx);
    praatProcessor_.scriptManager().setActiveScript (scr);
    refreshAnalyzeButtonEnabledState();
}

void PraatPluginEditor::onWaveformSelectionChanged (juce::Range<double> sel)
{
    praatProcessor_.setAnalysisRegionInSeconds (sel);
    refreshAnalyzeButtonEnabledState();
}

//==============================================================================
// Refresh helpers

void PraatPluginEditor::refreshWaveformDisplay()
{
    // Always dismiss the recording overlay and playhead when showing a new buffer.
    waveformDisplay_.setRecordingMode (false);
    waveformDisplay_.setPlayheadPosition (-1.0);
    processedWaveformDisplay_.setPlayheadPosition (-1.0);

    // ── Original waveform ─────────────────────────────────────────────────────
    if (praatProcessor_.hasAudioLoaded())
    {
        waveformDisplay_.displayAudioBuffer (&praatProcessor_.loadedAudioBuffer(),
                                              praatProcessor_.loadedAudioSampleRate());
        waveformDisplay_.setSelectedRegionInSeconds (
            praatProcessor_.selectedRegionInSeconds());
    }
    else
    {
        waveformDisplay_.displayAudioBuffer (nullptr, 44100.0);
    }

    // ── Processed waveform ────────────────────────────────────────────────────
    if (praatProcessor_.hasProcessedAudio())
    {
        processedWaveformDisplay_.displayAudioBuffer (
            &praatProcessor_.processedAudioBuffer(),
            praatProcessor_.processedAudioSampleRate());
    }
    else
    {
        processedWaveformDisplay_.displayAudioBuffer (nullptr, 44100.0);
    }
}

void PraatPluginEditor::refreshScriptSelectorContents()
{
    scriptSelectorDropdown_.clear (juce::dontSendNotification);

    const auto scripts = praatProcessor_.scriptManager().availableScripts();

    if (scripts.isEmpty())
    {
        scriptSelectorDropdown_.addItem ("(no scripts)", 1);
        scriptSelectorDropdown_.setEnabled (false);
    }
    else
    {
        scriptSelectorDropdown_.setEnabled (true);
        for (int i = 0; i < scripts.size(); ++i)
            scriptSelectorDropdown_.addItem (scripts[i].getFileNameWithoutExtension(), i + 1);

        const int active = praatProcessor_.scriptManager().indexOfActiveScript();
        scriptSelectorDropdown_.setSelectedItemIndex (active >= 0 ? active : 0,
                                                       juce::dontSendNotification);
    }
}

void PraatPluginEditor::refreshResultsDisplay (const AnalysisResult& result)
{
    juce::String display;

    if (! result.parsedSuccessfully)
    {
        display = "Analysis failed:\n" + result.failureReason;
        if (result.praatErrorOutput.isNotEmpty())
            display += "\n\nPraat output:\n" + result.praatErrorOutput;
    }
    else
    {
        if (result.hasStructuredResults())
        {
            for (int i = 0; i < result.keyValuePairs.size(); ++i)
                display += result.keyValuePairs.getAllKeys()[i] + ":  "
                         + result.keyValuePairs.getAllValues()[i] + "\n";
        }

        if (result.rawConsoleOutput.isNotEmpty())
        {
            if (display.isNotEmpty()) display += "\n";
            display += result.rawConsoleOutput;
        }
    }

    resultsTextDisplay_.setText (display, juce::dontSendNotification);
}

void PraatPluginEditor::refreshStatusBar()
{
    statusBarLabel_.setText (praatProcessor_.currentStatusMessage(),
                              juce::dontSendNotification);

    const auto newColour = colourForCurrentStatus();
    if (newColour != statusDotColour_)
    {
        statusDotColour_ = newColour;
        repaint();
    }
}

void PraatPluginEditor::refreshTransportButtonStates()
{
    const bool loaded    = praatProcessor_.hasAudioLoaded();
    const bool processed = praatProcessor_.hasProcessedAudio();
    const bool playing   = praatProcessor_.isPlayingBack();
    const bool capturing = praatProcessor_.isCapturing();

    playOriginalButton_.setEnabled  (loaded    && ! playing && ! capturing);
    playProcessedButton_.setEnabled (processed && ! playing && ! capturing);
    stopButton_.setEnabled          (playing || capturing);

    // Keep record button toggle state in sync with the processor.
    if (recordButton_.getToggleState() != capturing)
        recordButton_.setToggleState (capturing, juce::dontSendNotification);
}

void PraatPluginEditor::refreshAnalyzeButtonEnabledState()
{
    const bool canAnalyze = praatProcessor_.isPraatAvailable()
                             && praatProcessor_.hasAudioLoaded()
                             && praatProcessor_.scriptManager().hasActiveScriptSelected()
                             && ! praatProcessor_.isAnalysisInProgress()
                             && ! praatProcessor_.isCapturing();

    analyzeButton_.setEnabled (canAnalyze);
}

juce::Colour PraatPluginEditor::colourForCurrentStatus() const
{
    if (! praatProcessor_.isPraatAvailable())  return PraatColours::statusRed;
    if (praatProcessor_.isCapturing())          return PraatColours::recordRed;
    if (praatProcessor_.isPlayingBack())        return PraatColours::statusBlue;
    if (praatProcessor_.isAnalysisInProgress()) return PraatColours::statusAmber;

    const auto result = praatProcessor_.mostRecentAnalysisResult();
    if (result.failureReason.isNotEmpty())      return PraatColours::statusRed;

    return PraatColours::statusGreen;
}
