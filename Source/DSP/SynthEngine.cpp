#include "SynthEngine.h"
#include <bit>

namespace Replicant303
{
    SynthEngine::SynthEngine()
    {
        reset();
    }

    void SynthEngine::prepare(double newSampleRate) noexcept
    {
        sampleRate = std::max(1000.0, newSampleRate);

        osc.prepare(sampleRate);
        filter.prepare(sampleRate);
        env.prepare(sampleRate);
        distortion.prepare(sampleRate);

        updateSlideCoefficient();
        reset();
    }

    void SynthEngine::reset() noexcept
    {
        osc.reset();
        filter.reset();
        env.reset();
        distortion.reset();

        numHeldNotes = 0;
        targetPitchHz = 110.0f;
        currentPitchHz = 110.0f;
        pitchBendSemi = 0.0f;
        pitchBendFactor = 1.0f;
        lastSetPitchHz = -1.0f;
        lastNoteNumber = -1;
        lastVelocity = 100;

        slideLedActive = false;
        accentLedActive = false;
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
        bool isLegato = (numHeldNotes > 0);

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

        // Bounded push. At capacity the oldest note is forgotten rather than
        // letting a runaway controller grow the stack without limit; the note
        // still sounds, it just stops being tracked for the legato hand-off.
        if (numHeldNotes < maxHeldNotes)
        {
            noteStack[(size_t) numHeldNotes++] = { noteNumber, velocity };
        }
        else
        {
            for (int i = 1; i < maxHeldNotes; ++i)
                noteStack[(size_t) (i - 1)] = noteStack[(size_t) i];

            noteStack[(size_t) (maxHeldNotes - 1)] = { noteNumber, velocity };
        }
    }

    void SynthEngine::handleNoteOff(int noteNumber) noexcept
    {
        for (int i = 0; i < numHeldNotes; ++i)
        {
            if (noteStack[(size_t) i].noteNumber != noteNumber)
                continue;

            // Compact in place instead of erasing from a heap container: this
            // runs from the audio callback.
            for (int j = i + 1; j < numHeldNotes; ++j)
                noteStack[(size_t) (j - 1)] = noteStack[(size_t) j];

            --numHeldNotes;
            break;
        }

        if (numHeldNotes == 0)
        {
            env.noteOff();
            slideLedActive = false;
            accentLedActive = false;
            // No voice is left, so telemetry must stop reporting one. The panel
            // reads this to light a key, and a value that outlived the note
            // kept the LED on until the next note replaced it.
            lastNoteNumber = -1;
        }
        else
        {
            // Slide back to the remaining held note
            const auto& topNote = noteStack[(size_t) (numHeldNotes - 1)];
            targetPitchHz = midiToFreq(static_cast<float>(topNote.noteNumber));
            slideLedActive = true;

            // The pitch slides back, but the panel LED must follow what is
            // *held* now, not the note that just ended. Without this the LED
            // stayed on the released key until a new note replaced it.
            lastNoteNumber = topNote.noteNumber;
        }
    }

    void SynthEngine::handlePitchWheel(int wheelValue) noexcept
    {
        // 0 to 16383, center is 8192. Range: +/- 2 semitones
        pitchBendSemi = (static_cast<float>(wheelValue - 8192) / 8192.0f) * 2.0f;
        // Cache the exponential here so renderSamples only multiplies.
        pitchBendFactor = std::pow(2.0f, pitchBendSemi / 12.0f);
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

        // Pitch multiplier for pitch bend (cached by handlePitchWheel)
        const float bendFactor = pitchBendFactor;

        for (int i = startSample; i < startSample + numSamples; ++i)
        {
            // 1. Analog slide smoothing (continuous capacitor charging)
            float effectiveTarget = targetPitchHz * bendFactor;
            currentPitchHz += (effectiveTarget - currentPitchHz) * pitchSlideCoeff;

            // 2. Generate raw 303 oscillator sample. setFrequency is a pure
            // function of (pitch, tuning, sampleRate), so re-running it with
            // an unchanged pitch — e.g. during rests — would change nothing.
            // Compared bitwise to honour -Wfloat-equal: identical bits imply
            // an identical phaseInc, which is the redundancy being skipped.
            if (std::bit_cast<std::int32_t>(currentPitchHz)
                != std::bit_cast<std::int32_t>(lastSetPitchHz))
            {
                osc.setFrequency(currentPitchHz);
                lastSetPitchHz = currentPitchHz;
            }
            float rawOsc = osc.process();

            // 3. Compute envelopes (VCF, VCA, Accent)
            float vcfEnv = 0.0f;
            float vcaEnv = 0.0f;
            float accentEnv = 0.0f;
            env.process(vcfEnv, vcaEnv, accentEnv);

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
