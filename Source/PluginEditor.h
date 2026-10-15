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

    ControlKnob tuningKnob;
    ControlKnob cutoffKnob;
    ControlKnob resonanceKnob;
    ControlKnob envModKnob;
    ControlKnob decayKnob;
    ControlKnob accentKnob;
    ControlKnob volumeKnob;

    ControlKnob distDriveKnob;
    ControlKnob distToneKnob;
    ControlKnob distMixKnob;

    // -------------------------------------------------------------------------
    // Buttons & Dropdowns
    // -------------------------------------------------------------------------
    juce::ToggleButton waveformSawBtn { "SAW" };
    juce::ToggleButton waveformSqrBtn { "SQR" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> sawAttachment;

    juce::ToggleButton distActiveToggle { "ACTIVE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> distActiveAttachment;

    juce::ComboBox distTypeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> distTypeAttachment;

    // -------------------------------------------------------------------------
    // Helper Methods
    // -------------------------------------------------------------------------
    void setupKnob(ControlKnob& knob, const juce::String& paramId, const juce::String& labelText);
    void drawPcbTraces(juce::Graphics& g);
    void drawLed(juce::Graphics& g, int x, int y, int size, bool active, juce::Colour activeColour, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Replicant303AudioProcessorEditor)
};
