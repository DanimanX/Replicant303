#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Replicant303
{
    class TranslucentGreenLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        TranslucentGreenLookAndFeel();
        ~TranslucentGreenLookAndFeel() override = default;

        // Custom Rotary Slider Drawing (TD-3 style: fluted cylindrical skirt, spun aluminum cap)
        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPosProportional, float rotaryStartAngle,
                              float rotaryEndAngle, juce::Slider& slider) override;

        // Custom Toggle Button (Tactile button with LED)
        void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

        // Custom ComboBox
        void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                          int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override;

        void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                               bool isSeparator, bool isActive, bool isHighlighted,
                               bool isTicked, bool hasSubMenu, const juce::String& text,
                               const juce::String& shortcutKeyText,
                               const juce::Drawable* icon, const juce::Colour* textColour) override;

        // Helpers for drawing authentic hardware elements
        static void drawKnobTicks(juce::Graphics& g, float centerX, float centerY, float radius,
                                 float startAngle, float endAngle, int numTicks = 11);
        static void drawJackSocket(juce::Graphics& g, float centerX, float centerY, float diameter, const juce::String& label);
        static void drawSlideSwitch(juce::Graphics& g, const juce::Rectangle<float>& bounds, bool isOn, const juce::String& labelLeft, const juce::String& labelRight);

        // Color Constants based on the Behringer TD-3-TG
        static const juce::Colour acrylicGreen;       // Vibrant translucent green
        static const juce::Colour acrylicHighlight;   // Glossy rim green
        static const juce::Colour pcbDarkGreen;       // PCB substrate below
        static const juce::Colour pcbTraceGreen;      // Copper traces
        static const juce::Colour moldBossGreen;      // Translucent mold bosses/pillars
        static const juce::Colour ledBrightRed;       // 303 Red LED active
        static const juce::Colour ledDarkRed;         // 303 Red LED unlit
        static const juce::Colour spunAluminumBright; // Knob cap top
        static const juce::Colour spunAluminumDark;   // Knob cap shadow
        static const juce::Colour knobRibColor;       // Fluted knob skirt
        static const juce::Colour silkscreenWhite;    // Crisp white panel print
    };
}
