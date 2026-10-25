#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "GUI/Replicant303LookAndFeel.h"
#include "GUI/Replicant303Theme.h"
#include "GUI/PanelLayout.h"
#include "GUI/PatternDrawer.h"
#include "GUI/WaveformSelector.h"

class Replicant303AudioProcessorEditor : public juce::AudioProcessorEditor,
                                         private juce::Timer,
                                         private juce::ChangeListener
{
public:
    explicit Replicant303AudioProcessorEditor(Replicant303AudioProcessor&);
    ~Replicant303AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;

    /** Opens or closes the slide-down pattern editor. `animate = false` is
        used by the offline snapshot tool. */
    void setPatternDrawerOpen(bool shouldBeOpen, bool animate = true);
    bool isPatternDrawerOpen() const noexcept { return drawerOpen; }

    /** Adjusts the UI zoom factor (clamped to 0.50 .. 3.00, i.e. 50% .. 300%). */
    void setZoomFactor(float newZoom);
    float getZoomFactor() const noexcept { return zoomFactor; }

    /** Opens or closes the slide-down waveform bank. Used by the scope and by
        the offline snapshot tool. */
    void setWaveformMenuOpen(bool shouldBeOpen);
    bool isWaveformMenuOpen() const noexcept { return waveformMenuOpen; }

    /** Selects a shape by bank index (clamped) and updates the parameter. */
    void selectWaveformIndex(int index);

private:
    class PanelCanvas : public juce::Component
    {
    public:
        explicit PanelCanvas(Replicant303AudioProcessorEditor& o) : owner(o)
        {
            setInterceptsMouseClicks(true, true);
        }

        void paint(juce::Graphics& g) override { owner.paintCanvas(g); }
        void mouseDown(const juce::MouseEvent& e) override { owner.canvasMouseDown(e); }
        void mouseUp(const juce::MouseEvent& e) override { owner.canvasMouseUp(e); }
        void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
        {
            owner.canvasMouseWheelMove(e, wheel);
        }

    private:
        Replicant303AudioProcessorEditor& owner;
    };

    void paintCanvas(juce::Graphics& g);
    void canvasMouseDown(const juce::MouseEvent& e);
    void canvasMouseUp(const juce::MouseEvent& e);
    void canvasMouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel);

    void timerCallback() override;

    /** Fired by the ThemeManager whenever the user picks another panel skin. */
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    /** Palette currently in force (all procedural drawing reads from here). */
    static const Replicant303::Theme& theme() noexcept
    {
        return Replicant303::ThemeManager::getInstance().getCurrentTheme();
    }

    Replicant303AudioProcessor& audioProcessor;
    Replicant303::Replicant303LookAndFeel customLookAndFeel;
    PanelCanvas contentCanvas { *this };

    // -------------------------------------------------------------------------
    // Sliders & Attachments
    // -------------------------------------------------------------------------
    struct ControlKnob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    // Upper Synth Row
    ControlKnob tuneKnob;
    ControlKnob cutoffKnob;
    ControlKnob resonanceKnob;
    ControlKnob envModKnob;
    ControlKnob decayKnob;
    ControlKnob accentKnob;

    // Upper Distortion Row
    ControlKnob distDriveKnob;
    ControlKnob distToneKnob;
    ControlKnob distMixKnob;

    // Middle Row Large Knobs & Selectors
    ControlKnob tempoKnob;     // Visual/MIDI clock reference
    ControlKnob volumeKnob;    // Master volume
    ControlKnob trackKnob;     // Track / Pattern Group rotary switch
    ControlKnob distTypeKnob;  // Distortion Model rotary switch

    // -------------------------------------------------------------------------
    // Switches & Selectors
    // -------------------------------------------------------------------------
    juce::ToggleButton distActiveSwitch; // Distortion Off / On slide switch
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> distActiveAttachment;

    juce::ComboBox distModelBox; // 5-model distortion selector
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> distModelAttachment;

    juce::ComboBox themeBox;     // Panel skin selector (Green / Black-Red / ...)
    std::unique_ptr<juce::ParameterAttachment> themeAttachment;

    // -------------------------------------------------------------------------
    // Waveform bank: scope + slide-down menu
    // -------------------------------------------------------------------------
    Replicant303::WaveformSelector waveformSelector;
    std::unique_ptr<Replicant303::WaveformMenu> waveformMenu;
    std::unique_ptr<juce::ParameterAttachment> waveformAttachment;
    bool waveformMenuOpen = false;

    struct ClickWatcher : public juce::MouseListener
    {
        explicit ClickWatcher(Replicant303AudioProcessorEditor& o) : owner(o) {}
        void mouseDown(const juce::MouseEvent& e) override { owner.globalMouseDown(e); }

        Replicant303AudioProcessorEditor& owner;
    };
    ClickWatcher clickWatcher { *this };

    void toggleWaveformMenu();
    void globalMouseDown(const juce::MouseEvent& e);

    juce::ComboBox zoomBox;      // UI Zoom selector (50% - 300%)
    std::unique_ptr<juce::ParameterAttachment> zoomAttachment;
    float zoomFactor = Replicant303::Layout::defaultZoomFactor;
    bool isApplyingProgrammaticSize = false;
    bool ignoreZoomAttachmentCallback = false;

    void syncZoomBoxText();
    int getUnscaledHeight() const noexcept;
    void applyWindowSizeForZoom();
    void updateCanvasTransform();
    void layoutCanvasChildren();

    // -------------------------------------------------------------------------
    // Interactive 13-Key Keyboard & Function Buttons (Bottom Section)
    // -------------------------------------------------------------------------
    struct PianoKey
    {
        int midiNote;
        juce::String name;
        bool isBlack;
        juce::Rectangle<float> bounds;
        juce::Point<float> ledPos;
        bool isPressed = false;
        juce::String subLabel = "";
        juce::String stepLabel = "";
    };
    std::vector<PianoKey> pianoKeys;
    int activePlayedKey = -1;

    struct FunctionButton
    {
        juce::String label;
        juce::Rectangle<float> bounds;
        juce::Point<float> ledPos;
        bool hasLed = true;
        bool isLit = false;
        juce::String subLabel = "";
        juce::String stepLabel = "";
    };
    std::vector<FunctionButton> functionButtons;

    // -------------------------------------------------------------------------
    // Procedural 3D Drawing Methods
    // -------------------------------------------------------------------------
    void setupKnob(ControlKnob& knob, const juce::String& paramId, const juce::String& labelText, bool hideLabel = false);
    void setupKeyboardLayout();

    void draw3DCircuitBoard(juce::Graphics& g);
    void drawTranslucentAcrylicPanel(juce::Graphics& g);
    void drawSilkscreenGraphics(juce::Graphics& g);
    void drawPianoKeysAndLeds(juce::Graphics& g);
    void drawScrew(juce::Graphics& g, float x, float y);
    void drawMoldBoss(juce::Graphics& g, float x, float y, float radius);
    void drawIcChip(juce::Graphics& g, float x, float y, float w, float h, int numPins, const juce::String& partNumber);
    void drawTransistor(juce::Graphics& g, float x, float y, const juce::String& partNumber);
    void drawCapacitor(juce::Graphics& g, float x, float y, float radius, juce::Colour canColour);
    void drawResistor(juce::Graphics& g, float x1, float y1, float x2, float y2, juce::Colour b1, juce::Colour b2, juce::Colour b3);
    void drawDigitalTempoClock(juce::Graphics& g);
    void draw7SegmentDigit(juce::Graphics& g, float x, float y, float w, float h, char digit);
    void drawSequencerTab(juce::Graphics& g);

    //==========================================================================
    // Slide-down drawer state
    std::unique_ptr<PatternDrawer> patternDrawer;
    bool drawerOpen = false;
    bool drawerAnimating = false;
    float drawerProgress = 0.0f;
    float drawerAnimFrom = 0.0f;
    double drawerAnimStartMs = 0.0;
    void applyDrawerProgress();

    /** Last pattern group seen by the TRACK knob, so the knob and the drawer's
        A–H slots stay in sync without fighting each other. */
    int lastPatternGroup = -1;
    void syncPatternGroup();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Replicant303AudioProcessorEditor)
};
