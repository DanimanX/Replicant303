#include "WaveformBank.h"

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>

namespace Replicant303
{
namespace
{
    constexpr double twoPi = 6.283185307179586476925286766559;

    double wrap(double t) noexcept
    {
        return t - std::floor(t);
    }

    double saw(double t) noexcept
    {
        return 2.0 * wrap(t) - 1.0;
    }

    /** Triangle that swings -1..+1 with its crest at phase 0.5. */
    double triangle(double t) noexcept
    {
        return 1.0 - 4.0 * std::abs(wrap(t) - 0.5);
    }

    /** 303 square: 48.5% duty plus the RC droop of the original shaper. */
    double square303(double t) noexcept
    {
        const double s = (wrap(t) < 0.485) ? 1.0 : -1.0;
        return s - 0.10 * triangle(t);
    }

    double pulse(double t, double width) noexcept
    {
        return (wrap(t) < width) ? 1.0 : -1.0;
    }

    /** Identity inside [-1, 1], mirrored outside: a West-coast folder. */
    double fold(double x) noexcept
    {
        return 4.0 * std::abs(0.25 * x - std::floor(0.25 * x + 0.75) + 0.25) - 1.0;
    }

    double gaussian(double f, double centre, double bandwidth) noexcept
    {
        const double d = (f - centre) / bandwidth;
        return std::exp(-0.5 * d * d);
    }

    struct FormantBand { double freq, bandwidth, gain; };

    /** Vowel-like additive shape: a 110 Hz glottal source through three
        resonant formant bands. */
    double formantShape(double t, int vowel) noexcept
    {
        static const FormantBand vowels[3][3] =
        {
            { { 730.0,  90.0, 1.00 }, { 1090.0, 110.0, 0.75 }, { 2440.0, 160.0, 0.50 } }, // A
            { { 530.0,  70.0, 1.00 }, { 1840.0, 100.0, 0.60 }, { 2480.0, 130.0, 0.45 } }, // E
            { { 570.0,  80.0, 1.00 }, {  840.0,  90.0, 0.55 }, { 2410.0, 150.0, 0.35 } }  // O
        };

        const int v = std::clamp(vowel, 0, 2);
        double y = 0.0;

        for (int n = 1; n <= 56; ++n)
        {
            const double f = 110.0 * (double) n;
            double envelope = 0.0;

            for (const auto& band : vowels[v])
                envelope += band.gain * gaussian(f, band.freq, band.bandwidth);

            y += (envelope / (double) n) * std::sin(twoPi * (double) n * t);
        }

        return y;
    }

    /** Deterministic pseudo-random value per step, in [-1, 1]. */
    double hashToUnit(int step) noexcept
    {
        std::uint32_t h = (std::uint32_t) step * 2654435761u + 0x9E3779B9u;
        h ^= h >> 15;
        h *= 2246822519u;
        h ^= h >> 13;
        h *= 3266489917u;
        h ^= h >> 16;
        return (double) (h & 0xFFFFFFu) / 8388607.5 - 1.0;
    }

    double quantise(double x, int steps) noexcept
    {
        const double scaled = std::clamp((x + 1.0) * 0.5, 0.0, 0.9999999);
        return std::floor(scaled * (double) steps) / (double) steps * 2.0 - 1.0;
    }

    /** Sum of sines; phases alternate so the shape keeps some bite. */
    double bellShape(double t) noexcept
    {
        static const int partials[] = { 1, 2, 3, 5, 7, 11 };
        static const double amps[]  = { 1.0, 0.65, 0.45, 0.28, 0.18, 0.10 };

        double y = 0.0;
        for (int i = 0; i < 6; ++i)
        {
            const double sign = (i % 2 == 0) ? 1.0 : -1.0;
            y += sign * amps[i] * std::sin(twoPi * (double) partials[i] * t);
        }
        return y;
    }

    double metallicShape(double t) noexcept
    {
        double y = 0.0;
        for (int n = 1; n <= 10; ++n)
        {
            const double amp = 1.0 / std::pow((double) n, 0.65);
            y += amp * std::sin(twoPi * (double) n * t + 0.6 * (double) (n * n));
        }
        return y;
    }

    double harmonicStack(double t, int first, int count, int stride, double tilt) noexcept
    {
        double y = 0.0;
        for (int i = 0; i < count; ++i)
        {
            const int n = first + i * stride;
            y += std::sin(twoPi * (double) n * t) / std::pow((double) n, tilt);
        }
        return y;
    }

    /** Draws amps[n] on harmonics 1..count. */
    double drawbarShape(double t) noexcept
    {
        static const double amps[] = { 1.0, 0.55, 0.35, 0.22, 0.12, 0.08 };
        double y = 0.0;
        for (int n = 1; n <= 6; ++n)
            y += amps[n - 1] * std::sin(twoPi * (double) n * t);
        return y;
    }

