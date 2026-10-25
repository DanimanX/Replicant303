#include "Oscillator303.h"

namespace Replicant303
{
    void Oscillator303::prepare(double newSampleRate) noexcept
    {
        sampleRate = std::max(1000.0, newSampleRate);
        bank = &WavetableBank::getInstance();

        setWaveform(static_cast<Waveform>(waveformIndex));
        setFrequency(targetFreq);
    }

    void Oscillator303::reset() noexcept
    {
        phase = 0.0f;
    }

    void Oscillator303::setWaveform(Waveform wave) noexcept
    {
        waveformIndex = std::clamp(static_cast<int>(wave), 0, numWaveforms - 1);

        if (bank != nullptr)
            currentTable = bank->table(waveformIndex, mipLevel);
    }

    void Oscillator303::setFrequency(float freqHz) noexcept
    {
        targetFreq = freqHz;
        const float tunedFreq = targetFreq * std::pow(2.0f, tuningSemi / 12.0f);
        actualFreq = std::clamp(tunedFreq, 10.0f, static_cast<float>(sampleRate * 0.45));
        phaseInc = actualFreq / static_cast<float>(sampleRate);

        updateMipLevel();
    }

    void Oscillator303::setTuningSemi(float semi) noexcept
    {
        tuningSemi = semi;
        setFrequency(targetFreq);
    }

    void Oscillator303::updateMipLevel() noexcept
    {
        // Highest harmonic that still fits under Nyquist for this note. 0.45
        // leaves a small guard band instead of sitting right on the edge.
        const float allowed = 0.45f * static_cast<float>(sampleRate) / std::max(1.0f, actualFreq);

        int level = 0;
        int harmonics = WavetableBank::maxHarmonic;

        while (harmonics > allowed && level < WavetableBank::numMips - 1)
        {
            harmonics >>= 1;
            ++level;
        }

        mipLevel = level;

        if (bank != nullptr)
            currentTable = bank->table(waveformIndex, mipLevel);
    }

    float Oscillator303::process() noexcept
    {
        phase += phaseInc;

        if (phase >= 1.0f)
            phase -= 1.0f;

        if (currentTable == nullptr)
            return 0.0f;

        // Linear interpolation between table samples.
        const float position = phase * static_cast<float>(tableSize);
        int index0 = static_cast<int>(position);

        if (index0 < 0)
            index0 = 0;
        else if (index0 >= tableSize)
            index0 = tableSize - 1;

        const int index1 = (index0 + 1) & (tableSize - 1);
        const float fraction = position - static_cast<float>(index0);

        const float a = currentTable[index0];
        const float b = currentTable[index1];

        return a + fraction * (b - a);
    }
}
