#include "plugin/PraatPluginEditor.h"

//==============================================================================
// Colour palette — defined once here so every part of the UI stays consistent.
namespace PraatColours
{
    // Backgrounds
    static const juce::Colour background    { 0xff0d0d1f };
    static const juce::Colour headerBg      { 0xff111128 };
    static const juce::Colour cardBg        { 0xff080814 };
    static const juce::Colour statusBg      { 0xff0a0a1a };

    // Borders & separators
    static const juce::Colour divider       { 0xff1c1c38 };
    static const juce::Colour cardBorder    { 0xff1e1e40 };

    // Text
    static const juce::Colour textPrimary   { 0xffe8e8f4 };
    static const juce::Colour textSecondary { 0xff6060a8 };
    static const juce::Colour textMuted     { 0xff3a3a68 };

    // Buttons
    static const juce::Colour buttonDark    { 0xff141430 };
    static const juce::Colour buttonBorder  { 0xff252550 };

    // Accent
    static const juce::Colour accentCyan    { 0xff00c8e0 };
    static const juce::Colour accentBlue    { 0xff0058c0 };

    // State colours
    static const juce::Colour statusGreen   { 0xff22d47e };
    static const juce::Colour statusAmber   { 0xfffbbf24 };
    static const juce::Colour statusRed     { 0xffee4466 };
    static const juce::Colour statusBlue    { 0xff4488ff };
    static const juce::Colour recordRed     { 0xffff2244 };
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
                   juce::Colour (0xff0f0f28));
        setColour (juce::PopupMenu::textColourId,
                   PraatColours::textPrimary);
        setColour (juce::PopupMenu::highlightedBackgroundColourId,
                   PraatColours::accentCyan.withAlpha (0.2f));
        setColour (juce::PopupMenu::highlightedTextColourId,
                   PraatColours::textPrimary);
    }

    //--------------------------------------------------------------------------
    // Classify buttons by their registered colour hue so we can style them
    // differently without needing subclasses.
    //
    //  Cyan hue  (0.48-0.60) → primary ANALYZE-style accent
    //  Red  hue  (0.93-1.00 or 0.00-0.07) → record button
    //  Anything else          → secondary (dark outline)
    //--------------------------------------------------------------------------
    enum class ButtonRole { Primary, Record, Secondary };

    static ButtonRole roleOf (juce::Button& btn)
    {
        const float h = btn.findColour (juce::TextButton::buttonColourId).getHue();
        if (h > 0.48f && h < 0.60f) return ButtonRole::Primary;
        if (h > 0.93f || h < 0.07f) return ButtonRole::Record;
        return ButtonRole::Secondary;
    }

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button&   btn,
                               const juce::Colour& /*bg*/,
                               bool isMouseOver, bool isDown) override
    {
        const auto  b = btn.getLocalBounds().toFloat().reduced (0.5f);
        constexpr float r = 5.0f;
        const bool  on  = btn.getToggleState();

        switch (roleOf (btn))
        {
            case ButtonRole::Primary:
            {
                auto c1 = PraatColours::accentCyan;
                auto c2 = PraatColours::accentBlue;
                if (! btn.isEnabled())    { c1 = c1.withAlpha (0.3f); c2 = c2.withAlpha (0.3f); }
                else if (isDown || on)    { c1 = c1.darker (0.15f);   c2 = c2.darker (0.15f);   }
                else if (isMouseOver)     { c1 = c1.brighter (0.1f);  c2 = c2.brighter (0.1f);  }

                g.setGradientFill (juce::ColourGradient (c1, b.getX(), b.getCentreY(),
                                                         c2, b.getRight(), b.getCentreY(), false));
                g.fillRoundedRectangle (b, r);

                // Specular highlight
                g.setColour (juce::Colours::white.withAlpha (0.07f));
                g.fillRoundedRectangle (b.withHeight (b.getHeight() * 0.45f), r);
                break;
            }

            case ButtonRole::Record:
            {
                const bool recording = on;
                auto base = recording ? PraatColours::recordRed
                                      : PraatColours::buttonDark;

                if (! btn.isEnabled())    base = base.withAlpha (0.4f);
                else if (isDown)          base = base.darker (0.2f);
                else if (isMouseOver)     base = base.brighter (0.12f);

                g.setColour (base);
                g.fillRoundedRectangle (b, r);

                // Border — brighter red when recording
                g.setColour (recording ? PraatColours::recordRed.brighter (0.3f)
                                       : PraatColours::buttonBorder);
                g.drawRoundedRectangle (b, r, 1.0f);

                // Pulsing dot when recording
                if (recording)
                {
                    constexpr float dotR = 4.0f;
                    const float cx = b.getX() + 12.0f;
                    const float cy = b.getCentreY();
                    g.setColour (juce::Colours::white.withAlpha (0.9f));
                    g.fillEllipse (cx - dotR, cy - dotR, dotR * 2.0f, dotR * 2.0f);
                }
                break;
            }

            case ButtonRole::Secondary:
            {
                auto base = PraatColours::buttonDark;
                if (! btn.isEnabled())  base = base.withAlpha (0.4f);
                else if (isDown)        base = base.darker (0.15f);
                else if (isMouseOver)   base = base.brighter (0.1f);

                g.setColour (base);
                g.fillRoundedRectangle (b, r);
                g.setColour (PraatColours::buttonBorder);
                g.drawRoundedRectangle (b, r, 1.0f);
                break;
            }
        }
    }

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& btn,
                         bool /*isMouseOver*/, bool /*isDown*/) override
    {
        const float alpha = btn.isEnabled() ? 1.0f : 0.35f;
        const bool  on    = btn.getToggleState();

        switch (roleOf (btn))
        {
            case ButtonRole::Primary:
                g.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)));
                g.setColour (juce::Colours::white.withAlpha (alpha));
                break;

            case ButtonRole::Record:
                g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
                g.setColour ((on ? juce::Colours::white
                                 : PraatColours::textSecondary).withAlpha (alpha));
                break;

            case ButtonRole::Secondary:
                g.setFont (juce::Font (juce::FontOptions (11.5f)));
                g.setColour (PraatColours::textSecondary.brighter (0.2f).withAlpha (alpha));
                break;
        }

        g.drawFittedText (btn.getButtonText(),
                          btn.getLocalBounds(),
                          juce::Justification::centred, 1);
    }

    //--------------------------------------------------------------------------
    // ComboBox — dark rounded box with minimal chevron
    //--------------------------------------------------------------------------
    void drawComboBox (juce::Graphics& g,
                       int w, int h, bool /*isDown*/,
                       int arrowX, int arrowY, int arrowW, int arrowH,
                       juce::ComboBox& box) override
    {
        const auto b = juce::Rectangle<float> (0, 0, (float)w, (float)h);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (b, 5.0f);
        g.setColour (PraatColours::buttonBorder);
        g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, 1.0f);

        // Chevron
        const float cx = arrowX + arrowW * 0.5f;
        const float cy = arrowY + arrowH * 0.5f;
        juce::Path p;
        p.startNewSubPath (cx - 4.5f, cy - 2.0f);
        p.lineTo (cx,         cy + 2.5f);
        p.lineTo (cx + 4.5f,  cy - 2.0f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.strokePath (p, juce::PathStrokeType (1.5f,
                         juce::PathStrokeType::curved,
                         juce::PathStrokeType::rounded));
    }

    //--------------------------------------------------------------------------
    // Scrollbar — 4px thumb, invisible track
    //--------------------------------------------------------------------------
    int  getScrollbarButtonSize (juce::ScrollBar&) override { return 0; }

    void drawScrollbar (juce::Graphics& g, juce::ScrollBar& sb,
                        int x, int y, int w, int h, bool isVertical,
                        int thumbStart, int thumbSize,
                        bool /*hover*/, bool /*down*/) override
    {
        constexpr float t = 4.0f;
        juce::Rectangle<float> thumb;
        if (isVertical)
        {
            const float cx = x + w * 0.5f;
            thumb = { cx - t * 0.5f, (float)(y + thumbStart), t, (float)thumbSize };
        }
        else
        {
            const float cy = y + h * 0.5f;
            thumb = { (float)(x + thumbStart), cy - t * 0.5f, (float)thumbSize, t };
        }
        g.setColour (sb.findColour (juce::ScrollBar::thumbColourId).withAlpha (0.6f));
        g.fillRoundedRectangle (thumb, 2.0f);
    }
};

