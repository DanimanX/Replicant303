#include "PatternDrawer.h"

namespace
{
    constexpr int kVisibleRows = 25;   // C1 (24) .. C3 (48)
    constexpr int kTopNote     = 48;
    constexpr int kNumActions  = 6;

    const char* const kActionLabels[kNumActions] =
        { "CLEAR", "COPY", "PASTE", "RANDOM", "INIT", "EXPORT MIDI" };

    bool isBlackKeyRow (int note) noexcept
    {
        const int pc = note % 12;
        return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
    }

    const char* const kPatternLetters = "ABCDEFGH";
}

//==============================================================================
PatternDrawer::PatternDrawer (Replicant303AudioProcessor& p)
    : processor (p), sequencer (p.getSequencer())
{
    setOpaque (true);
}

//==============================================================================
void PatternDrawer::resized()
{
    const float w = (float) getWidth();

    handleArea  = { 0.0f, 0.0f, w, 26.0f };
    closeButton = { w - 92.0f, 3.0f, 82.0f, 20.0f };
    headerArea  = { 12.0f, 30.0f, w - 24.0f, 38.0f };

    playButton  = { 14.0f, 41.0f, 84.0f, 26.0f };

    for (int i = 0; i < 8; ++i)
        slotButtons[(size_t) i] = { 106.0f + i * 40.0f, 41.0f, 36.0f, 26.0f };

    swingArea  = { 438.0f, 30.0f, 152.0f, 38.0f };
    swingTrack = { 446.0f, 50.0f, 136.0f, 8.0f };

    lengthMinus = { 604.0f, 41.0f, 26.0f, 26.0f };
    lengthValue = { 632.0f, 38.0f, 42.0f, 32.0f };
    lengthPlus  = { 676.0f, 41.0f, 26.0f, 26.0f };

    syncButton = { 714.0f, 41.0f, 94.0f, 26.0f };

    inspectorArea = { w - 208.0f, 76.0f, 190.0f, 278.0f };
    gridArea      = { 64.0f, 76.0f, inspectorArea.getX() - 12.0f - 64.0f, 200.0f };

    gateLane   = { 64.0f, 282.0f, gridArea.getWidth(), 22.0f };
    accentLane = { 64.0f, 308.0f, gridArea.getWidth(), 22.0f };
    slideLane  = { 64.0f, 334.0f, gridArea.getWidth(), 22.0f };

    actionArea = { 12.0f, 362.0f, w - 24.0f, 28.0f };

    const float actionW = (actionArea.getWidth() - (kNumActions - 1) * 8.0f) / (float) kNumActions;
    for (int i = 0; i < kNumActions; ++i)
        actionButtons[(size_t) i] = { actionArea.getX() + i * (actionW + 8.0f),
                                      actionArea.getY(), actionW, actionArea.getHeight() };

    // Preset vault: 8 pages x 8 slots = the 64 presets of the current letter.
    presetArea = { 12.0f, 396.0f, w - 24.0f, 32.0f };
    presetNameArea = { 552.0f, 398.0f, 228.0f, 28.0f };

    for (int i = 0; i < 8; ++i)
    {
        presetPageButtons[(size_t) i] = { 96.0f + i * 25.0f, 400.0f, 22.0f, 24.0f };
        presetSlotButtons[(size_t) i] = { 304.0f + i * 30.0f, 400.0f, 28.0f, 24.0f };
    }

    presetGenButton    = { 788.0f, 400.0f,  54.0f, 24.0f };
    presetStoreButton  = { 848.0f, 400.0f,  58.0f, 24.0f };
    presetImportButton = { 912.0f, 400.0f,  70.0f, 24.0f };
    presetExportButton = { 988.0f, 400.0f,  70.0f, 24.0f };

    const float ix = inspectorArea.getX();
    const float iw = inspectorArea.getWidth();

    noteMinus12 = { ix + 10.0f, 152.0f, 78.0f, 26.0f };
    notePlus12  = { ix + 96.0f, 152.0f, 78.0f, 26.0f };
    noteMinus1  = { ix + 10.0f, 182.0f, 78.0f, 26.0f };
    notePlus1   = { ix + 96.0f, 182.0f, 78.0f, 26.0f };

    inspGate   = { ix + 10.0f, 220.0f, iw - 20.0f, 30.0f };
    inspAccent = { ix + 10.0f, 256.0f, iw - 20.0f, 30.0f };
    inspSlide  = { ix + 10.0f, 292.0f, iw - 20.0f, 30.0f };
}

//==============================================================================
void PatternDrawer::refresh()
{
    const int step = sequencer.getCurrentStep();
    const int pattern = sequencer.getCurrentPattern();
    const bool running = sequencer.isPlaying();

    float tempo = 125.0f;
    if (auto* p = processor.apvts.getRawParameterValue ("tempo"))
        tempo = p->load();

    bool needsRepaint = false;

    const bool patternChanged = (pattern != lastPattern);

    if (patternChanged)
    {
        // The active letter owns the visible preset page/slot. Switching
        // patterns from the hardware buttons or the TRACK knob must update
        // the vault too.
        const int index = processor.getPresetBank().getSelectedSlot (pattern);
        presetPage = index / 8;
        presetSlot = index % 8;
        needsRepaint = true;
    }

    if (step != lastStep || patternChanged || running != lastRunning
        || ! juce::approximatelyEqual (tempo, lastTempo))
    {
        lastStep = step;
        lastPattern = pattern;
        lastRunning = running;
        lastTempo = tempo;
        needsRepaint = true;
    }

    if (presetStatusUntilMs > 0.0
        && juce::Time::getMillisecondCounterHiRes() >= presetStatusUntilMs)
    {
        presetStatus.clear();
        presetStatusUntilMs = 0.0;
        needsRepaint = true;
    }

    if (needsRepaint)
        repaint();
}

//==============================================================================
void PatternDrawer::setRevealOffset (float pixels)
{
    if (juce::approximatelyEqual (revealOffset, pixels))
        return;

    revealOffset = pixels;
    repaint();
}

