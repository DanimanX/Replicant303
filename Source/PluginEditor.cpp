#include "PluginEditor.h"

Replicant303AudioProcessorEditor::Replicant303AudioProcessorEditor(Replicant303AudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&customLookAndFeel);

    // Setup Synth Knobs
    setupKnob(tuningKnob,    "tuning",    "TUNING");
    setupKnob(cutoffKnob,    "cutoff",    "CUTOFF");
    setupKnob(resonanceKnob, "resonance", "RESONANCE");
    setupKnob(envModKnob,    "env_mod",   "ENV MOD");
    setupKnob(decayKnob,     "decay",     "DECAY");
    setupKnob(accentKnob,    "accent",    "ACCENT");
    setupKnob(volumeKnob,    "volume",    "VOLUME");

    // Setup Distortion Knobs
    setupKnob(distDriveKnob, "dist_drive", "DRIVE");
    setupKnob(distToneKnob,  "dist_tone",  "TONE");
    setupKnob(distMixKnob,   "dist_mix",   "MIX");

    // Waveform Radio Buttons
    addAndMakeVisible(waveformSawBtn);
    addAndMakeVisible(waveformSqrBtn);
    waveformSawBtn.setRadioGroupId(1001);
    waveformSqrBtn.setRadioGroupId(1001);

    auto* waveParam = audioProcessor.apvts.getParameter("waveform");
    if (waveParam != nullptr)
    {
        bool isSquare = (waveParam->getValue() > 0.5f);
        waveformSawBtn.setToggleState(!isSquare, juce::dontSendNotification);
        waveformSqrBtn.setToggleState(isSquare, juce::dontSendNotification);
    }

    waveformSawBtn.onClick = [this, waveParam]() {
        if (waveParam != nullptr) waveParam->setValueNotifyingHost(0.0f);
    };
    waveformSqrBtn.onClick = [this, waveParam]() {
        if (waveParam != nullptr) waveParam->setValueNotifyingHost(1.0f);
    };

    // Distortion Active Toggle
    addAndMakeVisible(distActiveToggle);
    distActiveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "dist_active", distActiveToggle);

    // Distortion Type ComboBox
    distTypeBox.addItem("PocoRat (ProCo Rat)", 1);
    distTypeBox.addItem("TubeDrive (Overdrive)", 2);
    distTypeBox.addItem("HardClip (DS-1)", 3);
    distTypeBox.addItem("Germanium Fuzz", 4);
    distTypeBox.addItem("Acid Wavefolder", 5);
    addAndMakeVisible(distTypeBox);
    distTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "dist_type", distTypeBox);

    setSize(920, 520);
    startTimerHz(30); // 30 FPS UI refresh for LEDs & display
}

Replicant303AudioProcessorEditor::~Replicant303AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void Replicant303AudioProcessorEditor::setupKnob(ControlKnob& knob, const juce::String& paramId, const juce::String& labelText)
{
    knob.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(knob.slider);

    knob.label.setText(labelText, juce::dontSendNotification);
    knob.label.setJustificationType(juce::Justification::centred);
    knob.label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    knob.label.setColour(juce::Label::textColourId, Replicant303::TranslucentGreenLookAndFeel::textSilver);
    addAndMakeVisible(knob.label);

    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, paramId, knob.slider);
}

void Replicant303AudioProcessorEditor::timerCallback()
{
    // Check if waveform parameter was changed externally (automation/preset)
    auto* waveParam = audioProcessor.apvts.getParameter("waveform");
    if (waveParam != nullptr)
    {
        bool isSquare = (waveParam->getValue() > 0.5f);
        if (waveformSawBtn.getToggleState() != (!isSquare))
        {
            waveformSawBtn.setToggleState(!isSquare, juce::dontSendNotification);
            waveformSqrBtn.setToggleState(isSquare, juce::dontSendNotification);
        }
    }

    // Trigger repaint of the status area (telemetry LEDs and note display)
    repaint(juce::Rectangle<int>(0, 440, getWidth(), 80));
}

void Replicant303AudioProcessorEditor::drawPcbTraces(juce::Graphics& g)
{
    // Draw subtle translucent circuit traces under the acrylic chassis
    g.setColour(juce::Colour(0xff1f6b39).withAlpha(0.20f));

    // Horizontal and angled PCB traces
    g.drawLine(30.0f, 95.0f, 890.0f, 95.0f, 2.0f);
    g.drawLine(30.0f, 280.0f, 890.0f, 280.0f, 1.5f);
    g.drawLine(150.0f, 95.0f, 180.0f, 125.0f, 1.5f);
    g.drawLine(180.0f, 125.0f, 180.0f, 280.0f, 1.5f);
    g.drawLine(450.0f, 280.0f, 480.0f, 310.0f, 1.5f);
    g.drawLine(480.0f, 310.0f, 880.0f, 310.0f, 1.5f);

    // Simulated IC chip pads & via dots
    g.setColour(juce::Colour(0xff328e4f).withAlpha(0.25f));
    for (int i = 0; i < 12; ++i)
    {
        g.fillEllipse(50.0f + i * 70.0f, 100.0f, 4.0f, 4.0f);
        g.fillEllipse(60.0f + i * 70.0f, 275.0f, 4.0f, 4.0f);
    }
}

