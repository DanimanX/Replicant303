#include "PluginEditor.h"

Replicant303AudioProcessorEditor::Replicant303AudioProcessorEditor(Replicant303AudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // -------------------------------------------------------------------------
    // 0. Panel skin: restore the saved skin, then bind the selector combo box.
    //    This must happen before anything is painted.
    // -------------------------------------------------------------------------
    themeBox.addItemList(Replicant303::ThemeManager::getThemeNames(), 1);
    addAndMakeVisible(themeBox);

    if (auto* themeParam = audioProcessor.apvts.getParameter("ui_theme"))
    {
        themeAttachment = std::make_unique<juce::ParameterAttachment>(
            *themeParam,
            [this](float newNormalisedValue)
            {
                const int index = Replicant303::ThemeManager::normalizedToIndex(newNormalisedValue);
                themeBox.setSelectedId(index + 1, juce::dontSendNotification);
                Replicant303::ThemeManager::getInstance().setCurrentIndex(index);
            });

        themeAttachment->sendInitialUpdate();
    }

    themeBox.onChange = [this]()
    {
        if (themeAttachment != nullptr)
            themeAttachment->setValueAsCompleteGesture(
                Replicant303::ThemeManager::indexToNormalized(themeBox.getSelectedId() - 1));
    };

    Replicant303::ThemeManager::getInstance().addChangeListener(this);
    customLookAndFeel.applyTheme();
    setLookAndFeel(&customLookAndFeel);

    // 1. Upper Synth Knobs
    setupKnob(tuneKnob,      "tuning",    "TUNE");
    setupKnob(cutoffKnob,    "cutoff",    "CUTOFF");
    setupKnob(resonanceKnob, "resonance", "RESONANCE");
    setupKnob(envModKnob,    "env_mod",   "ENVELOPE");
    setupKnob(decayKnob,     "decay",     "DECAY");
    setupKnob(accentKnob,    "accent",    "ACCENT");

    // 2. Upper Distortion Knobs
    setupKnob(distDriveKnob, "dist_drive", "DISTORTION");
    setupKnob(distToneKnob,  "dist_tone",  "TONE");
    setupKnob(distMixKnob,   "dist_mix",   "LEVEL");

    // 3. Middle Row Knobs & Selectors
    setupKnob(tempoKnob,     "tempo",         "TEMPO",    true);
    setupKnob(trackKnob,     "pattern_group", "TRACK",    true);
    setupKnob(distTypeKnob,  "dist_type",     "MODE",     true);
    setupKnob(volumeKnob,    "volume",        "VOLUME",   true);

    tempoKnob.slider.onValueChange = [this]() {
        repaint(495, 8, 143, 40);
    };

    // 4. Waveform Slide Switch (Saw / Square)
    addAndMakeVisible(waveformSwitch);
    waveformSwitch.setButtonText("");
    auto* waveParam = audioProcessor.apvts.getParameter("waveform");
    if (waveParam != nullptr)
        waveformSwitch.setToggleState(waveParam->getValue() > 0.5f, juce::dontSendNotification);

    waveformSwitch.onClick = [this, waveParam]() {
        if (waveParam != nullptr)
            waveParam->setValueNotifyingHost(waveformSwitch.getToggleState() ? 1.0f : 0.0f);
        repaint();
    };

    // 5. Distortion On/Off Slide Switch
    addAndMakeVisible(distActiveSwitch);
    distActiveSwitch.setButtonText("");
    distActiveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "dist_active", distActiveSwitch);

    // 6. Distortion Model ComboBox Selector
    distModelBox.addItem("1. Poco Rat", 1);
    distModelBox.addItem("2. Tube Drive", 2);
    distModelBox.addItem("3. Hard Clip", 3);
    distModelBox.addItem("4. German Fuzz", 4);
    distModelBox.addItem("5. Wavefolder", 5);
    addAndMakeVisible(distModelBox);
    distModelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "dist_type", distModelBox);

    setupKeyboardLayout();

    setSize(1080, 590);
    startTimerHz(30); // 30 FPS reactive LEDs
}

Replicant303AudioProcessorEditor::~Replicant303AudioProcessorEditor()
{
    Replicant303::ThemeManager::getInstance().removeChangeListener(this);
    stopTimer();
    setLookAndFeel(nullptr);
}

void Replicant303AudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source != &Replicant303::ThemeManager::getInstance())
        return;

    customLookAndFeel.applyTheme();
    repaint();
}

void Replicant303AudioProcessorEditor::setupKnob(ControlKnob& knob, const juce::String& paramId, const juce::String& labelText, bool hideLabel)
{
    knob.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(knob.slider);

    if (!hideLabel)
    {
        knob.label.setText(labelText, juce::dontSendNotification);
        knob.label.setJustificationType(juce::Justification::centred);
        knob.label.setFont(Replicant303::uiFont(9.5f, juce::Font::bold));
        // No explicit text colour: the LookAndFeel supplies it, so the label
        // follows a skin change instead of being frozen at construction time.
        addAndMakeVisible(knob.label);
    }

    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, paramId, knob.slider);
}

void Replicant303AudioProcessorEditor::setupKeyboardLayout()
{
    pianoKeys.clear();
    functionButtons.clear();

    // 13-note Tactile Keyboard (C2 to C3, MIDI 36 to 48)
    const float whiteKeyW = 38.0f;
    const float whiteKeyH = 88.0f;
    const float blackKeyW = 28.0f;
    const float blackKeyH = 56.0f;
    const float startX = 202.0f;
    const float whiteKeyY = 412.0f;
    const float blackKeyY = 306.0f;
    const float stride = 46.0f;

    // 8 White Keys: C, D, E, F, G, A, B, high C
    int whiteNotes[] = { 36, 38, 40, 41, 43, 45, 47, 48 };
    juce::String whiteNames[] = { "C", "D", "E", "F", "G", "A", "B", "C" };
    juce::String whiteSubs[]  = { "INT", "MIDI", "USB", "TRIG", "1PPS", "2PPQ", "24PPQ", "48PPQ" };
    juce::String whiteSteps[] = { "1", "2", "3", "4", "5", "6", "7", "8" };

    for (int i = 0; i < 8; ++i)
    {
        float kx = startX + i * stride;
        PianoKey key;
        key.midiNote = whiteNotes[i];
        key.name = whiteNames[i];
        key.isBlack = false;
        key.bounds = juce::Rectangle<float>(kx, whiteKeyY, whiteKeyW, whiteKeyH);
        key.ledPos = juce::Point<float>(kx + whiteKeyW * 0.5f, 394.0f);
        key.subLabel = whiteSubs[i];
        key.stepLabel = whiteSteps[i];
        pianoKeys.push_back(key);
    }

    // 5 Black Keys: C#, D#, F#, G#, A#
    struct BlackDef { int note; const char* name; int whiteLeftIdx; const char* sub; };
    BlackDef blackDefs[] = {
        { 37, "C#", 0, "DEL" },
        { 39, "D#", 1, "INS" },
        { 42, "F#", 3, "CH" },
        { 44, "G#", 4, "COPY" },
        { 46, "A#", 5, "PASTE" }
    };

    for (auto& b : blackDefs)
    {
        float leftX = startX + b.whiteLeftIdx * stride;
        float kx = leftX + whiteKeyW - (blackKeyW * 0.5f) + 4.0f;
        PianoKey key;
        key.midiNote = b.note;
        key.name = b.name;
        key.isBlack = true;
        key.bounds = juce::Rectangle<float>(kx, blackKeyY, blackKeyW, blackKeyH);
        key.ledPos = juce::Point<float>(kx + blackKeyW * 0.5f, 288.0f);
        key.subLabel = b.sub;
        pianoKeys.push_back(key);
    }

    // Left Function Buttons
    functionButtons.push_back({ "PITCH MODE",  juce::Rectangle<float>(92.0f, 306.0f, 42.0f, 28.0f), juce::Point<float>(113.0f, 292.0f), true, false, "CLEAR", "" });
    functionButtons.push_back({ "FUNCTION",    juce::Rectangle<float>(92.0f, 374.0f, 42.0f, 28.0f), juce::Point<float>(148.0f, 374.0f), true, false, "NORMAL MODE", "" });
    functionButtons.push_back({ "START / STOP", juce::Rectangle<float>(42.0f, 442.0f, 54.0f, 54.0f), juce::Point<float>(69.0f, 428.0f), true, false, "", "" });

    // Right Function Buttons (TIME MODE)
    // Top Row
    functionButtons.push_back({ "DOWN",   juce::Rectangle<float>(636.0f, 330.0f, 38.0f, 28.0f), juce::Point<float>(655.0f, 312.0f), true, false, "TRANSPOSE", "" });
    functionButtons.push_back({ "UP",     juce::Rectangle<float>(696.0f, 330.0f, 38.0f, 28.0f), juce::Point<float>(715.0f, 312.0f), true, false, "", "" });
    functionButtons.push_back({ "ACCENT", juce::Rectangle<float>(766.0f, 330.0f, 38.0f, 28.0f), juce::Point<float>(785.0f, 312.0f), true, false, "", "" });
    functionButtons.push_back({ "SLIDE",  juce::Rectangle<float>(826.0f, 330.0f, 38.0f, 28.0f), juce::Point<float>(845.0f, 312.0f), true, false, "", "" });

    // Bottom Row
    functionButtons.push_back({ "STEP",   juce::Rectangle<float>(636.0f, 430.0f, 38.0f, 28.0f), juce::Point<float>(655.0f, 412.0f), true, false, "", "9" });
    functionButtons.push_back({ "TRIPLET",juce::Rectangle<float>(696.0f, 430.0f, 38.0f, 28.0f), juce::Point<float>(715.0f, 412.0f), true, false, "", "0" });
    functionButtons.push_back({ "A",      juce::Rectangle<float>(766.0f, 430.0f, 38.0f, 28.0f), juce::Point<float>(785.0f, 412.0f), true, false, "PATTERN SECTION", "100" });
    functionButtons.push_back({ "B",      juce::Rectangle<float>(826.0f, 430.0f, 38.0f, 28.0f), juce::Point<float>(845.0f, 412.0f), true, false, "", "200" });

    // Far Right Controls
    functionButtons.push_back({ "BACK",   juce::Rectangle<float>(922.0f, 330.0f, 38.0f, 28.0f), juce::Point<float>(941.0f, 312.0f), true, false, "DEL", "" });
    functionButtons.push_back({ "SYNC",   juce::Rectangle<float>(922.0f, 430.0f, 38.0f, 28.0f), juce::Point<float>(941.0f, 412.0f), true, false, "D.S.", "" });
    functionButtons.push_back({ "WRITE / NEXT", juce::Rectangle<float>(984.0f, 442.0f, 54.0f, 54.0f), juce::Point<float>(1011.0f, 428.0f), true, false, "TAP", "" });
}