void PatternDrawer::paint (juce::Graphics& g)
{
    // While the drawer slides out from under the panel, the whole block is
    // drawn shifted downward instead of the window being resized every frame.
    // The exposed slot above it is filled with the shadowed gap under the
    // panel, so the drawer reads as emerging from the chassis.
    g.fillAll (theme().pcbDark.darker (0.88f));

    if (revealOffset > 1.0f)
    {
        // Soft shadow under the panel's lip while the drawer is still emerging.
        const float depth = juce::jmin (revealOffset, 46.0f);
        juce::ColourGradient shadow (juce::Colours::black.withAlpha (0.62f), 0.0f, 0.0f,
                                     juce::Colours::transparentBlack, 0.0f, depth, false);
        g.setGradientFill (shadow);
        g.fillRect (0.0f, 0.0f, (float) getWidth(), depth);
    }

    juce::Graphics::ScopedSaveState saved (g);
    g.addTransform (juce::AffineTransform::translation (0.0f, revealOffset));

    drawBackground (g);
    drawHandle (g);
    drawTransport (g);
    drawGrid (g);
    drawLanes (g);
    drawInspector (g);
    drawActions (g);
    drawPresetVault (g);
}

void PatternDrawer::drawBackground (juce::Graphics& g)
{
    const auto& t = theme();
    const float w = (float) getWidth();
    const float h = (float) getHeight();

    juce::ColourGradient grad (t.pcbDark.brighter (0.05f), 0.0f, 0.0f,
                               t.pcbDark.darker (0.78f), 0.0f, h, false);
    g.setGradientFill (grad);
    g.fillAll();

    // Faint etched circuit traces so the drawer reads as the same hardware.
    g.setColour (t.pcbTrace.withAlpha (0.10f));
    for (int i = 0; i < 9; ++i)
    {
        const float y = 70.0f + i * 37.0f;
        g.drawLine (0.0f, y, w * 0.18f, y, 1.0f);
        g.drawLine (w * 0.18f, y, w * 0.24f, y + 18.0f, 1.0f);
        g.drawLine (w * 0.24f, y + 18.0f, w, y + 18.0f, 1.0f);
    }

    // Scanline texture.
    g.setColour (juce::Colours::white.withAlpha (0.014f));
    for (float y = 28.0f; y < h; y += 4.0f)
        g.drawHorizontalLine ((int) y, 0.0f, w);

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawHorizontalLine (0, 0.0f, w);
}

void PatternDrawer::drawHandle (juce::Graphics& g)
{
    const auto& t = theme();
    const float w = (float) getWidth();

    juce::ColourGradient metal (t.metalMid.brighter (0.30f), 0.0f, 0.0f,
                                t.metalEdge.darker (0.25f), 0.0f, handleArea.getBottom(), false);
    g.setGradientFill (metal);
    g.fillRect (handleArea);

    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.drawHorizontalLine (0, 0.0f, w);
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.drawHorizontalLine ((int) handleArea.getBottom() - 1, 0.0f, w);

    // Central grip ridges.
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    for (int i = 0; i < 3; ++i)
        g.fillRoundedRectangle (w * 0.5f - 22.0f + i * 16.0f, 10.0f, 12.0f, 5.0f, 2.0f);

    g.setColour (t.silkscreen.withAlpha (0.92f));
    g.setFont (Replicant303::uiFont (9.5f, juce::Font::bold));
    g.drawText ("PATTERN CORE", 16, 2, 140, 22, juce::Justification::centredLeft);

    g.setColour (t.silkscreen.withAlpha (0.50f));
    g.setFont (Replicant303::uiFont (8.0f, juce::Font::plain));
    g.drawText ("8 PATTERNS x 16 STEPS  /  GATE  ACCENT  SLIDE  /  MIDI OUT",
                128, 2, 520, 22, juce::Justification::centredLeft);

    // Close tab.
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (closeButton, 3.0f);
    g.setColour (t.silkscreen.withAlpha (0.55f));
    g.drawRoundedRectangle (closeButton, 3.0f, 1.0f);
    g.setColour (t.silkscreen);
    g.setFont (Replicant303::uiFont (9.0f, juce::Font::bold));
    g.drawText ("CLOSE  ^", closeButton, juce::Justification::centred);
}

