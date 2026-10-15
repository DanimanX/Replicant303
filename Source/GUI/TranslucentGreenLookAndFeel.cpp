#include "TranslucentGreenLookAndFeel.h"

namespace Replicant303
{
    const juce::Colour TranslucentGreenLookAndFeel::bgDeepGreen     = juce::Colour(0xff081c0e);
    const juce::Colour TranslucentGreenLookAndFeel::bgMidGreen      = juce::Colour(0xff113a1e);
    const juce::Colour TranslucentGreenLookAndFeel::bgLightGreen    = juce::Colour(0xff1c592f);
    const juce::Colour TranslucentGreenLookAndFeel::neonGreen       = juce::Colour(0xff00ff66);
    const juce::Colour TranslucentGreenLookAndFeel::neonRed         = juce::Colour(0xffff283b);
    const juce::Colour TranslucentGreenLookAndFeel::knobMetalBright = juce::Colour(0xffdbe0d7);
    const juce::Colour TranslucentGreenLookAndFeel::knobMetalDark   = juce::Colour(0xff2b332c);
    const juce::Colour TranslucentGreenLookAndFeel::textSilver      = juce::Colour(0xffd3e3cd);
    const juce::Colour TranslucentGreenLookAndFeel::borderOutline   = juce::Colour(0xff26733e);

