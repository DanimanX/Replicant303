#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <algorithm>

namespace Replicant303
{
    //==============================================================================
    /** One step of a 303 pattern.

        The four flags mirror the hardware memory of a TB-303 step: whether the
        step sounds, which note it plays, whether it is accented and whether it
        slides into the following step.
    */
    struct Step
    {
        bool gate   = true;
        int  note   = 36;     // MIDI note number (C2 in this panel's labelling)
        bool accent = false;
        bool slide  = false;

        bool operator== (const Step& other) const noexcept
        {
            return gate == other.gate && note == other.note
                && accent == other.accent && slide == other.slide;
        }

        bool operator!= (const Step& other) const noexcept { return ! (*this == other); }
    };

    //==============================================================================
    /** A 16-step pattern: the unit the drawer edits and the clock plays. */
    struct Pattern
    {
        static constexpr int maxSteps = 16;

        /** Lowest / highest note the drawer exposes (C1 .. C3). */
        static constexpr int lowestNote  = 24;
        static constexpr int highestNote = 48;

        std::array<Step, (size_t) maxSteps> steps {};
        int length = maxSteps;

        Step& operator[] (int index) noexcept
        {
            return steps[(size_t) juce::jlimit (0, maxSteps - 1, index)];
        }

        const Step& operator[] (int index) const noexcept
        {
            return steps[(size_t) juce::jlimit (0, maxSteps - 1, index)];
        }

        bool operator== (const Pattern& other) const noexcept
        {
            if (length != other.length)
                return false;

            for (int i = 0; i < maxSteps; ++i)
                if (steps[(size_t) i] != other.steps[(size_t) i])
                    return false;

            return true;
        }

        bool operator!= (const Pattern& other) const noexcept { return ! (*this == other); }

        /** Clamps length / notes into the ranges the sequencer can play. */
        void sanitise() noexcept
        {
            length = juce::jlimit (1, maxSteps, length);
            for (auto& s : steps)
                s.note = juce::jlimit (lowestNote, highestNote, s.note);
        }

        //==========================================================================
        juce::ValueTree toValueTree() const
        {
            juce::ValueTree tree ("PATTERN");
            tree.setProperty ("length", length, nullptr);

            for (int i = 0; i < maxSteps; ++i)
            {
                const auto& s = steps[(size_t) i];
                juce::ValueTree step ("STEP");
                step.setProperty ("gate",   s.gate,   nullptr);
                step.setProperty ("note",   s.note,   nullptr);
                step.setProperty ("accent", s.accent, nullptr);
                step.setProperty ("slide",  s.slide,  nullptr);
                tree.appendChild (step, nullptr);
            }

            return tree;
        }

        static Pattern fromValueTree (const juce::ValueTree& tree)
        {
            Pattern p;

            if (! tree.isValid())
                return p;

            p.length = (int) tree.getProperty ("length", maxSteps);

            const int numChildren = std::min (tree.getNumChildren(), maxSteps);
            for (int i = 0; i < numChildren; ++i)
            {
                const auto child = tree.getChild (i);
                if (! child.hasType ("STEP"))
                    continue;

                auto& s = p.steps[(size_t) i];
                s.gate   = (bool) child.getProperty ("gate",   true);
                s.note   = (int)  child.getProperty ("note",   36);
                s.accent = (bool) child.getProperty ("accent", false);
                s.slide  = (bool) child.getProperty ("slide",  false);
            }

            p.sanitise();
            return p;
        }
    };