void Replicant303AudioProcessorEditor::drawLed(juce::Graphics& g, int x, int y, int size, bool active,
                                              juce::Colour activeColour, const juce::String& label)
{
    auto ledRect = juce::Rectangle<float>((float)x, (float)y, (float)size, (float)size);

    // Outer bezel
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.fillEllipse(ledRect.expanded(2.0f));
    g.setColour(Replicant303::TranslucentGreenLookAndFeel::borderOutline);
    g.drawEllipse(ledRect.expanded(2.0f), 1.0f);

    if (active)
    {
        // Glowing halo
        g.setColour(activeColour.withAlpha(0.35f));
        g.fillEllipse(ledRect.expanded(6.0f));

        // Core LED
        g.setColour(activeColour);
        g.fillEllipse(ledRect);

        // Specular highlight
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.fillEllipse(ledRect.reduced(size * 0.3f).translated(-1.0f, -1.0f));
    }
    else
    {
        // Dark unlit LED
        g.setColour(activeColour.darker(0.85f).withAlpha(0.6f));
        g.fillEllipse(ledRect);
    }

    // Label below LED
    g.setColour(active ? activeColour : Replicant303::TranslucentGreenLookAndFeel::textSilver.withAlpha(0.7f));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText(label, x - 20, y + size + 4, size + 40, 14, juce::Justification::centred);
}

