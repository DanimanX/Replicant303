#include "DiodeLadderFilter.h"

namespace Replicant303
{
    DiodeLadderFilter::DiodeLadderFilter()
    {
        reset();
    }

    void DiodeLadderFilter::prepare(double newSampleRate) noexcept
    {
        sampleRate = std::max(1000.0, newSampleRate);
        // High-pass coupling capacitor between 303 feedback and stages (~40Hz cutoff)
        float rc = 1.0f / (2.0f * 3.14159265f * 40.0f);
        float dt = 1.0f / static_cast<float>(sampleRate);
        hpAlpha = rc / (rc + dt);

        reset();
    }

    void DiodeLadderFilter::reset() noexcept
    {
        for (int i = 0; i < 4; ++i)
            s[i] = 0.0f;
        hpState = 0.0f;
    }

    void DiodeLadderFilter::setCutoff(float cutoffHz) noexcept
    {
        baseCutoffHz = std::clamp(cutoffHz, 25.0f, 4800.0f);
    }

    void DiodeLadderFilter::setResonance(float res01) noexcept
    {
        resonance = std::clamp(res01, 0.0f, 1.0f);
    }

    float DiodeLadderFilter::process(float input, float envMod01, float envVal, float accentVal) noexcept
    {
        // 1. Calculate modulated cutoff frequency
        // 303 Envelope Mod can sweep up to ~5 octaves, and accent adds extra ~2 octaves squelch
        float octavesMod = (envMod01 * envVal * 5.2f) + (accentVal * 2.2f);
        float modCutoff = baseCutoffHz * std::pow(2.0f, octavesMod);
        float maxFreq = static_cast<float>(sampleRate * 0.42);
        modCutoff = std::clamp(modCutoff, 25.0f, maxFreq);

        // Normalized digital frequency coefficient: g = tan(pi * fc / fs)
        float wd = 2.0f * 3.14159265f * modCutoff;
        float t = 1.0f / static_cast<float>(sampleRate);
        float wa = (2.0f / t) * std::tan(wd * t * 0.5f);
        float g = wa * t * 0.5f;
        g = std::clamp(g, 0.001f, 0.95f);

        // Resonance feedback gain (303 self-oscillates at high resonance)
        // Authentic 303 resonance factor ranges from 0 to ~17.5
        float k = resonance * 17.2f;

        // Bass loss compensation parameter typical of 303 diode ladder
        float drive = 1.0f + resonance * 0.4f;

        // Perform 2x internal oversampling sub-steps for smooth diode non-linear behavior
        float out = 0.0f;
        float subG = g * 0.5f;

        for (int step = 0; step < 2; ++step)
        {
            // Feedback with 303 high-pass AC decoupling
            float fbSignal = s[3];
            float hpOut = hpAlpha * (hpState + fbSignal);
            hpState = hpOut - fbSignal;

            // Diode feedback clipping with soft saturation
            float saturatedFeedback = fastTanh(k * hpOut);

            // Input stage with non-linear diode subtraction
            float u = fastTanh((input * drive) - saturatedFeedback);

            // 4 cascaded diode ladder integrator stages with tanh saturation per diode pair:
            // Stage 0
            float du0 = subG * (u - fastTanh(s[0]));
            s[0] += du0;
            s[0] = std::clamp(s[0], -4.0f, 4.0f);

            // Stage 1
            float du1 = subG * (fastTanh(s[0]) - fastTanh(s[1]));
            s[1] += du1;
            s[1] = std::clamp(s[1], -4.0f, 4.0f);

            // Stage 2
            float du2 = subG * (fastTanh(s[1]) - fastTanh(s[2]));
            s[2] += du2;
            s[2] = std::clamp(s[2], -4.0f, 4.0f);

            // Stage 3
            float du3 = subG * (fastTanh(s[2]) - fastTanh(s[3]));
            s[3] += du3;
            s[3] = std::clamp(s[3], -4.0f, 4.0f);

            out = s[3];
        }

        // Output scaling with subtle diode output saturation
        return fastTanh(out * 1.5f);
    }
}
