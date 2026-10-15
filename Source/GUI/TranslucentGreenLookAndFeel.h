#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Replicant303
{
    class TranslucentGreenLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        TranslucentGreenLookAndFeel();
        ~TranslucentGreenLookAndFeel() override = default;

        // Custom Rotary Slider Drawing (303 style with metallic cap and neon pointer)
        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPosProportional, float rotaryStartAngle,
                              float rotaryEndAngle, juce::Slider& slider) override;

        // Custom Toggle Button (Vintage tactile switch)
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

        // Color Constants
        static const juce::Colour bgDeepGreen;
        static const juce::Colour bgMidGreen;
        static const juce::Colour bgLightGreen;
        static const juce::Colour neonGreen;
        static const juce::Colour neonRed;
        static const juce::Colour knobMetalBright;
        static const juce::Colour knobMetalDark;
        static const juce::Colour textSilver;
        static const juce::Colour borderOutline;
    };
}
