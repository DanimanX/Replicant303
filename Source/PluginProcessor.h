#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/SynthEngine.h"

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

private:
    Replicant303::SynthEngine synthEngine;

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
    std::atomic<float>* patternGroupParam = nullptr;

    std::atomic<float>* distActiveParam = nullptr;
    std::atomic<float>* distTypeParam = nullptr;
    std::atomic<float>* distDriveParam = nullptr;
    std::atomic<float>* distToneParam = nullptr;
    std::atomic<float>* distMixParam = nullptr;

    void updateParameters() noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Replicant303AudioProcessor)
};
