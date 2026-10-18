#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "GUI/Replicant303Theme.h"
#include "Presets/PresetIO.h"

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
    accentParam       = apvts.getRawParameterValue("accent");
    volumeParam       = apvts.getRawParameterValue("volume");
    tempoParam        = apvts.getRawParameterValue("tempo");
    patternGroupParam = apvts.getRawParameterValue("pattern_group");

    distActiveParam   = apvts.getRawParameterValue("dist_active");
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

    // 2b. Tempo (40 to 300 BPM)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "tempo", 1 }, "Tempo",
        juce::NormalisableRange<float>(40.0f, 300.0f, 0.5f), 125.0f,
        juce::AudioParameterFloatAttributes().withLabel("BPM")));

    // 2c. Pattern Group (I, II, III, IV)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "pattern_group", 1 }, "Pattern Group",
        juce::StringArray{ "Group I", "Group II", "Group III", "Group IV" }, 0));

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

    // 14. UI Theme (panel skin selector; saved with the plugin state)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "ui_theme", 1 }, "UI Theme",
        Replicant303::ThemeManager::getThemeNames(), 0));

    return { params.begin(), params.end() };
}

void Replicant303AudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    synthEngine.prepare(sampleRate);

    // The pattern clock is sample-rate dependent too: without this the
    // sequencer kept its 44.1 kHz default and played at the wrong tempo in
    // every 48/96 kHz session.
    sequencer.prepare(sampleRate);
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

    // -------------------------------------------------------------------------
    // Transport / clock: the host play head supplies tempo and play state for
    // the Host sync mode; the internal mode uses the tempo parameter.
    // -------------------------------------------------------------------------
    double hostBpm = 0.0;
    bool hostPlaying = false;

    if (auto* hostPlayHead = getPlayHead())
    {
        if (auto position = hostPlayHead->getPosition())
        {
            if (auto bpm = position->getBpm())
                hostBpm = *bpm;

            hostPlaying = position->getIsPlaying();
        }
    }

    sequencer.setTempo(tempoParam != nullptr ? (double) tempoParam->load() : 125.0);
    sequencer.setHostState(hostPlaying, hostBpm);

    // -------------------------------------------------------------------------
    // Merge host MIDI + on-screen keyboard + sequencer into one sample-accurate
    // event stream. The engine splits rendering at every event boundary.
    // -------------------------------------------------------------------------
    // The on-screen keyboard keeps its own buffer: feeding host MIDI through
    // MidiKeyboardState would let a host note-off clear a key the user is
    // physically holding, leaving a stuck note in the voice stack.
    keyboardScratch.clear();
    keyboardState.processNextMidiBuffer(keyboardScratch, 0, buffer.getNumSamples(), true);

    midiScratch.clear();
    midiScratch.addEvents(midiMessages, 0, buffer.getNumSamples(), 0);
    midiScratch.addEvents(keyboardScratch, 0, buffer.getNumSamples(), 0);
    sequencer.process(buffer.getNumSamples(), midiScratch);

    synthEngine.processBlock(buffer, midiScratch);
}

juce::AudioProcessorEditor* Replicant303AudioProcessor::createEditor()
{
    return new Replicant303AudioProcessorEditor(*this);
}

void Replicant303AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    // Pattern bank + transport configuration live next to the parameters.
    state.removeChild(state.getChildWithName("SEQUENCER"), nullptr);
    state.appendChild(sequencer.toValueTree(), nullptr);

    // User preset overrides live next to the sequencer. Factory slots are
    // regenerated on load, so only edited/imported slots cost state space.
    state.removeChild(state.getChildWithName("PRESETBANK"), nullptr);
    state.appendChild(presetBank.toValueTree(), nullptr);

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void Replicant303AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml(*xmlState);
        presetBank.restoreFromValueTree(tree.getChildWithName("PRESETBANK"));
        sequencer.restoreFromValueTree(tree.getChildWithName("SEQUENCER"));
        apvts.replaceState(tree);
    }
}

bool Replicant303AudioProcessor::exportPatternToMidi(const juce::File& file, int patternIndex)
{
    const double bpm = tempoParam != nullptr ? (double) tempoParam->load() : 125.0;
    return sequencer.writeMidiFile(file, patternIndex, bpm);
}

// ------------------------------------------------------------------------------
// Preset vault
// ------------------------------------------------------------------------------
void Replicant303AudioProcessor::setParameterValue(const juce::String& parameterId, float plainValue)
{
    if (auto* parameter = apvts.getParameter(parameterId))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
        parameter->endChangeGesture();
    }
}