//==============================================================================
// ScriptParameterPanel
//
// A scrollable panel of labeled sliders/toggles, one per controllable
// parameter in the active script's form block.  Rebuilt each time the
// script selection changes.  Empty (zero height) when the script has no
// user-controllable parameters.
//==============================================================================
class ScriptParameterPanel : public juce::Component
{
public:
    // Fired when the user changes any control; value is the new numeric value.
    std::function<void (const juce::String& parameterName, double value)> onParameterChanged;

    ScriptParameterPanel()
    {
        viewport_.setScrollBarsShown (true, false);
        viewport_.setScrollBarThickness (6);
        viewport_.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId,
                                                    juce::Colour (0xff303068));
        viewport_.setViewedComponent (&content_, false);  // content_ is owned here, not by viewport
        addAndMakeVisible (viewport_);
    }

    // Replace all controls with those derived from params and currentValues.
    void rebuild (const juce::Array<ScriptParameter>& params,
                  const juce::StringPairArray&        currentValues)
    {
        content_.removeAllChildren();
        rows_.clear();

        for (const auto& param : params)
        {
            auto& row = rows_.emplace_back();
            row.name = param.name;

            row.label = std::make_unique<juce::Label>();
            row.label->setText (
                param.name.replaceCharacter ('_', ' ').toLowerCase(),
                juce::dontSendNotification);
            row.label->setFont (juce::Font (juce::FontOptions (11.0f)));
            row.label->setColour (juce::Label::textColourId, PraatColours::textSecondary);
            row.label->setJustificationType (juce::Justification::centredLeft);
            content_.addAndMakeVisible (row.label.get());

            const juce::String storedStr = currentValues.getValue (param.name, {});
            const double initialValue    = storedStr.isNotEmpty()
                                               ? storedStr.getDoubleValue()
                                               : param.defaultValue;

            if (param.type == ScriptParameter::Type::Boolean)
            {
                row.toggle = std::make_unique<juce::ToggleButton>();
                row.toggle->setToggleState (initialValue >= 0.5,
                                            juce::dontSendNotification);
                row.toggle->setColour (juce::ToggleButton::textColourId,
                                       PraatColours::textSecondary);
                row.toggle->onClick = [this, paramName = param.name,
                                        btn = row.toggle.get()]
                {
                    if (onParameterChanged)
                        onParameterChanged (paramName, btn->getToggleState() ? 1.0 : 0.0);
                };
                content_.addAndMakeVisible (row.toggle.get());
            }
            else
            {
                row.slider = std::make_unique<juce::Slider>();
                row.slider->setSliderStyle (juce::Slider::LinearBar);
                row.slider->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 72, 20);

                double lo, hi, interval;
                if (param.type == ScriptParameter::Type::Integer)
                {
                    lo       = 0.0;
                    hi       = juce::jmax (20.0, 5.0 * std::abs (param.defaultValue));
                    interval = 1.0;
                }
                else if (param.type == ScriptParameter::Type::Positive)
                {
                    // Positive fields must be > 0 — clamp lo so the slider
                    // can never produce a zero/negative value.
                    const double mag = std::abs (param.defaultValue);
                    lo               = juce::jmax (0.001, mag / 20.0);
                    hi               = juce::jmax (mag * 10.0, mag + 5.0);
                    interval         = 0.001;
                }
                else
                {
                    const double mag = std::abs (param.defaultValue);
                    lo               = juce::jmin (-100.0, -5.0 * mag);
                    hi               = juce::jmax ( 100.0,  5.0 * mag);
                    interval         = 0.001;
                }

                row.slider->setRange (lo, hi, interval);
                row.slider->setValue (initialValue, juce::dontSendNotification);
                row.slider->setColour (juce::Slider::backgroundColourId,
                                       PraatColours::buttonDark);
                row.slider->setColour (juce::Slider::trackColourId,
                                       PraatColours::accentCyan.withAlpha (0.4f));
                row.slider->setColour (juce::Slider::textBoxTextColourId,
                                       PraatColours::textPrimary);
                row.slider->setColour (juce::Slider::textBoxBackgroundColourId,
                                       juce::Colours::transparentBlack);
                row.slider->setColour (juce::Slider::textBoxOutlineColourId,
                                       juce::Colours::transparentBlack);
                row.slider->onValueChange = [this, paramName = param.name,
                                              sl = row.slider.get()]
                {
                    if (onParameterChanged)
                        onParameterChanged (paramName, sl->getValue());
                };
                content_.addAndMakeVisible (row.slider.get());
            }
        }

        resized();
    }

    // Height this panel wants to occupy (capped so the panel scrolls rather
    // than pushing other components off screen).
    int preferredHeight() const noexcept
    {
        if (rows_.empty()) return 0;
        return juce::jmin ((int) rows_.size() * kRowH + 4, kMaxH);
    }

    void resized() override
    {
        viewport_.setBounds (getLocalBounds());

        constexpr int kLabelW   = 150;
        constexpr int kControlX = kLabelW + 6;
        const int     kControlW = juce::jmax (1, getWidth() - kControlX - 4);
        const int     totalH    = juce::jmax (1, (int) rows_.size() * kRowH + 4);

        content_.setSize (juce::jmax (1, getWidth()), totalH);

        int y = 2;
        for (auto& row : rows_)
        {
            const int rowH = kRowH - 4;
            if (row.label)  row.label->setBounds (4, y, kLabelW, rowH);
            if (row.slider) row.slider->setBounds (kControlX, y, kControlW, rowH);
            if (row.toggle) row.toggle->setBounds (kControlX, y, kControlW, rowH);
            y += kRowH;
        }
    }

