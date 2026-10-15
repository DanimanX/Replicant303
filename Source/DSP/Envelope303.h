#pragma once

#include "Common303.h"

namespace Replicant303
{
    class Envelope303
    {
    public:
        Envelope303() = default;
        ~Envelope303() = default;

        void prepare(double newSampleRate) noexcept
        {
            sampleRate = std::max(1000.0, newSampleRate);
            updateCoefficients();
        }

        void reset() noexcept
        {
            vcfEnv = 0.0f;
            vcaEnv = 0.0f;
            accentEnv = 0.0f;
            gate = false;
        }

        void setDecayTime(float seconds) noexcept
        {
            decayTimeSec = std::clamp(seconds, 0.15f, 2.8f);
            updateCoefficients();
        }

        // Trigger note (isSlide = true if legato overlap)
        void noteOn(bool isAccented, bool isSlide) noexcept
        {
            gate = true;
            hasAccent = isAccented;

            // In 303: If note is a slide, envelope is NOT retriggered
            if (!isSlide)
            {
                // Instant attack (charges capacitor quickly ~3ms)
                vcfEnv = 1.0f;
                vcaEnv = 1.0f;
                if (isAccented)
                {
                    accentEnv = 1.0f;
                }
            }
            else
            {
                // In slide, if it was already dying down, it continues from its current state
                if (isAccented && accentEnv < 0.2f)
                {
                    accentEnv = 0.8f;
                }
            }
        }

        void noteOff() noexcept
        {
            gate = false;
        }

        // Advance one sample and compute envelope values
        void process(float& outVcfEnv, float& outVcaEnv, float& outAccentEnv) noexcept
        {
            // 1. VCF Decay:
            // When accented, the main decay is forced to be shorter and punchier (~200ms)
            float activeVcfDecayCoeff = hasAccent ? accentDecayCoeff : normalDecayCoeff;
            vcfEnv *= activeVcfDecayCoeff;

            // 2. Accent envelope (very fast decay ~180ms)
            accentEnv *= accentDecayCoeff;

            // 3. VCA Envelope (fast release if gate is closed, otherwise holds or long tail)
            if (gate)
            {
                // While gate is high, sustain/decay gently
                vcaEnv *= vcaHoldDecayCoeff;
            }
            else
            {
                // Gate closed: fast natural release ~25ms
                vcaEnv *= vcaReleaseCoeff;
            }

            outVcfEnv = vcfEnv;
            outVcaEnv = vcaEnv;
            outAccentEnv = accentEnv;
        }

        bool isActive() const noexcept
        {
            return (vcaEnv > 0.0001f) || gate;
        }

    private:
        void updateCoefficients() noexcept
        {
            float dt = 1.0f / static_cast<float>(sampleRate);

            // Normal adjustable decay (200ms to 2.5s)
            normalDecayCoeff = std::exp(-dt / decayTimeSec);

            // Accent decay: fixed short time constant (~0.20s)
            accentDecayCoeff = std::exp(-dt / 0.18f);

            // VCA while gate held: subtle natural decay (~4.0s)
            vcaHoldDecayCoeff = std::exp(-dt / 4.0f);

            // VCA release when key released (~0.025s)
            vcaReleaseCoeff = std::exp(-dt / 0.025f);
        }

        double sampleRate = 44100.0;
        float decayTimeSec = 0.8f;

        float normalDecayCoeff = 0.999f;
        float accentDecayCoeff = 0.999f;
        float vcaHoldDecayCoeff = 0.9999f;
        float vcaReleaseCoeff = 0.99f;

        float vcfEnv = 0.0f;
        float vcaEnv = 0.0f;
        float accentEnv = 0.0f;

        bool gate = false;
        bool hasAccent = false;
    };
}
