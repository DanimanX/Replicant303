#include "DistortionUnit.h"

namespace Replicant303
{
    DistortionUnit::DistortionUnit()
    {
        reset();
    }

    void DistortionUnit::prepare(double newSampleRate) noexcept
    {
        sampleRate = std::max(1000.0, newSampleRate);

        // Pre-highpass filter (~450Hz) to tighten low-end before saturation
        float rc = 1.0f / (2.0f * 3.14159265f * 450.0f);
        float dt = 1.0f / static_cast<float>(sampleRate);
        preHpAlpha = rc / (rc + dt);

        setTone(tone);
        reset();
    }

    void DistortionUnit::reset() noexcept
    {
        preHpState = 0.0f;
        postLpState = 0.0f;
        slewState = 0.0f;
    }

    void DistortionUnit::setDrive(float drive01) noexcept
    {
        drive = std::clamp(drive01, 0.0f, 1.0f);
    }

    void DistortionUnit::setTone(float tone01) noexcept
    {
        tone = std::clamp(tone01, 0.0f, 1.0f);

        // Post-filter cutoff ranges from 800 Hz (dark) to 16000 Hz (bright)
        float cutoffHz = 800.0f * std::pow(20.0f, tone);
        float rc = 1.0f / (2.0f * 3.14159265f * cutoffHz);
        float dt = 1.0f / static_cast<float>(sampleRate);
        postLpAlpha = dt / (rc + dt);
    }

    void DistortionUnit::setMix(float mix01) noexcept
    {
        mix = std::clamp(mix01, 0.0f, 1.0f);
    }

    // -------------------------------------------------------------------------
    // Distortion Models
    // -------------------------------------------------------------------------

    float DistortionUnit::processPocoRat(float in) noexcept
    {
        // 1. Rat Pre-filtering (Tightens the low end like the 700Hz/60Hz twin filters)
        float hpOut = preHpAlpha * (preHpState + in);
        preHpState = hpOut - in;

        // 2. Op-Amp Gain Stage (Up to ~46dB gain)
        float gain = 1.0f + (drive * drive * 40.0f);
        float amplified = hpOut * gain;

        // 3. Slew-Rate Limiting (Emulates the iconic LM308 slow op-amp)
        float maxSlew = 0.18f + (1.0f - drive) * 0.40f;
        float diff = amplified - slewState;
        slewState += std::clamp(diff, -maxSlew, maxSlew);

        // 4. Symmetrical Silicon Diode Clipping to Ground
        float clipped = fastTanh(slewState * 1.6f);

        return clipped * 0.85f;
    }

    float DistortionUnit::processTubeDrive(float in) noexcept
    {
        // TS-Style Soft Clipping (Diodes in op-amp negative feedback)
        float gain = 1.0f + drive * 24.0f;
        float driven = in * gain;

        // Soft cubic saturation with mid warmth
        float sat = softClip(driven);

        // Subtle asymmetry
        if (sat > 0.0f)
            sat = std::pow(sat, 0.94f);

        return sat * 0.90f;
    }

    float DistortionUnit::processHardClip(float in) noexcept
    {
        // DS-1 Style Hard Diode Clipping
        float gain = 1.0f + drive * 35.0f;
        float driven = in * gain;

        // Symmetrical hard clipping with sharp threshold
        float clipped = hardClip(driven * 0.8f, 0.95f);
        return fastTanh(clipped * 1.2f) * 0.85f;
    }

    float DistortionUnit::processGermaniumFuzz(float in) noexcept
    {
        // Fuzz Face Germanium Asymmetry and soft knee
        float gain = 1.0f + drive * 45.0f;
        float driven = in * gain;

        // Asymmetric transistor bias
        float biased = driven + 0.18f * (drive * drive);

        float clipped = 0.0f;
        if (biased > 0.0f)
            clipped = diodeClip(biased * 1.4f);
        else
            clipped = fastTanh(biased * 2.1f);

        return (clipped - 0.12f * drive) * 0.80f;
    }

    float DistortionUnit::processWavefolder(float in) noexcept
    {
        // West Coast Acid Folding
        float foldOut = wavefold(in, drive);
        return foldOut * 0.80f;
    }

    // -------------------------------------------------------------------------
    // Main Processing
    // -------------------------------------------------------------------------

    float DistortionUnit::processSample(float input) noexcept
    {
        if (!active)
            return input;

        float wet = 0.0f;
        switch (currentType)
        {
            case DistortionType::PocoRat:
                wet = processPocoRat(input);
                break;
            case DistortionType::TubeDrive:
                wet = processTubeDrive(input);
                break;
            case DistortionType::HardClip:
                wet = processHardClip(input);
                break;
            case DistortionType::GermaniumFuzz:
                wet = processGermaniumFuzz(input);
                break;
            case DistortionType::Wavefolder:
                wet = processWavefolder(input);
                break;
        }

        // Post Tone Filter (Low-pass smoothing)
        postLpState += postLpAlpha * (wet - postLpState);
        wet = postLpState;

        // Wet / Dry blend
        return (1.0f - mix) * input + (mix * wet);
    }
}