void PatternDrawer::drawTransport (juce::Graphics& g)
{
    const auto& t = theme();
    const bool armed = sequencer.isTransportArmed();
    const bool playing = sequencer.isPlaying();

    g.setColour (t.silkscreen.withAlpha (0.45f));
    g.setFont (Replicant303::uiFont (7.5f, juce::Font::bold));
    g.drawText ("TRANSPORT", 14, 29, 84, 10, juce::Justification::centredLeft);
    g.drawText ("PATTERN", 106, 29, 120, 10, juce::Justification::centredLeft);
    g.drawText ("SWING", 438, 29, 60, 10, juce::Justification::centredLeft);
    g.drawText ("LENGTH", 604, 29, 60, 10, juce::Justification::centredLeft);
    g.drawText ("SYNC", 714, 29, 94, 10, juce::Justification::centredLeft);
    g.drawText ("CLOCK", 820, 29, 150, 10, juce::Justification::centredLeft);

    drawTactileButton (g, playButton, armed ? "STOP" : "PLAY", playing);

    const int current = sequencer.getCurrentPattern();
    for (int i = 0; i < 8; ++i)
    {
        auto r = slotButtons[(size_t) i];
        const bool active = (i == current);

        g.setColour (juce::Colours::black.withAlpha (0.50f));
        g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 2.5f);

        juce::Colour face = active ? t.buttonFaceLit : t.buttonFaceIdle.darker (0.55f);
        juce::ColourGradient grad (face.brighter (0.10f), r.getX(), r.getY(),
                                   face.darker (0.45f), r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 2.5f);

        g.setColour (active ? t.ledBright : t.silkscreen.withAlpha (0.35f));
        g.drawRoundedRectangle (r, 2.5f, active ? 1.6f : 1.0f);

        g.setColour (active ? juce::Colours::black.withAlpha (0.85f) : t.silkscreen);
        g.setFont (Replicant303::uiFont (12.0f, juce::Font::bold));
        g.drawText (juce::String::charToString (kPatternLetters[i]), r, juce::Justification::centred);
    }

    // Swing track.
    const float swing = sequencer.getSwing();
    const float swingNorm = juce::jlimit (0.0f, 1.0f, swing / 0.66f);

    g.setColour (juce::Colours::black.withAlpha (0.70f));
    g.fillRoundedRectangle (swingTrack, 4.0f);
    g.setColour (t.silkscreen.withAlpha (0.35f));
    g.drawRoundedRectangle (swingTrack, 4.0f, 1.0f);

    auto filled = swingTrack.withWidth (juce::jmax (swingTrack.getHeight(), swingTrack.getWidth() * swingNorm));
    g.setColour (t.ledBright.withAlpha (0.75f));
    g.fillRoundedRectangle (filled, 4.0f);

    g.setColour (t.silkscreen);
    g.fillEllipse (filled.getRight() - 4.0f, swingTrack.getCentreY() - 5.0f, 10.0f, 10.0f);

    g.setFont (Replicant303::uiFont (8.5f, juce::Font::bold));
    g.drawText (juce::String (juce::roundToInt (swing * 100.0f)) + "%",
                swingArea.getRight() - 32.0f, 30.0f, 32.0f, 12.0f,
                juce::Justification::centredRight);

    // Length stepper.
    drawTactileButton (g, lengthMinus, "-", false, true);
    drawTactileButton (g, lengthPlus, "+", false, true);

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (lengthValue, 3.0f);
    g.setColour (t.silkscreen.withAlpha (0.30f));
    g.drawRoundedRectangle (lengthValue, 3.0f, 1.0f);
    g.setColour (t.ledBright);
    g.setFont (Replicant303::uiFont (17.0f, juce::Font::bold));
    g.drawText (juce::String (sequencer.getLength (current)), lengthValue, juce::Justification::centred);

    // Sync mode.
    const bool host = sequencer.getSyncMode() == Replicant303::PatternSequencer::SyncMode::Host;
    drawTactileButton (g, syncButton, host ? "HOST" : "INT", host);

    float bpm = 125.0f;
    if (auto* p = processor.apvts.getRawParameterValue ("tempo"))
        bpm = p->load();

    g.setColour (t.silkscreen.withAlpha (0.75f));
    g.setFont (Replicant303::uiFont (9.5f, juce::Font::bold));
    g.drawText (juce::String (bpm, 1) + " BPM",
                820.0f, 38.0f, 150.0f, 16.0f, juce::Justification::centredLeft);

    g.setFont (Replicant303::uiFont (7.5f, juce::Font::plain));
    g.setColour (t.silkscreen.withAlpha (0.40f));
    g.drawText (host ? "FOLLOWING HOST TRANSPORT" : "FREE RUNNING CLOCK",
                820.0f, 52.0f, 220.0f, 12.0f, juce::Justification::centredLeft);
}

void PatternDrawer::drawGrid (juce::Graphics& g)
{
    const auto& t = theme();
    const auto pattern = sequencer.getPattern (sequencer.getCurrentPattern());
    const int length = pattern.length;
    const int playingStep = sequencer.getCurrentStep();
    const bool running = sequencer.isPlaying();

    const float colW = gridArea.getWidth() / 16.0f;
    const float rowH = gridArea.getHeight() / (float) kVisibleRows;

    // Recessed grid frame.
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (gridArea.expanded (4.0f), 4.0f);
    g.setColour (t.silkscreen.withAlpha (0.22f));
    g.drawRoundedRectangle (gridArea.expanded (4.0f), 4.0f, 1.0f);

    // Row bands.
    for (int row = 0; row < kVisibleRows; ++row)
    {
        const int note = kTopNote - row;
        auto rowRect = juce::Rectangle<float> (gridArea.getX(), gridArea.getY() + row * rowH,
                                               gridArea.getWidth(), rowH);

        const bool isRoot = (note % 12 == 0);
        const bool isBlack = isBlackKeyRow (note);

        if (isRoot || isBlack)
        {
            g.setColour (isRoot ? t.silkscreen.withAlpha (0.055f)
                                : juce::Colours::black.withAlpha (0.22f));
            g.fillRect (rowRect);
        }

        if (isRoot)
        {
            g.setColour (t.silkscreen.withAlpha (0.16f));
            g.drawHorizontalLine ((int) rowRect.getBottom(), gridArea.getX(), gridArea.getRight());
        }

        // Miniature keyboard at the left edge of the roll.
        auto keyRect = juce::Rectangle<float> (16.0f, rowRect.getY() + 0.5f,
                                               42.0f, juce::jmax (2.0f, rowH - 1.0f));
        g.setColour (isBlack ? juce::Colours::black.withAlpha (0.72f)
                             : t.silkscreen.withAlpha (0.13f));
        g.fillRect (keyRect);

        if (isRoot)
        {
            g.setColour (t.silkscreen.withAlpha (0.85f));
            g.setFont (Replicant303::uiFont (7.0f, juce::Font::bold));
            g.drawText (Replicant303::PatternSequencer::getNoteName (note),
                        keyRect, juce::Justification::centredRight);
        }
        else if (note % 12 == 5) // F rows get a subtle marker
        {
            g.setColour (t.silkscreen.withAlpha (0.25f));
            g.setFont (Replicant303::uiFont (6.0f, juce::Font::plain));
            g.drawText ("F", keyRect, juce::Justification::centredRight);
        }
    }

    // Beat divisions every four steps.
    for (int beat = 0; beat <= 4; ++beat)
    {
        const float x = gridArea.getX() + beat * colW * 4.0f;
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawVerticalLine ((int) x, gridArea.getY(), gridArea.getBottom());
        g.setColour (t.silkscreen.withAlpha (0.10f));
        g.drawVerticalLine ((int) x + 1, gridArea.getY(), gridArea.getBottom());
    }

    // Steps outside the pattern length are dimmed.
    if (length < 16)
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRect (gridArea.getX() + length * colW, gridArea.getY(),
                    (16 - length) * colW, gridArea.getHeight());
    }

    // Playing column.
    if (running)
    {
        g.setColour (t.ledBright.withAlpha (0.08f));
        g.fillRect (gridArea.getX() + playingStep * colW, gridArea.getY(), colW, gridArea.getHeight());

        g.setColour (t.ledBright);
        juce::Path marker;
        marker.addTriangle (gridArea.getX() + playingStep * colW + colW * 0.5f - 5.0f,
                            gridArea.getY() - 7.0f,
                            gridArea.getX() + playingStep * colW + colW * 0.5f + 5.0f,
                            gridArea.getY() - 7.0f,
                            gridArea.getX() + playingStep * colW + colW * 0.5f,
                            gridArea.getY() - 1.0f);
        g.fillPath (marker);
    }

    // Note bars.
    for (int step = 0; step < 16; ++step)
    {
        const auto& s = pattern[step];
        const int row = kTopNote - juce::jlimit (24, 48, s.note);
        const float x = gridArea.getX() + step * colW;
        const float y = gridArea.getY() + row * rowH;

        if (step >= length)
            continue;

        const juce::Colour noteColour = s.accent ? t.displayFilament : t.ledBright;
        const float alpha = s.gate ? 0.92f : 0.22f;
        auto bar = juce::Rectangle<float> (x + 1.5f, y + 1.2f, colW - 3.0f, rowH - 2.4f);

        if (s.gate)
        {
            g.setColour (noteColour.withAlpha (0.20f));
            g.fillRoundedRectangle (bar.expanded (1.5f, 1.5f), 2.0f);
        }

        g.setColour (noteColour.withAlpha (alpha));
        g.fillRoundedRectangle (bar, 1.8f);

        if (s.gate)
        {
            g.setColour (juce::Colours::white.withAlpha (s.accent ? 0.35f : 0.18f));
            g.fillRoundedRectangle (bar.withHeight (1.2f).reduced (1.0f, 0.0f), 0.6f);
        }
    }

    // Selected step outline.
    g.setColour (t.silkscreen.withAlpha (0.65f));
    g.drawRoundedRectangle ({ gridArea.getX() + selectedStep * colW + 1.0f, gridArea.getY() + 1.0f,
                              colW - 2.0f, gridArea.getHeight() - 2.0f }, 2.0f, 1.2f);
}

