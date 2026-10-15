#include "PluginProcessor.h"
#include "PluginEditor.h"

Replicant303AudioProcessor::Replicant303AudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    // Cache raw pointers for thread-safe audio-rate access
    waveformParam   = apvts.getRawParameterValue("waveform");
    tuningParam     = apvts.getRawParameterValue("tuning");
    cutoffParam     = apvts.getRawParameterValue("cutoff");
    resonanceParam  = apvts.getRawParameterValue("resonance");
    envModParam     = apvts.getRawParameterValue("env_mod");
    decayParam      = apvts.getRawParameterValue("decay");
    accentParam     = apvts.getRawParameterValue("accent");
    volumeParam     = apvts.getRawParameterValue("volume");

    distActiveParam = apvts.getRawParameterValue("dist_active");
    distTypeParam   = apvts.getRawParameterValue("dist_type");
    distDriveParam  = apvts.getRawParameterValue("dist_drive");
    distToneParam   = apvts.getRawParameterValue("dist_tone");
    distMixParam    = apvts.getRawParameterValue("dist_mix");
}

Replicant303AudioProcessor::~Replicant303AudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout Replicant303AudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 1. Waveform Switch (Saw / Square)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "waveform", 1 }, "Waveform",
        juce::StringArray{ "Sawtooth", "Square" }, 0));

    // 2. Tuning (-12 to +12 semitones)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "tuning", 1 }, "Tuning",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("semi")));

    // 3. Cutoff (25 Hz to 4500 Hz, logarithmic skew)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "cutoff", 1 }, "Cutoff",
        juce::NormalisableRange<float>(25.0f, 4500.0f, 0.5f, 0.35f), 350.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    // 4. Resonance (0.0 to 1.0)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "resonance", 1 }, "Resonance",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.005f), 0.65f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // 5. Env Mod (0.0 to 1.0)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "env_mod", 1 }, "Env Mod",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.005f), 0.60f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // 6. Decay (0.2s to 2.5s, skewed at 0.8s)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "decay", 1 }, "Decay",
        juce::NormalisableRange<float>(0.20f, 2.50f, 0.01f, 0.5f), 0.75f,
        juce::AudioParameterFloatAttributes().withLabel("s")));

    // 7. Accent (0.0 to 1.0)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "accent", 1 }, "Accent",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.005f), 0.70f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // 8. Master Volume (-60 dB to +6 dB)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "volume", 1 }, "Master Volume",
        juce::NormalisableRange<float>(-60.0f, 6.0f, 0.1f, 2.0f), -3.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    // -------------------------------------------------------------------------
    // Distortion Parameters
    // -------------------------------------------------------------------------

    // 9. Distortion Active
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "dist_active", 1 }, "Distortion Active", true));

    // 10. Distortion Type
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "dist_type", 1 }, "Distortion Type",
        juce::StringArray{ "PocoRat", "TubeDrive", "HardClip", "GermaniumFuzz", "Wavefolder" }, 0));

    // 11. Distortion Drive
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "dist_drive", 1 }, "Distortion Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.005f), 0.50f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // 12. Distortion Tone (Filter)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "dist_tone", 1 }, "Distortion Tone",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.005f), 0.60f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // 13. Distortion Mix (Wet / Dry)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "dist_mix", 1 }, "Distortion Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.005f), 0.85f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    return { params.begin(), params.end() };
}

void Replicant303AudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    synthEngine.prepare(sampleRate);
}

void Replicant303AudioProcessor::releaseResources()
{
    synthEngine.reset();
}

bool Replicant303AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Must be stereo output
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void Replicant303AudioProcessor::updateParameters() noexcept
{
    if (waveformParam != nullptr)
        synthEngine.setWaveform(static_cast<Replicant303::Waveform>(static_cast<int>(waveformParam->load())));

    if (tuningParam != nullptr)
        synthEngine.setTuning(tuningParam->load());

    if (cutoffParam != nullptr)
        synthEngine.setCutoff(cutoffParam->load());

    if (resonanceParam != nullptr)
        synthEngine.setResonance(resonanceParam->load());

    if (envModParam != nullptr)
        synthEngine.setEnvMod(envModParam->load());

    if (decayParam != nullptr)
        synthEngine.setDecay(decayParam->load());

    if (accentParam != nullptr)
        synthEngine.setAccent(accentParam->load());

    if (volumeParam != nullptr)
    {
        float db = volumeParam->load();
        float gainLinear = (db <= -59.5f) ? 0.0f : juce::Decibels::decibelsToGain(db);
        synthEngine.setMasterVolume(gainLinear);
    }

    if (distActiveParam != nullptr)
        synthEngine.setDistortionActive(distActiveParam->load() > 0.5f);

    if (distTypeParam != nullptr)
        synthEngine.setDistortionType(static_cast<Replicant303::DistortionType>(static_cast<int>(distTypeParam->load())));

    if (distDriveParam != nullptr)
        synthEngine.setDistortionDrive(distDriveParam->load());

    if (distToneParam != nullptr)
        synthEngine.setDistortionTone(distToneParam->load());

    if (distMixParam != nullptr)
        synthEngine.setDistortionMix(distMixParam->load());
}

void Replicant303AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Clear output buffer
    buffer.clear();

    // Update real-time DSP parameters
    updateParameters();

    // Process MIDI buffer
    synthEngine.handleMidiBuffer(midiMessages, buffer.getNumSamples());

    // Render audio
    synthEngine.processBlock(buffer);
}

juce::AudioProcessorEditor* Replicant303AudioProcessor::createEditor()
{
    return new Replicant303AudioProcessorEditor(*this);
}

void Replicant303AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void Replicant303AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

// ------------------------------------------------------------------------------
// Plugin instantiation entry point
// ------------------------------------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Replicant303AudioProcessor();
}
