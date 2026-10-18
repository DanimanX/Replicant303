#pragma once

namespace Replicant303::Layout
{
    //==============================================================================
    /** Fixed geometry shared by painting and resizing.

        The panel is a piece of hardware: its proportions never change, so the
        numbers live in one place instead of being duplicated between paint()
        and resized().
    */
    inline constexpr int panelWidth  = 1080;
    inline constexpr int panelHeight = 590;

    /** Supported UI zoom factor range (50% to 300%). */
    inline constexpr float minZoomFactor     = 0.50f;
    inline constexpr float maxZoomFactor     = 3.00f;
    inline constexpr float defaultZoomFactor = 1.00f;

    /** Header digital tempo display and controls. */
    inline constexpr float tempoBoxX = 446.0f;
    inline constexpr float tempoBoxY = 11.0f;
    inline constexpr float tempoBoxW = 132.0f;
    inline constexpr float tempoBoxH = 32.0f;

    inline constexpr int themeBoxX = 586;
    inline constexpr int themeBoxY = 13;
    inline constexpr int themeBoxW = 118;
    inline constexpr int themeBoxH = 25;

    /** Header tab that opens the slide-down pattern drawer. */
    inline constexpr float seqTabX = 712.0f;
    inline constexpr float seqTabY = 11.0f;
    inline constexpr float seqTabW = 114.0f;
    inline constexpr float seqTabH = 27.0f;

    /** Header zoom selector (50% - 300%). */
    inline constexpr int zoomBoxX = 834;
    inline constexpr int zoomBoxY = 13;
    inline constexpr int zoomBoxW = 86;
    inline constexpr int zoomBoxH = 25;

    //==============================================================================
    /** 13-key tactile keyboard geometry. */
    namespace Keybed
    {
        inline constexpr float whiteKeyW = 38.0f;
        inline constexpr float whiteKeyH = 88.0f;
        inline constexpr float blackKeyW = 28.0f;
        inline constexpr float blackKeyH = 56.0f;
        inline constexpr float startX    = 202.0f;
        inline constexpr float whiteKeyY = 412.0f;
        inline constexpr float blackKeyY = 306.0f;
        inline constexpr float stride    = 46.0f;
    }
}
