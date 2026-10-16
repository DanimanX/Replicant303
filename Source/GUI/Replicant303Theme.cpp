#include "Replicant303Theme.h"

namespace Replicant303
{
    //==============================================================================
    // Skin 1: Behringer TD-3 Translucent Green (original panel, palette sampled
    //         directly from the hardware and unchanged since the first release).
    //==============================================================================
    static Theme makeTranslucentGreenTheme()
    {
        const juce::Colour acrylicGreen     = juce::Colour(0xff1ec836);

        Theme t;
        t.name        = "TD-3 Green";
        t.description = "Behringer TD-3 translucent green acrylic";

        t.chassisUp           = acrylicGreen;
        t.chassisDown         = acrylicGreen.darker(0.22f);
        t.chassisHighlight    = juce::Colour(0xff6ef979);
        t.chassisEmbossShadow = juce::Colour(0xff092810);
        t.chassisAlphaUp      = 0.68f;
        t.chassisAlphaDown    = 0.74f;

        t.pcbDark     = juce::Colour(0xff0a3313);
        t.pcbTrace    = juce::Colour(0xff2cc246);
        t.pcbVia      = juce::Colour(0xff45f058);
        t.pcbViaHole  = juce::Colour(0xff061f0c);

        t.moldBoss    = juce::Colour(0xff42ea56);
        t.silkscreen  = juce::Colour(0xffffffff);

        t.ledBright     = juce::Colour(0xffff2028);
        t.ledDark       = juce::Colour(0xff4a080c);
        t.ledBezel      = juce::Colour(0xff6a726c);
        t.ledBezelDark  = juce::Colour(0xff3a423d);

        t.displayChamber   = juce::Colour(0xff050706);
        t.displayBezelUp   = juce::Colour(0xff28302a);
        t.displayBezelDown = juce::Colour(0xff101411);
        t.displayCaption   = juce::Colour(0xffb8c2ba);
        t.displayLit       = juce::Colour(0xffff2832);
        t.displayGlow      = juce::Colour(0xffff1220);
        t.displayFilament  = juce::Colour(0xffffd0d8);
        t.displayUnlit     = juce::Colour(0xff220407);

        t.knobCapBright   = juce::Colour(0xfff0f4f2);
        t.knobCapDark     = juce::Colour(0xff7c857e);
        t.knobRib         = juce::Colour(0xff222723);
        t.knobSkirtFlute  = juce::Colour(0xff161a17);
        t.knobPointer     = juce::Colour(0xff111412);

        t.metalBright = juce::Colour(0xffc5cec8);
        t.metalMid    = juce::Colour(0xff222824);
        t.metalEdge   = juce::Colour(0xff454d47);

        t.buttonFaceIdle        = juce::Colour(0xffa8b0aa);
        t.buttonFaceLit         = juce::Colour(0xffdbe2dd);
        t.switchFaceOff         = juce::Colour(0xff8a948d);
        t.switchFaceOffBottom   = juce::Colour(0xff4a524c);
        t.switchFaceOnBottom    = juce::Colour(0xff9aa29c);

        t.keyFaceUp             = juce::Colour(0xfff2f6f3);
        t.keyFaceDown           = juce::Colour(0xffbec6c0);
        t.keyFacePressedUp      = juce::Colour(0xffc5cdc7);
        t.keyFacePressedDown    = juce::Colour(0xff8a928c);

        t.darkKeyFaceUp          = juce::Colour(0xff3a403b);
        t.darkKeyFaceDown        = juce::Colour(0xff1c201e);
        t.darkKeyFacePressedUp   = juce::Colour(0xff181c19);
        t.darkKeyFacePressedDown = juce::Colour(0xff0c0f0d);

        return t;
    }

