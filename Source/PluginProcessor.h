#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/SynthEngine.h"
#include "DSP/PatternSequencer.h"
#include "Presets/PresetBank.h"

class Replicant303AudioProcessor : public juce::AudioProcessor
{
public:
    Replicant303AudioProcessor();
    ~Replicant303AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

#ifndef JucePlugin_Name
#define JucePlugin_Name "Replicant303"
#endif

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // APVTS Parameter layout
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Access to synth engine for UI telemetry
    Replicant303::SynthEngine& getSynthEngine() noexcept { return synthEngine; }

    // Pattern sequencer (pattern bank, clock, MIDI export)
    Replicant303::PatternSequencer& getSequencer() noexcept { return sequencer; }
    const Replicant303::PatternSequencer& getSequencer() const noexcept { return sequencer; }

    /** Thread-safe note source for the on-screen keyboard, consumed on the
        audio thread. Replaces the old direct engine calls from the editor. */
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }

    /** Writes a pattern from the bank to a Standard MIDI File. */
    bool exportPatternToMidi(const juce::File& file, int patternIndex);

    // -------------------------------------------------------------------------
    // Preset vault (eight banks of 64 presets, one bank per pattern letter)
    // -------------------------------------------------------------------------
    Replicant303::PresetBank& getPresetBank() noexcept { return presetBank; }
    const Replicant303::PresetBank& getPresetBank() const noexcept { return presetBank; }

    /** Loads a bank slot into its pattern letter and applies the full voice. */
    void loadPreset(int bank, int slot);

    /** Captures the live sound + current pattern under a display name. */
    Replicant303::Preset captureCurrentPreset(const juce::String& name) const;

    /** Stores the live state in a bank slot as a user override. */
    void storeCurrentPreset(int bank, int slot, const juce::String& name);

    /** Generates a new procedural preset, stores it and loads it. */
    void generatePreset(int bank, int slot, juce::uint32 seed);

    /** `.midi` preset import / export (pattern + full voice). */
    bool exportPresetToMidi(const juce::File& file, int bank, int slot);
    bool importPresetFromMidi(const juce::File& file, int bank, int slot);

private:
    Replicant303::SynthEngine synthEngine;
    Replicant303::PatternSequencer sequencer;
    Replicant303::PresetBank presetBank;
    juce::MidiKeyboardState keyboardState;

    /** Reused per block so the audio thread does not allocate a fresh MIDI
        buffer every callback. clear() keeps the capacity. */
    juce::MidiBuffer midiScratch;
    juce::MidiBuffer keyboardScratch;

    // Cached parameter pointers for high performance
    std::atomic<float>* waveformParam = nullptr;
    std::atomic<float>* tuningParam = nullptr;
    std::atomic<float>* cutoffParam = nullptr;
    std::atomic<float>* resonanceParam = nullptr;
    std::atomic<float>* envModParam = nullptr;
    std::atomic<float>* decayParam = nullptr;
    std::atomic<float>* accentParam = nullptr;
    std::atomic<float>* volumeParam = nullptr;
    std::atomic<float>* tempoParam = nullptr;
    juce::RangedAudioParameter* tempoRangedParam = nullptr;
    std::atomic<float>* patternGroupParam = nullptr;
    std::atomic<float>* uiZoomParam = nullptr;

    std::atomic<float>* distActiveParam = nullptr;
    std::atomic<float>* distTypeParam = nullptr;
    std::atomic<float>* distDriveParam = nullptr;
    std::atomic<float>* distToneParam = nullptr;
    std::atomic<float>* distMixParam = nullptr;

    bool hasSyncedInitialHostTempo = false;
    double lastHostBpm = 0.0;

    void updateParameters() noexcept;
    void syncHostPlayHead() noexcept;

    /** Sets one APVTS parameter from a plain (not normalised) value. */
    void setParameterValue(const juce::String& parameterId, float plainValue);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Replicant303AudioProcessor)
};