private:
    static constexpr int kRowH = 28;
    static constexpr int kMaxH = 120;  // ~4 rows before scrolling

    struct ParameterRow
    {
        juce::String                        name;
        std::unique_ptr<juce::Label>        label;
        std::unique_ptr<juce::Slider>       slider;
        std::unique_ptr<juce::ToggleButton> toggle;
    };

    juce::Viewport               viewport_;
    juce::Component              content_;
    std::vector<ParameterRow>    rows_;
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
                     &exportButton_,
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
                     &exportButton_,
                     &loadScriptsDirButton_, &analyzeButton_ })
        b->addListener (this);

    waveformDisplay_.onSelectionChanged = [this] (juce::Range<double> sel)
    {
        onWaveformSelectionChanged (sel);
    };

    //──────────────────────────────────────────────────────────────────────────
    // Button colours — sets the hue that PraatLookAndFeel uses for role detection
    //──────────────────────────────────────────────────────────────────────────

    // ANALYZE: cyan hue → Primary role
    analyzeButton_.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff009ab0));
    analyzeButton_.setColour (juce::TextButton::textColourOffId, juce::Colours::white);

    // REC: red hue → Record role; toggleable
    recordButton_.setClickingTogglesState (true);
    recordButton_.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xffcc1133));
    recordButton_.setColour (juce::TextButton::textColourOffId, PraatColours::textSecondary);
    recordButton_.setColour (juce::TextButton::textColourOnId,  juce::Colours::white);

    // Secondary buttons
    for (auto* b : { &loadAudioButton_, &playOriginalButton_,
                     &playProcessedButton_, &stopButton_, &loadScriptsDirButton_,
                     &exportButton_ })
    {
        b->setColour (juce::TextButton::buttonColourId,  PraatColours::buttonDark);
        b->setColour (juce::TextButton::textColourOffId, PraatColours::textSecondary.brighter (0.2f));
    }

    // Processed waveform — green waveform colour + custom placeholder text
    processedWaveformDisplay_.setWaveformColour  (juce::Colour (0xff22d47e));
    processedWaveformDisplay_.setPlaceholderText ("Run a script to see the processed audio here");

    //──────────────────────────────────────────────────────────────────────────
    // Dropdown colours
    //──────────────────────────────────────────────────────────────────────────
    scriptSelectorDropdown_.setColour (juce::ComboBox::backgroundColourId, PraatColours::buttonDark);
    scriptSelectorDropdown_.setColour (juce::ComboBox::textColourId,       PraatColours::textPrimary);
    scriptSelectorDropdown_.setColour (juce::ComboBox::outlineColourId,    PraatColours::buttonBorder);
    scriptSelectorDropdown_.setColour (juce::ComboBox::arrowColourId,      PraatColours::accentCyan);

    //──────────────────────────────────────────────────────────────────────────
    // Results text display
    //──────────────────────────────────────────────────────────────────────────
    resultsTextDisplay_.setMultiLine (true);
    resultsTextDisplay_.setReadOnly (true);
    resultsTextDisplay_.setScrollbarsShown (true);
    resultsTextDisplay_.setFont (juce::Font (juce::FontOptions (
        juce::Font::getDefaultMonospacedFontName(), 12.5f, juce::Font::plain)));

    resultsTextDisplay_.setColour (juce::TextEditor::backgroundColourId,     PraatColours::cardBg);
    resultsTextDisplay_.setColour (juce::TextEditor::textColourId,           juce::Colour (0xff88f0b0));
    resultsTextDisplay_.setColour (juce::TextEditor::outlineColourId,        juce::Colours::transparentBlack);
    resultsTextDisplay_.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    resultsTextDisplay_.setColour (juce::TextEditor::highlightColourId,      PraatColours::accentCyan.withAlpha (0.25f));
    resultsTextDisplay_.setColour (juce::ScrollBar::thumbColourId,           juce::Colour (0xff303068));

    //──────────────────────────────────────────────────────────────────────────
    // Labels
    //──────────────────────────────────────────────────────────────────────────
    pluginTitleLabel_.setText ("PRAAT PLUGIN", juce::dontSendNotification);
    pluginTitleLabel_.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    pluginTitleLabel_.setColour (juce::Label::textColourId, PraatColours::accentCyan);
    pluginTitleLabel_.setJustificationType (juce::Justification::centredLeft);

    scriptSectionLabel_.setText ("SCRIPT", juce::dontSendNotification);
    scriptSectionLabel_.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
    scriptSectionLabel_.setColour (juce::Label::textColourId, PraatColours::textMuted.brighter (0.5f));
    scriptSectionLabel_.setJustificationType (juce::Justification::centredLeft);

    statusBarLabel_.setFont (juce::Font (juce::FontOptions (11.5f)));
    statusBarLabel_.setColour (juce::Label::textColourId, PraatColours::textSecondary);
    statusBarLabel_.setJustificationType (juce::Justification::centredLeft);

    //──────────────────────────────────────────────────────────────────────────
    // Parameter panel
    //──────────────────────────────────────────────────────────────────────────
    parameterPanel_ = std::make_unique<ScriptParameterPanel>();
    parameterPanel_->onParameterChanged = [this] (const juce::String& name, double value)
    {
        praatProcessor_.setScriptParameter (name, juce::String (value));
    };
    addAndMakeVisible (parameterPanel_.get());

    //──────────────────────────────────────────────────────────────────────────
    // Initial state
    //──────────────────────────────────────────────────────────────────────────
    refreshScriptSelectorContents();
    rebuildParameterPanel();
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
                     &exportButton_,
                     &loadScriptsDirButton_, &analyzeButton_ })
        b->removeListener (this);
}