void PatternDrawer::drawLanes (juce::Graphics& g)
{
    const auto& t = theme();
    const auto pattern = sequencer.getPattern (sequencer.getCurrentPattern());
    const int length = pattern.length;
    const float colW = gridArea.getWidth() / 16.0f;

    struct LaneDef
    {
        const char* label;
        juce::Rectangle<float> area;
        int flag;
    };

    LaneDef lanes[] =
    {
        { "GATE",   gateLane,   0 },
        { "ACCENT", accentLane, 1 },
        { "SLIDE",  slideLane,  2 }
    };

    for (const auto& lane : lanes)
    {
        g.setColour (t.silkscreen.withAlpha (0.55f));
        g.setFont (Replicant303::uiFont (7.5f, juce::Font::bold));
        g.drawText (lane.label, 8.0f, lane.area.getY(), 50.0f, lane.area.getHeight(),
                    juce::Justification::centredRight);

        for (int step = 0; step < 16; ++step)
        {
            auto cell = juce::Rectangle<float> (lane.area.getX() + step * colW + 1.5f,
                                                lane.area.getY() + 2.0f,
                                                colW - 3.0f, lane.area.getHeight() - 4.0f);
            const auto& s = pattern[step];
            const bool beyond = step >= length;

            bool on = false;
            juce::Colour onColour = t.ledBright;

            if (lane.flag == 0)      { on = s.gate;   onColour = t.ledBright; }
            else if (lane.flag == 1) { on = s.accent; onColour = t.displayLit; }
            else                     { on = s.slide;  onColour = t.silkscreen; }

            g.setColour (juce::Colours::black.withAlpha (beyond ? 0.55f : 0.45f));
            g.fillRoundedRectangle (cell, 2.0f);
            g.setColour (t.silkscreen.withAlpha (beyond ? 0.08f : 0.18f));
            g.drawRoundedRectangle (cell, 2.0f, 1.0f);

            if (beyond)
                continue;

            if (lane.flag == 0)
            {
                if (on)
                {
                    g.setColour (onColour.withAlpha (0.25f));
                    g.fillRoundedRectangle (cell.expanded (1.0f), 2.0f);
                    g.setColour (onColour);
                    g.fillRoundedRectangle (cell.reduced (2.0f, 3.0f), 1.5f);
                }
            }
            else if (lane.flag == 1)
            {
                juce::Path triangle;
                triangle.addTriangle (cell.getCentreX(), cell.getY() + 3.0f,
                                      cell.getCentreX() - 6.0f, cell.getBottom() - 3.0f,
                                      cell.getCentreX() + 6.0f, cell.getBottom() - 3.0f);
                if (on)
                {
                    g.setColour (onColour.withAlpha (0.28f));
                    g.fillEllipse (cell.getCentreX() - 9.0f, cell.getCentreY() - 9.0f, 18.0f, 18.0f);
                    g.setColour (onColour);
                    g.fillPath (triangle);
                }
                else
                {
                    g.setColour (t.silkscreen.withAlpha (0.22f));
                    g.strokePath (triangle, juce::PathStrokeType (1.0f));
                }
            }
            else
            {
                auto bar = cell.reduced (4.0f, 6.0f).withHeight (4.0f)
                               .withCentre (cell.getCentre());
                if (on)
                {
                    g.setColour (onColour.withAlpha (0.85f));
                    g.fillRoundedRectangle (bar, 2.0f);
                    juce::Path arrow;
                    arrow.addTriangle (bar.getRight(), bar.getCentreY() - 4.0f,
                                       bar.getRight() + 7.0f, bar.getCentreY(),
                                       bar.getRight(), bar.getCentreY() + 4.0f);
                    g.fillPath (arrow);
                }
                else
                {
                    g.setColour (t.silkscreen.withAlpha (0.18f));
                    g.drawRoundedRectangle (bar, 2.0f, 1.0f);
                }
            }

            if (step == selectedStep)
            {
                g.setColour (t.silkscreen.withAlpha (0.55f));
                g.drawRoundedRectangle (cell.expanded (0.5f), 2.0f, 1.0f);
            }
        }
    }
}

