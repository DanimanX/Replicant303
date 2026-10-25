#pragma once

#include "Common303.h"

namespace Replicant303
{
    class Oscillator303
    {
    public:
        Oscillator303();
        ~Oscillator303() = default;

        void prepare(double sampleRate) noexcept;
        void reset() noexcept;

        void setWaveform(Waveform wave) noexcept { currentWaveform = wave; }
        Waveform getWaveform() const noexcept { return currentWaveform; }
        void setFrequency(float freqHz) noexcept;
        void setTuningSemi(float semi) noexcept;

        // Render one sample of authentic 303 waveform
        float process() noexcept;

    private:
        double sampleRate = 44100.0;
        Waveform currentWaveform = Waveform::Sawtooth;

        float targetFreq = 110.0f;
        float actualFreq = 110.0f;
        float tuningSemi = 0.0f;

        float phase = 0.0f;
        float phaseInc = 0.0f;

        // Square wave 303 RC droop high-pass filter state
        float sqrHpState = 0.0f;
        float sqrHpCoeff = 0.992f;

        // Anti-aliasing PolyBLEP helper
        static float polyBlep(float t, float dt) noexcept;
    };
}