//==============================================================================
// Paint

void PraatPluginEditor::paint (juce::Graphics& g)
{
    const int w = getWidth();
    const int h = getHeight();

    // ── Main background ──────────────────────────────────────────────────────
    g.fillAll (PraatColours::background);

    // ── Header band ──────────────────────────────────────────────────────────
    g.setGradientFill (juce::ColourGradient (
        PraatColours::headerBg, 0.f, 0.f,
        PraatColours::background.brighter (0.03f), (float)w, 0.f, false));
    g.fillRect (0, 0, w, kHeaderH);

    // ── 2-px accent stripe at very top ───────────────────────────────────────
    g.setGradientFill (juce::ColourGradient (
        PraatColours::accentCyan, 0.f, 0.f,
        PraatColours::accentBlue, (float)w, 0.f, false));
    g.fillRect (0.f, 0.f, (float)w, 2.f);

    // ── Horizontal dividers ───────────────────────────────────────────────────
    // Recalculate the same cumulative y-positions as resized() so these line up.
    auto drawDivider = [&] (int divY)
    {
        g.setColour (PraatColours::divider);
        g.fillRect (0, divY, w, kDivider);
    };

    // The two waveform sections sit between the header divider and transport.
    //   header → divider(50) → BEFORE label(16) → waveform(90) →
    //   gap(6) → AFTER label(16) → processed waveform(90) → divider → transport
    drawDivider (kHeaderH);                           // below header
    {
        const int afterWaveforms = kHeaderH + kDivider
            + kWaveformLabelH + kWaveformH
            + kWaveformGap
            + kWaveformLabelH + kWaveformH;
        drawDivider (afterWaveforms);                 // below processed waveform
        const int afterTransport = afterWaveforms + kDivider + kTransportH;
        drawDivider (afterTransport);                 // below transport
        drawDivider (afterTransport + kDivider + kScriptRowH);  // below script row

        // Draw a divider below the parameter panel if it is visible.
        if (parameterPanel_ != nullptr && parameterPanel_->isVisible())
        {
            const int panelBottom = parameterPanel_->getBottom();
            drawDivider (panelBottom);
        }
    }

    // ── Status bar background ─────────────────────────────────────────────────
    g.setColour (PraatColours::statusBg);
    g.fillRect (0, h - kStatusH, w, kStatusH);
    g.setColour (PraatColours::divider);
    g.fillRect (0, h - kStatusH, w, kDivider);

    // ── BEFORE / AFTER section labels ─────────────────────────────────────────
    {
        const int beforeY = kHeaderH + kDivider;
        paintSectionLabel (g, "CLEAN",
            { kPadH, beforeY, w - kPadH * 2, kWaveformLabelH });

        const int afterY = beforeY + kWaveformLabelH + kWaveformH + kWaveformGap;
        paintSectionLabel (g, "MORPHED",
            { kPadH, afterY, w - kPadH * 2, kWaveformLabelH });
    }

    // ── "RESULTS ─────────" section label ─────────────────────────────────────
    {
        const auto resultsLabel = juce::Rectangle<int> (
            kPadH,
            resultsTextDisplay_.getY() - kResultsLabelH - 4,
            getWidth() - kPadH * 2,
            kResultsLabelH);
        paintSectionLabel (g, "RESULTS", resultsLabel);
    }

    // ── Card border + soft glow around results panel ──────────────────────────
    {
        const auto panel = resultsTextDisplay_.getBounds().toFloat();
        const auto card  = panel.expanded (1.0f);
        constexpr float cr = 4.0f;

        for (int i = 3; i >= 1; --i)
        {
            g.setColour (PraatColours::accentCyan.withAlpha (0.025f * (float)i));
            g.drawRoundedRectangle (card.expanded ((float)(i + 1)),
                                    cr + (float)i, 1.0f);
        }
        g.setColour (PraatColours::cardBorder);
        g.drawRoundedRectangle (card, cr, 1.0f);
    }

    // ── Glowing status dot ────────────────────────────────────────────────────
    {
        constexpr float dotD  = 9.0f;
        constexpr float glowD = 20.0f;
        const float dotX = (float)kPadH;
        const float dotY = (float)(h - kStatusH) + ((float)kStatusH - dotD) * 0.5f;
        const float cx   = dotX + dotD * 0.5f;
        const float cy   = dotY + dotD * 0.5f;

        // Radial glow
        g.setGradientFill (juce::ColourGradient (
            statusDotColour_.withAlpha (0.4f), cx, cy,
            statusDotColour_.withAlpha (0.0f), cx + glowD * 0.5f, cy, true));
        const float go = (glowD - dotD) * 0.5f;
        g.fillEllipse (dotX - go, dotY - go, glowD, glowD);

        // Solid dot
        g.setColour (statusDotColour_);
        g.fillEllipse (dotX, dotY, dotD, dotD);

        // Specular highlight
        g.setColour (juce::Colours::white.withAlpha (0.40f));
        g.fillEllipse (dotX + 1.8f, dotY + 1.2f, 3.2f, 2.6f);
    }
}