    //==============================================================================
    // Skin 2: Obsidian Red - black anodised acrylic panel with red silkscreen
    //         lettering, red copper traces and red knob pointers.
    //==============================================================================
    static Theme makeObsidianRedTheme()
    {
        Theme t;
        t.name        = "Obsidian Red";
        t.description = "Black acrylic panel with red silkscreen";

        t.chassisUp           = juce::Colour(0xff141518);
        t.chassisDown         = juce::Colour(0xff0a0b0d);
        t.chassisHighlight    = juce::Colour(0xffd01f27);
        t.chassisEmbossShadow = juce::Colour(0xff000000);
        t.chassisAlphaUp      = 0.82f;
        t.chassisAlphaDown    = 0.88f;

        t.pcbDark     = juce::Colour(0xff0b0b0d);
        t.pcbTrace    = juce::Colour(0xff8e1f26);
        t.pcbVia      = juce::Colour(0xffd92a33);
        t.pcbViaHole  = juce::Colour(0xff160406);

        t.moldBoss    = juce::Colour(0xffc22a31);
        t.silkscreen  = juce::Colour(0xffff3b42);

        t.ledBright     = juce::Colour(0xffff3b42);
        t.ledDark       = juce::Colour(0xff3a0508);
        t.ledBezel      = juce::Colour(0xff5a2024);
        t.ledBezelDark  = juce::Colour(0xff2a1416);

        t.displayChamber   = juce::Colour(0xff050506);
        t.displayBezelUp   = juce::Colour(0xff2a1416);
        t.displayBezelDown = juce::Colour(0xff120809);
        t.displayCaption   = juce::Colour(0xff9a7f82);
        t.displayLit       = juce::Colour(0xffff3b42);
        t.displayGlow      = juce::Colour(0xffff1220);
        t.displayFilament  = juce::Colour(0xffffd0d8);
        t.displayUnlit     = juce::Colour(0xff220407);

        t.knobCapBright   = juce::Colour(0xffaab2b6);
        t.knobCapDark     = juce::Colour(0xff3c4348);
        t.knobRib         = juce::Colour(0xff1a1d20);
        t.knobSkirtFlute  = juce::Colour(0xff0f1113);
        t.knobPointer     = juce::Colour(0xffff3b42);

        t.metalBright = juce::Colour(0xff9aa2a6);
        t.metalMid    = juce::Colour(0xff1b1e21);
        t.metalEdge   = juce::Colour(0xff2c3034);

        t.buttonFaceIdle        = juce::Colour(0xff5a6165);
        t.buttonFaceLit         = juce::Colour(0xffb9c0c4);
        t.switchFaceOff         = juce::Colour(0xff40464a);
        t.switchFaceOffBottom   = juce::Colour(0xff202427);
        t.switchFaceOnBottom    = juce::Colour(0xff6a7175);

        t.keyFaceUp             = juce::Colour(0xffd7dcde);
        t.keyFaceDown           = juce::Colour(0xff8b9396);
        t.keyFacePressedUp      = juce::Colour(0xffa9b1b4);
        t.keyFacePressedDown    = juce::Colour(0xff60686b);

        t.darkKeyFaceUp          = juce::Colour(0xff2c3033);
        t.darkKeyFaceDown        = juce::Colour(0xff14171a);
        t.darkKeyFacePressedUp   = juce::Colour(0xff121517);
        t.darkKeyFacePressedDown = juce::Colour(0xff08090b);

        return t;
    }

    //==============================================================================
    ThemeManager& ThemeManager::getInstance()
    {
        static ThemeManager instance;
        return instance;
    }

    ThemeManager::ThemeManager()
    {
        themes.push_back(makeTranslucentGreenTheme());
        themes.push_back(makeObsidianRedTheme());
    }

    int ThemeManager::getNumThemes()
    {
        return (int) getInstance().themes.size();
    }

    juce::String ThemeManager::getThemeName(int index)
    {
        auto& tm = getInstance();
        return tm.themes[(size_t) juce::jlimit(0, (int) tm.themes.size() - 1, index)].name;
    }

    juce::StringArray ThemeManager::getThemeNames()
    {
        juce::StringArray names;
        for (int i = 0; i < getNumThemes(); ++i)
            names.add(getThemeName(i));
        return names;
    }

    void ThemeManager::setCurrentIndex(int index)
    {
        const int clamped = juce::jlimit(0, (int) themes.size() - 1, index);
        if (clamped == currentIndex)
            return;

        currentIndex = clamped;
        sendChangeMessage();
    }

    float ThemeManager::indexToNormalized(int index)
    {
        const int n = getNumThemes();
        if (n <= 1)
            return 0.0f;
        return (float) juce::jlimit(0, n - 1, index) / (float) (n - 1);
    }

    int ThemeManager::normalizedToIndex(float normalized)
    {
        const int n = getNumThemes();
        if (n <= 1)
            return 0;
        return juce::jlimit(0, n - 1, juce::roundToInt(normalized * (float) (n - 1)));
    }
}
