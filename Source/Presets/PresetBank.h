#pragma once

#include "DSP/Pattern.h"

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include <array>

namespace Replicant303
{
    //==============================================================================
    /** A complete Replicant 303 preset.

        A preset carries the acid line *and* the voice that plays it: the
        16-step pattern, the VCO/VCF/envelope controls, the distortion model
        and the sequencer's tempo/swing. Master volume is deliberately absent:
        loading a preset must never jump the level of a live set.
    */
    struct Preset
    {
        juce::String name;
        Pattern pattern;

        // Synth voice
        int   waveform  = 0;        // 0 Saw, 1 Square
        float tuning    = 0.0f;     // semitones
        float cutoff    = 350.0f;   // Hz
        float resonance = 0.65f;
        float envMod    = 0.60f;
        float decay     = 0.75f;    // seconds
        float accent    = 0.70f;    // accent amount

        // Sequencer / groove
        float tempo = 125.0f;       // BPM
        float swing = 0.0f;         // 0 .. 0.66

        // Distortion unit
        bool  distActive = true;
        int   distType   = 0;       // 0 .. 4
        float distDrive  = 0.50f;
        float distTone   = 0.60f;
        float distMix    = 0.85f;

        /** Clamps every field into the ranges the engine and UI understand. */
        void sanitise() noexcept;

        bool operator== (const Preset& other) const noexcept;
        bool operator!= (const Preset& other) const noexcept { return ! (*this == other); }

        juce::ValueTree toValueTree() const;
        static Preset fromValueTree (const juce::ValueTree& tree);
    };

    //==============================================================================
    /** The factory preset vault: eight banks (one per pattern letter A-H),
        each holding 64 complete presets.

        Slot 0 of every bank is the original factory pattern for that letter, so
        the patterns that shipped before the vault existed remain one click
        away. Slots 1-63 are produced by the deterministic procedural generator
        below. User stores/imports replace individual slots and are serialised
        sparsely: only the overridden slots travel in the plugin state.
    */
    class PresetBank
    {
    public:
        static constexpr int numBanks       = 8;
        static constexpr int presetsPerBank = 64;
        static constexpr int numPresets     = numBanks * presetsPerBank;

        PresetBank();

        static juce::String getBankLetter (int bank);
        static juce::String getBankName (int bank);

        Preset getPreset (int bank, int slot) const;
        void setPreset (int bank, int slot, const Preset& preset);
        void resetPreset (int bank, int slot);
        bool isOverridden (int bank, int slot) const noexcept;

        /** The preset slot the UI last loaded in each bank. Persisted with the
            plugin state so a reloaded session shows the same preset name. */
        int getSelectedSlot (int bank) const noexcept;
        void setSelectedSlot (int bank, int slot) noexcept;

        /** Deterministic factory content. Slot 0 is the legacy pattern. */
        static Preset makeFactoryPreset (int bank, int slot);

        /** Procedural generator shared by the factory bank, the GEN button and
            tools/generate_presets. The same (bank, slot, seed) always produces
            exactly the same preset on every platform. */
        static Preset generatePreset (int bank, int slot, juce::uint32 seed);

        /** Serialises only the user overrides, not the 512 regenerable slots. */
        juce::ValueTree toValueTree() const;

        /** Restores a saved vault. A missing/invalid tree resets every slot to
            factory content, so old plugin state cannot leak user presets. */
        void restoreFromValueTree (const juce::ValueTree& tree);

    private:
        using Bank = std::array<Preset, (size_t) presetsPerBank>;

        std::array<Bank, (size_t) numBanks> presets;
        std::array<std::array<bool, (size_t) presetsPerBank>, (size_t) numBanks> overridden {};
        std::array<int, (size_t) numBanks> selectedSlots {};

        mutable juce::CriticalSection lock;
    };
}
