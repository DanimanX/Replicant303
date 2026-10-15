#pragma once

#include "Common303.h"

namespace Replicant303
{
    class DistortionUnit
    {
    public:
        DistortionUnit();
        ~DistortionUnit() = default;

        void prepare(double sampleRate) noexcept;
        void reset() noexcept;

        void setActive(bool isActive) noexcept { active = isActive; }
        void setType(DistortionType type) noexcept { currentType = type; }
        void setDrive(float drive01) noexcept;
        void setTone(float tone01) noexcept;
        void setMix(float mix01) noexcept;

        // Process a single audio sample
        float processSample(float input) noexcept;

    private:
        double sampleRate = 44100.0;
        bool active = true;
        DistortionType currentType = DistortionType::PocoRat;

        float drive = 0.5f;
        float tone = 0.5f;
        float mix = 1.0f; // Wet / Dry balance

        // Internal filter states for tone sculpting:
        // Pre-emphasis highpass & Mid-hump filter
        float preHpState = 0.0f;
        float preHpAlpha = 0.98f;

        // Post-distortion lowpass filter (Rat filter / Tone control)
        float postLpState = 0.0f;
        float postLpAlpha = 0.85f;

        // Op-amp slew rate limiting state for authentic LM308 Rat response
        float slewState = 0.0f;

        // Model processors
        float processPocoRat(float in) noexcept;
        float processTubeDrive(float in) noexcept;
        float processHardClip(float in) noexcept;
        float processGermaniumFuzz(float in) noexcept;
        float processWavefolder(float in) noexcept;
    };
}