void Replicant303AudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    auto pos = e.position;

    // 1. Check sharp piano keys first (they layer over white keys)
    for (auto& key : pianoKeys)
    {
        if (key.isBlack && key.bounds.contains(pos))
        {
            key.isPressed = true;
            activePlayedKey = key.midiNote;
            juce::MidiBuffer buf;
            buf.addEvent(juce::MidiMessage::noteOn(1, key.midiNote, (juce::uint8)115), 0);
            audioProcessor.getSynthEngine().handleMidiBuffer(buf, 1);
            repaint();
            return;
        }
    }

    // 2. Check white piano keys
    for (auto& key : pianoKeys)
    {
        if (!key.isBlack && key.bounds.contains(pos))
        {
            key.isPressed = true;
            activePlayedKey = key.midiNote;
            juce::MidiBuffer buf;
            buf.addEvent(juce::MidiMessage::noteOn(1, key.midiNote, (juce::uint8)115), 0);
            audioProcessor.getSynthEngine().handleMidiBuffer(buf, 1);
            repaint();
            return;
        }
    }

    // 3. Check function buttons
    for (auto& btn : functionButtons)
    {
        if (btn.bounds.contains(pos))
        {
            btn.isLit = !btn.isLit;
            repaint();
            return;
        }
    }
}

void Replicant303AudioProcessorEditor::mouseUp(const juce::MouseEvent& /*e*/)
{
    if (activePlayedKey != -1)
    {
        juce::MidiBuffer buf;
        buf.addEvent(juce::MidiMessage::noteOff(1, activePlayedKey, (juce::uint8)0), 0);
        audioProcessor.getSynthEngine().handleMidiBuffer(buf, 1);

        for (auto& key : pianoKeys)
            key.isPressed = false;

        activePlayedKey = -1;
        repaint();
    }
}

void Replicant303AudioProcessorEditor::timerCallback()
{
    auto& engine = audioProcessor.getSynthEngine();
    bool slideOn = engine.isSlideActive();
    bool accentOn = engine.isAccentActive();
    int lastNote = engine.getLastMidiNote();

    for (auto& btn : functionButtons)
    {
        if (btn.label == "SLIDE")  btn.isLit = slideOn;
        if (btn.label == "ACCENT") btn.isLit = accentOn;
    }

    for (auto& key : pianoKeys)
    {
        if (key.midiNote == (lastNote % 12 + 36) || key.isPressed)
            key.isPressed = true;
        else
            key.isPressed = false;
    }

    repaint(495, 8, 143, 40);
    repaint(0, 245, getWidth(), getHeight() - 245);
}

// -----------------------------------------------------------------------------
// Procedural 3D Internal Circuit Board & Hardware Detailing
// -----------------------------------------------------------------------------

void Replicant303AudioProcessorEditor::drawScrew(juce::Graphics& g, float x, float y)
{
    // Standoff boss underneath screw
    g.setColour(theme().moldBoss.withAlpha(0.40f));
    g.fillEllipse(x - 9.0f, y - 9.0f, 18.0f, 18.0f);
    g.drawEllipse(x - 9.0f, y - 9.0f, 18.0f, 18.0f, 1.2f);

    // Recessed screw head
    g.setColour(juce::Colour(0xff222824));
    g.fillEllipse(x - 6.0f, y - 6.0f, 12.0f, 12.0f);

    // Nickel plating bevel
    g.setColour(juce::Colour(0xffc5cec8));
    g.drawEllipse(x - 5.5f, y - 5.5f, 11.0f, 11.0f, 1.1f);

    // Philips cross drive
    g.setColour(juce::Colour(0xff181c19));
    g.drawLine(x - 3.5f, y, x + 3.5f, y, 1.6f);
    g.drawLine(x, y - 3.5f, x, y + 3.5f, 1.6f);
}

void Replicant303AudioProcessorEditor::drawMoldBoss(juce::Graphics& g, float x, float y, float radius)
{
    // Radiant translucent circular boss visible through emerald acrylic
    g.setColour(theme().moldBoss.withAlpha(0.28f));
    g.fillEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f);

    // Concentric reinforcement rings
    g.setColour(theme().moldBoss.withAlpha(0.52f));
    g.drawEllipse(x - radius * 0.95f, y - radius * 0.95f, radius * 1.9f, radius * 1.9f, 1.2f);
    g.drawEllipse(x - radius * 0.65f, y - radius * 0.65f, radius * 1.3f, radius * 1.3f, 1.0f);

    // Center shaft/screw hole
    g.setColour(theme().pcbDark.darker(0.6f));
    g.fillEllipse(x - radius * 0.28f, y - radius * 0.28f, radius * 0.56f, radius * 0.56f);
}

