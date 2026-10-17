#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "Replicant303Theme.h"

//==============================================================================
/** Slide-down pattern drawer.

    A Phoscyon 2 style step editor that lives underneath the main panel: eight
    patterns of sixteen steps, a two-octave piano-roll grid, per-step gate /
    accent / slide lanes, swing, pattern length, host-or-internal sync and MIDI
    export. It edits the PatternSequencer model directly and is drawn entirely
    with the active skin palette.
*/
class PatternDrawer : public juce::Component
{
public:
    explicit PatternDrawer (Replicant303AudioProcessor&);
    ~PatternDrawer() override = default;

    static constexpr int drawerHeight = 400;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    /** Called from the editor timer: repaints only when the clock moved. */
    void refresh();

    /** Slides the whole drawer content down by `pixels`, so the editor can
        animate the reveal without resizing the plugin window every frame. */
    void setRevealOffset (float pixels);

    /** Raised by the CLOSE handle in the drawer header. */
    std::function<void()> onCloseRequested;

private:
    //==========================================================================
    static const Replicant303::Theme& theme() noexcept
    {
        return Replicant303::ThemeManager::getInstance().getCurrentTheme();
    }

    void drawBackground (juce::Graphics&);
    void drawHandle (juce::Graphics&);
    void drawTransport (juce::Graphics&);
    void drawGrid (juce::Graphics&);
    void drawLanes (juce::Graphics&);
    void drawInspector (juce::Graphics&);
    void drawActions (juce::Graphics&);

    void drawTactileButton (juce::Graphics&, juce::Rectangle<float>,
                            const juce::String& text, bool lit, bool compact = false);
    void drawLed (juce::Graphics&, juce::Rectangle<float> centre, bool lit,
                  juce::Colour litColour) const;

    float revealOffset = 0.0f;

    int stepAt (juce::Point<float>) const;
    int noteAt (juce::Point<float>) const;
    bool gridContains (juce::Point<float>) const;

    void setNoteAt (int step, int note);
    void toggleGate (int step);
    void toggleFlag (int step, int flag); // 0 gate, 1 accent, 2 slide
    void startOrStop();
    void choosePattern (int index);
    void randomizeCurrentPattern();
    void exportCurrentPattern();

    Replicant303AudioProcessor& processor;
    Replicant303::PatternSequencer& sequencer;

    int selectedStep = 0;
    int draggedLane = -1;
    bool draggingGrid = false;

    Replicant303::Pattern clipboard;
    bool hasClipboard = false;

    std::unique_ptr<juce::FileChooser> fileChooser;

    // Last clock state, used to avoid pointless repaints.
    int   lastStep = -1;
    int   lastPattern = -1;
    bool  lastRunning = false;
    float lastTempo = -1.0f;

    //==========================================================================
    juce::Rectangle<float> handleArea, headerArea, gridArea, inspectorArea;
    juce::Rectangle<float> gateLane, accentLane, slideLane, actionArea;
    juce::Rectangle<float> playButton, syncButton, closeButton;
    juce::Rectangle<float> lengthMinus, lengthValue, lengthPlus, swingArea, swingTrack;
    std::array<juce::Rectangle<float>, 8> slotButtons;
    std::array<juce::Rectangle<float>, 6> actionButtons;

    juce::Rectangle<float> noteMinus12, notePlus12, noteMinus1, notePlus1;
    juce::Rectangle<float> inspGate, inspAccent, inspSlide;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternDrawer)
};
