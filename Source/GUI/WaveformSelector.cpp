#include "WaveformSelector.h"
#include "Replicant303Theme.h"

#include <array>
#include <cmath>

namespace Replicant303
{
namespace
{
    const Replicant303::Theme& theme()
    {
        return ThemeManager::getInstance().getCurrentTheme();
    }

    /** Draws one cycle of a shape, normalised to the glyph box. */
    void paintWaveGlyph(juce::Graphics& g,
                        juce::Rectangle<float> area,
                        int waveform,
                        juce::Colour colour,
                        float thickness,
                        bool glow)
    {
        constexpr int points = 160;
        std::array<float, points + 1> values {};
        float peak = 1.0e-6f;

        for (int i = 0; i <= points; ++i)
        {
            const float v = evaluateWaveformShape(waveform, (double) i / (double) points);
            values[(size_t) i] = v;
            peak = juce::jmax(peak, std::abs(v));
        }

        const float halfHeight = juce::jmax(1.0f, area.getHeight() * 0.42f);
        const float centreY = area.getCentreY();

        juce::Path path;

        for (int i = 0; i <= points; ++i)
        {
            const float x = area.getX() + area.getWidth() * (float) i / (float) points;
            const float y = centreY - (values[(size_t) i] / peak) * halfHeight;

            if (i == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }

        if (glow)
        {
            g.setColour(colour.withAlpha(0.22f));
            g.strokePath(path, juce::PathStrokeType(thickness * 3.2f,
                                                    juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
        }

        g.setColour(colour);
        g.strokePath(path, juce::PathStrokeType(thickness,
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    }
}

//==============================================================================
void WaveformSelector::setWaveformIndex(int index)
{
    const int clamped = juce::jlimit(0, numWaveforms - 1, index);

    if (clamped == currentIndex)
        return;

    currentIndex = clamped;
    repaint();
}

void WaveformSelector::paint(juce::Graphics& g)
{
    const auto& t = theme();
    auto bounds = getLocalBounds().toFloat();

    // Header: bank name + slot number.
    auto header = bounds.removeFromTop(11.0f);
    g.setColour(t.silkscreen.withAlpha(0.85f));
    g.setFont(uiFont(7.6f, juce::Font::bold));
    g.drawText("WAVEFORM", header.withTrimmedLeft(1.0f), juce::Justification::centredLeft);

    g.setColour(t.silkscreen.withAlpha(0.45f));
    g.drawText(juce::String(currentIndex + 1).paddedLeft('0', 2) + "/"
                   + juce::String(numWaveforms),
               header, juce::Justification::centredRight);

    // Footer: shape name + dropdown chevron.
    auto footer = bounds.removeFromBottom(22.0f);
    auto chevronArea = footer.removeFromRight(14.0f);

    // The scope itself: recessed smoked chamber with an LED trace.
    auto screen = bounds.reduced(0.5f, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(screen.translated(0.0f, 1.2f), 3.0f);

    g.setColour(t.displayChamber);
    g.fillRoundedRectangle(screen, 3.0f);

    g.setColour(t.displayBezelUp.withAlpha(0.85f));
    g.drawRoundedRectangle(screen, 3.0f, 1.0f);

    auto grid = screen.reduced(5.0f, 4.0f);
    g.setColour(t.silkscreen.withAlpha(0.10f));

    for (int i = 1; i < 4; ++i)
    {
        const float x = grid.getX() + grid.getWidth() * (float) i / 4.0f;
        g.drawVerticalLine((int) x, grid.getY(), grid.getBottom());
    }

    g.setColour(t.silkscreen.withAlpha(0.16f));
    g.drawHorizontalLine((int) grid.getCentreY(), grid.getX(), grid.getRight());

    // Micro-ribs, matching the tempo display.
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    for (float y = grid.getY(); y < grid.getBottom(); y += 2.0f)
        g.drawHorizontalLine((int) y, grid.getX(), grid.getRight());

    paintWaveGlyph(g, grid, currentIndex, t.displayLit, 1.5f, true);

    // Shape name.
    const auto& spec = getWaveformSpec(currentIndex);
    g.setColour(t.silkscreen);
    g.setFont(uiFont(8.4f, juce::Font::bold));
    g.drawFittedText(spec.name, footer.toNearestInt().withTrimmedRight(2),
                     juce::Justification::centredLeft, 1, 0.85f);

    // Chevron.
    juce::Path arrow;
    const float cx = chevronArea.getCentreX();
    const float cy = chevronArea.getCentreY() + 1.0f;
    arrow.addTriangle(cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);

    g.setColour(hovering ? t.displayLit : t.silkscreen.withAlpha(0.75f));
    g.fillPath(arrow);

    if (hovering)
    {
        g.setColour(t.displayLit.withAlpha(0.55f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 3.0f, 1.0f);
    }
}

void WaveformSelector::mouseDown(const juce::MouseEvent& e)
{
    if (e.getNumberOfClicks() <= 1 && onClicked != nullptr)
        onClicked();
}

void WaveformSelector::mouseDoubleClick(const juce::MouseEvent&)
{
    if (onMenuRequested != nullptr)
        onMenuRequested();
}

void WaveformSelector::mouseEnter(const juce::MouseEvent&)
{
    hovering = true;
    repaint();
}

void WaveformSelector::mouseExit(const juce::MouseEvent&)
{
    hovering = false;
    repaint();
}

//==============================================================================
WaveformMenu::WaveformMenu()
{
    setOpaque(false);

    list.setRowHeight(30);
    list.setMultipleSelectionEnabled(false);
    list.setOutlineThickness(0);
    list.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    list.setWantsKeyboardFocus(false);
    addAndMakeVisible(list);
}

void WaveformMenu::setSelectedIndex(int index)
{
    selectedIndex = juce::jlimit(0, numWaveforms - 1, index);

    // Before the first layout the ListBox has no viewport, so only sync the
    // selection; scrolling waits until the menu actually has a size.
    if (list.getWidth() > 0)
    {
        list.selectRow(selectedIndex, false);
        list.scrollToEnsureRowIsOnscreen(selectedIndex);
    }
    else
    {
        list.selectRow(selectedIndex, true);
    }

    repaint();
}

void WaveformMenu::paint(juce::Graphics& g)
{
    const auto& t = theme();
    auto bounds = getLocalBounds().toFloat();

    // Acrylic flap with a pointer back up at the scope.
    g.setColour(t.pcbDark.withAlpha(0.97f));
    g.fillRoundedRectangle(bounds.reduced(1.0f), 5.0f);

    g.setColour(t.silkscreen.withAlpha(0.60f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 5.0f, 1.2f);

    // Style the list scrollbar with the panel palette rather than the
    // LookAndFeel's default blue, and only when the colour actually changes.
    auto& scrollBar = list.getVerticalScrollBar();
    const auto thumb = t.displayLit.withAlpha(0.65f);

    if (scrollBar.findColour(juce::ScrollBar::thumbColourId) != thumb)
    {
        scrollBar.setColour(juce::ScrollBar::thumbColourId, thumb);
        scrollBar.setColour(juce::ScrollBar::trackColourId, t.silkscreen.withAlpha(0.10f));
        scrollBar.setColour(juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);
    }

    // Header.
    g.setColour(t.silkscreen);
    g.setFont(uiFont(9.5f, juce::Font::bold));
    g.drawText("WAVEFORM BANK", 14, 4, 170, 22, juce::Justification::centredLeft);

    g.setColour(t.silkscreen.withAlpha(0.5f));
    g.setFont(uiFont(7.5f, juce::Font::bold));
    g.drawText(juce::String(numWaveforms) + " SHAPES  /  SCROLL",
               getWidth() - 240, 4, 200, 22, juce::Justification::centredRight);

    // Close cross.
    juce::Path cross;
    const float cx = (float) closeButton.getCentreX();
    const float cy = (float) closeButton.getCentreY();
    cross.startNewSubPath(cx - 4.0f, cy - 4.0f);
    cross.lineTo(cx + 4.0f, cy + 4.0f);
    cross.startNewSubPath(cx + 4.0f, cy - 4.0f);
    cross.lineTo(cx - 4.0f, cy + 4.0f);
    g.setColour(t.silkscreen.withAlpha(0.85f));
    g.strokePath(cross, juce::PathStrokeType(1.4f));

    g.setColour(t.silkscreen.withAlpha(0.14f));
    g.drawHorizontalLine(31, 10.0f, (float) getWidth() - 10.0f);
}

void WaveformMenu::resized()
{
    closeButton = { getWidth() - 30, 4, 24, 22 };
    list.setBounds(8, 33, getWidth() - 16, getHeight() - 41);
}

void WaveformMenu::mouseDown(const juce::MouseEvent& e)
{
    if (closeButton.contains(e.getPosition()) && onDismiss != nullptr)
        onDismiss();
}

int WaveformMenu::getNumRows()
{
    return numWaveforms;
}

void WaveformMenu::paintListBoxItem(int rowNumber, juce::Graphics& g,
                                    int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= numWaveforms)
        return;

    const auto& t = theme();
    const auto& spec = getWaveformSpec(rowNumber);
    const bool selected = rowNumber == selectedIndex;
    const bool startsFamily = rowNumber == 0
        || juce::String(spec.family) != juce::String(getWaveformSpec(rowNumber - 1).family);

    auto row = juce::Rectangle<float>(2.0f, 1.0f, (float) width - 6.0f, (float) height - 2.0f);

    if (selected)
    {
        g.setColour(t.displayLit.withAlpha(0.16f));
        g.fillRoundedRectangle(row, 3.0f);
        g.setColour(t.displayLit.withAlpha(0.55f));
        g.drawRoundedRectangle(row, 3.0f, 1.0f);
    }
    else if (rowIsSelected)
    {
        g.setColour(t.silkscreen.withAlpha(0.08f));
        g.fillRoundedRectangle(row, 3.0f);
    }

    if (startsFamily && rowNumber > 0)
    {
        g.setColour(t.silkscreen.withAlpha(0.16f));
        g.drawHorizontalLine(0, row.getX() + 4.0f, row.getRight() - 4.0f);
    }

    auto content = row.reduced(6.0f, 3.0f);

    // Slot number.
    g.setColour(t.silkscreen.withAlpha(selected ? 0.8f : 0.38f));
    g.setFont(uiFont(7.4f, juce::Font::bold));
    g.drawText(juce::String(rowNumber + 1).paddedLeft('0', 2),
               content.removeFromLeft(20.0f).toNearestInt(),
               juce::Justification::centredLeft);

    // Scope preview.
    auto preview = content.removeFromLeft(70.0f).reduced(2.0f, 3.0f);
    paintWaveGlyph(g, preview, rowNumber,
                   selected ? t.displayLit : t.silkscreen.withAlpha(0.55f),
                   1.1f, selected);

    // Name.
    g.setColour(selected ? t.silkscreen.brighter(0.35f) : t.silkscreen);
    g.setFont(uiFont(9.2f, juce::Font::bold));
    g.drawFittedText(spec.name, content.toNearestInt().withTrimmedRight(58),
                     juce::Justification::centredLeft, 1, 0.85f);

    // Family tag only where the group starts, so it reads as a section marker.
    if (startsFamily)
    {
        g.setColour(t.silkscreen.withAlpha(0.38f));
        g.setFont(uiFont(6.8f, juce::Font::bold));
        g.drawText(spec.family, content.toNearestInt().removeFromRight(58),
                   juce::Justification::centredRight);
    }
}

void WaveformMenu::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= numWaveforms)
        return;

    setSelectedIndex(row);

    if (onSelect != nullptr)
        onSelect(row);
}

void WaveformMenu::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= numWaveforms)
        return;

    setSelectedIndex(row);

    if (onSelect != nullptr)
        onSelect(row);
}
}