void Replicant303AudioProcessorEditor::drawIcChip(juce::Graphics& g, float x, float y, float w, float h,
                                                 int numPins, const juce::String& partNumber)
{
    auto chipRect = juce::Rectangle<float>(x, y, w, h);

    // Cast shadow on PCB
    g.setColour(juce::Colours::black.withAlpha(0.60f));
    g.fillRoundedRectangle(chipRect.translated(2.0f, 3.0f), 2.0f);

    // Silver DIP pins soldered to board
    g.setColour(juce::Colour(0xffc8d0cb));
    int pinsPerSide = numPins / 2;
    float pinSpacing = (w - 10.0f) / (float)(pinsPerSide - 1);
    for (int i = 0; i < pinsPerSide; ++i)
    {
        float px = x + 5.0f + i * pinSpacing;
        g.fillRect(px - 1.2f, y - 4.5f, 2.4f, 4.5f);
        g.fillRect(px - 1.2f, y + h, 2.4f, 4.5f);
        // Solder pad
        g.setColour(theme().pcbTrace);
        g.fillEllipse(px - 2.5f, y - 6.5f, 5.0f, 3.0f);
        g.fillEllipse(px - 2.5f, y + h + 3.5f, 5.0f, 3.0f);
        g.setColour(juce::Colour(0xffc8d0cb));
    }

    // Black epoxy DIP package
    juce::ColourGradient chipGrad(juce::Colour(0xff242a26), x, y, juce::Colour(0xff111613), x, y + h, false);
    g.setGradientFill(chipGrad);
    g.fillRoundedRectangle(chipRect, 2.0f);

    // Pin 1 notch
    g.setColour(juce::Colour(0xff0d100e));
    g.fillEllipse(x - 2.0f, y + h * 0.5f - 3.0f, 6.0f, 6.0f);

    // Laser-etched white part number
    g.setColour(juce::Colours::white.withAlpha(0.75f));
    g.setFont(Replicant303::uiFont(8.5f, juce::Font::bold));
    g.drawText(partNumber, chipRect, juce::Justification::centred);
}

void Replicant303AudioProcessorEditor::drawTransistor(juce::Graphics& g, float x, float y, const juce::String& partNumber)
{
    // TO-92 Transistor Package (D-shape)
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillEllipse(x - 6.0f, y - 4.0f, 14.0f, 10.0f);

    // 3 Silver Pins
    g.setColour(juce::Colour(0xffc8d0cb));
    g.fillRect(x - 4.0f, y + 4.0f, 1.5f, 4.0f);
    g.fillRect(x, y + 4.0f, 1.5f, 4.0f);
    g.fillRect(x + 4.0f, y + 4.0f, 1.5f, 4.0f);

    // Black D-shaped body
    g.setColour(juce::Colour(0xff181d1a));
    g.fillRoundedRectangle(x - 6.0f, y - 5.0f, 13.0f, 8.0f, 3.0f);

    // Silkscreen text
    g.setColour(juce::Colours::white.withAlpha(0.60f));
    g.setFont(Replicant303::uiFont(6.5f, juce::Font::bold));
    g.drawText(partNumber, (int)(x - 8.0f), (int)(y - 5.0f), 17, 8, juce::Justification::centred);
}

void Replicant303AudioProcessorEditor::drawCapacitor(juce::Graphics& g, float x, float y, float radius, juce::Colour canColour)
{
    // Drop shadow
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillEllipse(x - radius + 2.0f, y - radius + 3.0f, radius * 2.0f, radius * 2.0f);

    // Aluminum cylindrical can
    juce::ColourGradient canGrad(canColour.brighter(0.25f), x - radius * 0.4f, y - radius * 0.4f,
                                 canColour.darker(0.45f), x + radius * 0.5f, y + radius * 0.5f, false);
    g.setGradientFill(canGrad);
    g.fillEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f);

    // Silver vent score lines (cross)
    g.setColour(juce::Colour(0xffdbe2dc).withAlpha(0.85f));
    g.drawEllipse(x - radius * 0.85f, y - radius * 0.85f, radius * 1.7f, radius * 1.7f, 0.9f);
    g.drawLine(x - radius * 0.55f, y, x + radius * 0.55f, y, 1.0f);
    g.drawLine(x, y - radius * 0.55f, x, y + radius * 0.55f, 1.0f);
}

void Replicant303AudioProcessorEditor::drawResistor(juce::Graphics& g, float x1, float y1, float x2, float y2,
                                                   juce::Colour b1, juce::Colour b2, juce::Colour b3)
{
    // Copper wire leads
    g.setColour(juce::Colour(0xffc0cac2));
    g.drawLine(x1, y1, x2, y2, 1.3f);

    float cx = (x1 + x2) * 0.5f;
    float cy = (y1 + y2) * 0.5f;
    auto body = juce::Rectangle<float>(cx - 8.0f, cy - 3.0f, 16.0f, 6.0f);

    // Ceramic beige body
    g.setColour(juce::Colour(0xffd5c4a1));
    g.fillRoundedRectangle(body, 2.0f);

    // Color stripes
    g.setColour(b1); g.fillRect(cx - 5.0f, cy - 3.0f, 1.8f, 6.0f);
    g.setColour(b2); g.fillRect(cx - 1.5f, cy - 3.0f, 1.8f, 6.0f);
    g.setColour(b3); g.fillRect(cx + 2.0f, cy - 3.0f, 1.8f, 6.0f);
    g.setColour(juce::Colour(0xffd4af37)); g.fillRect(cx + 5.0f, cy - 3.0f, 1.5f, 6.0f); // Gold
}