    //==============================================================================
    /** Factory content: eight acid patterns that also serve as the audible
        self-test of the sequencer. Index 0 is the classic 303 line, 7 the
        hardest one. All of them stay inside C1..C3.
    */
    inline Pattern makeDefaultPattern (int index)
    {
        struct DefaultLine
        {
            const char* name;
            std::array<int, 16> notes;
            std::array<int, 16> accents;  // step indices
            int numAccents;
            std::array<int, 16> slides;   // step indices
            int numSlides;
            std::array<int, 16> muted;    // step indices with gate closed
            int numMuted;
            int length;
        };

        // Compact helper data. -1 terminated by the count fields.
        static const DefaultLine lines[] =
        {
            { "Amber Line",
              { 36, 36, 48, 36, 39, 36, 43, 36, 36, 48, 36, 39, 36, 43, 36, 36 },
              { 1, 6, 10, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }, 3,
              { 4, 8, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }, 2,
              { 3, 11, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }, 2,
              16 },
            { "Sub Level",
              { 24, 24, 36, 24, 27, 24, 31, 24, 24, 36, 24, 27, 31, 24, 24, 24 },
              { 1, 5, 9, 13, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }, 4,
              { 2, 10, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }, 2,
              { 7, 15, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }, 2,
              16 },
            { "Neon Run",
              { 36, 48, 36, 43, 36, 48, 39, 36, 36, 43, 48, 36, 39, 36, 43, 36 },
              { 1, 3, 5, 7, 9, 11, 13, 15, -1,-1,-1,-1,-1,-1,-1,-1 }, 8,
              { 2, 6, 10, 14, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }, 4,
              { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }, 0,
              16 },
            { "Acid Rain",
              { 24, 36, 24, 31, 24, 36, 27, 24, 31, 36, 24, 27, 24, 31, 36, 24 },
              { 0, 4, 8, 12, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }, 4,
              { 1, 5, 9, 13, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }, 4,
              { 6, 14, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }, 2,
              16 },
            { "Blade Pulse",
              { 36, 36, 39, 36, 43, 36, 46, 43, 36, 36, 39, 43, 36, 36, 31, 36 },
              { 3, 7, 11, 15, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }, 4,
              { 4, 12, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }, 2,
              { 1, 9, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }, 2,
              16 },
            { "Rooftop Chase",
              { 48, 36, 48, 39, 48, 43, 48, 36, 46, 36, 46, 39, 43, 39, 36, 36 },
              { 0, 2, 6, 8, 12, 14, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }, 6,
              { 2, 6, 10, 14, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }, 4,
              { 4, 12, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }, 2,
              16 },
            { "Off World",
              { 24, 27, 24, 36, 24, 31, 27, 24, 36, 24, 27, 31, 24, 27, 24, 24 },
              { 1, 5, 9, 13, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }, 4,
              { 3, 7, 11, 15, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }, 4,
              { 2, 10, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }, 2,
              16 },
            { "Last Replicant",
              { 36, 31, 36, 43, 36, 31, 39, 36, 36, 31, 36, 43, 46, 43, 39, 36 },
              { 0, 3, 6, 9, 12, 15, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }, 6,
              { 1, 5, 9, 13, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }, 4,
              { 7, 14, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }, 2,
              16 }
        };

        constexpr int numLines = (int) (sizeof (lines) / sizeof (lines[0]));
        const auto& line = lines[juce::jlimit (0, numLines - 1, index)];

        Pattern p;
        p.length = juce::jlimit (1, Pattern::maxSteps, line.length);

        for (int i = 0; i < Pattern::maxSteps; ++i)
        {
            auto& s = p.steps[(size_t) i];
            s.gate   = true;
            s.note   = line.notes[(size_t) i];
            s.accent = false;
            s.slide  = false;
        }

        for (int i = 0; i < line.numAccents; ++i)
            if (line.accents[(size_t) i] >= 0)
                p[line.accents[(size_t) i]].accent = true;

        for (int i = 0; i < line.numSlides; ++i)
            if (line.slides[(size_t) i] >= 0)
                p[line.slides[(size_t) i]].slide = true;

        for (int i = 0; i < line.numMuted; ++i)
            if (line.muted[(size_t) i] >= 0)
                p[line.muted[(size_t) i]].gate = false;

        p.sanitise();
        return p;
    }
}
