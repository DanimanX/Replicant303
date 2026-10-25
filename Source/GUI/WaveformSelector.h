#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

#include "DSP/WaveformBank.h"

namespace Replicant303
{
    //==========================================================================
    /** Recessed LED scope in the WAVEFORM box.

        It draws the selected shape as an oscilloscope trace and asks its owner
        to drop down the full bank when clicked. The double-click is the
        documented shortcut; a single click opens it too, because a hardware
        scope that ignores a click feels broken.
    */
    class WaveformSelector : public juce::Component
    {
    public:
        WaveformSelector() = default;
        ~WaveformSelector() override = default;

        void setWaveformIndex(int index);
        int getWaveformIndex() const noexcept { return currentIndex; }

        /** Fired on a single click (owner toggles the dropdown). */
        std::function<void()> onClicked;

        /** Fired on a double click (owner always opens the dropdown). */
        std::function<void()> onMenuRequested;

        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDoubleClick(const juce::MouseEvent&) override;
        void mouseEnter(const juce::MouseEvent&) override;
        void mouseExit(const juce::MouseEvent&) override;

    private:
        int currentIndex = 0;
        bool hovering = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformSelector)
    };

    //==========================================================================
    /** Dropdown that lists every shape in the bank with a live scope preview. */
    class WaveformMenu : public juce::Component,
                         private juce::ListBoxModel
    {
    public:
        WaveformMenu();
        ~WaveformMenu() override = default;

        void setSelectedIndex(int index);
        int getSelectedIndex() const noexcept { return selectedIndex; }

        std::function<void(int)> onSelect;
        std::function<void()> onDismiss;

        void paint(juce::Graphics&) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent&) override;

        // ListBoxModel
        int getNumRows() override;
        void paintListBoxItem(int rowNumber, juce::Graphics&,
                              int width, int height, bool rowIsSelected) override;
        void listBoxItemClicked(int row, const juce::MouseEvent&) override;
        void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;

    private:
        juce::ListBox list { "waveformBank", this };
        int selectedIndex = 0;
        juce::Rectangle<int> closeButton;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformMenu)
    };
}