void PatternDrawer::drawInspector (juce::Graphics& g)
{
    const auto& t = theme();
    const auto pattern = sequencer.getPattern (sequencer.getCurrentPattern());
    const auto step = pattern[selectedStep];

    g.setColour (juce::Colours::black.withAlpha (0.38f));
    g.fillRoundedRectangle (inspectorArea, 4.0f);
    g.setColour (t.silkscreen.withAlpha (0.22f));
    g.drawRoundedRectangle (inspectorArea, 4.0f, 1.0f);

    g.setColour (t.silkscreen.withAlpha (0.55f));
    g.setFont (Replicant303::uiFont (7.5f, juce::Font::bold));
    g.drawText ("STEP", inspectorArea.getX() + 10.0f, 84.0f, 40.0f, 10.0f,
                juce::Justification::centredLeft);

    g.setColour (t.ledBright);
    g.setFont (Replicant303::uiFont (16.0f, juce::Font::bold));
    g.drawText (juce::String (selectedStep + 1).paddedLeft ('0', 2) + " / 16",
                inspectorArea.getX() + 44.0f, 80.0f, 136.0f, 22.0f,
                juce::Justification::centredLeft);

    g.setColour (t.silkscreen.withAlpha (0.55f));
    g.setFont (Replicant303::uiFont (7.5f, juce::Font::bold));
    g.drawText ("NOTE", inspectorArea.getX() + 10.0f, 104.0f, 60.0f, 10.0f,
                juce::Justification::centredLeft);

    g.setColour (step.gate ? t.ledBright : t.silkscreen.withAlpha (0.45f));
    g.setFont (Replicant303::uiFont (30.0f, juce::Font::bold));
    g.drawText (Replicant303::PatternSequencer::getNoteName (step.note),
                inspectorArea.getX() + 8.0f, 114.0f, 120.0f, 36.0f,
                juce::Justification::centredLeft);

    g.setColour (t.silkscreen.withAlpha (0.45f));
    g.setFont (Replicant303::uiFont (9.0f, juce::Font::plain));
    g.drawText ("MIDI " + juce::String (step.note),
                inspectorArea.getX() + 112.0f, 126.0f, 68.0f, 16.0f,
                juce::Justification::centredRight);

    drawTactileButton (g, noteMinus12, "-12", false, true);
    drawTactileButton (g, notePlus12,  "+12", false, true);
    drawTactileButton (g, noteMinus1,  "-1",  false, true);
    drawTactileButton (g, notePlus1,   "+1",  false, true);

    drawTactileButton (g, inspGate,   "GATE",   step.gate,   true);
    drawTactileButton (g, inspAccent, "ACCENT", step.accent, true);
    drawTactileButton (g, inspSlide,  "SLIDE",  step.slide,  true);

    g.setColour (t.silkscreen.withAlpha (0.32f));
    g.setFont (Replicant303::uiFont (7.5f, juce::Font::plain));
    g.drawFittedText ("DRAG THE GRID TO PAINT NOTES  /  RIGHT-CLICK MUTES",
                      juce::Rectangle<int> ((int) (inspectorArea.getX() + 8.0f), 330,
                                            (int) (inspectorArea.getWidth() - 16.0f), 18),
                      juce::Justification::centred, 2, 0.9f);
}

void PatternDrawer::drawActions (juce::Graphics& g)
{
    for (int i = 0; i < kNumActions; ++i)
        drawTactileButton (g, actionButtons[(size_t) i], kActionLabels[i],
                           i == 2 && hasClipboard, true);
}

void PatternDrawer::drawPresetVault (juce::Graphics& g)
{
    const auto& t = theme();
    const int bank = currentBank();
    const int index = currentPresetIndex();
    const int loadedIndex = processor.getPresetBank().getSelectedSlot (bank);
    const bool selectionLoaded = (index == loadedIndex);
    const auto& preset = processor.getPresetBank().getPreset (bank, index);

    // Recessed smoked-acrylic strip under the pattern actions.
    g.setColour (juce::Colours::black.withAlpha (0.30f));
    g.fillRoundedRectangle (presetArea, 4.0f);
    g.setColour (t.silkscreen.withAlpha (0.12f));
    g.drawRoundedRectangle (presetArea, 4.0f, 1.0f);

    g.setColour (t.silkscreen.withAlpha (0.55f));
    g.setFont (Replicant303::uiFont (7.0f, juce::Font::bold));
    g.drawText ("PRESET", 12, 396, 46, 16, juce::Justification::centredLeft);

    g.setColour (t.ledBright.withAlpha (0.90f));
    g.setFont (Replicant303::uiFont (7.0f, juce::Font::bold));
    g.drawText ("BANK " + Replicant303::PresetBank::getBankLetter (bank),
                12, 408, 46, 16, juce::Justification::centredLeft);

    g.setColour (t.silkscreen.withAlpha (0.55f));
    g.setFont (Replicant303::uiFont (7.5f, juce::Font::bold));
    g.drawText ("PAGE", 64, 396, 30, 32, juce::Justification::centredLeft);

    for (int i = 0; i < 8; ++i)
    {
        drawPresetKey (g, presetPageButtons[(size_t) i],
                       juce::String (i + 1), i == presetPage);
        drawPresetKey (g, presetSlotButtons[(size_t) i],
                       juce::String (i + 1).paddedLeft ('0', 2),
                       selectionLoaded && i == presetSlot);
    }

    // Name / status window.
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (presetNameArea, 3.0f);
    g.setColour (t.silkscreen.withAlpha (0.25f));
    g.drawRoundedRectangle (presetNameArea, 3.0f, 1.0f);

    const auto textArea = presetNameArea.reduced (8.0f, 0.0f).toNearestInt();

    if (presetStatus.isNotEmpty())
    {
        g.setColour (t.displayLit);
        g.setFont (Replicant303::uiFont (9.0f, juce::Font::bold));
        g.drawFittedText (presetStatus, textArea, juce::Justification::centredLeft, 1, 0.82f);
    }
    else if (! selectionLoaded)
    {
        // Changing page browses without loading: the user has not committed to
        // a slot yet, so the window asks for one instead of lying about it.
        g.setColour (t.silkscreen.withAlpha (0.70f));
        g.setFont (Replicant303::uiFont (9.0f, juce::Font::bold));
        g.drawFittedText ("PAGE " + juce::String (presetPage + 1)
                              + "  /  SELECT 01-08",
                          textArea, juce::Justification::centredLeft, 1, 0.82f);
    }
    else
    {
        const auto label = Replicant303::PresetBank::getBankLetter (bank)
                           + " " + juce::String (index + 1).paddedLeft ('0', 2)
                           + "  " + preset.name;

        g.setColour (t.ledBright);
        g.setFont (Replicant303::uiFont (10.0f, juce::Font::bold));
        g.drawFittedText (label, textArea, juce::Justification::centredLeft, 1, 0.82f);
    }

    drawTactileButton (g, presetGenButton,    "GEN",    false, true);
    drawTactileButton (g, presetStoreButton,  "STORE",  false, true);
    drawTactileButton (g, presetImportButton, "IMPORT", false, true);
    drawTactileButton (g, presetExportButton, "EXPORT", false, true);
}

