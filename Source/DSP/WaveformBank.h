#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace Replicant303
{
    //==========================================================================
    /** Every shape the VCO can generate.

        The numeric values are part of the preset / plugin-state format, so the
        order is append-only. The first two entries keep the original 303 saw
        and square indices, which means patches written before the bank existed
        still load the same waveform. `Count` is not a shape; it is the size of
        the bank.
    */
    enum class Waveform : int
    {
        // Classic analog
        Sawtooth = 0,
        Square,
        Triangle,
        Sine,
        SoftSaw,
        SkewSaw,

        // Pulse widths
        Pulse5,
        Pulse10,
        Pulse20,
        Pulse25,
        Pulse33,
        Pulse40,
        Pulse60,
        Pulse66,
        Pulse75,
        Pulse80,
        Pulse90,
        Pulse95,

        // Hard sync (integer ratios keep the shape periodic)
        Sync2,
        Sync3,
        Sync4,
        Sync5,
        Sync7,
        Sync8,

        // West-coast folding & rectification
        Fold2,
        Fold3,
        Fold4,
        Fold5,
        FoldSine,
        Rectified,

        // Harmonic / additive recipes
        OddStack,
        EvenStack,
        Organ,
        Octaves,
        Fifths,
        Bell,
        FormantA,
        FormantE,
        FormantO,
        Metallic,

        // Digital / quantised
        Bit2,
        Bit3,
        Bit4,
        Bit5,
        Steps4,
        Steps8,
        Steps16,
        ChipTriangle,
        SampleHold,
        PhaseDistSine,

        // Hybrid blends & non-linear shapes
        SawSquare,
        SawTriangle,
        SineSaw,
        TriSquare,
        AsymTriangle,
        SuperSaw,
        RingMod,
        ExpSaw,
        ReverseSaw,
        ClippedSine,

        Count
    };

    inline constexpr int numWaveforms = static_cast<int>(Waveform::Count);

    //==========================================================================
    /** Metadata for one entry of the bank. */
    struct WaveformSpec
    {
        const char* name   = "";
        const char* family = "";
    };

    /** Metadata for `index`; out-of-range indices are clamped. */
    const WaveformSpec& getWaveformSpec(int index) noexcept;

    /** Display name for `index`; out-of-range indices are clamped. */
    const char* getWaveformName(int index) noexcept;

    /** Evaluates the ideal single-cycle shape at `phase` in [0, 1).

        This is the single source of truth for the shape: the wavetable builder
        and the UI previews both call it, so what the scope draws is exactly
        what the oscillator plays (before band-limiting).
    */
    float evaluateWaveformShape(int index, double phase) noexcept;

    //==========================================================================
    /** Band-limited wavetable bank shared by every voice.

        Each shape is stored at eleven mip levels: level 0 keeps 1024 harmonics,
        each following level halves that count. The oscillator picks the level
        whose highest harmonic still sits below Nyquist for the current note,
        which is what keeps a 5 kHz saw from folding back into the audible band.

        The tables are sample-rate independent (mip choice is not), so the bank
        is built once per process and shared.
    */
    class WavetableBank
    {
    public:
        static constexpr int tableSize   = 2048;
        static constexpr int numMips     = 11;              // 1024 .. 1 harmonics
        static constexpr int maxHarmonic = tableSize / 2;   // bin at Nyquist is unused

        static const WavetableBank& getInstance();

        /** Table for `waveform` at `mip`; both indices are clamped. */
        const float* table(int waveform, int mip) const noexcept;

    private:
        WavetableBank();

        std::array<std::vector<float>, (size_t) numWaveforms * numMips> tables;
    };
}
