#pragma once

#include "Common303.h"

namespace Replicant303
{
    class DiodeLadderFilter
    {
    public:
        DiodeLadderFilter();
        ~DiodeLadderFilter() = default;

        void prepare(double sampleRate) noexcept;
        void reset() noexcept;

        // Set baseline parameters
        void setCutoff(float cutoffHz) noexcept;
        void setResonance(float res01) noexcept; // 0.0 to 1.0 (self-oscillation near 0.95 - 1.0)

        // Process a single sample with envelope & accent modulation
        // envMod01: 0.0 to 1.0 envelope amount
        // envVal: current normalized envelope value (0.0 to 1.0)
        // accentVal: 0.0 to 1.0 (adds extra cutoff boost and squelch)
        float process(float input, float envMod01, float envVal, float accentVal) noexcept;

    private:
        double sampleRate = 44100.0;
        float baseCutoffHz = 400.0f;
        float resonance = 0.5f;

        // 4 filter stage states (capacitors)
        float s[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

        // Highpass decoupling state (for authentic 303 ~18dB/oct slope character)
        float hpState = 0.0f;
        float hpAlpha = 0.995f;

        // Thermal voltage constant scaling for non-linear saturation
        static constexpr float VT = 0.026f;
        static constexpr float INV_VT = 1.0f / VT;
    };
}
