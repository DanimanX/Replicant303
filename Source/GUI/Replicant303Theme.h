#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Replicant303
{
    //==============================================================================
    /** Global UI typography scale.

        Every hard-coded point size in the interface is multiplied by this factor,
        so the whole panel typography can be nudged from a single place.
    */
    inline constexpr float uiFontScale = 1.15f;

    /** Builds a FontOptions with the global UI scale already applied. */
    inline juce::FontOptions uiFont(float baseSize, int style = juce::Font::plain)
    {
        return juce::FontOptions(baseSize * uiFontScale, style);
    }

    //==============================================================================
    /** Complete palette + metadata for one selectable panel skin.

        A theme carries every colour that defines the identity of the hardware
        front panel: acrylic chassis, PCB underlay, silkscreen print, LEDs, the
        7-segment display, the knob metal and the tactile keys/buttons.
    */
    struct Theme
    {
        juce::String name;
        juce::String description;

        // Chassis (translucent acrylic housing)
        juce::Colour chassisUp;              // glaze top colour
        juce::Colour chassisDown;            // glaze bottom colour
        juce::Colour chassisHighlight;       // specular rim / embossed lettering
        juce::Colour chassisEmbossShadow;    // recessed logo drop shadow
        float chassisAlphaUp   = 1.0f;       // glaze translucency (top)
        float chassisAlphaDown = 1.0f;       // glaze translucency (bottom)

        // PCB underlay
        juce::Colour pcbDark;                // FR4 substrate
        juce::Colour pcbTrace;               // copper traces
        juce::Colour pcbVia;                 // plated through-hole pads
        juce::Colour pcbViaHole;             // drilled via centre

        // Moulded standoff bosses
        juce::Colour moldBoss;

        // Silkscreen print (all panel lettering)
        juce::Colour silkscreen;

        // Panel LEDs
        juce::Colour ledBright;
        juce::Colour ledDark;
        juce::Colour ledBezel;
        juce::Colour ledBezelDark;

        // 80s 7-segment tempo display
        juce::Colour displayChamber;
        juce::Colour displayBezelUp;
        juce::Colour displayBezelDown;
        juce::Colour displayCaption;
        juce::Colour displayLit;
        juce::Colour displayGlow;
        juce::Colour displayFilament;
        juce::Colour displayUnlit;

        // Knobs (fluted skirt + spun cap)
        juce::Colour knobCapBright;
        juce::Colour knobCapDark;
        juce::Colour knobRib;
        juce::Colour knobSkirtFlute;
        juce::Colour knobPointer;

        // Jack / screw hardware metal
        juce::Colour metalBright;
        juce::Colour metalMid;
        juce::Colour metalEdge;

        // Tactile sequencer buttons & slide switches
        juce::Colour buttonFaceIdle;
        juce::Colour buttonFaceLit;
        juce::Colour switchFaceOff;
        juce::Colour switchFaceOffBottom;
        juce::Colour switchFaceOnBottom;

        // Keyboard keys
        juce::Colour keyFaceUp;
        juce::Colour keyFaceDown;
        juce::Colour keyFacePressedUp;
        juce::Colour keyFacePressedDown;
        juce::Colour darkKeyFaceUp;
        juce::Colour darkKeyFaceDown;
        juce::Colour darkKeyFacePressedUp;
        juce::Colour darkKeyFacePressedDown;
    };

    //==============================================================================
    /** Holds the available skins and broadcasts whenever the active one changes. */
    class ThemeManager : public juce::ChangeBroadcaster
    {
    public:
        static ThemeManager& getInstance();

        static int getNumThemes();
        static juce::String getThemeName(int index);
        static juce::StringArray getThemeNames();

        const Theme& getCurrentTheme() const noexcept { return themes[(size_t) currentIndex]; }
        int getCurrentIndex() const noexcept { return currentIndex; }

        /** Selects a skin. Out-of-range indices are clamped. No-op if unchanged. */
        void setCurrentIndex(int index);

        /** Index <-> normalised helpers matching an AudioParameterChoice range. */
        static float indexToNormalized(int index);
        static int normalizedToIndex(float normalized);

    private:
        ThemeManager();

        std::vector<Theme> themes;
        int currentIndex = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThemeManager)
    };
}