void PatternDrawer::drawPresetKey (juce::Graphics& g, juce::Rectangle<float> r,
                                   const juce::String& text, bool active)
{
    const auto& t = theme();

    g.setColour (juce::Colours::black.withAlpha (0.50f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f), 2.0f);

    const juce::Colour face = active ? t.buttonFaceLit : t.buttonFaceIdle.darker (0.68f);
    juce::ColourGradient grad (face.brighter (0.10f), r.getX(), r.getY(),
                               face.darker (0.42f), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 2.0f);

    g.setColour (active ? t.ledBright : t.silkscreen.withAlpha (0.28f));
    g.drawRoundedRectangle (r, 2.0f, active ? 1.4f : 1.0f);

    g.setColour (active ? juce::Colours::black.withAlpha (0.88f)
                        : t.silkscreen.withAlpha (0.80f));
    g.setFont (Replicant303::uiFont (8.5f, juce::Font::bold));
    g.drawText (text, r, juce::Justification::centred);
}

//==============================================================================
void PatternDrawer::drawTactileButton (juce::Graphics& g, juce::Rectangle<float> r,
                                       const juce::String& text, bool lit, bool compact)
{
    const auto& t = theme();

    g.setColour (juce::Colours::black.withAlpha (0.50f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 2.5f);

    const juce::Colour face = lit ? t.buttonFaceLit : t.buttonFaceIdle.darker (0.55f);
    juce::ColourGradient grad (face.brighter (0.12f), r.getX(), r.getY(),
                               face.darker (0.42f), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 2.5f);
    g.setColour (juce::Colours::black.withAlpha (0.62f));
    g.drawRoundedRectangle (r, 2.5f, 1.0f);

    const float ledSize = compact ? 5.0f : 6.0f;
    auto ledRect = juce::Rectangle<float> (r.getX() + 7.0f,
                                           r.getCentreY() - ledSize * 0.5f,
                                           ledSize, ledSize);
    drawLed (g, ledRect, lit, t.ledBright);

    g.setColour (lit ? juce::Colours::black.withAlpha (0.85f) : t.silkscreen);
    g.setFont (Replicant303::uiFont (compact ? 9.5f : 8.5f, juce::Font::bold));
    g.drawText (text, r.withTrimmedLeft (18.0f), juce::Justification::centredLeft);
}

void PatternDrawer::drawLed (juce::Graphics& g, juce::Rectangle<float> centre,
                             bool lit, juce::Colour litColour) const
{
    g.setColour (theme().ledBezelDark);
    g.drawEllipse (centre.expanded (1.0f), 1.0f);

    if (lit)
    {
        g.setColour (litColour.withAlpha (0.35f));
        g.fillEllipse (centre.expanded (4.0f));
        g.setColour (litColour);
        g.fillEllipse (centre);
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.fillEllipse (centre.reduced (1.5f).translated (-0.4f, -0.4f));
    }
    else
    {
        g.setColour (theme().ledDark);
        g.fillEllipse (centre);
    }
}

//==============================================================================
int PatternDrawer::stepAt (juce::Point<float> p) const
{
    const float colW = gridArea.getWidth() / 16.0f;
    return juce::jlimit (0, 15, (int) std::floor ((p.x - gridArea.getX()) / colW));
}

int PatternDrawer::noteAt (juce::Point<float> p) const
{
    const float rowH = gridArea.getHeight() / (float) kVisibleRows;
    const int row = juce::jlimit (0, kVisibleRows - 1, (int) std::floor ((p.y - gridArea.getY()) / rowH));
    return kTopNote - row;
}

bool PatternDrawer::gridContains (juce::Point<float> p) const
{
    return gridArea.contains (p);
}

//==============================================================================
void PatternDrawer::mouseDown (const juce::MouseEvent& e)
{
    auto p = e.position;
    p.y -= revealOffset;
    const int pattern = sequencer.getCurrentPattern();

    if (closeButton.contains (p))
    {
        if (onCloseRequested)
            onCloseRequested();
        return;
    }

    if (playButton.contains (p))       { startOrStop(); return; }
    if (syncButton.contains (p))
    {
        sequencer.setSyncMode (sequencer.getSyncMode() == Replicant303::PatternSequencer::SyncMode::Host
                                   ? Replicant303::PatternSequencer::SyncMode::Internal
                                   : Replicant303::PatternSequencer::SyncMode::Host);
        repaint();
        return;
    }

    for (int i = 0; i < 8; ++i)
    {
        if (slotButtons[(size_t) i].contains (p))
        {
            choosePattern (i);
            return;
        }
    }

    if (lengthMinus.contains (p))
    {
        sequencer.setLength (pattern, sequencer.getLength (pattern) - 1);
        repaint();
        return;
    }

    if (lengthPlus.contains (p))
    {
        sequencer.setLength (pattern, sequencer.getLength (pattern) + 1);
        repaint();
        return;
    }

    if (swingArea.contains (p))
    {
        draggedLane = 3;
        const float norm = juce::jlimit (0.0f, 1.0f,
                                         (p.x - swingTrack.getX()) / swingTrack.getWidth());
        sequencer.setSwing (norm * 0.66f);
        repaint();
        return;
    }

    if (gridContains (p))
    {
        const int step = stepAt (p);
        selectedStep = step;

        if (e.mods.isPopupMenu())
            toggleGate (step);
        else
        {
            setNoteAt (step, noteAt (p));
            draggingGrid = true;
        }

        repaint();
        return;
    }

    if (gateLane.contains (p))   { selectedStep = stepAt (p); toggleFlag (selectedStep, 0); repaint(); return; }
    if (accentLane.contains (p)) { selectedStep = stepAt (p); toggleFlag (selectedStep, 1); repaint(); return; }
    if (slideLane.contains (p))  { selectedStep = stepAt (p); toggleFlag (selectedStep, 2); repaint(); return; }

    if (noteMinus12.contains (p) || notePlus12.contains (p)
        || noteMinus1.contains (p) || notePlus1.contains (p))
    {
        int delta = 0;
        if (noteMinus12.contains (p)) delta = -12;
        if (notePlus12.contains (p))  delta = 12;
        if (noteMinus1.contains (p))  delta = -1;
        if (notePlus1.contains (p))   delta = 1;

        sequencer.updateStep (pattern, selectedStep,
                              [delta] (Replicant303::Step& s)
                              {
                                  s.note = juce::jlimit (Replicant303::Pattern::lowestNote,
                                                         Replicant303::Pattern::highestNote,
                                                         s.note + delta);
                              });
        repaint();
        return;
    }

    if (inspGate.contains (p))   { toggleFlag (selectedStep, 0); repaint(); return; }
    if (inspAccent.contains (p)) { toggleFlag (selectedStep, 1); repaint(); return; }
    if (inspSlide.contains (p))  { toggleFlag (selectedStep, 2); repaint(); return; }

    // Preset vault row.
    for (int i = 0; i < 8; ++i)
    {
        if (presetPageButtons[(size_t) i].contains (p))
        {
            choosePresetPage (i);
            return;
        }

        if (presetSlotButtons[(size_t) i].contains (p))
        {
            loadPreset (presetPage * 8 + i);
            return;
        }
    }

    if (presetGenButton.contains (p))    { generatePreset(); return; }
    if (presetStoreButton.contains (p))  { storePreset(); return; }
    if (presetImportButton.contains (p)) { importPreset(); return; }
    if (presetExportButton.contains (p)) { exportPreset(); return; }

    for (int i = 0; i < kNumActions; ++i)
    {
        if (! actionButtons[(size_t) i].contains (p))
            continue;

        switch (i)
        {
            case 0: sequencer.clearPattern (pattern); break;
            case 1: clipboard = sequencer.getPattern (pattern); hasClipboard = true; break;
            case 2: if (hasClipboard) sequencer.setPattern (pattern, clipboard); break;
            case 3: randomizeCurrentPattern(); break;
            case 4: sequencer.initPattern (pattern); break;
            case 5: exportCurrentPattern(); break;
            default: break;
        }

        repaint();
        return;
    }
}

void PatternDrawer::mouseDrag (const juce::MouseEvent& e)
{
    auto p = e.position;
    p.y -= revealOffset;

    if (draggedLane == 3)
    {
        const float norm = juce::jlimit (0.0f, 1.0f,
                                         (p.x - swingTrack.getX()) / swingTrack.getWidth());
        sequencer.setSwing (norm * 0.66f);
        repaint();
        return;
    }

    if (draggingGrid && gridContains (p))
    {
        const int step = stepAt (p);
        selectedStep = step;
        setNoteAt (step, noteAt (p));
        repaint();
    }
}

void PatternDrawer::mouseUp (const juce::MouseEvent&)
{
    draggingGrid = false;
    draggedLane = -1;
}

//==============================================================================
void PatternDrawer::setNoteAt (int step, int note)
{
    const int pattern = sequencer.getCurrentPattern();
    sequencer.updateStep (pattern, step,
                          [note] (Replicant303::Step& s)
                          {
                              s.note = note;
                              s.gate = true;
                          });
}

void PatternDrawer::toggleGate (int step)
{
    sequencer.updateStep (sequencer.getCurrentPattern(), step,
                          [] (Replicant303::Step& s) { s.gate = ! s.gate; });
}

void PatternDrawer::toggleFlag (int step, int flag)
{
    sequencer.updateStep (sequencer.getCurrentPattern(), step,
                          [flag] (Replicant303::Step& s)
                          {
                              if (flag == 0)      s.gate = ! s.gate;
                              else if (flag == 1) s.accent = ! s.accent;
                              else                s.slide = ! s.slide;
                          });
}

void PatternDrawer::startOrStop()
{
    // Toggle on the request, not on the running state: in host sync the
    // transport is armed while the DAW is stopped, and PLAY must still be
    // pressable to change your mind.
    if (sequencer.isTransportArmed())
        sequencer.stop();
    else
        sequencer.start();

    repaint();
}

void PatternDrawer::choosePattern (int index)
{
    sequencer.setCurrentPattern (index);
    selectedStep = 0;
    repaint();
}

void PatternDrawer::randomizeCurrentPattern()
{
    juce::Random rng (juce::Time::getMillisecondCounter()
                      ^ (juce::int64) juce::Time::getHighResolutionTicks());

    const int scale[] = { 0, 3, 5, 7, 10 };   // C minor pentatonic
    const int root = 36;

    Replicant303::Pattern p;
    p.length = 16;

    for (int i = 0; i < Replicant303::Pattern::maxSteps; ++i)
    {
        auto& s = p.steps[(size_t) i];
        s.gate = rng.nextFloat() < 0.82f;
        s.accent = rng.nextFloat() < 0.28f;
        s.slide = rng.nextFloat() < 0.22f;

        const int degree = rng.nextInt (5);
        const int octave = rng.nextFloat() < 0.18f ? 12 : 0;
        s.note = juce::jlimit (Replicant303::Pattern::lowestNote,
                               Replicant303::Pattern::highestNote,
                               root + scale[degree] + octave);
    }

    // A random line should always open with a sounding note.
    p[0].gate = true;

    sequencer.setPattern (sequencer.getCurrentPattern(), p);
    selectedStep = 0;
}

void PatternDrawer::exportCurrentPattern()
{
    const int pattern = sequencer.getCurrentPattern();

    fileChooser = std::make_unique<juce::FileChooser> (
        "Export pattern " + juce::String::charToString (kPatternLetters[pattern]) + " as MIDI",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("Replicant303_pattern_" + juce::String (pattern + 1) + ".midi"),
        "*.midi;*.mid");

    auto safeThis = juce::Component::SafePointer<PatternDrawer> (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safeThis, pattern] (const juce::FileChooser& chooser)
                              {
                                  if (safeThis == nullptr)
                                      return;

                                  const auto file = chooser.getResult();
                                  if (file.getFullPathName().isEmpty())
                                      return;

                                  const bool ok = safeThis->processor.exportPatternToMidi (file, pattern);
                                  juce::ignoreUnused (ok);
                              });
}

