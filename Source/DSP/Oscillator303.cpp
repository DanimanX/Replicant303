#include "Oscillator303.h"

namespace Replicant303
{
    Oscillator303::Oscillator303()
    {
        reset();
    }

    void Oscillator303::prepare(double newSampleRate) noexcept
    {
        sampleRate = std::max(1000.0, newSampleRate);
        // RC droop coefficient for 303 square wave high-pass characteristic (~25Hz cutoff)
        float rc = 1.0f / (2.0f * 3.14159265f * 25.0f);
        float dt = 1.0f / static_cast<float>(sampleRate);
        sqrHpCoeff = rc / (rc + dt);

        setFrequency(targetFreq);
    }

    void Oscillator303::reset() noexcept
    {
        phase = 0.0f;
        sqrHpState = 0.0f;
    }

    void Oscillator303::setFrequency(float freqHz) noexcept
    {
        targetFreq = freqHz;
        float tunedFreq = targetFreq * std::pow(2.0f, tuningSemi / 12.0f);
        actualFreq = std::clamp(tunedFreq, 10.0f, static_cast<float>(sampleRate * 0.45));
        phaseInc = actualFreq / static_cast<float>(sampleRate);
    }

    void Oscillator303::setTuningSemi(float semi) noexcept
    {
        tuningSemi = semi;
        setFrequency(targetFreq);
    }

    float Oscillator303::polyBlep(float t, float dt) noexcept
    {
        if (dt <= 0.0f) return 0.0f;
        // 0 <= t < dt
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        // 1 - dt < t <= 1
        else if (t > 1.0f - dt)
        {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }

    float Oscillator303::process() noexcept
    {
        // Advance phase
        phase += phaseInc;
        if (phase >= 1.0f)
            phase -= 1.0f;

        float dt = phaseInc;

        if (currentWaveform == Waveform::Sawtooth)
        {
            // 303 Sawtooth:
            // The raw sawtooth goes from -1 to +1 with subtle exponential curvature
            // x_exp = (1 - exp(-a * t)) / (1 - exp(-a))
            // Here modeled with slight parabolic curvature typical of 303 capacitor discharge:
            float naiveSaw = 2.0f * phase - 1.0f;
            // Add subtle analog curve
            float curvedSaw = naiveSaw - 0.15f * (naiveSaw * naiveSaw - 1.0f);
            
            // Apply bandlimiting PolyBLEP at the reset point
            curvedSaw -= polyBlep(phase, dt);

            // Small 303 DC offset compensation
            return curvedSaw * 0.95f;
        }
        else
        {
            // 303 Square:
            // Derived from saw via comparator (~50% duty cycle, slightly offset to 48%)
            float naiveSqr = (phase < 0.485f) ? 1.0f : -1.0f;

            // Apply PolyBLEP to both transitions (0.0 and 0.485)
            naiveSqr += polyBlep(phase, dt);
            float phaseShifted = phase - 0.485f;
            if (phaseShifted < 0.0f) phaseShifted += 1.0f;
            naiveSqr -= polyBlep(phaseShifted, dt);

            // Iconic 303 RC droop high-pass filter:
            // High-pass filter y[n] = alpha * (y[n-1] + x[n] - x[n-1])
            float hpOut = sqrHpCoeff * (sqrHpState + naiveSqr);
            sqrHpState = hpOut - naiveSqr;

            // Scale to match level
            return hpOut * 0.90f;
        }
    }
}