//==============================================================================
// Layout

void PraatPluginEditor::resized()
{
    int y = 0;

    // ── Header ───────────────────────────────────────────────────────────────
    // Inset 9px top/bottom so buttons don't press against the header border.
    {
        juce::Rectangle<int> row = juce::Rectangle<int> (0, y, getWidth(), kHeaderH)
                                       .reduced (kPadH, 9);
        recordButton_.setBounds    (row.removeFromRight (40));
        row.removeFromRight (5);
        loadAudioButton_.setBounds (row.removeFromRight (68));
        row.removeFromRight (8);
        pluginTitleLabel_.setBounds (row);
    }
    y += kHeaderH + kDivider;

    // ── BEFORE label (drawn in paint(), not a component) + original waveform ─
    y += kWaveformLabelH;
    waveformDisplay_.setBounds (kPadH, y, getWidth() - kPadH * 2, kWaveformH);
    y += kWaveformH + kWaveformGap;

    // ── AFTER label (drawn in paint(), not a component) + processed waveform ─
    y += kWaveformLabelH;
    processedWaveformDisplay_.setBounds (kPadH, y, getWidth() - kPadH * 2, kWaveformH);
    y += kWaveformH + kDivider;

    // ── Transport: PLAY A  PLAY B  STOP  [space]  EXPORT ────────────────────
    {
        juce::Rectangle<int> row (kPadH, y, getWidth() - kPadH * 2, kTransportH);
        playOriginalButton_.setBounds  (row.removeFromLeft (82));
        row.removeFromLeft (6);
        playProcessedButton_.setBounds (row.removeFromLeft (82));
        row.removeFromLeft (6);
        stopButton_.setBounds          (row.removeFromLeft (82));
        exportButton_.setBounds        (row.removeFromRight (82));
    }
    y += kTransportH + kDivider;

    // ── Script row ───────────────────────────────────────────────────────────
    {
        juce::Rectangle<int> row (kPadH, y, getWidth() - kPadH * 2, kScriptRowH);
        loadScriptsDirButton_.setBounds (row.removeFromRight (36));
        row.removeFromRight (6);
        scriptSelectorDropdown_.setBounds (row.removeFromRight (row.getWidth() - 58));
        row.removeFromRight (6);
        scriptSectionLabel_.setBounds (row);
    }
    y += kScriptRowH + kDivider;

    // ── Parameter panel (dynamic — 0 height if script has no parameters) ─────
    if (parameterPanel_ != nullptr)
    {
        const int panelH = parameterPanel_->preferredHeight();
        if (panelH > 0)
        {
            parameterPanel_->setBounds (kPadH, y, getWidth() - kPadH * 2, panelH);
            y += panelH + kDivider;
        }
        else
        {
            parameterPanel_->setBounds (0, y, 0, 0);  // hidden, no space
        }
    }

    // ── Content below the last divider (Analyze + Results) ───────────────────
    const int contentBottom = getHeight() - kStatusH - kDivider;
    juce::Rectangle<int> content (kPadH, y, getWidth() - kPadH * 2,
                                   contentBottom - y);

    content.removeFromTop (6);
    analyzeButton_.setBounds (content.removeFromTop (kAnalyzeH));
    content.removeFromTop (8);

    // "RESULTS ─────" label is drawn in paint() — reserve space only.
    const auto resultsLabelArea = content.removeFromTop (kResultsLabelH);
    juce::ignoreUnused (resultsLabelArea);
    content.removeFromTop (4);

    resultsTextDisplay_.setBounds (content);
}

