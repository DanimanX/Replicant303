#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "GUI/TranslucentGreenLookAndFeel.h"

class Replicant303AudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit Replicant303AudioProcessorEditor(Replicant303AudioProcessor&);
    ~Replicant303AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;

    Replicant303AudioProcessor& audioProcessor;
    Replicant303::TranslucentGreenLookAndFeel customLookAndFeel;

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
    juce::ToggleButton waveformSwitch;  // Saw / Square slide switch
    juce::ToggleButton distActiveSwitch; // Distortion Off / On slide switch
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> distActiveAttachment;

    juce::ComboBox distModelBox; // 5-model distortion selector
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> distModelAttachment;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Replicant303AudioProcessorEditor)
};