void Replicant303AudioProcessorEditor::paint(juce::Graphics& g)
{
    // 1. Translucent Green Acrylic Background
    juce::ColourGradient bgGrad(
        Replicant303::TranslucentGreenLookAndFeel::bgLightGreen, 0.0f, 0.0f,
        Replicant303::TranslucentGreenLookAndFeel::bgDeepGreen, 0.0f, (float)getHeight(), false);
    g.setGradientFill(bgGrad);
    g.fillAll();

    // 2. PCB Circuit Trace Underlay
    drawPcbTraces(g);

    // 3. Beveled Metallic Enclosure Border
    g.setColour(Replicant303::TranslucentGreenLookAndFeel::borderOutline.withAlpha(0.8f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(4.0f), 10.0f, 2.5f);

    // Recessed Corner Screws
    auto drawScrew = [&g](float sx, float sy) {
        g.setColour(juce::Colour(0xff222a24));
        g.fillEllipse(sx - 7, sy - 7, 14, 14);
        g.setColour(Replicant303::TranslucentGreenLookAndFeel::knobMetalBright.withAlpha(0.7f));
        g.drawEllipse(sx - 7, sy - 7, 14, 14, 1.2f);
        g.drawLine(sx - 4, sy - 4, sx + 4, sy + 4, 1.5f);
        g.drawLine(sx - 4, sy + 4, sx + 4, sy - 4, 1.5f);
    };
    drawScrew(18, 18);
    drawScrew((float)getWidth() - 18, 18);
    drawScrew(18, (float)getHeight() - 18);
    drawScrew((float)getWidth() - 18, (float)getHeight() - 18);

    // 4. Header Bar
    g.setColour(Replicant303::TranslucentGreenLookAndFeel::neonGreen);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("REPLICANT 303", 40, 16, 260, 26, juce::Justification::centredLeft);

    g.setColour(Replicant303::TranslucentGreenLookAndFeel::textSilver.withAlpha(0.85f));
    g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    g.drawText("VIRTUAL ANALOG TB-303 SYNTHESIZER  //  TRANSLUCENT GREEN EDITION", 40, 42, 450, 16, juce::Justification::centredLeft);

    // Top Right Badge
    auto badgeArea = juce::Rectangle<int>(getWidth() - 220, 18, 180, 28);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRoundedRectangle(badgeArea.toFloat(), 4.0f);
    g.setColour(Replicant303::TranslucentGreenLookAndFeel::borderOutline);
    g.drawRoundedRectangle(badgeArea.toFloat(), 4.0f, 1.0f);
    g.setColour(Replicant303::TranslucentGreenLookAndFeel::neonGreen);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("POCO-RAT ENGINE", badgeArea, juce::Justification::centred);

    // 5. Section Dividers & Panels
    auto drawSectionBox = [&g](const juce::Rectangle<int>& rect, const juce::String& title) {
        g.setColour(juce::Colours::black.withAlpha(0.40f));
        g.fillRoundedRectangle(rect.toFloat(), 6.0f);
        g.setColour(Replicant303::TranslucentGreenLookAndFeel::borderOutline.withAlpha(0.7f));
        g.drawRoundedRectangle(rect.toFloat(), 6.0f, 1.2f);

        // Section Title Tag
        g.setColour(Replicant303::TranslucentGreenLookAndFeel::bgMidGreen);
        g.fillRect(rect.getX() + 16, rect.getY() - 8, 170, 16);
        g.setColour(Replicant303::TranslucentGreenLookAndFeel::borderOutline);
        g.drawRect(rect.getX() + 16, rect.getY() - 8, 170, 16, 1);
        g.setColour(Replicant303::TranslucentGreenLookAndFeel::neonGreen);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText(title, rect.getX() + 16, rect.getY() - 8, 170, 16, juce::Justification::centred);
    };

    drawSectionBox(juce::Rectangle<int>(30, 75, getWidth() - 60, 180), "CORE 303 SYNTH VOICING");
    drawSectionBox(juce::Rectangle<int>(30, 280, getWidth() - 60, 150), "ENHANCED DISTORTION UNIT");

    // 6. Bottom Telemetry Bar (LEDs & Display)
    auto& engine = audioProcessor.getSynthEngine();
    bool isSlide = engine.isSlideActive();
    bool isAccent = engine.isAccentActive();
    int lastNote = engine.getLastMidiNote();
    int lastVel = engine.getLastVelocity();

    drawLed(g, 70, 460, 14, isSlide, Replicant303::TranslucentGreenLookAndFeel::neonGreen, "SLIDE");
    drawLed(g, 140, 460, 14, isAccent, Replicant303::TranslucentGreenLookAndFeel::neonRed, "ACCENT");

    // Retro Digital 7-Segment style Display
    auto displayArea = juce::Rectangle<int>(220, 452, 380, 36);
    g.setColour(juce::Colours::black.withAlpha(0.85f));
    g.fillRoundedRectangle(displayArea.toFloat(), 4.0f);
    g.setColour(Replicant303::TranslucentGreenLookAndFeel::borderOutline);
    g.drawRoundedRectangle(displayArea.toFloat(), 4.0f, 1.2f);

    juce::String noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    juce::String noteStr = noteNames[lastNote % 12] + juce::String((lastNote / 12) - 1);

    g.setColour(Replicant303::TranslucentGreenLookAndFeel::neonGreen);
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    juce::String statusText = "NOTE: " + noteStr + "  |  VEL: " + juce::String(lastVel) +
                             "  |  MODE: " + distTypeBox.getText();
    g.drawText(statusText, displayArea, juce::Justification::centred);

    // Right-bottom branding
    g.setColour(Replicant303::TranslucentGreenLookAndFeel::textSilver.withAlpha(0.6f));
    g.setFont(juce::FontOptions(11.0f, juce::Font::italic));
    g.drawText("DanimanX Studio  //  VST3 • AU • Standalone", getWidth() - 320, 460, 280, 20, juce::Justification::centredRight);
}

void Replicant303AudioProcessorEditor::resized()
{
    // Waveform Radio Buttons
    waveformSawBtn.setBounds(50, 115, 60, 30);
    waveformSqrBtn.setBounds(50, 155, 60, 30);

    // Synth Knobs Layout (Top Row)
    const int synthKnobWidth = 75;
    const int synthKnobHeight = 75;
    const int startX = 135;
    const int stepX = 105;
    const int knobY = 115;

    auto layoutKnob = [synthKnobWidth, synthKnobHeight](ControlKnob& knob, int x, int y) {
        knob.slider.setBounds(x, y, synthKnobWidth, synthKnobHeight);
        knob.label.setBounds(x - 10, y + synthKnobHeight + 2, synthKnobWidth + 20, 18);
    };

    layoutKnob(tuningKnob,    startX + 0 * stepX, knobY);
    layoutKnob(cutoffKnob,    startX + 1 * stepX, knobY);
    layoutKnob(resonanceKnob, startX + 2 * stepX, knobY);
    layoutKnob(envModKnob,    startX + 3 * stepX, knobY);
    layoutKnob(decayKnob,     startX + 4 * stepX, knobY);
    layoutKnob(accentKnob,    startX + 5 * stepX, knobY);
    layoutKnob(volumeKnob,    startX + 6 * stepX, knobY);

    // Distortion Controls Layout (Bottom Section)
    distActiveToggle.setBounds(55, 335, 95, 30);
    distTypeBox.setBounds(170, 335, 200, 32);

    const int distStartX = 430;
    const int distStepX = 130;
    const int distKnobY = 310;

    layoutKnob(distDriveKnob, distStartX + 0 * distStepX, distKnobY);
    layoutKnob(distToneKnob,  distStartX + 1 * distStepX, distKnobY);
    layoutKnob(distMixKnob,   distStartX + 2 * distStepX, distKnobY);
}