    TranslucentGreenLookAndFeel::TranslucentGreenLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, bgDeepGreen);
        setColour(juce::Label::textColourId, textSilver);
        setColour(juce::ComboBox::backgroundColourId, bgDeepGreen.withAlpha(0.85f));
        setColour(juce::ComboBox::textColourId, textSilver);
        setColour(juce::ComboBox::outlineColourId, borderOutline);
        setColour(juce::ComboBox::arrowColourId, neonGreen);
        setColour(juce::PopupMenu::backgroundColourId, bgDeepGreen.withAlpha(0.95f));
        setColour(juce::PopupMenu::textColourId, textSilver);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, bgLightGreen);
        setColour(juce::PopupMenu::highlightedTextColourId, neonGreen);
    }

    void TranslucentGreenLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                                       float sliderPosProportional, float rotaryStartAngle,
                                                       float rotaryEndAngle, juce::Slider& /*slider*/)
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto center = bounds.getCentre();
        auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // 1. Arc track background (dark acrylic groove)
        auto arcRadius = radius - 2.0f;
        juce::Path trackPath;
        trackPath.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.strokePath(trackPath, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 2. Active illuminated neon value arc
        if (sliderPosProportional > 0.001f)
        {
            juce::Path valuePath;
            valuePath.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, toAngle, true);
            g.setColour(neonGreen.withAlpha(0.85f));
            g.strokePath(valuePath, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 3. Knob Outer Metallic Knurled Rim
        auto knobRadius = radius - 8.0f;
        auto knobBounds = juce::Rectangle<float>(center.x - knobRadius, center.y - knobRadius,
                                                knobRadius * 2.0f, knobRadius * 2.0f);

        // Shadow behind knob
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillEllipse(knobBounds.translated(0.0f, 2.5f));

        // Metallic bevel gradient
        juce::ColourGradient rimGrad(knobMetalBright, knobBounds.getX(), knobBounds.getY(),
                                     knobMetalDark, knobBounds.getRight(), knobBounds.getBottom(), true);
        g.setGradientFill(rimGrad);
        g.fillEllipse(knobBounds);

        // Subtle knurling ribs (303 flutes)
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        for (int i = 0; i < 18; ++i)
        {
            float angle = i * (juce::MathConstants<float>::twoPi / 18.0f);
            float rInner = knobRadius * 0.78f;
            float rOuter = knobRadius * 0.98f;
            float x1 = center.x + rInner * std::cos(angle);
            float y1 = center.y + rInner * std::sin(angle);
            float x2 = center.x + rOuter * std::cos(angle);
            float y2 = center.y + rOuter * std::sin(angle);
            g.drawLine(x1, y1, x2, y2, 1.2f);
        }

        // 4. Knob Center Cap (Brushed dark metal with inner bevel)
        auto capRadius = knobRadius * 0.72f;
        auto capBounds = juce::Rectangle<float>(center.x - capRadius, center.y - capRadius,
                                               capRadius * 2.0f, capRadius * 2.0f);

        juce::ColourGradient capGrad(knobMetalDark.brighter(0.2f), center.x, center.y - capRadius,
                                     knobMetalDark.darker(0.6f), center.x, center.y + capRadius, false);
        g.setGradientFill(capGrad);
        g.fillEllipse(capBounds);

        // Thin metallic edge ring around cap
        g.setColour(knobMetalBright.withAlpha(0.6f));
        g.drawEllipse(capBounds, 1.0f);

        // 5. Fluorescent Neon Indicator Needle / Pointer
        auto pointerLength = knobRadius * 0.85f;
        auto pointerThickness = 2.8f;

        juce::Path p;
        p.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength * 0.55f, 1.2f);
        p.applyTransform(juce::AffineTransform::rotation(toAngle).translated(center.x, center.y));

        // Pointer glow
        g.setColour(neonGreen.withAlpha(0.4f));
        g.strokePath(p, juce::PathStrokeType(4.0f));

        // Pointer body
        g.setColour(neonGreen);
        g.fillPath(p);
    }

    void TranslucentGreenLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                                       bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(2.0f);
        bool isToggled = button.getToggleState();

        // Button background (Tactile 303 switch)
        auto btnArea = bounds.removeFromLeft(bounds.getHeight()).reduced(3.0f);
        
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillRoundedRectangle(btnArea.translated(0.0f, 1.5f), 3.0f);

        juce::Colour btnColour = isToggled ? bgLightGreen.brighter(0.3f) : bgMidGreen.darker(0.3f);
        if (shouldDrawButtonAsHighlighted)
            btnColour = btnColour.brighter(0.2f);
        if (shouldDrawButtonAsDown)
            btnColour = btnColour.darker(0.2f);

        g.setColour(btnColour);
        g.fillRoundedRectangle(btnArea, 3.0f);

        g.setColour(isToggled ? neonGreen : borderOutline);
        g.drawRoundedRectangle(btnArea, 3.0f, 1.2f);

        // LED center indicator
        auto ledArea = btnArea.reduced(btnArea.getWidth() * 0.28f);
        if (isToggled)
        {
            // Glowing neon LED
            g.setColour(neonGreen.withAlpha(0.45f));
            g.fillEllipse(ledArea.expanded(2.0f));
            g.setColour(neonGreen);
            g.fillEllipse(ledArea);
        }
        else
        {
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.fillEllipse(ledArea);
        }

        // Text label
        g.setColour(isToggled ? neonGreen : textSilver);
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(button.getButtonText(), bounds.reduced(6.0f, 0.0f), juce::Justification::centredLeft);
    }

    void TranslucentGreenLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                                   int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                                                   juce::ComboBox& box)
    {
        auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height).reduced(1.0f);

        // Acrylic translucent container
        g.setColour(bgDeepGreen.withAlpha(0.85f));
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(borderOutline);
        g.drawRoundedRectangle(bounds, 4.0f, 1.2f);

        // Arrow icon in neon green
        auto arrowArea = bounds.removeFromRight(24.0f).reduced(7.0f);
        juce::Path arrow;
        arrow.addTriangle(arrowArea.getX(), arrowArea.getY() + arrowArea.getHeight() * 0.35f,
                          arrowArea.getRight(), arrowArea.getY() + arrowArea.getHeight() * 0.35f,
                          arrowArea.getCentreX(), arrowArea.getBottom() - arrowArea.getHeight() * 0.25f);

        g.setColour(box.isEnabled() ? neonGreen : borderOutline);
        g.fillPath(arrow);
    }

    void TranslucentGreenLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                                        bool isSeparator, bool isActive, bool isHighlighted,
                                                        bool isTicked, bool /*hasSubMenu*/, const juce::String& text,
                                                        const juce::String& /*shortcutKeyText*/,
                                                        const juce::Drawable* /*icon*/, const juce::Colour* /*textColour*/)
    {
        if (isSeparator)
        {
            g.setColour(borderOutline.withAlpha(0.5f));
            g.fillRect(area.reduced(4, 0).withHeight(1));
            return;
        }

        if (isHighlighted && isActive)
        {
            g.setColour(bgLightGreen);
            g.fillRect(area);
        }

        g.setColour(isHighlighted ? neonGreen : textSilver);
        g.setFont(juce::FontOptions(13.0f, juce::Font::plain));

        auto textBounds = area.reduced(10, 0);
        g.drawFittedText(text, textBounds, juce::Justification::centredLeft, 1);

        if (isTicked)
        {
            g.setColour(neonGreen);
            auto itemArea = area;
            g.fillEllipse(itemArea.removeFromRight(20).reduced(6).toFloat());
        }
    }
}
