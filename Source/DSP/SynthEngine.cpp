#include "SynthEngine.h"

namespace Replicant303
{
    SynthEngine::SynthEngine()
    {
        noteStack.reserve(8);
        reset();
    }

    void SynthEngine::prepare(double newSampleRate) noexcept
    {
        sampleRate = std::max(1000.0, newSampleRate);

        osc.prepare(sampleRate);
        filter.prepare(sampleRate);
        env.prepare(sampleRate);
        distortion.prepare(sampleRate);

        scopeTap.setSampleRate(sampleRate);

        updateSlideCoefficient();
        reset();
    }

    void SynthEngine::reset() noexcept
    {
        osc.reset();
        filter.reset();
        env.reset();
        distortion.reset();

        noteStack.clear();
        targetPitchHz = 110.0f;
        currentPitchHz = 110.0f;
        pitchBendSemi = 0.0f;

        slideLedActive = false;
        accentLedActive = false;

        scopeTap.reset();
        scopeTap.setNoteState(static_cast<int>(osc.getWaveform()), 110.0f, 0.0f);
    }

    void SynthEngine::updateSlideCoefficient() noexcept
    {
        // 303 portamento slide time constant (~60 ms)
        float dt = 1.0f / static_cast<float>(sampleRate);
        float tau = 0.060f;
        pitchSlideCoeff = 1.0f - std::exp(-dt / tau);
    }

    void SynthEngine::handleNoteOn(int noteNumber, int velocity) noexcept
    {
        lastNoteNumber = noteNumber;
        lastVelocity = velocity;

        bool isAccented = (velocity >= 100);
        bool isLegato = !noteStack.empty();

        targetPitchHz = midiToFreq(static_cast<float>(noteNumber));

        if (!isLegato)
        {
            // Immediate jump to frequency if first note
            currentPitchHz = targetPitchHz;
            slideLedActive = false;
        }
        else
        {
            // Slide / Portamento engages
            slideLedActive = true;
        }

        accentLedActive = isAccented;
        env.noteOn(isAccented, isLegato);

        noteStack.push_back({ noteNumber, velocity });
    }

    void SynthEngine::handleNoteOff(int noteNumber) noexcept
    {
        auto it = std::find_if(noteStack.begin(), noteStack.end(),
                               [noteNumber](const ActiveNote& n) { return n.noteNumber == noteNumber; });

        if (it != noteStack.end())
            noteStack.erase(it);

        if (noteStack.empty())
        {
            env.noteOff();
            slideLedActive = false;
            accentLedActive = false;
        }
        else
        {
            // Slide back to the remaining held note
            const auto& topNote = noteStack.back();
            targetPitchHz = midiToFreq(static_cast<float>(topNote.noteNumber));
            slideLedActive = true;
        }
    }

    void SynthEngine::handlePitchWheel(int wheelValue) noexcept
    {
        // 0 to 16383, center is 8192. Range: +/- 2 semitones
        pitchBendSemi = (static_cast<float>(wheelValue - 8192) / 8192.0f) * 2.0f;
    }

    void SynthEngine::handleMidiMessage(const juce::MidiMessage& msg) noexcept
    {
        if (msg.isNoteOn())
        {
            handleNoteOn(msg.getNoteNumber(), msg.getVelocity());
        }
        else if (msg.isNoteOff())
        {
            handleNoteOff(msg.getNoteNumber());
        }
        else if (msg.isPitchWheel())
        {
            handlePitchWheel(msg.getPitchWheelValue());
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            reset();
        }
    }

    void SynthEngine::processBlock(juce::AudioBuffer<float>& buffer,
                                   const juce::MidiBuffer& midiMessages) noexcept
    {
        const int numSamples = buffer.getNumSamples();

        // A zero-length block can still carry note-offs (some hosts flush the
        // tail this way). Always consume the events, even with nothing to render.
        if (buffer.getNumChannels() == 0 || numSamples == 0)
        {
            for (const auto metadata : midiMessages)
                handleMidiMessage(metadata.getMessage());

            return;
        }

        int rendered = 0;

        for (const auto metadata : midiMessages)
        {
            const int eventSample = juce::jlimit(0, numSamples, metadata.samplePosition);

            if (eventSample > rendered)
            {
                renderSamples(buffer, rendered, eventSample - rendered);
                rendered = eventSample;
            }

            handleMidiMessage(metadata.getMessage());
        }

        if (rendered < numSamples)
            renderSamples(buffer, rendered, numSamples - rendered);
    }

    void SynthEngine::renderSamples(juce::AudioBuffer<float>& buffer,
                                    int startSample, int numSamples) noexcept
    {
        const int numChannels = buffer.getNumChannels();

        // Pitch multiplier for pitch bend
        const float bendFactor = std::pow(2.0f, pitchBendSemi / 12.0f);

        for (int i = startSample; i < startSample + numSamples; ++i)
        {
            // 1. Analog slide smoothing (continuous capacitor charging)
            float effectiveTarget = targetPitchHz * bendFactor;
            currentPitchHz += (effectiveTarget - currentPitchHz) * pitchSlideCoeff;

            // 2. Generate raw 303 oscillator sample
            osc.setFrequency(currentPitchHz);
            float rawOsc = osc.process();

            // Feed the 3D wavetable scope with the raw VCO, before the filter:
            // the display shows the oscillator the 303 actually plays.
            scopeTap.push(rawOsc);

            // 3. Compute envelopes (VCF, VCA, Accent)
            float vcfEnv = 0.0f;
            float vcaEnv = 0.0f;
            float accentEnv = 0.0f;
            env.process(vcfEnv, vcaEnv, accentEnv);

            scopeTap.setNoteState(static_cast<int>(osc.getWaveform()),
                                  currentPitchHz * bendFactor, vcaEnv);

            // 4. Diode Ladder Filter processing
            float filtered = filter.process(rawOsc, envMod, vcfEnv, accentEnv * accentAmount);

            // 5. VCA with accent boost
            float vcaBoost = 1.0f + (accentEnv * accentAmount * 0.45f);
            float amplified = filtered * vcaEnv * vcaBoost;

            // 6. Multi-Distortion Unit processing
            float distorted = distortion.processSample(amplified);

            // 7. Master Gain
            float finalSample = distorted * masterGain;

            // Write to all output channels
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample(ch, i, finalSample);
        }
    }
}
