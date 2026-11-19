#include "Oscillator303.h"

namespace Replicant303
{
    void Oscillator303::prepare(double newSampleRate) noexcept
    {
        sampleRate = std::max(1000.0, newSampleRate);
        // ~1 ms ramp at this rate: long enough to hide the table swap, short
        // enough to stay inside the slide that caused it.
        mipCrossfadeInc = static_cast<float>(1.0 / (0.001 * sampleRate));
        bank = &WavetableBank::getInstance();

        setWaveform(static_cast<Waveform>(waveformIndex));
        setFrequency(targetFreq);
    }

    void Oscillator303::reset() noexcept
    {
        phase = 0.0f;
        mipCrossfade = 1.0f;
        previousTable = nullptr;
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
        // Tuning uses the factor cached by setTuningSemi: automation would
        // otherwise drag a powf() into every sample of the audio callback.
        const float tunedFreq = targetFreq * tuningFactor;
        actualFreq = std::clamp(tunedFreq, 10.0f, static_cast<float>(sampleRate * 0.45));
        phaseInc = actualFreq / static_cast<float>(sampleRate);

        updateMipLevel();
    }

    void Oscillator303::setTuningSemi(float semi) noexcept
    {
        tuningSemi = semi;
        tuningFactor = std::pow (2.0f, semi / 12.0f);
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

        if (level == mipLevel && currentTable != nullptr)
            return;

        // Changing the table mid-note is only safe if the two tables agree at
        // the current phase, and they do not: the band-limited versions differ.
        // A slide crosses an octave boundary in about a millisecond, so swap in
        // the new level and crossfade out of the old one instead of jumping.
        if (bank != nullptr && currentTable != nullptr)
        {
            previousTable = currentTable;
            mipCrossfade = 0.0f;
        }

        mipLevel = level;

        if (bank != nullptr)
            currentTable = bank->table(waveformIndex, mipLevel);
    }

    float Oscillator303::renderTable(const float* table) noexcept
    {
        const float position = phase * static_cast<float>(tableSize);
        int index0 = static_cast<int>(position);

        if (index0 < 0)
            index0 = 0;
        else if (index0 >= tableSize)
            index0 = tableSize - 1;

        const int index1 = (index0 + 1) & (tableSize - 1);
        const float fraction = position - static_cast<float>(index0);

        const float a = table[index0];
        const float b = table[index1];

        return a + fraction * (b - a);
    }

    float Oscillator303::process() noexcept
    {
        phase += phaseInc;

        if (phase >= 1.0f)
            phase -= 1.0f;

        if (currentTable == nullptr)
            return 0.0f;

        float out = renderTable(currentTable);

        if (mipCrossfade < 1.0f && previousTable != nullptr)
        {
            const float old = renderTable(previousTable);
            mipCrossfade = std::min(1.0f, mipCrossfade + mipCrossfadeInc);
            out = old + (out - old) * mipCrossfade;

            if (mipCrossfade >= 1.0f)
                previousTable = nullptr;
        }

        return out;
    }
}
