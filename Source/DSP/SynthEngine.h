#pragma once

#include "Common303.h"
#include "Oscillator303.h"
#include "DiodeLadderFilter.h"
#include "Envelope303.h"
#include "DistortionUnit.h"
#include "ScopeTap.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace Replicant303
{
    class SynthEngine
    {
    public:
        SynthEngine();
        ~SynthEngine() = default;

        void prepare(double sampleRate) noexcept;
        void reset() noexcept;

        // Block audio rendering.
        // MIDI events are rendered sample-accurately: the block is split at
        // every event boundary instead of applying the whole buffer up front.
        void processBlock(juce::AudioBuffer<float>& buffer,
                          const juce::MidiBuffer& midiMessages) noexcept;

        // 303 Synth Parameter Setters
        void setWaveform(Waveform wave) noexcept { osc.setWaveform(wave); }
        void setTuning(float semi) noexcept { tuningSemi = semi; osc.setTuningSemi(semi); }
        void setCutoff(float cutoffHz) noexcept { filter.setCutoff(cutoffHz); }
        void setResonance(float res01) noexcept { filter.setResonance(res01); }
        void setEnvMod(float mod01) noexcept { envMod = mod01; }
        void setDecay(float decaySec) noexcept { env.setDecayTime(decaySec); }
        void setAccent(float acc01) noexcept { accentAmount = acc01; }
        void setMasterVolume(float gainLinear) noexcept { masterGain = gainLinear; }

        // Distortion Parameter Setters
        void setDistortionActive(bool active) noexcept { distortion.setActive(active); }
        void setDistortionType(DistortionType type) noexcept { distortion.setType(type); }
        void setDistortionDrive(float drive01) noexcept { distortion.setDrive(drive01); }
        void setDistortionTone(float tone01) noexcept { distortion.setTone(tone01); }
        void setDistortionMix(float mix01) noexcept { distortion.setMix(mix01); }

        /** Oscilloscope feed for the 3D wavetable display (audio thread writes,
            UI thread reads). */
        const ScopeTap& getScopeTap() const noexcept { return scopeTap; }

        // Realtime UI telemetry
        bool isSlideActive() const noexcept { return slideLedActive; }
        bool isAccentActive() const noexcept { return accentLedActive; }
        int getLastMidiNote() const noexcept { return lastNoteNumber; }
        int getLastVelocity() const noexcept { return lastVelocity; }

    private:
        double sampleRate = 44100.0;

        // DSP Submodules
        Oscillator303 osc;
        DiodeLadderFilter filter;
        ScopeTap scopeTap;
        Envelope303 env;
        DistortionUnit distortion;

        // Synthesis controls
        float envMod = 0.5f;
        float accentAmount = 0.7f;
        float tuningSemi = 0.0f;
        float masterGain = 0.8f;

        // Voice state & Pitch slide (Portamento)
        struct ActiveNote
        {
            int noteNumber = -1;
            int velocity = 0;
        };
        std::vector<ActiveNote> noteStack;

        float targetPitchHz = 110.0f;
        float currentPitchHz = 110.0f;
        float pitchSlideCoeff = 0.999f;
        float pitchBendSemi = 0.0f;

        // Telemetry for LED display
        bool slideLedActive = false;
        bool accentLedActive = false;
        int lastNoteNumber = 36; // C1 / C2
        int lastVelocity = 100;

        void handleMidiMessage(const juce::MidiMessage& msg) noexcept;
        void handleNoteOn(int noteNumber, int velocity) noexcept;
        void handleNoteOff(int noteNumber) noexcept;
        void handlePitchWheel(int wheelValue) noexcept;
        void renderSamples(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept;
        void updateSlideCoefficient() noexcept;
    };
}