void Replicant303AudioProcessor::loadPreset(int bank, int slot)
{
    const int safeBank = juce::jlimit(0, Replicant303::PresetBank::numBanks - 1, bank);
    const int safeSlot = juce::jlimit(0, Replicant303::PresetBank::presetsPerBank - 1, slot);
    const auto& preset = presetBank.getPreset(safeBank, safeSlot);

    presetBank.setSelectedSlot(safeBank, safeSlot);

    // The bank letter is the pattern slot: loading A/17 writes pattern A.
    sequencer.setCurrentPattern(safeBank);
    sequencer.setPattern(safeBank, preset.pattern);
    sequencer.setSwing(preset.swing);
    sequencer.requestReset();

    setParameterValue("waveform", (float) preset.waveform);
    setParameterValue("tuning", preset.tuning);
    setParameterValue("cutoff", preset.cutoff);
    setParameterValue("resonance", preset.resonance);
    setParameterValue("env_mod", preset.envMod);
    setParameterValue("decay", preset.decay);
    setParameterValue("accent", preset.accent);
    setParameterValue("tempo", preset.tempo);
    setParameterValue("dist_active", preset.distActive ? 1.0f : 0.0f);
    setParameterValue("dist_type", (float) preset.distType);
    setParameterValue("dist_drive", preset.distDrive);
    setParameterValue("dist_tone", preset.distTone);
    setParameterValue("dist_mix", preset.distMix);

    // Keep the hardware TRACK knob in step with the loaded letter.
    setParameterValue("pattern_group", (float) (safeBank / 2));
}

Replicant303::Preset Replicant303AudioProcessor::captureCurrentPreset(const juce::String& name) const
{
    Replicant303::Preset preset;
    preset.name = name;
    preset.pattern = sequencer.getPattern(sequencer.getCurrentPattern());
    preset.swing = sequencer.getSwing();

    auto raw = [this] (const char* id, float fallback)
    {
        if (auto* value = apvts.getRawParameterValue(id))
            return value->load();

        return fallback;
    };

    preset.waveform = juce::roundToInt(raw("waveform", 0.0f));
    preset.tuning = raw("tuning", 0.0f);
    preset.cutoff = raw("cutoff", 350.0f);
    preset.resonance = raw("resonance", 0.65f);
    preset.envMod = raw("env_mod", 0.60f);
    preset.decay = raw("decay", 0.75f);
    preset.accent = raw("accent", 0.70f);
    preset.tempo = raw("tempo", 125.0f);
    preset.distActive = raw("dist_active", 1.0f) > 0.5f;
    preset.distType = juce::roundToInt(raw("dist_type", 0.0f));
    preset.distDrive = raw("dist_drive", 0.50f);
    preset.distTone = raw("dist_tone", 0.60f);
    preset.distMix = raw("dist_mix", 0.85f);

    preset.sanitise();
    return preset;
}

void Replicant303AudioProcessor::storeCurrentPreset(int bank, int slot, const juce::String& name)
{
    const auto preset = captureCurrentPreset(name);
    presetBank.setPreset(bank, slot, preset);
    presetBank.setSelectedSlot(bank, slot);
}

void Replicant303AudioProcessor::generatePreset(int bank, int slot, juce::uint32 seed)
{
    const int safeBank = juce::jlimit(0, Replicant303::PresetBank::numBanks - 1, bank);
    const int safeSlot = juce::jlimit(0, Replicant303::PresetBank::presetsPerBank - 1, slot);

    presetBank.setPreset(safeBank, safeSlot,
                         Replicant303::PresetBank::generatePreset(safeBank, safeSlot, seed));
    loadPreset(safeBank, safeSlot);
}

bool Replicant303AudioProcessor::exportPresetToMidi(const juce::File& file, int bank, int slot)
{
    const int safeBank = juce::jlimit(0, Replicant303::PresetBank::numBanks - 1, bank);
    const int safeSlot = juce::jlimit(0, Replicant303::PresetBank::presetsPerBank - 1, slot);
    const auto name = presetBank.getPreset(safeBank, safeSlot).name;

    return Replicant303::PresetIO::writeMidiFile(captureCurrentPreset(name), file);
}

bool Replicant303AudioProcessor::importPresetFromMidi(const juce::File& file, int bank, int slot)
{
    const int safeBank = juce::jlimit(0, Replicant303::PresetBank::numBanks - 1, bank);
    const int safeSlot = juce::jlimit(0, Replicant303::PresetBank::presetsPerBank - 1, slot);

    Replicant303::Preset preset;

    if (! Replicant303::PresetIO::readMidiFile(file, preset))
        return false;

    if (preset.name.isEmpty())
        preset.name = file.getFileNameWithoutExtension();

    presetBank.setPreset(safeBank, safeSlot, preset);
    loadPreset(safeBank, safeSlot);
    return true;
}

// ------------------------------------------------------------------------------
// Plugin instantiation entry point
// ------------------------------------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Replicant303AudioProcessor();
}