//==============================================================================
void PatternDrawer::syncPresetSelection()
{
    const int bank = currentBank();
    const int index = processor.getPresetBank().getSelectedSlot (bank);

    presetPage = index / 8;
    presetSlot = index % 8;
    repaint();
}

//==============================================================================
void PatternDrawer::choosePresetPage (int page)
{
    presetPage = juce::jlimit (0, 7, page);
    repaint();
}

void PatternDrawer::loadPreset (int index)
{
    const int bank = currentBank();
    const int safeIndex = juce::jlimit (0, Replicant303::PresetBank::presetsPerBank - 1, index);

    presetPage = safeIndex / 8;
    presetSlot = safeIndex % 8;

    processor.loadPreset (bank, safeIndex);
    selectedStep = 0;
    repaint();
}

void PatternDrawer::generatePreset()
{
    const int bank = currentBank();
    const int index = currentPresetIndex();
    const auto seed = (juce::uint32) (juce::Time::getHighResolutionTicks()
                                      ^ (juce::int64) (juce::Time::getMillisecondCounter()
                                                       * 2654435761u));

    processor.generatePreset (bank, index, seed);

    presetPage = index / 8;
    presetSlot = index % 8;

    showPresetStatus ("GENERATED "
                      + Replicant303::PresetBank::getBankLetter (bank) + " "
                      + juce::String (index + 1).paddedLeft ('0', 2));
}

