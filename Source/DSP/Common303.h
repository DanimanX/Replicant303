#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>

namespace Replicant303
{
    enum class Waveform : int
    {
        Sawtooth = 0,
        Square   = 1
    };

    enum class DistortionType : int
    {
        PocoRat       = 0, // ProCo Rat emulation with inverted low-pass filter
        TubeDrive     = 1, // Symmetrical soft-clip overdrive with mid-hump
        HardClip      = 2, // Hard diode clipper (DS-1 style)
        GermaniumFuzz = 3, // Asymmetrical fuzz with warm saturation
        Wavefolder    = 4  // Buchla-style multi-stage wavefolder
    };

    // Math & DSP Utilities
    inline float midiToFreq(float midiNote) noexcept
    {
        return 440.0f * std::pow(2.0f, (midiNote - 69.0f) / 12.0f);
    }

    // Fast tanh approximation with smooth saturation
    inline float fastTanh(float x) noexcept
    {
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    // Diode non-linearity approximation: v / sqrt(1 + v^2)
    inline float diodeClip(float x) noexcept
    {
        return x / std::sqrt(1.0f + x * x);
    }

    // Hard clip with variable threshold
    inline float hardClip(float x, float threshold = 1.0f) noexcept
    {
        return std::clamp(x, -threshold, threshold);
    }

    // Cubic soft saturation
    inline float softClip(float x) noexcept
    {
        if (x <= -1.5f) return -1.0f;
        if (x >=  1.5f) return  1.0f;
        return x - (4.0f / 27.0f) * (x * x * x);
    }

    // Wavefolder function (West Coast style folding)
    inline float wavefold(float x, float foldAmount) noexcept
    {
        float gain = 1.0f + foldAmount * 5.0f;
        float scaled = x * gain;
        // Periodic triangular folding
        float folded = 4.0f * std::abs(0.25f * scaled - std::floor(0.25f * scaled + 0.75f) + 0.25f) - 1.0f;
        return softClip(folded);
    }
}