void PraatPluginEditor::paintSectionLabel (juce::Graphics& g,
                                            const juce::String& labelText,
                                            juce::Rectangle<int> area) const
{
    if (area.isEmpty()) return;

    juce::GlyphArrangement ga;
    ga.addLineOfText (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)),
                      labelText, 0.0f, 0.0f);
    const float labelW = ga.getBoundingBox (0, -1, true).getWidth() + 8.0f;
    const float lineY  = area.getCentreY();

    g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
    g.setColour (PraatColours::textMuted.brighter (0.5f));
    g.drawText (labelText,
                area.getX(), area.getY(), (int)labelW, area.getHeight(),
                juce::Justification::centredLeft);

    // Trailing line
    g.setColour (PraatColours::divider.brighter (0.3f));
    g.fillRect ((float)area.getX() + labelW + 4.0f, lineY,
                (float)area.getWidth() - labelW - 4.0f, 1.0f);
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
    if (b == &exportButton_)         onExportButtonClicked();
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

void PraatPluginEditor::onExportButtonClicked()
{
    if (! praatProcessor_.hasProcessedAudio())
        return;

    // Suggest a filename based on the original file (or a generic name).
    const auto originalFile = praatProcessor_.loadedAudioFile();
    const auto suggestedName = originalFile.existsAsFile()
        ? originalFile.getFileNameWithoutExtension() + "_morphed.wav"
        : "morphed_output.wav";

    activeFileChooser_ = std::make_unique<juce::FileChooser> (
        "Export processed audio",
        juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
            .getChildFile (suggestedName),
        "*.wav");

    activeFileChooser_->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto dest = fc.getResult();
            if (dest != juce::File{})
                praatProcessor_.exportProcessedAudioToFile (dest);

            activeFileChooser_.reset();
        });
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
    rebuildParameterPanel();
    refreshAnalyzeButtonEnabledState();
}

void PraatPluginEditor::onWaveformSelectionChanged (juce::Range<double> sel)
{
    praatProcessor_.setAnalysisRegionInSeconds (sel);
    refreshAnalyzeButtonEnabledState();
}

//==============================================================================
// Refresh helpers

void PraatPluginEditor::rebuildParameterPanel()
{
    const juce::File activeScript = praatProcessor_.scriptManager().activeScript();
    const auto params             = ScriptParameterParser::parse (activeScript);
    const auto currentValues      = praatProcessor_.currentScriptParameters();

    parameterPanel_->rebuild (params, currentValues);

    // Recalculate our overall size — may grow or shrink depending on how many
    // parameters the new script exposes.
    const int panelH = parameterPanel_->preferredHeight();
    parameterPanel_->setVisible (panelH > 0);

    resized();
    repaint();
}

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
    exportButton_.setEnabled        (processed && ! capturing);

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
