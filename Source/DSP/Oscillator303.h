#pragma once

#include "Common303.h"

namespace Replicant303
{
    /** Band-limited wavetable VCO covering the 60-shape bank.

        Playback is interpolated and mip-mapped: the oscillator picks the table
        whose highest harmonic still falls below Nyquist for the note being
        played, so high notes stay clean instead of aliasing back down.
    */
    class Oscillator303
    {
    public:
        Oscillator303() = default;
        ~Oscillator303() = default;

        void prepare(double sampleRate) noexcept;
        void reset() noexcept;

        void setWaveform(Waveform wave) noexcept;
        void setFrequency(float freqHz) noexcept;
        void setTuningSemi(float semi) noexcept;

        int getWaveformIndex() const noexcept { return waveformIndex; }
        int getMipLevel() const noexcept { return mipLevel; }

        /** True while a mip change is still fading out of the previous table.
            The panel never sees this; it exists so the regression suite can
            prove a level change is smoothed rather than a hard jump. */
        bool isMipCrossfading() const noexcept
        {
            return mipCrossfade < 1.0f && previousTable != nullptr;
        }

        // Render one sample of the selected shape
        float process() noexcept;

    private:
        static constexpr int tableSize = WavetableBank::tableSize;

        double sampleRate = 44100.0;

        int waveformIndex = 0;
        const WavetableBank* bank = nullptr;
        const float* currentTable = nullptr;
        int mipLevel = 0;

        float targetFreq = 110.0f;
        float actualFreq = 110.0f;
        float tuningSemi = 0.0f;
        float tuningFactor = 1.0f; // pow(2, semi/12), cached so the audio
                                   // callback never runs a transcendental

        float phase = 0.0f;
        float phaseInc = 0.0f;

        /** 0..1 crossfade weight for the previous table after a mip change. */
        float mipCrossfade = 1.0f;
        /** Per-sample step of the crossfade: ~1 ms at the current rate. */
        float mipCrossfadeInc = 0.0f;
        const float* previousTable = nullptr;

        void updateMipLevel() noexcept;
        float renderTable(const float* table) noexcept;
    };
}