void PatternDrawer::storePreset()
{
    const int bank = currentBank();
    const int index = currentPresetIndex();
    const auto name = processor.getPresetBank().getPreset (bank, index).name;

    processor.storeCurrentPreset (bank, index, name);

    showPresetStatus ("STORED "
                      + Replicant303::PresetBank::getBankLetter (bank) + " "
                      + juce::String (index + 1).paddedLeft ('0', 2));
}

void PatternDrawer::importPreset()
{
    const int bank = currentBank();
    const int index = currentPresetIndex();

    fileChooser = std::make_unique<juce::FileChooser> (
        "Import Replicant303 preset (" + Replicant303::PresetBank::getBankLetter (bank)
            + " " + juce::String (index + 1).paddedLeft ('0', 2) + ")",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.midi;*.mid");

    auto safeThis = juce::Component::SafePointer<PatternDrawer> (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis, bank, index] (const juce::FileChooser& chooser)
                              {
                                  if (safeThis == nullptr)
                                      return;

                                  const auto file = chooser.getResult();

                                  if (file.getFullPathName().isEmpty())
                                      return;

                                  const bool ok = safeThis->processor.importPresetFromMidi (file, bank, index);

                                  if (ok)
                                  {
                                      safeThis->presetPage = index / 8;
                                      safeThis->presetSlot = index % 8;
                                  }

                                  safeThis->showPresetStatus (
                                      ok ? "IMPORTED " + file.getFileName()
                                         : "IMPORT FAILED",
                                      3000);
                              });
}

void PatternDrawer::exportPreset()
{
    const int bank = currentBank();
    const int index = currentPresetIndex();
    const auto& preset = processor.getPresetBank().getPreset (bank, index);

    auto safeName = preset.name.replaceCharacter (' ', '_');
    safeName = safeName.retainCharacters ("abcdefghijklmnopqrstuvwxyz"
                                          "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                          "0123456789_-");

    if (safeName.isEmpty())
        safeName = "Preset";

    const auto defaultFile = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Replicant303_"
                       + Replicant303::PresetBank::getBankLetter (bank)
                       + juce::String (index + 1).paddedLeft ('0', 2)
                       + "_" + safeName + ".midi");

    fileChooser = std::make_unique<juce::FileChooser> (
        "Export preset as .midi", defaultFile, "*.midi;*.mid");

    auto safeThis = juce::Component::SafePointer<PatternDrawer> (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safeThis, bank, index] (const juce::FileChooser& chooser)
                              {
                                  if (safeThis == nullptr)
                                      return;

                                  auto file = chooser.getResult();

                                  if (file.getFullPathName().isEmpty())
                                      return;

                                  if (file.getFileExtension().isEmpty())
                                      file = file.getSiblingFile (file.getFileNameWithoutExtension()
                                                                  + ".midi");

                                  const bool ok = safeThis->processor.exportPresetToMidi (file, bank, index);

                                  safeThis->showPresetStatus (
                                      ok ? "EXPORTED " + file.getFileName()
                                         : "EXPORT FAILED",
                                      3000);
                              });
}

void PatternDrawer::showPresetStatus (const juce::String& text, int milliseconds)
{
    presetStatus = text;
    presetStatusUntilMs = juce::Time::getMillisecondCounterHiRes() + (double) milliseconds;
    repaint();
}