    double octaveShape(double t) noexcept
    {
        static const int partials[] = { 1, 2, 4, 8 };
        static const double amps[]  = { 1.0, 0.50, 0.28, 0.15 };

        double y = 0.0;
        for (int i = 0; i < 4; ++i)
            y += amps[i] * std::sin(twoPi * (double) partials[i] * t);
        return y;
    }

    double fifthsShape(double t) noexcept
    {
        static const int partials[] = { 1, 3, 9 };
        static const double amps[]  = { 1.0, 0.50, 0.30 };

        double y = 0.0;
        for (int i = 0; i < 3; ++i)
            y += amps[i] * std::sin(twoPi * (double) partials[i] * t);
        return y;
    }

    double asymmetricTriangle(double t) noexcept
    {
        const double p = wrap(t);
        return (p < 0.30) ? (-1.0 + 2.0 * p / 0.30)
                          : (1.0 - 2.0 * (p - 0.30) / 0.70);
    }

    double superSawShape(double t) noexcept
    {
        return 0.25 * (saw(t) + saw(t + 0.011) + saw(t + 0.019) + saw(t + 0.027));
    }
}

//==============================================================================
const WaveformSpec& getWaveformSpec(int index) noexcept
{
    static const WaveformSpec catalog[(size_t) numWaveforms] =
    {
        { "Sawtooth",    "CLASSIC" },
        { "Square",      "CLASSIC" },
        { "Triangle",    "CLASSIC" },
        { "Sine",        "CLASSIC" },
        { "Soft Saw",    "CLASSIC" },
        { "Skew Saw",    "CLASSIC" },

        { "Pulse 5%",    "PULSE" },
        { "Pulse 10%",   "PULSE" },
        { "Pulse 20%",   "PULSE" },
        { "Pulse 25%",   "PULSE" },
        { "Pulse 33%",   "PULSE" },
        { "Pulse 40%",   "PULSE" },
        { "Pulse 60%",   "PULSE" },
        { "Pulse 66%",   "PULSE" },
        { "Pulse 75%",   "PULSE" },
        { "Pulse 80%",   "PULSE" },
        { "Pulse 90%",   "PULSE" },
        { "Pulse 95%",   "PULSE" },

        { "Sync 2x",     "SYNC" },
        { "Sync 3x",     "SYNC" },
        { "Sync 4x",     "SYNC" },
        { "Sync 5x",     "SYNC" },
        { "Sync 7x",     "SYNC" },
        { "Sync 8x",     "SYNC" },

        { "Fold 2",      "FOLD" },
        { "Fold 3",      "FOLD" },
        { "Fold 4",      "FOLD" },
        { "Fold 5",      "FOLD" },
        { "Fold Sine",   "FOLD" },
        { "Rectified",   "FOLD" },

        { "Odd Stack",   "HARMONIC" },
        { "Even Stack",  "HARMONIC" },
        { "Organ",       "HARMONIC" },
        { "Octaves",     "HARMONIC" },
        { "Fifths",      "HARMONIC" },
        { "Bell",        "HARMONIC" },
        { "Formant A",   "HARMONIC" },
        { "Formant E",   "HARMONIC" },
        { "Formant O",   "HARMONIC" },
        { "Metallic",    "HARMONIC" },

        { "Bit 2",       "DIGITAL" },
        { "Bit 3",       "DIGITAL" },
        { "Bit 4",       "DIGITAL" },
        { "Bit 5",       "DIGITAL" },
        { "Steps 4",     "DIGITAL" },
        { "Steps 8",     "DIGITAL" },
        { "Steps 16",    "DIGITAL" },
        { "Chip Tri",    "DIGITAL" },
        { "Sample & Hold", "DIGITAL" },
        { "Phase Dist",  "DIGITAL" },

        { "Saw + Square", "HYBRID" },
        { "Saw + Tri",   "HYBRID" },
        { "Sine + Saw",  "HYBRID" },
        { "Tri + Square", "HYBRID" },
        { "Asym Tri",    "HYBRID" },
        { "Super Saw",   "HYBRID" },
        { "Ring Mod",    "HYBRID" },
        { "Exp Saw",     "HYBRID" },
        { "Reverse Saw", "HYBRID" },
        { "Clipped Sine", "HYBRID" }
    };

    static_assert(std::size(catalog) == (size_t) numWaveforms,
                  "catalog and Waveform enum drifted apart");

    return catalog[(size_t) std::clamp(index, 0, numWaveforms - 1)];
}

const char* getWaveformName(int index) noexcept
{
    return getWaveformSpec(index).name;
}

//==============================================================================
float evaluateWaveformShape(int index, double phase) noexcept
{
    const int clamped = std::clamp(index, 0, numWaveforms - 1);
    const double t = wrap(phase);
    const auto wave = static_cast<Waveform>(clamped);

    switch (wave)
    {
        case Waveform::Sawtooth:
        {
            // The 303 saw has a slight capacitor-discharge curvature.
            const double y = 2.0 * t - 1.0;
            return (float) (y - 0.15 * (y * y - 1.0));
        }

        case Waveform::Square:      return (float) square303(t);
        case Waveform::Triangle:    return (float) triangle(t);
        case Waveform::Sine:        return (float) std::sin(twoPi * t);
        case Waveform::SoftSaw:     return (float) (std::tanh(1.7 * (2.0 * t - 1.0)) / std::tanh(1.7));
        case Waveform::SkewSaw:
            return (float) ((t < 0.72) ? (-1.0 + 2.0 * t / 0.72)
                                       : (1.0 - 2.0 * (t - 0.72) / 0.28));

        case Waveform::Pulse5:  return (float) pulse(t, 0.05);
        case Waveform::Pulse10: return (float) pulse(t, 0.10);
        case Waveform::Pulse20: return (float) pulse(t, 0.20);
        case Waveform::Pulse25: return (float) pulse(t, 0.25);
        case Waveform::Pulse33: return (float) pulse(t, 0.33);
        case Waveform::Pulse40: return (float) pulse(t, 0.40);
        case Waveform::Pulse60: return (float) pulse(t, 0.60);
        case Waveform::Pulse66: return (float) pulse(t, 0.66);
        case Waveform::Pulse75: return (float) pulse(t, 0.75);
        case Waveform::Pulse80: return (float) pulse(t, 0.80);
        case Waveform::Pulse90: return (float) pulse(t, 0.90);
        case Waveform::Pulse95: return (float) pulse(t, 0.95);

        case Waveform::Sync2: return (float) saw(2.0 * t);
        case Waveform::Sync3: return (float) saw(3.0 * t);
        case Waveform::Sync4: return (float) saw(4.0 * t);
        case Waveform::Sync5: return (float) saw(5.0 * t);
        case Waveform::Sync7: return (float) saw(7.0 * t);
        case Waveform::Sync8: return (float) saw(8.0 * t);

        case Waveform::Fold2:    return (float) fold(2.0 * triangle(t));
        case Waveform::Fold3:    return (float) fold(3.0 * triangle(t));
        case Waveform::Fold4:    return (float) fold(4.0 * triangle(t));
        case Waveform::Fold5:    return (float) fold(5.0 * triangle(t));
        case Waveform::FoldSine: return (float) fold(3.0 * std::sin(twoPi * t));
        case Waveform::Rectified: return (float) (2.0 * std::abs(std::sin(twoPi * t)) - 1.0);

        case Waveform::OddStack:  return (float) harmonicStack(t, 1, 8, 2, 1.0);
        case Waveform::EvenStack: return (float) harmonicStack(t, 2, 8, 2, 1.0);
        case Waveform::Organ:     return (float) drawbarShape(t);
        case Waveform::Octaves:   return (float) octaveShape(t);
        case Waveform::Fifths:    return (float) fifthsShape(t);
        case Waveform::Bell:      return (float) bellShape(t);
        case Waveform::FormantA:  return (float) formantShape(t, 0);
        case Waveform::FormantE:  return (float) formantShape(t, 1);
        case Waveform::FormantO:  return (float) formantShape(t, 2);
        case Waveform::Metallic:  return (float) metallicShape(t);

        case Waveform::Bit2: return (float) quantise(2.0 * t - 1.0, 4);
        case Waveform::Bit3: return (float) quantise(2.0 * t - 1.0, 8);
        case Waveform::Bit4: return (float) quantise(2.0 * t - 1.0, 16);
        case Waveform::Bit5: return (float) quantise(2.0 * t - 1.0, 32);

        case Waveform::Steps4:  return (float) (2.0 * (std::floor(t * 4.0) + 0.5) / 4.0 - 1.0);
        case Waveform::Steps8:  return (float) (2.0 * (std::floor(t * 8.0) + 0.5) / 8.0 - 1.0);
        case Waveform::Steps16: return (float) (2.0 * (std::floor(t * 16.0) + 0.5) / 16.0 - 1.0);
        case Waveform::ChipTriangle: return (float) quantise(triangle(t), 8);
        case Waveform::SampleHold:
        {
            const int step = (int) std::floor(t * 16.0);
            return (float) hashToUnit(step);
        }
        case Waveform::PhaseDistSine:
            return (float) std::sin(twoPi * (t - 0.16 * std::sin(twoPi * t)));

        case Waveform::SawSquare:  return (float) (0.60 * saw(t) + 0.40 * square303(t));
        case Waveform::SawTriangle: return (float) (0.60 * saw(t) + 0.40 * triangle(t));
        case Waveform::SineSaw:    return (float) (0.60 * std::sin(twoPi * t) + 0.40 * saw(t));
        case Waveform::TriSquare:  return (float) (0.60 * triangle(t) + 0.40 * square303(t));
        case Waveform::AsymTriangle: return (float) asymmetricTriangle(t);
        case Waveform::SuperSaw:   return (float) superSawShape(t);
        case Waveform::RingMod:
            return (float) (std::sin(twoPi * t) * std::sin(twoPi * 3.0 * t));
        case Waveform::ExpSaw:
            return (float) ((std::exp(4.0 * t) - 1.0) / (std::exp(4.0) - 1.0) * 2.0 - 1.0);
        case Waveform::ReverseSaw: return (float) (1.0 - 2.0 * t);
        case Waveform::ClippedSine:
            return (float) std::clamp(2.2 * std::sin(twoPi * t), -0.9, 0.9);

        case Waveform::Count:
        default:
            break;
    }

    return 0.0f;
}

//==============================================================================
const WavetableBank& WavetableBank::getInstance()
{
    static const WavetableBank bank;
    return bank;
}

const float* WavetableBank::table(int waveform, int mip) const noexcept
{
    const int w = std::clamp(waveform, 0, numWaveforms - 1);
    const int m = std::clamp(mip, 0, numMips - 1);
    return tables[(size_t) w * numMips + (size_t) m].data();
}

WavetableBank::WavetableBank()
{
    constexpr int N = tableSize;
    constexpr int fftOrder = 11; // 2^11 == 2048

    juce::dsp::FFT fft(fftOrder);

    std::vector<juce::dsp::Complex<float>> spectrum((size_t) N);
    std::vector<juce::dsp::Complex<float>> filtered((size_t) N);
    std::vector<juce::dsp::Complex<float>> timeDomain((size_t) N);
    std::vector<double> shape((size_t) N);

    constexpr double targetRms = 0.50;
    constexpr double peakCeiling = 0.98;

    for (int w = 0; w < numWaveforms; ++w)
    {
        // 1. Ideal single-cycle shape, DC removed.
        double mean = 0.0;
        for (int i = 0; i < N; ++i)
        {
            shape[(size_t) i] = evaluateWaveformShape(w, (double) i / (double) N);
            mean += shape[(size_t) i];
        }
        mean /= (double) N;

        double sumSquares = 0.0;
        for (int i = 0; i < N; ++i)
        {
            shape[(size_t) i] -= mean;
            sumSquares += shape[(size_t) i] * shape[(size_t) i];
        }

        const double rms = std::sqrt(sumSquares / (double) N);
        const double rmsGain = (rms > 1.0e-9) ? targetRms / rms : 1.0;

        // 2. Every shape lands on the same RMS so switching waveforms does not
        //    jump the level feeding the ladder filter; crest factor is capped.
        double peak = 0.0;
        for (int i = 0; i < N; ++i)
        {
            shape[(size_t) i] *= rmsGain;
            peak = std::max(peak, std::abs(shape[(size_t) i]));
        }

        if (peak > peakCeiling)
        {
            const double limiter = peakCeiling / peak;
            for (int i = 0; i < N; ++i)
                shape[(size_t) i] *= limiter;
        }

        for (int i = 0; i < N; ++i)
            spectrum[(size_t) i] = { (float) shape[(size_t) i], 0.0f };

        fft.perform(spectrum.data(), filtered.data(), false);
        std::copy(filtered.begin(), filtered.end(), spectrum.begin());

        // 3. One table per mip level, each rebuilt from the same spectrum with
        //    the harmonics above the level's ceiling removed.
        for (int mip = 0; mip < numMips; ++mip)
        {
            const int highest = std::max(1, maxHarmonic >> mip);

            std::copy(spectrum.begin(), spectrum.end(), filtered.begin());
            filtered[0] = {};
            filtered[(size_t) (N / 2)] = {};

            for (int k = highest + 1; k < N / 2; ++k)
            {
                filtered[(size_t) k] = {};
                filtered[(size_t) (N - k)] = {};
            }

            fft.perform(filtered.data(), timeDomain.data(), true);

            auto& table = tables[(size_t) w * numMips + (size_t) mip];
            table.resize((size_t) N);

            double tablePeak = 0.0;
            for (int i = 0; i < N; ++i)
            {
                table[(size_t) i] = timeDomain[(size_t) i].real();
                tablePeak = std::max(tablePeak, (double) std::abs(table[(size_t) i]));
            }

            if (tablePeak > peakCeiling)
            {
                const float gain = (float) (peakCeiling / tablePeak);
                for (auto& sample : table)
                    sample *= gain;
            }
        }
    }
}
}