void Replicant303AudioProcessorEditor::draw3DCircuitBoard(juce::Graphics& g)
{
    // 1. Dark Green FR4 PCB Ground Plane Substrate
    juce::ColourGradient pcbGrad(
        theme().pcbDark.brighter(0.12f), 0.0f, 0.0f,
        theme().pcbDark.darker(0.35f), 0.0f, (float)getHeight(), false);
    g.setGradientFill(pcbGrad);
    g.fillAll();

    // 2. Copper Routing Traces & Ground Pours
    g.setColour(theme().pcbTrace.withAlpha(0.68f));

    // Horizontal power buses
    g.drawLine(20.0f, 115.0f, 1060.0f, 115.0f, 2.5f);
    g.drawLine(20.0f, 120.0f, 1060.0f, 120.0f, 1.5f);
    g.drawLine(20.0f, 245.0f, 1060.0f, 245.0f, 2.0f);
    g.drawLine(20.0f, 385.0f, 1060.0f, 385.0f, 2.8f);

    auto drawAngledTrace = [&g](float x1, float y1, float x2, float y2, float x3, float y3) {
        juce::Path p;
        p.startNewSubPath(x1, y1);
        p.lineTo(x2, y2);
        p.lineTo(x3, y3);
        g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    drawAngledTrace(65.0f,  115.0f, 85.0f,  140.0f, 85.0f,  235.0f);
    drawAngledTrace(200.0f, 115.0f, 220.0f, 140.0f, 310.0f, 140.0f);
    drawAngledTrace(330.0f, 140.0f, 350.0f, 160.0f, 350.0f, 240.0f);
    drawAngledTrace(460.0f, 115.0f, 490.0f, 150.0f, 620.0f, 150.0f);
    drawAngledTrace(750.0f, 115.0f, 775.0f, 140.0f, 775.0f, 235.0f);
    drawAngledTrace(890.0f, 115.0f, 915.0f, 140.0f, 915.0f, 235.0f);

    // Keyboard bus lines
    for (int i = 0; i < 16; ++i)
    {
        float kx = 210.0f + i * 36.0f;
        g.drawLine(kx, 385.0f, kx + 8.0f, 400.0f, 1.2f);
        g.drawLine(kx + 8.0f, 400.0f, kx + 8.0f, 495.0f, 1.2f);
    }

    // Solder pads and Vias (copper dots with dark holes)
    g.setColour(theme().pcbVia.withAlpha(0.65f));
    for (int i = 0; i < 55; ++i)
    {
        float vx = 35.0f + static_cast<float>((i * 31) % 1010);
        float vy = 45.0f + static_cast<float>((i * 43) % 490);
        g.fillEllipse(vx - 2.5f, vy - 2.5f, 5.0f, 5.0f);
        g.setColour(theme().pcbViaHole);
        g.fillEllipse(vx - 1.0f, vy - 1.0f, 2.0f, 2.0f);
        g.setColour(theme().pcbVia.withAlpha(0.65f));
    }

    // 3. 3D Electronic Components
    // DIP IC Packages
    drawIcChip(g, 140.0f, 185.0f, 38.0f, 22.0f, 8, "BA662");     // Roland 303 VCA
    drawIcChip(g, 340.0f, 185.0f, 42.0f, 22.0f, 8, "AN6562");    // Filter Buffer
    drawIcChip(g, 725.0f, 185.0f, 40.0f, 22.0f, 8, "LM308N");    // ProCo Rat Op-Amp!
    drawIcChip(g, 885.0f, 185.0f, 38.0f, 22.0f, 8, "TL072");     // Distortion Tone Stage
    drawIcChip(g, 470.0f, 340.0f, 74.0f, 28.0f, 16, "D8048C");   // 303 CPU Microcontroller

    // Transistors (TO-92 packages)
    drawTransistor(g, 105.0f, 195.0f, "C1815");
    drawTransistor(g, 260.0f, 195.0f, "C1815");
    drawTransistor(g, 400.0f, 195.0f, "C1815");
    drawTransistor(g, 650.0f, 195.0f, "C1815");
    drawTransistor(g, 860.0f, 195.0f, "C1815");

    // Capacitors (Radial cans)
    drawCapacitor(g, 115.0f, 150.0f, 8.5f, juce::Colour(0xff2a5885)); // Blue
    drawCapacitor(g, 215.0f, 205.0f, 8.0f, juce::Colour(0xff222824)); // Black
    drawCapacitor(g, 395.0f, 150.0f, 9.5f, juce::Colour(0xff2a5885));
    drawCapacitor(g, 700.0f, 205.0f, 8.5f, juce::Colour(0xff222824));
    drawCapacitor(g, 860.0f, 150.0f, 9.0f, juce::Colour(0xff2a5885));

    // Resistors
    drawResistor(g, 100.0f, 140.0f, 130.0f, 140.0f, juce::Colours::brown, juce::Colours::black, juce::Colours::red);
    drawResistor(g, 250.0f, 150.0f, 280.0f, 150.0f, juce::Colours::yellow, juce::Colours::purple, juce::Colours::orange);
    drawResistor(g, 450.0f, 200.0f, 480.0f, 200.0f, juce::Colours::red, juce::Colours::red, juce::Colours::brown);
    drawResistor(g, 780.0f, 150.0f, 810.0f, 150.0f, juce::Colours::brown, juce::Colours::black, juce::Colours::orange);
}

void Replicant303AudioProcessorEditor::drawTranslucentAcrylicPanel(juce::Graphics& g)
{
    // 1. Translucent Acrylic Glaze (colour + translucency come from the active skin)
    juce::ColourGradient acrylicGrad(
        theme().chassisUp.withAlpha(theme().chassisAlphaUp), 0.0f, 0.0f,
        theme().chassisDown.withAlpha(theme().chassisAlphaDown), 0.0f, (float)getHeight(), false);
    g.setGradientFill(acrylicGrad);
    g.fillAll();

    // 2. Translucent Internal Mold Bosses (Circular Standoff Pillars)
    // Upper row knobs:
    float upperY = 78.0f;
    float upperXs[] = { 65.0f, 131.0f, 197.0f, 263.0f, 329.0f, 395.0f, 755.0f, 825.0f, 895.0f };
    for (float kx : upperXs)
        drawMoldBoss(g, kx, upperY, 27.0f);

    // Audio / CV jacks in the middle:
    float jackY = 78.0f;
    float jackXs[] = { 465.0f, 515.0f, 565.0f, 615.0f, 665.0f };
    for (float jx : jackXs)
        drawMoldBoss(g, jx, jackY, 19.0f);

    // Middle row controls:
    drawMoldBoss(g, 70.0f, 183.0f, 38.0f);   // Tempo
    drawMoldBoss(g, 331.0f, 186.0f, 28.0f);  // Track knob
    drawMoldBoss(g, 446.0f, 186.0f, 28.0f);  // Dist Model knob
    drawMoldBoss(g, 668.0f, 185.0f, 32.0f);  // Logo boss
    drawMoldBoss(g, 1002.0f, 183.0f, 38.0f); // Volume

    // Structural bosses under buttons
    drawMoldBoss(g, 69.0f, 469.0f, 28.0f);   // Start/Stop
    drawMoldBoss(g, 1011.0f, 469.0f, 28.0f); // Write/Next

    // 3. 3D Beveled Chassis Edges & Specular Glare
    auto bounds = getLocalBounds().toFloat();

    // Outer case beveled rim
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.drawRect(bounds.reduced(1.0f), 2.5f);

    // Specular highlight edges
    g.setColour(theme().chassisHighlight.withAlpha(0.70f));
    g.drawLine(3.0f, 3.0f, bounds.getRight() - 3.0f, 3.0f, 2.0f);
    g.drawLine(3.0f, 3.0f, 3.0f, bounds.getBottom() - 3.0f, 2.0f);

    // Diagonal ambient light reflection
    juce::Path glare;
    glare.startNewSubPath(20.0f, 0.0f);
    glare.lineTo(bounds.getRight() * 0.75f, 0.0f);
    glare.lineTo(bounds.getRight() * 0.40f, bounds.getBottom() * 0.45f);
    glare.lineTo(0.0f, bounds.getBottom() * 0.45f);
    glare.closeSubPath();
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.fillPath(glare);

    // 4. Physical Corner Screws & Structural Mounts
    drawScrew(g, 22.0f, 22.0f);
    drawScrew(g, (float)getWidth() - 22.0f, 22.0f);
    drawScrew(g, 22.0f, (float)getHeight() - 22.0f);
    drawScrew(g, (float)getWidth() - 22.0f, (float)getHeight() - 22.0f);

    drawScrew(g, 435.0f, 22.0f);
    drawScrew(g, 700.0f, 22.0f);
    drawScrew(g, 435.0f, 568.0f);
    drawScrew(g, 700.0f, 568.0f);
}

void Replicant303AudioProcessorEditor::drawSilkscreenGraphics(juce::Graphics& g)
{
    g.setColour(theme().silkscreen);

    // 1. Header Typography
    g.setFont(Replicant303::uiFont(26.0f, juce::Font::bold));
    g.drawText("REPLICANT-303", 38, 12, 230, 28, juce::Justification::centredLeft);

    g.setFont(Replicant303::uiFont(11.5f, juce::Font::plain));
    g.drawText("Computer Controlled", 265, 20, 160, 16, juce::Justification::centredLeft);

    g.setFont(Replicant303::uiFont(13.0f, juce::Font::bold));
    g.drawText("Analog Bass Line Synthesizer", getWidth() - 310, 16, 270, 20, juce::Justification::centredRight);

    // 2. 5 Patch Jacks (Center Top)
    float jackY = 78.0f;
    float jackXs[] = { 465.0f, 515.0f, 565.0f, 615.0f, 665.0f };
    const char* jackLabels[] = { "FILTER IN", "SYNC IN", "CV OUT", "GATE OUT", "PHONES" };
    for (int i = 0; i < 5; ++i)
        Replicant303::Replicant303LookAndFeel::drawJackSocket(g, jackXs[i], jackY, 19.0f, jackLabels[i]);

    // 3. Distortion Section Grouping Box (Top Right, wraps knobs AND switch)
    auto distBox = juce::Rectangle<float>(715.0f, 42.0f, 225.0f, 193.0f);
    g.setColour(theme().silkscreen.withAlpha(0.75f));
    g.drawRoundedRectangle(distBox, 4.0f, 1.2f);
    g.setColour(theme().silkscreen);
    g.setFont(Replicant303::uiFont(10.5f, juce::Font::bold));
    g.drawText("DISTORTION", 725, 45, 90, 14, juce::Justification::centredLeft);

    // Divider inside distortion box separating knobs and switch
    g.setColour(theme().silkscreen.withAlpha(0.45f));
    g.drawLine(725.0f, 138.0f, 930.0f, 138.0f, 1.0f);

    // Distortion switch OFF / ON labels
    g.setColour(theme().silkscreen);
    g.setFont(Replicant303::uiFont(9.5f, juce::Font::bold));
    g.drawText("OFF", 760, 172, 32, 16, juce::Justification::centred);
    g.drawText("ON",  856, 172, 32, 16, juce::Justification::centred);

    // Distortion Active LED above switch
    auto distLedRect = juce::Rectangle<float>(820.0f, 150.0f, 8.0f, 8.0f);
    g.setColour(theme().ledBezelDark);
    g.drawEllipse(distLedRect.expanded(1.0f), 1.0f);
    bool distOn = distActiveSwitch.getToggleState();
    if (distOn)
    {
        g.setColour(theme().ledBright.withAlpha(0.45f));
        g.fillEllipse(distLedRect.expanded(4.0f));
        g.setColour(theme().ledBright);
        g.fillEllipse(distLedRect);
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.fillEllipse(distLedRect.reduced(2.0f).translated(-0.6f, -0.6f));
    }
    else
    {
        g.setColour(theme().ledDark);
        g.fillEllipse(distLedRect);
    }

    // 4. Middle Section Silkscreen Frames
    // Waveform Box
    auto waveBox = juce::Rectangle<float>(132.0f, 136.0f, 126.0f, 99.0f);
    g.setColour(theme().silkscreen.withAlpha(0.70f));
    g.drawRoundedRectangle(waveBox, 3.5f, 1.0f);
    g.setColour(theme().silkscreen);
    g.setFont(Replicant303::uiFont(9.5f, juce::Font::bold));
    g.drawText("WAVEFORM", (int)waveBox.getX(), (int)(waveBox.getY() + 4.0f), (int)waveBox.getWidth(), 14, juce::Justification::centred);

    // Saw & Square wave glyphs
    juce::Path sawGlyph;
    sawGlyph.startNewSubPath(146.0f, 188.0f);
    sawGlyph.lineTo(162.0f, 176.0f);
    sawGlyph.lineTo(162.0f, 188.0f);
    sawGlyph.lineTo(168.0f, 184.0f);
    g.strokePath(sawGlyph, juce::PathStrokeType(1.6f));

    juce::Path sqrGlyph;
    sqrGlyph.startNewSubPath(226.0f, 188.0f);
    sqrGlyph.lineTo(226.0f, 176.0f);
    sqrGlyph.lineTo(239.0f, 176.0f);
    sqrGlyph.lineTo(239.0f, 188.0f);
    sqrGlyph.lineTo(248.0f, 188.0f);
    g.strokePath(sqrGlyph, juce::PathStrokeType(1.6f));

    // Track / Pattern Group Box
    auto trkBox = juce::Rectangle<float>(268.0f, 136.0f, 126.0f, 99.0f);
    g.setColour(theme().silkscreen.withAlpha(0.70f));
    g.drawRoundedRectangle(trkBox, 3.5f, 1.0f);
    g.setColour(theme().silkscreen);
    g.setFont(Replicant303::uiFont(9.0f, juce::Font::bold));
    g.drawFittedText("TRACK", juce::Rectangle<int>((int)trkBox.getX(), (int)(trkBox.getY() + 4.0f), 44, 13), juce::Justification::centred, 1, 0.85f);
    g.drawFittedText("PATTERN GROUP", juce::Rectangle<int>((int)trkBox.getX() + 44, (int)(trkBox.getY() + 4.0f), 82, 13), juce::Justification::centred, 1, 0.85f);
    g.setFont(Replicant303::uiFont(8.0f, juce::Font::bold));
    g.drawText("I", 282, 178, 12, 12, juce::Justification::centred);
    g.drawText("II", 290, 160, 12, 12, juce::Justification::centred);
    g.drawText("III", 360, 160, 14, 12, juce::Justification::centred);
    g.drawText("IV", 368, 178, 14, 12, juce::Justification::centred);

    // Distortion Model Box
    auto modeBox = juce::Rectangle<float>(404.0f, 136.0f, 216.0f, 99.0f);
    g.setColour(theme().silkscreen.withAlpha(0.70f));
    g.drawRoundedRectangle(modeBox, 3.5f, 1.0f);
    g.setColour(theme().silkscreen);
    g.setFont(Replicant303::uiFont(9.5f, juce::Font::bold));
    g.drawText("DISTORTION MODEL", (int)modeBox.getX(), (int)(modeBox.getY() + 4.0f), (int)modeBox.getWidth(), 14, juce::Justification::centred);

    // Model ticks around rotary knob
    g.setFont(Replicant303::uiFont(7.5f, juce::Font::bold));
    g.drawText("RAT",  418, 208, 24, 10, juce::Justification::centred);
    g.drawText("TUBE", 404, 180, 26, 10, juce::Justification::centred);
    g.drawText("CLIP", 434, 146, 24, 10, juce::Justification::centred);
    g.drawText("FUZZ", 464, 180, 24, 10, juce::Justification::centred);
    g.drawText("FOLD", 450, 208, 26, 10, juce::Justification::centred);

    // Central Brand Logo (TB Wave Emblem + DanimanX)
    auto logoCenter = juce::Point<float>(668.0f, 185.0f);
    juce::Path logo;
    logo.addEllipse(logoCenter.x - 19.0f, logoCenter.y - 19.0f, 38.0f, 38.0f);
    g.setColour(theme().silkscreen.withAlpha(0.85f));
    g.strokePath(logo, juce::PathStrokeType(1.6f));

    juce::Path innerWave;
    innerWave.startNewSubPath(logoCenter.x - 13.0f, logoCenter.y + 2.0f);
    innerWave.cubicTo(logoCenter.x - 6.0f, logoCenter.y - 15.0f, logoCenter.x + 6.0f, logoCenter.y + 15.0f, logoCenter.x + 13.0f, logoCenter.y - 2.0f);
    g.strokePath(innerWave, juce::PathStrokeType(2.4f));

    g.setFont(Replicant303::uiFont(11.0f, juce::Font::bold));
    g.drawText("DanimanX", (int)(logoCenter.x - 45.0f), (int)(logoCenter.y + 22.0f), 90, 14, juce::Justification::centred);

    // Tempo & Volume Markings
    g.setFont(Replicant303::uiFont(9.5f, juce::Font::bold));
    g.drawText("TEMPO", 36, 130, 68, 14, juce::Justification::centred);
    g.drawText("VOLUME", 968, 130, 68, 14, juce::Justification::centred);

    g.setFont(Replicant303::uiFont(8.5f, juce::Font::bold));
    g.drawText("SLOW /", 28, 224, 38, 14, juce::Justification::centredLeft);
    g.drawText("FAST",   80, 224, 32, 14, juce::Justification::centredRight);
    g.drawText("MIN /",  960, 224, 34, 14, juce::Justification::centredLeft);
    g.drawText("MAX",   1012, 224, 28, 14, juce::Justification::centredRight);

    // 5. Bottom Horizontal Silkscreen Divider Bar
    g.setColour(theme().silkscreen.withAlpha(0.85f));
    g.drawLine(20.0f, 250.0f, (float)getWidth() - 20.0f, 250.0f, 1.5f);
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.drawLine(20.0f, 251.5f, (float)getWidth() - 20.0f, 251.5f, 1.0f);

    // 6. Sequencer Silkscreen Framing Boxes
    // Left Box
    auto leftBox = juce::Rectangle<float>(24.0f, 260.0f, 152.0f, 265.0f);
    g.setColour(theme().silkscreen.withAlpha(0.60f));
    g.drawRoundedRectangle(leftBox, 3.5f, 1.0f);
    g.setColour(theme().silkscreen);
    g.setFont(Replicant303::uiFont(8.5f, juce::Font::bold));
    g.drawText("D.C.", 32, 264, 24, 12, juce::Justification::centredLeft);
    g.drawText("BAR RESET", 62, 264, 58, 12, juce::Justification::centred);
    g.drawText("CLEAR", 124, 264, 40, 12, juce::Justification::centred);

    // Keyboard Box
    auto kbdBox = juce::Rectangle<float>(186.0f, 260.0f, 420.0f, 265.0f);
    g.setColour(theme().silkscreen.withAlpha(0.60f));
    g.drawRoundedRectangle(kbdBox, 3.5f, 1.0f);

    // Time Mode Box
    auto timeBox = juce::Rectangle<float>(616.0f, 260.0f, 276.0f, 265.0f);
    g.setColour(theme().silkscreen.withAlpha(0.60f));
    g.drawRoundedRectangle(timeBox, 3.5f, 1.0f);
    g.setColour(theme().silkscreen);
    g.setFont(Replicant303::uiFont(9.5f, juce::Font::bold));
    g.drawText("TIME MODE", (int)timeBox.getX(), (int)(timeBox.getY() + 4.0f), (int)timeBox.getWidth(), 14, juce::Justification::centred);

    // Far Right Sequencer Box
    auto rgtBox = juce::Rectangle<float>(902.0f, 260.0f, 154.0f, 265.0f);
    g.setColour(theme().silkscreen.withAlpha(0.60f));
    g.drawRoundedRectangle(rgtBox, 3.5f, 1.0f);

    // 7. Step Numbers and Sublabels
    g.setColour(theme().silkscreen);
    g.setFont(Replicant303::uiFont(8.5f, juce::Font::bold));

    // Keyboard Black Key Labels (above black LEDs)
    for (auto& key : pianoKeys)
    {
        if (key.isBlack && key.subLabel.isNotEmpty())
        {
            g.drawText(key.subLabel, (int)(key.bounds.getX() - 6.0f), (int)(key.bounds.getY() - 32.0f), (int)(key.bounds.getWidth() + 12.0f), 12, juce::Justification::centred);
        }
    }

    // Keyboard White Key Labels (above white LEDs) and Step Numbers (below keys)
    for (auto& key : pianoKeys)
    {
        if (!key.isBlack)
        {
            if (key.subLabel.isNotEmpty())
                g.drawText(key.subLabel, (int)(key.bounds.getX() - 6.0f), (int)(key.bounds.getY() - 32.0f), (int)(key.bounds.getWidth() + 12.0f), 12, juce::Justification::centred);
            if (key.stepLabel.isNotEmpty())
            {
                g.setFont(Replicant303::uiFont(10.0f, juce::Font::bold));
                g.drawText(key.stepLabel, (int)(key.bounds.getX() - 4.0f), (int)(key.bounds.getBottom() + 4.0f), (int)(key.bounds.getWidth() + 8.0f), 14, juce::Justification::centred);
                g.setFont(Replicant303::uiFont(8.5f, juce::Font::bold));
            }
        }
    }

    // Step numbers 9, 0, 100, 200 under right buttons
    for (auto& btn : functionButtons)
    {
        if (btn.stepLabel.isNotEmpty())
        {
            g.setFont(Replicant303::uiFont(10.0f, juce::Font::bold));
            g.drawText(btn.stepLabel, (int)(btn.bounds.getX() - 4.0f), 504, (int)(btn.bounds.getWidth() + 8.0f), 14, juce::Justification::centred);
            g.setFont(Replicant303::uiFont(8.5f, juce::Font::bold));
        }
    }

    // 8. 3D Embossed Transparent Plastic Logo across Bottom Lip
    juce::String embossedText = "R  E  P  L  I  C  A  N  T   -   3  0  3";
    auto embRect = juce::Rectangle<float>(0.0f, 548.0f, (float)getWidth(), 30.0f);
    g.setFont(Replicant303::uiFont(18.0f, juce::Font::bold));

    // Recessed drop shadow
    g.setColour(theme().chassisEmbossShadow.withAlpha(0.70f));
    g.drawText(embossedText, embRect.translated(0.0f, 1.5f), juce::Justification::centred);

    // Specular plastic highlight
    g.setColour(theme().chassisHighlight.withAlpha(0.60f));
    g.drawText(embossedText, embRect.translated(0.0f, -1.0f), juce::Justification::centred);

    // Core embossed lettering
    g.setColour(theme().chassisUp.brighter(0.15f).withAlpha(0.40f));
    g.drawText(embossedText, embRect, juce::Justification::centred);
}

void Replicant303AudioProcessorEditor::drawPianoKeysAndLeds(juce::Graphics& g)
{
    // 1. Draw Function Buttons
    for (auto& btn : functionButtons)
    {
        // Drop shadow
        g.setColour(juce::Colours::black.withAlpha(0.50f));
        g.fillRoundedRectangle(btn.bounds.translated(0.0f, 2.5f), 2.5f);

        // Tactile button top
        juce::Colour btnCol = btn.isLit ? theme().buttonFaceLit : theme().buttonFaceIdle;
        juce::ColourGradient grad(btnCol.brighter(0.12f), btn.bounds.getX(), btn.bounds.getY(),
                                  btnCol.darker(0.35f), btn.bounds.getX(), btn.bounds.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(btn.bounds, 2.0f);
        g.setColour(juce::Colours::black.withAlpha(0.65f));
        g.drawRoundedRectangle(btn.bounds, 2.0f, 1.0f);

        // Label on/below button
        g.setColour(theme().silkscreen);
        g.setFont(Replicant303::uiFont(8.5f, juce::Font::bold));
        g.drawText(btn.label, (int)(btn.bounds.getX() - 8.0f), (int)(btn.bounds.getBottom() + 2.0f), (int)(btn.bounds.getWidth() + 16.0f), 14, juce::Justification::centred);

        if (btn.subLabel.isNotEmpty())
        {
            g.setFont(Replicant303::uiFont(7.5f, juce::Font::plain));
            // Buttons sit close together, so allow a small horizontal squeeze rather
            // than ellipsising labels such as "PATTERN SECTION".
            g.drawFittedText(btn.subLabel,
                             juce::Rectangle<int>((int)(btn.bounds.getX() - 16.0f), (int)(btn.bounds.getBottom() + 14.0f),
                                                  (int)(btn.bounds.getWidth() + 32.0f), 13),
                             juce::Justification::centred, 1, 0.72f);
        }

        // Circular Red LED
        if (btn.hasLed)
        {
            auto ledRect = juce::Rectangle<float>(btn.ledPos.x - 4.0f, btn.ledPos.y - 4.0f, 8.0f, 8.0f);
            g.setColour(theme().ledBezelDark);
            g.drawEllipse(ledRect.expanded(1.0f), 1.0f);

            if (btn.isLit)
            {
                g.setColour(theme().ledBright.withAlpha(0.50f));
                g.fillEllipse(ledRect.expanded(4.5f));
                g.setColour(theme().ledBright);
                g.fillEllipse(ledRect);
                g.setColour(juce::Colours::white.withAlpha(0.90f));
                g.fillEllipse(ledRect.reduced(2.0f).translated(-0.6f, -0.6f));
            }
            else
            {
                g.setColour(theme().ledDark);
                g.fillEllipse(ledRect);
            }
        }
    }

    // 2. Draw White Piano Keys
    for (auto& key : pianoKeys)
    {
        if (key.isBlack) continue;

        // Drop shadow
        g.setColour(juce::Colours::black.withAlpha(0.50f));
        g.fillRoundedRectangle(key.bounds.translated(0.0f, 3.0f), 3.0f);

        // Tactile button (light metal when lit, darker when idle)
        juce::Colour keyTop = key.isPressed ? theme().keyFacePressedUp : theme().keyFaceUp;
        juce::Colour keyBottom = key.isPressed ? theme().keyFacePressedDown : theme().keyFaceDown;
        juce::ColourGradient keyGrad(keyTop, key.bounds.getX(), key.bounds.getY(),
                                     keyBottom, key.bounds.getX(), key.bounds.getBottom(), false);
        g.setGradientFill(keyGrad);
        g.fillRoundedRectangle(key.bounds, 2.5f);

        g.setColour(juce::Colours::black.withAlpha(0.60f));
        g.drawRoundedRectangle(key.bounds, 2.5f, 1.0f);

        // Note letter on key
        g.setColour(juce::Colour(0xff181c19));
        g.setFont(Replicant303::uiFont(11.0f, juce::Font::bold));
        g.drawText(key.name, key.bounds.withTrimmedTop(key.bounds.getHeight() * 0.45f), juce::Justification::centred);

        // Red LED above key
        auto ledRect = juce::Rectangle<float>(key.ledPos.x - 4.0f, key.ledPos.y - 4.0f, 8.0f, 8.0f);
        g.setColour(theme().ledBezelDark);
        g.drawEllipse(ledRect.expanded(1.0f), 1.0f);

        if (key.isPressed)
        {
            g.setColour(theme().ledBright.withAlpha(0.50f));
            g.fillEllipse(ledRect.expanded(5.0f));
            g.setColour(theme().ledBright);
            g.fillEllipse(ledRect);
            g.setColour(juce::Colours::white.withAlpha(0.90f));
            g.fillEllipse(ledRect.reduced(2.0f).translated(-0.6f, -0.6f));
        }
        else
        {
            g.setColour(theme().ledDark);
            g.fillEllipse(ledRect);
        }
    }

    // 3. Draw Black Piano Keys (on top of white keys)
    for (auto& key : pianoKeys)
    {
        if (!key.isBlack) continue;

        // Shadow
        g.setColour(juce::Colours::black.withAlpha(0.65f));
        g.fillRoundedRectangle(key.bounds.translated(0.0f, 3.5f), 2.5f);

        // Tactile dark charcoal button
        juce::Colour keyTop = key.isPressed ? theme().darkKeyFacePressedUp : theme().darkKeyFaceUp;
        juce::Colour keyBottom = key.isPressed ? theme().darkKeyFacePressedDown : theme().darkKeyFaceDown;
        juce::ColourGradient keyGrad(keyTop, key.bounds.getX(), key.bounds.getY(),
                                     keyBottom, key.bounds.getX(), key.bounds.getBottom(), false);
        g.setGradientFill(keyGrad);
        g.fillRoundedRectangle(key.bounds, 2.0f);

        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(key.bounds, 2.0f, 1.0f);

        // Note letter on black key
        g.setColour(theme().silkscreen);
        g.setFont(Replicant303::uiFont(10.0f, juce::Font::bold));
        g.drawText(key.name, key.bounds, juce::Justification::centred);

        // Red LED above black key
        auto ledRect = juce::Rectangle<float>(key.ledPos.x - 4.0f, key.ledPos.y - 4.0f, 8.0f, 8.0f);
        g.setColour(theme().ledBezelDark);
        g.drawEllipse(ledRect.expanded(1.0f), 1.0f);

        if (key.isPressed)
        {
            g.setColour(theme().ledBright.withAlpha(0.50f));
            g.fillEllipse(ledRect.expanded(5.0f));
            g.setColour(theme().ledBright);
            g.fillEllipse(ledRect);
            g.setColour(juce::Colours::white.withAlpha(0.90f));
            g.fillEllipse(ledRect.reduced(2.0f).translated(-0.6f, -0.6f));
        }
        else
        {
            g.setColour(theme().ledDark);
            g.fillEllipse(ledRect);
        }
    }
}

void Replicant303AudioProcessorEditor::draw7SegmentDigit(juce::Graphics& g, float x, float y, float w, float h, char digit)
{
    // Bitmask for segments: bit 0=a, 1=b, 2=c, 3=d, 4=e, 5=f, 6=g
    uint8_t mask = 0;
    switch (digit)
    {
        case '0': mask = 0b00111111; break; // a, b, c, d, e, f
        case '1': mask = 0b00000110; break; // b, c
        case '2': mask = 0b01011011; break; // a, b, d, e, g
        case '3': mask = 0b01001111; break; // a, b, c, d, g
        case '4': mask = 0b01100110; break; // b, c, f, g
        case '5': mask = 0b01101101; break; // a, c, d, f, g
        case '6': mask = 0b01111101; break; // a, c, d, e, f, g
        case '7': mask = 0b00000111; break; // a, b, c
        case '8': mask = 0b01111111; break; // all 7
        case '9': mask = 0b01101111; break; // a, b, c, d, f, g
        case '-': mask = 0b01000000; break; // g only
        default:  mask = 0;          break; // space / unlit
    }

    const float t = 2.4f;      // Segment thickness
    const float gap = 0.75f;   // Gap between segments
    const float my = y + h * 0.5f;

    juce::Path segA, segB, segC, segD, segE, segF, segG;

    // Segment A (Top horizontal)
    segA.startNewSubPath(x + t * 0.9f + gap, y);
    segA.lineTo(x + w - t * 0.9f - gap, y);
    segA.lineTo(x + w - t * 1.6f - gap, y + t);
    segA.lineTo(x + t * 1.6f + gap, y + t);
    segA.closeSubPath();

    // Segment B (Top-right vertical)
    segB.startNewSubPath(x + w - t, y + t * 1.5f + gap);
    segB.lineTo(x + w, y + t * 0.9f + gap);
    segB.lineTo(x + w, my - gap * 0.6f);
    segB.lineTo(x + w - t * 0.5f, my - gap * 0.6f);
    segB.lineTo(x + w - t, my - t * 0.5f - gap * 0.6f);
    segB.closeSubPath();

    // Segment C (Bottom-right vertical)
    segC.startNewSubPath(x + w - t, my + t * 0.5f + gap * 0.6f);
    segC.lineTo(x + w - t * 0.5f, my + gap * 0.6f);
    segC.lineTo(x + w, my + gap * 0.6f);
    segC.lineTo(x + w, y + h - t * 0.9f - gap);
    segC.lineTo(x + w - t, y + h - t * 1.5f - gap);
    segC.closeSubPath();

    // Segment D (Bottom horizontal)
    segD.startNewSubPath(x + t * 1.6f + gap, y + h - t);
    segD.lineTo(x + w - t * 1.6f - gap, y + h - t);
    segD.lineTo(x + w - t * 0.9f - gap, y + h);
    segD.lineTo(x + t * 0.9f + gap, y + h);
    segD.closeSubPath();

    // Segment E (Bottom-left vertical)
    segE.startNewSubPath(x, my + gap * 0.6f);
    segE.lineTo(x + t * 0.5f, my + gap * 0.6f);
    segE.lineTo(x + t, my + t * 0.5f + gap * 0.6f);
    segE.lineTo(x + t, y + h - t * 1.5f - gap);
    segE.lineTo(x, y + h - t * 0.9f - gap);
    segE.closeSubPath();

    // Segment F (Top-left vertical)
    segF.startNewSubPath(x, y + t * 0.9f + gap);
    segF.lineTo(x + t, y + t * 1.5f + gap);
    segF.lineTo(x + t, my - t * 0.5f - gap * 0.6f);
    segF.lineTo(x + t * 0.5f, my - gap * 0.6f);
    segF.lineTo(x, my - gap * 0.6f);
    segF.closeSubPath();

    // Segment G (Middle horizontal)
    segG.startNewSubPath(x + t * 0.9f + gap, my);
    segG.lineTo(x + t * 1.4f + gap, my - t * 0.5f);
    segG.lineTo(x + w - t * 1.4f - gap, my - t * 0.5f);
    segG.lineTo(x + w - t * 0.9f - gap, my);
    segG.lineTo(x + w - t * 1.4f - gap, my + t * 0.5f);
    segG.lineTo(x + t * 1.4f + gap, my + t * 0.5f);
    segG.closeSubPath();

    // 80s LED display slight italic slant (~5 degrees)
    auto slantTransform = juce::AffineTransform::shear(-0.09f, 0.0f)
                              .translated((y + h * 0.5f) * 0.09f, 0.0f);

    juce::Path* segs[7] = { &segA, &segB, &segC, &segD, &segE, &segF, &segG };
    for (auto* p : segs)
        p->applyTransform(slantTransform);

    // LED Color Palette (skin driven)
    const juce::Colour unlitSegColor  = theme().displayUnlit.withAlpha(0.70f);   // Dark ghost segment
    const juce::Colour litGlowColor   = theme().displayGlow.withAlpha(0.38f);    // Diffuse bloom
    const juce::Colour litCoreColor   = theme().displayLit;                      // Solid hot LED
    const juce::Colour litFilament    = theme().displayFilament.withAlpha(0.85f); // White-hot center filament

    for (int i = 0; i < 7; ++i)
    {
        bool isLit = (mask & (1 << i)) != 0;
        if (isLit)
        {
            // 1. Soft red diffuse bloom halo
            g.setColour(litGlowColor);
            g.strokePath(*segs[i], juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // 2. Solid bright LED body
            g.setColour(litCoreColor);
            g.fillPath(*segs[i]);

            // 3. Hot filament core highlight
            g.setColour(litFilament);
            g.strokePath(*segs[i], juce::PathStrokeType(0.8f));
        }
        else
        {
            // Faint dark red unlit segment visible behind smoked filter
            g.setColour(unlitSegColor);
            g.fillPath(*segs[i]);
        }
    }
}

void Replicant303AudioProcessorEditor::drawDigitalTempoClock(juce::Graphics& g)
{
    // 80s Digital LED Clock Display (Centered in Header: x = 502, y = 11, w = 132, h = 32)
    const float boxX = 502.0f;
    const float boxY = 11.0f;
    const float boxW = 132.0f;
    const float boxH = 32.0f;

    // 1. Recessed drop shadow in green acrylic chassis
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(boxX, boxY + 1.5f, boxW, boxH, 3.5f);

    // 2. Beveled Dark Frame / Bezel
    juce::ColourGradient bezelGrad(theme().displayBezelUp, boxX, boxY,
                                   theme().displayBezelDown, boxX, boxY + boxH, false);
    g.setGradientFill(bezelGrad);
    g.fillRoundedRectangle(boxX, boxY, boxW, boxH, 3.5f);

    g.setColour(juce::Colour(0xff3f4d42));
    g.drawRoundedRectangle(boxX, boxY, boxW, boxH, 3.5f, 1.0f);

    // 3. Deep Black Display Chamber
    auto displayArea = juce::Rectangle<float>(boxX + 4.0f, boxY + 3.0f, boxW - 8.0f, boxH - 6.0f);
    g.setColour(theme().displayChamber);
    g.fillRoundedRectangle(displayArea, 2.0f);

    // 4. Smoked Filter Glass (dark red-tinted acrylic lens)
    g.setColour(juce::Colour(0xff180204).withAlpha(0.60f));
    g.fillRoundedRectangle(displayArea, 2.0f);

    // 5. Authentic 80s LED filter horizontal micro-ribs
    g.setColour(juce::Colours::black.withAlpha(0.20f));
    for (float ry = displayArea.getY() + 2.0f; ry < displayArea.getBottom() - 1.0f; ry += 2.0f)
        g.drawHorizontalLine((int)ry, displayArea.getX() + 2.0f, displayArea.getRight() - 2.0f);

    // 6. Labels: TEMPO (top-left) and BPM (bottom-right)
    g.setColour(theme().displayCaption);
    g.setFont(Replicant303::uiFont(7.5f, juce::Font::bold));
    g.drawText("TEMPO", (int)displayArea.getX() + 5, (int)displayArea.getY() + 8, 36, 10, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xffff2834).withAlpha(0.85f));
    g.setFont(Replicant303::uiFont(8.0f, juce::Font::bold));
    g.drawText("BPM", (int)displayArea.getRight() - 27, (int)displayArea.getBottom() - 12, 24, 10, juce::Justification::centredRight);

    // 7. Render 3 7-Segment Digits
    float tempoVal = 125.0f;
    auto* param = audioProcessor.apvts.getRawParameterValue("tempo");
    if (param != nullptr)
        tempoVal = param->load();
    else
        tempoVal = (float)tempoKnob.slider.getValue();

    int bpmInt = juce::jlimit(40, 300, juce::roundToInt(tempoVal));
    juce::String bpmStr = juce::String(bpmInt).paddedLeft(' ', 3);

    const float digitW = 15.0f;
    const float digitH = 19.0f;
    const float spacing = 5.0f;
    const float startX = boxX + 43.0f;
    const float digitY = boxY + 6.5f;

    for (int i = 0; i < 3; ++i)
    {
        char c = (i < bpmStr.length()) ? static_cast<char>(bpmStr[i]) : ' ';
        draw7SegmentDigit(g, startX + i * (digitW + spacing), digitY, digitW, digitH, c);
    }

    // 8. Flashing/Glowing Beat Dot (Active LED pulse)
    auto beatDot = juce::Rectangle<float>(boxX + 104.0f, boxY + 8.0f, 4.5f, 4.5f);
    g.setColour(theme().ledBright.withAlpha(0.40f));
    g.fillEllipse(beatDot.expanded(2.5f));
    g.setColour(theme().ledBright);
    g.fillEllipse(beatDot);
    g.setColour(juce::Colours::white.withAlpha(0.80f));
    g.fillEllipse(beatDot.reduced(1.2f).translated(-0.4f, -0.4f));

    // 9. Diagonal Specular Glass Reflection
    juce::Path glassGlare;
    glassGlare.startNewSubPath(displayArea.getX(), displayArea.getY());
    glassGlare.lineTo(displayArea.getX() + 45.0f, displayArea.getY());
    glassGlare.lineTo(displayArea.getX() + 18.0f, displayArea.getBottom());
    glassGlare.lineTo(displayArea.getX(), displayArea.getBottom());
    glassGlare.closeSubPath();
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.fillPath(glassGlare);
}

void Replicant303AudioProcessorEditor::paint(juce::Graphics& g)
{
    // Layer 1: Procedural 3D PCB Underlay with ICs, Capacitors, Resistors, Copper Traces & Vias
    draw3DCircuitBoard(g);

    // Layer 2: Translucent Emerald Green Acrylic Housing with Internal Mold Bosses
    drawTranslucentAcrylicPanel(g);

    // Layer 3: Crisp Silkscreen Markings, Jack Sockets, Labels & Embossed Logo
    drawSilkscreenGraphics(g);

    // 80s Digital Red LED Tempo Clock (Top Center)
    drawDigitalTempoClock(g);

    // Layer 4: 13-Key Tactile Keyboard, Function Buttons & Glowing Red LEDs
    drawPianoKeysAndLeds(g);
}

void Replicant303AudioProcessorEditor::resized()
{
    // Upper Synth Knobs Row
    const int knobSize = 54;
    const int upperY = 51;

    tuneKnob.slider.setBounds(38, upperY, knobSize, knobSize);
    tuneKnob.label.setBounds(33, upperY + knobSize + 2, knobSize + 10, 14);

    cutoffKnob.slider.setBounds(104, upperY, knobSize, knobSize);
    cutoffKnob.label.setBounds(99, upperY + knobSize + 2, knobSize + 10, 14);

    resonanceKnob.slider.setBounds(170, upperY, knobSize, knobSize);
    resonanceKnob.label.setBounds(165, upperY + knobSize + 2, knobSize + 10, 14);

    envModKnob.slider.setBounds(236, upperY, knobSize, knobSize);
    envModKnob.label.setBounds(231, upperY + knobSize + 2, knobSize + 10, 14);

    decayKnob.slider.setBounds(302, upperY, knobSize, knobSize);
    decayKnob.label.setBounds(297, upperY + knobSize + 2, knobSize + 10, 14);

    accentKnob.slider.setBounds(368, upperY, knobSize, knobSize);
    accentKnob.label.setBounds(363, upperY + knobSize + 2, knobSize + 10, 14);

    // Upper Distortion Knobs Row (Inside Distortion Box)
    distDriveKnob.slider.setBounds(728, upperY + 10, knobSize, knobSize);
    distDriveKnob.label.setBounds(723, upperY + knobSize + 12, knobSize + 10, 14);

    distToneKnob.slider.setBounds(798, upperY + 10, knobSize, knobSize);
    distToneKnob.label.setBounds(793, upperY + knobSize + 12, knobSize + 10, 14);

    distMixKnob.slider.setBounds(868, upperY + 10, knobSize, knobSize);
    distMixKnob.label.setBounds(863, upperY + knobSize + 12, knobSize + 10, 14);

    // Middle Row Controls
    const int largeKnobSize = 76;
    tempoKnob.slider.setBounds(32, 145, largeKnobSize, largeKnobSize);
    volumeKnob.slider.setBounds(964, 145, largeKnobSize, largeKnobSize);

    // Waveform Switch (Saw / Square)
    waveformSwitch.setBounds(172, 170, 46, 24);

    // Track Selector Rotary Switch
    trackKnob.slider.setBounds(303, 158, 56, 56);

    // Distortion Model Rotary Switch
    distTypeKnob.slider.setBounds(418, 158, 56, 56);

    // Distortion Model ComboBox
    distModelBox.setBounds(484, 172, 128, 28);

    // Panel skin selector (header, between the tempo display and the strapline)
    themeBox.setBounds(644, 13, 124, 25);

    // Distortion On/Off Switch (Inside Distortion Box, directly under distortion knobs!)
    distActiveSwitch.setBounds(800, 168, 48, 24);

    // Keyboard & Sequencer Layout
    setupKeyboardLayout();
}
