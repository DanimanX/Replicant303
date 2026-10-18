#include "Replicant303LookAndFeel.h"

namespace Replicant303
{
    //==============================================================================
    Replicant303LookAndFeel::Replicant303LookAndFeel()
    {
        applyTheme();
    }

    void Replicant303LookAndFeel::applyTheme()
    {
        const auto& t = theme();

        setColour(juce::ResizableWindow::backgroundColourId, t.chassisUp);

        setColour(juce::Label::textColourId, t.silkscreen);

        setColour(juce::ComboBox::backgroundColourId, t.pcbDark.withAlpha(0.92f));
        setColour(juce::ComboBox::textColourId, t.silkscreen);
        setColour(juce::ComboBox::outlineColourId, t.silkscreen.withAlpha(0.85f));
        setColour(juce::ComboBox::arrowColourId, t.silkscreen);
        setColour(juce::ComboBox::focusedOutlineColourId, t.ledBright);

        setColour(juce::PopupMenu::backgroundColourId, t.pcbDark.withAlpha(0.96f));
        setColour(juce::PopupMenu::textColourId, t.silkscreen);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, t.silkscreen.withAlpha(0.22f));
        setColour(juce::PopupMenu::highlightedTextColourId, t.silkscreen);

        setColour(juce::Slider::textBoxTextColourId, t.silkscreen);
        setColour(juce::TextButton::textColourOffId, t.silkscreen);
        setColour(juce::TextButton::textColourOnId, t.silkscreen);
        setColour(juce::ToggleButton::textColourId, t.silkscreen);
        setColour(juce::ToggleButton::tickColourId, t.ledBright);
        setColour(juce::TooltipWindow::backgroundColourId, t.pcbDark);
        setColour(juce::TooltipWindow::textColourId, t.silkscreen);
        setColour(juce::TooltipWindow::outlineColourId, t.silkscreen.withAlpha(0.6f));
    }

    //==============================================================================
    void Replicant303LookAndFeel::drawKnobTicks(juce::Graphics& g, float centerX, float centerY, float radius,
                                                float startAngle, float endAngle, int numTicks)
    {
        g.setColour(theme().silkscreen.withAlpha(0.90f));
        for (int i = 0; i < numTicks; ++i)
        {
            float norm = (numTicks > 1) ? (float)i / (float)(numTicks - 1) : 0.0f;
            float angle = startAngle + norm * (endAngle - startAngle);
            
            float rInner = radius + 2.0f;
            float rOuter = radius + ((i == 0 || i == numTicks - 1 || i == numTicks / 2) ? 6.5f : 4.5f);
            float thickness = ((i == 0 || i == numTicks - 1 || i == numTicks / 2) ? 1.6f : 1.1f);

            float cosA = std::cos(angle - juce::MathConstants<float>::halfPi);
            float sinA = std::sin(angle - juce::MathConstants<float>::halfPi);

            g.drawLine(centerX + rInner * cosA, centerY + rInner * sinA,
                       centerX + rOuter * cosA, centerY + rOuter * sinA, thickness);
        }
    }

    void Replicant303LookAndFeel::drawJackSocket(juce::Graphics& g, float centerX, float centerY, float diameter, const juce::String& label)
    {
        const auto& t = theme();
        auto rect = juce::Rectangle<float>(centerX - diameter * 0.5f, centerY - diameter * 0.5f, diameter, diameter);

        // Mold boss underneath jack
        g.setColour(t.moldBoss.withAlpha(0.35f));
        g.fillEllipse(rect.expanded(7.0f));
        g.setColour(t.moldBoss.withAlpha(0.60f));
        g.drawEllipse(rect.expanded(7.0f), 1.2f);

        // Drop shadow
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillEllipse(rect.translated(0.0f, 2.0f));

        // Chrome hex nut
        g.setColour(t.metalBright);
        juce::Path hex;
        for (int i = 0; i < 6; ++i)
        {
            float a = i * (juce::MathConstants<float>::twoPi / 6.0f);
            float hx = centerX + (diameter * 0.54f) * std::cos(a);
            float hy = centerY + (diameter * 0.54f) * std::sin(a);
            if (i == 0) hex.startNewSubPath(hx, hy);
            else hex.lineTo(hx, hy);
        }
        hex.closeSubPath();
        g.fillPath(hex);
        g.setColour(t.metalEdge);
        g.strokePath(hex, juce::PathStrokeType(1.1f));

        // Jack barrel bevel
        g.setColour(t.metalBright.brighter(0.25f));
        g.drawEllipse(rect.reduced(2.0f), 1.2f);

        // Center jack hole
        g.setColour(juce::Colours::black);
        g.fillEllipse(rect.reduced(diameter * 0.30f));

        // Inside contact reflection
        g.setColour(t.metalBright);
        g.fillEllipse(rect.reduced(diameter * 0.42f).translated(-0.5f, -0.5f));

        // Little arrow indicator (▼ for inputs, ▲ for outputs)
        bool isInput = label.contains("IN");
        float arrowY = centerY - diameter * 0.5f - 8.0f;
        g.setColour(t.silkscreen);
        juce::Path arr;
        if (isInput)
        {
            arr.addTriangle(centerX - 3.5f, arrowY, centerX + 3.5f, arrowY, centerX, arrowY + 4.5f);
        }
        else
        {
            arr.addTriangle(centerX - 3.5f, arrowY + 4.5f, centerX + 3.5f, arrowY + 4.5f, centerX, arrowY);
        }
        g.fillPath(arr);

        // Label above/below
        if (label.isNotEmpty())
        {
            g.setColour(t.silkscreen);
            g.setFont(uiFont(8.5f, juce::Font::bold));
            g.drawText(label, (int)(centerX - 32.0f), (int)(centerY + diameter * 0.5f + 3.0f), 64, 13, juce::Justification::centred);
        }
    }

    void Replicant303LookAndFeel::drawSlideSwitch(juce::Graphics& g, const juce::Rectangle<float>& bounds,
                                                  bool isOn, const juce::String& labelLeft, const juce::String& labelRight)
    {
        const auto& t = theme();

        // Recessed slot
        g.setColour(juce::Colours::black.withAlpha(0.70f));
        g.fillRoundedRectangle(bounds, 3.0f);
        g.setColour(t.silkscreen.withAlpha(0.65f));
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

        // Slide handle
        float handleW = bounds.getWidth() * 0.45f;
        float handleH = bounds.getHeight() + 4.0f;
        float handleX = isOn ? (bounds.getRight() - handleW - 1.0f) : (bounds.getX() + 1.0f);
        float handleY = bounds.getY() - 2.0f;
        auto handleRect = juce::Rectangle<float>(handleX, handleY, handleW, handleH);

        // Handle shadow
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRoundedRectangle(handleRect.translated(0.0f, 1.5f), 2.5f);

        // Molded ribbed black handle
        g.setColour(t.knobRib);
        g.fillRoundedRectangle(handleRect, 2.0f);
        
        // Ribbed flutes on switch
        g.setColour(t.knobRib.brighter(0.30f));
        for (float rx = handleRect.getX() + 3.0f; rx < handleRect.getRight() - 2.0f; rx += 3.0f)
            g.drawVerticalLine((int)rx, handleRect.getY() + 2.0f, handleRect.getBottom() - 2.0f);

        // Pip highlight
        g.setColour(t.metalBright);
        g.fillEllipse(handleRect.getCentreX() - 1.5f, handleRect.getCentreY() - 1.5f, 3.0f, 3.0f);

        // Labels
        g.setColour(t.silkscreen);
        g.setFont(uiFont(9.5f, juce::Font::bold));
        if (labelLeft.isNotEmpty())
            g.drawText(labelLeft, (int)(bounds.getX() - 34.0f), (int)(bounds.getY() - 2.0f), 30, (int)(bounds.getHeight() + 4.0f), juce::Justification::centredRight);
        if (labelRight.isNotEmpty())
            g.drawText(labelRight, (int)(bounds.getRight() + 4.0f), (int)(bounds.getY() - 2.0f), 30, (int)(bounds.getHeight() + 4.0f), juce::Justification::centredLeft);
    }

    void Replicant303LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                                   float sliderPosProportional, float rotaryStartAngle,
                                                   float rotaryEndAngle, juce::Slider& /*slider*/)
    {
        const auto& t = theme();

        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(2.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto center = bounds.getCentre();
        auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // 1. Internal translucent mold boss (plastic standoff under knob)
        g.setColour(t.moldBoss.withAlpha(0.30f));
        g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(t.moldBoss.withAlpha(0.50f));
        g.drawEllipse(center.x - radius * 0.95f, center.y - radius * 0.95f, radius * 1.9f, radius * 1.9f, 1.2f);
        g.drawEllipse(center.x - radius * 0.70f, center.y - radius * 0.70f, radius * 1.4f, radius * 1.4f, 1.0f);

        // 2. Knob Outer Skirt (Fluted cylindrical body)
        float knobRadius = radius * 0.76f;
        auto knobRect = juce::Rectangle<float>(center.x - knobRadius, center.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

        // 3. Printed radial tick marks framing the knob
        drawKnobTicks(g, center.x, center.y, knobRadius + 1.0f, rotaryStartAngle, rotaryEndAngle, 11);

        // Cast shadow onto translucent chassis
        g.setColour(juce::Colours::black.withAlpha(0.60f));
        g.fillEllipse(knobRect.translated(0.0f, 3.5f));

        // Ribbed dark plastic body
        g.setColour(t.knobRib);
        g.fillEllipse(knobRect);

        // Fluted serrations around skirt (18 flutes)
        g.setColour(t.knobSkirtFlute);
        for (int i = 0; i < 18; ++i)
        {
            float a = i * (juce::MathConstants<float>::twoPi / 18.0f);
            float rIn = knobRadius * 0.78f;
            float rOut = knobRadius * 0.98f;
            g.drawLine(center.x + rIn * std::cos(a), center.y + rIn * std::sin(a),
                       center.x + rOut * std::cos(a), center.y + rOut * std::sin(a), 1.5f);
        }

        // 4. Conical Spun Aluminum Metallic Cap
        float capRadius = knobRadius * 0.76f;
        auto capRect = juce::Rectangle<float>(center.x - capRadius, center.y - capRadius, capRadius * 2.0f, capRadius * 2.0f);

        // Outer beveled ring of the aluminum cap
        g.setColour(t.knobCapDark);
        g.fillEllipse(capRect);

        // Spun brushed aluminum gradient reflection
        juce::ColourGradient capGrad(
            t.knobCapBright, center.x - capRadius * 0.35f, center.y - capRadius * 0.5f,
            t.knobCapDark, center.x + capRadius * 0.5f, center.y + capRadius * 0.6f, false);
        g.setGradientFill(capGrad);
        g.fillEllipse(capRect.reduced(1.2f));

        // Subtle concentric spun lathe rings
        g.setColour(juce::Colours::white.withAlpha(0.40f));
        g.drawEllipse(capRect.reduced(capRadius * 0.35f), 0.8f);
        g.setColour(juce::Colours::black.withAlpha(0.18f));
        g.drawEllipse(capRect.reduced(capRadius * 0.60f), 0.8f);

        // 5. Crisp Pointer Line (indicator line on the cap)
        float pointerLength = capRadius * 0.88f;
        float cosP = std::sin(toAngle);
        float sinP = -std::cos(toAngle);

        // Sharp pointer
        g.setColour(t.knobPointer);
        g.drawLine(center.x, center.y, center.x + pointerLength * cosP, center.y + pointerLength * sinP, 2.5f);

        // Tip highlight
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.drawLine(center.x + pointerLength * 0.65f * cosP, center.y + pointerLength * 0.65f * sinP,
                   center.x + pointerLength * cosP, center.y + pointerLength * sinP, 1.2f);
    }

    void Replicant303LookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                                   bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
    {
        const auto& t = theme();

        auto bounds = button.getLocalBounds().toFloat().reduced(2.0f);
        bool isToggled = button.getToggleState();

        // Tactile rectangular switch button (like TD-3 sequencer buttons)
        auto btnArea = bounds.removeFromLeft(bounds.getHeight() * 1.1f).reduced(2.0f);

        // Drop shadow
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRoundedRectangle(btnArea.translated(0.0f, 2.0f), 3.0f);

        // Tactile button body (Light metallic grey or dark depending on state)
        juce::Colour btnTop = isToggled ? t.buttonFaceLit : t.switchFaceOff;
        juce::Colour btnBottom = isToggled ? t.switchFaceOnBottom : t.switchFaceOffBottom;
        if (shouldDrawButtonAsHighlighted) btnTop = btnTop.brighter(0.15f);
        if (shouldDrawButtonAsDown) btnTop = btnTop.darker(0.2f);

        juce::ColourGradient btnGrad(btnTop, btnArea.getX(), btnArea.getY(), btnBottom, btnArea.getX(), btnArea.getBottom(), false);
        g.setGradientFill(btnGrad);
        g.fillRoundedRectangle(btnArea, 2.5f);

        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawRoundedRectangle(btnArea, 2.5f, 1.0f);

        // Circular LED on button
        auto ledArea = juce::Rectangle<float>(btnArea.getCentreX() - 4.0f, btnArea.getY() + 3.5f, 8.0f, 8.0f);
        
        // LED Chrome bezel
        g.setColour(t.ledBezel);
        g.drawEllipse(ledArea.expanded(1.0f), 1.0f);

        if (isToggled)
        {
            // Glowing LED bloom
            g.setColour(t.ledBright.withAlpha(0.40f));
            g.fillEllipse(ledArea.expanded(4.0f));

            // Core lit LED
            g.setColour(t.ledBright);
            g.fillEllipse(ledArea);

            // Specular highlight on dome
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.fillEllipse(ledArea.reduced(2.2f).translated(-0.8f, -0.8f));
        }
        else
        {
            // Unlit dark LED
            g.setColour(t.ledDark);
            g.fillEllipse(ledArea);
            g.setColour(juce::Colours::white.withAlpha(0.25f));
            g.fillEllipse(ledArea.reduced(2.5f).translated(-0.8f, -0.8f));
        }

        // Button label
        g.setColour(t.silkscreen);
        g.setFont(uiFont(11.0f, juce::Font::bold));
        g.drawText(button.getButtonText(), bounds.reduced(6.0f, 0.0f), juce::Justification::centredLeft);
    }

    void Replicant303LookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                               int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                                               juce::ComboBox& box)
    {
        const auto& t = theme();

        auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height).reduced(1.0f);

        // Translucent container
        g.setColour(t.pcbDark.withAlpha(0.85f));
        g.fillRoundedRectangle(bounds, 3.0f);

        g.setColour(t.silkscreen.withAlpha(0.75f));
        g.drawRoundedRectangle(bounds, 3.0f, 1.2f);

        // Arrow icon
        auto arrowArea = bounds.removeFromRight(20.0f).reduced(6.0f);
        juce::Path arrow;
        arrow.addTriangle(arrowArea.getX(), arrowArea.getY() + arrowArea.getHeight() * 0.35f,
                          arrowArea.getRight(), arrowArea.getY() + arrowArea.getHeight() * 0.35f,
                          arrowArea.getCentreX(), arrowArea.getBottom() - arrowArea.getHeight() * 0.25f);

        g.setColour(box.isEnabled() ? t.silkscreen : t.silkscreen.withAlpha(0.4f));
        g.fillPath(arrow);
    }

    void Replicant303LookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                                    bool isSeparator, bool isActive, bool isHighlighted,
                                                    bool isTicked, bool /*hasSubMenu*/, const juce::String& text,
                                                    const juce::String& /*shortcutKeyText*/,
                                                    const juce::Drawable* /*icon*/, const juce::Colour* /*textColour*/)
    {
        const auto& t = theme();

        if (isSeparator)
        {
            g.setColour(t.silkscreen.withAlpha(0.3f));
            g.fillRect(area.reduced(4, 0).withHeight(1));
            return;
        }

        if (isHighlighted && isActive)
        {
            g.setColour(t.silkscreen.withAlpha(0.22f));
            g.fillRect(area);
        }

        g.setColour(isHighlighted ? t.silkscreen.brighter(0.4f) : t.silkscreen);
        g.setFont(uiFont(13.0f, juce::Font::plain));

        auto textBounds = area.reduced(10, 0);
        g.drawFittedText(text, textBounds, juce::Justification::centredLeft, 1);

        if (isTicked)
        {
            g.setColour(t.ledBright);
            auto itemArea = area;
            g.fillEllipse(itemArea.removeFromRight(20).reduced(6).toFloat());
        }
    }

    //==============================================================================
    juce::Font Replicant303LookAndFeel::getComboBoxFont(juce::ComboBox& box)
    {
        return juce::Font(juce::FontOptions(juce::jmin(15.0f * uiFontScale, (float) box.getHeight() * 0.85f)));
    }

    juce::Font Replicant303LookAndFeel::getPopupMenuFont()
    {
        return juce::Font(uiFont(15.0f));
    }

    void Replicant303LookAndFeel::drawCornerResizer(juce::Graphics& g, int w, int h,
                                                    bool isMouseOver, bool isMouseDragging)
    {
        const auto& t = theme();
        const juce::Colour gripCol = (isMouseDragging || isMouseOver)
                                         ? t.ledBright
                                         : t.silkscreen.withAlpha(0.70f);

        g.setColour(gripCol);
        const float fw = (float) w;
        const float fh = (float) h;

        for (int i = 0; i < 3; ++i)
        {
            const float offset = 4.0f + (float) i * 4.0f;
            g.drawLine(fw - offset, fh - 2.0f, fw - 2.0f, fh - offset, 1.5f);
        }
    }
}
