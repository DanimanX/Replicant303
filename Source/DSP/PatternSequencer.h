#pragma once

#include "Pattern.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <functional>

namespace Replicant303
{
    //==============================================================================
    /** Pattern clock + pattern bank.

        The sequencer owns eight 16-step patterns, plays the selected one as
        16th notes and emits ordinary MIDI note events for the synth engine.
        Accents are encoded as velocity >= 100 and slides as a legato overlap
        (the note of a sliding step is still held when the next step starts),
        which is exactly what the TB-303 voice expects.

        Threading:
          - the audio thread calls prepare()/process()/setTempo()/setHostState();
          - the message thread owns every pattern editing call and the
            transport requests; all shared mutable state is behind `patternLock`
            or an atomic.
    */
    class PatternSequencer
    {
    public:
        static constexpr int numPatterns = 8;

        enum class SyncMode : int
        {
            Internal = 0,   // free-running clock at the tempo parameter
            Host     = 1    // follow the host transport and host BPM
        };

        PatternSequencer();
        ~PatternSequencer() = default;

        //==========================================================================
        // Audio-thread lifecycle
        void prepare (double newSampleRate) noexcept;
        void reset() noexcept;

        double getSampleRate() const noexcept { return sampleRate; }

        void setTempo (double bpm) noexcept;

        /** Host transport information, fed once per block from the play head. */
        void setHostState (bool hostIsPlaying, double newHostTempo) noexcept;

        /** Appends the sequencer's note events for this block into `midiOut`. */
        void process (int numSamples, juce::MidiBuffer& midiOut) noexcept;

        //==========================================================================
        // Transport (message thread requests; the audio thread obeys)
        void start() noexcept;
        void stop() noexcept;

        /** True when the transport is actually advancing. In Host sync this
            also requires the host to be rolling, so the button never lies. */
        bool isPlaying() const noexcept { return running.load (std::memory_order_relaxed); }

        /** True when the transport *should* run (i.e. PLAY was pressed). */
        bool isTransportArmed() const noexcept { return playRequested.load (std::memory_order_relaxed); }

        /** Asks the audio thread to release any held note and restart the
            pattern from step 0 on the next block. Used when a preset replaces
            the line under a running clock. */
        void requestReset() noexcept { resetRequested.store (true, std::memory_order_release); }

        void  setSyncMode (SyncMode mode) noexcept;
        SyncMode getSyncMode() const noexcept { return static_cast<SyncMode> (syncMode.load (std::memory_order_relaxed)); }

        /** Returns the last BPM reported by the DAW playhead (0.0 if none). */
        double getHostTempo() const noexcept { return hostTempo.load (std::memory_order_relaxed); }

        /** Returns true if the DAW transport is currently rolling. */
        bool isHostPlaying() const noexcept { return hostPlaying.load (std::memory_order_relaxed); }

        /** Returns the active tempo in BPM (host BPM in Host mode when valid,
            otherwise the internal tempo parameter). */
        double getEffectiveTempo() const noexcept;

        //==========================================================================
        // Telemetry for the UI
        int getCurrentStep() const noexcept { return currentStep.load (std::memory_order_relaxed); }
        int getCurrentPattern() const noexcept { return currentPatternIndex.load (std::memory_order_relaxed); }

        //==========================================================================
        // Pattern bank (message thread)
        Pattern getPattern (int index) const;
        void    setPattern (int index, const Pattern& pattern);

        Step getStep (int patternIndex, int stepIndex) const;
        void setStep (int patternIndex, int stepIndex, const Step& step);

        /** Edits one step in place, leaving everything else untouched. */
        void updateStep (int patternIndex, int stepIndex,
                         const std::function<void (Step&)>& edit);

        void clearPattern (int index);       // all gates off, notes reset
        void initPattern (int index);        // restore factory content
        void copyPattern (int from, int to);
        void setLength (int index, int newLength);
        int  getLength (int index) const;

        void setCurrentPattern (int index);
        void nextPattern();
        void previousPattern();

        void  setSwing (float amount01) noexcept;
        float getSwing() const noexcept { return swingAmount.load (std::memory_order_relaxed); }

        //==========================================================================
        /** Serialises the whole bank + transport configuration. */
        juce::ValueTree toValueTree() const;

        /** Restores a saved bank. An invalid or absent tree restores the
            factory content instead of keeping whatever was loaded before, so
            old presets cannot leak patterns across projects. */
        void restoreFromValueTree (const juce::ValueTree& tree);

        /** Writes one pattern as a Standard MIDI File (16th grid, accents as
            velocity 110, slides as full-length notes). Returns false if the
            file cannot be written.
        */
        bool writeMidiFile (const juce::File& file, int patternIndex, double bpm) const;

        /** "C2", "F#1", ... (MIDI 24 == C1). */
        static juce::String getNoteName (int midiNote);

    private:
        //==========================================================================
        double triggerStep (int stepIndex, const Pattern& pattern,
                            juce::MidiBuffer& midiOut, int sampleOffset) noexcept;
        void releaseActiveNote (juce::MidiBuffer& midiOut, int sampleOffset) noexcept;
        double computeStepSamples() const noexcept;

        /** Publishes the pattern the audio thread will play. Caller must hold
            patternLock. Uses a sequence counter so the audio thread can read
            the pattern without ever taking a lock. */
        void publishCurrentLocked() noexcept;
        Pattern readPublishedPattern() const noexcept;

        /** Applies a reset requested from the message thread (preset load). */
        void applyPendingReset (juce::MidiBuffer& midiOut) noexcept;

        double sampleRate = 44100.0;

        // --- transport ---
        std::atomic<bool> playRequested { false };
        std::atomic<bool> manualPlayWhileHostStopped { false };
        std::atomic<bool> running { false };
        std::atomic<int>  syncMode { (int) SyncMode::Internal };
        std::atomic<bool> hostPlaying { false };
        std::atomic<double> hostTempo { 0.0 };
        std::atomic<double> tempoBpm { 125.0 };

        // --- audio-thread clock state ---
        double samplesUntilStep = 0.0;
        int    nextStepIndex = 0;
        double gateSamplesRemaining = 0.0;
        bool   heldBySlide = false;
        int    activeNote = -1;

        // --- shared pattern bank ---
        mutable juce::SpinLock patternLock;
        std::array<Pattern, (size_t) numPatterns> patterns;
        std::atomic<int> currentPatternIndex { 0 };
        std::atomic<float> swingAmount { 0.0f };

        // Lock-free published snapshot of the pattern currently playing.
        std::atomic<juce::uint32> publishSequence { 0 };
        Pattern publishedPattern;
        std::atomic<bool> resetRequested { false };

        std::atomic<int> currentStep { 0 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternSequencer)
    };
}
