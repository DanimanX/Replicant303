#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Replicant303Theme.h"

namespace Replicant303
{
    /** Theme driven hardware Look & Feel.

        Every colour comes from the currently active Theme, so switching skins
        restyles the whole panel (component chrome, knobs, jacks, LEDs, keys)
        without touching the editor layout.
    */
    class Replicant303LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        Replicant303LookAndFeel();
        ~Replicant303LookAndFeel() override = default;

        /** Re-reads the active theme and re-applies every component colour.
            Message thread only. */
        void applyTheme();

        /** The palette currently in force. */
        static const Theme& theme() noexcept { return ThemeManager::getInstance().getCurrentTheme(); }

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

        // Scaled typography for component-drawn text
        juce::Font getComboBoxFont(juce::ComboBox& box) override;
        juce::Font getPopupMenuFont() override;

        void drawCornerResizer(juce::Graphics& g, int w, int h,
                               bool isMouseOver, bool isMouseDragging) override;

        // Helpers for drawing authentic hardware elements
        static void drawKnobTicks(juce::Graphics& g, float centerX, float centerY, float radius,
                                 float startAngle, float endAngle, int numTicks = 11);
        static void drawJackSocket(juce::Graphics& g, float centerX, float centerY, float diameter, const juce::String& label);
        static void drawSlideSwitch(juce::Graphics& g, const juce::Rectangle<float>& bounds, bool isOn, const juce::String& labelLeft, const juce::String& labelRight);

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Replicant303LookAndFeel)
    };
}
