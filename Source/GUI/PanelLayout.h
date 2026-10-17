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

    /** Header tab that opens the slide-down pattern drawer. */
    inline constexpr float seqTabX = 778.0f;
    inline constexpr float seqTabY = 11.0f;
    inline constexpr float seqTabW = 124.0f;
    inline constexpr float seqTabH = 27.0f;

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
