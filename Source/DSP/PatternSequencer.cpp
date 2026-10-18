#include "PatternSequencer.h"

#include <cmath>

namespace Replicant303
{
    //==============================================================================
    PatternSequencer::PatternSequencer()
    {
        for (int i = 0; i < numPatterns; ++i)
            patterns[(size_t) i] = makeDefaultPattern (i);

        publishedPattern = patterns[0];
        reset();
    }

    void PatternSequencer::prepare (double newSampleRate) noexcept
    {
        sampleRate = juce::jmax (1000.0, newSampleRate);
        reset();
    }

    void PatternSequencer::reset() noexcept
    {
        samplesUntilStep = 0.0;
        nextStepIndex = 0;
        gateSamplesRemaining = 0.0;
        heldBySlide = false;
        activeNote = -1;
        currentStep.store (0, std::memory_order_relaxed);

        // A device change or an explicit reset must not leave the transport
        // running from an arbitrary position, but a pending play request is
        // honoured on the next block (starting from step 0).
        running.store (false, std::memory_order_relaxed);
    }

    //==============================================================================
    void PatternSequencer::setTempo (double bpm) noexcept
    {
        tempoBpm.store (juce::jlimit (20.0, 400.0, bpm), std::memory_order_relaxed);
    }

    void PatternSequencer::setHostState (bool hostIsPlaying, double newHostTempo) noexcept
    {
        hostPlaying.store (hostIsPlaying, std::memory_order_relaxed);
        hostTempo.store (newHostTempo > 1.0 ? newHostTempo : 0.0, std::memory_order_relaxed);
    }

    void PatternSequencer::start() noexcept
    {
        playRequested.store (true, std::memory_order_relaxed);

        if (getSyncMode() == SyncMode::Host && ! hostPlaying.load (std::memory_order_relaxed))
            manualPlayWhileHostStopped.store (true, std::memory_order_relaxed);
    }

    void PatternSequencer::stop() noexcept
    {
        if (getSyncMode() == SyncMode::Host
            && ! hostPlaying.load (std::memory_order_relaxed)
            && manualPlayWhileHostStopped.exchange (false, std::memory_order_relaxed))
        {
            return;
        }

        manualPlayWhileHostStopped.store (false, std::memory_order_relaxed);
        playRequested.store (false, std::memory_order_relaxed);
    }

    void PatternSequencer::setSyncMode (SyncMode mode) noexcept
    {
        manualPlayWhileHostStopped.store (false, std::memory_order_relaxed);
        syncMode.store ((int) mode, std::memory_order_relaxed);

        // Switching to the host clock arms the transport: the host's play state
        // then decides when the pattern actually runs.
        if (mode == SyncMode::Host && ! playRequested.load (std::memory_order_relaxed))
            playRequested.store (true, std::memory_order_relaxed);
    }

    double PatternSequencer::getEffectiveTempo() const noexcept
    {
        const auto mode = (SyncMode) syncMode.load (std::memory_order_relaxed);
        double bpm = tempoBpm.load (std::memory_order_relaxed);

        if (mode == SyncMode::Host)
        {
            const double host = hostTempo.load (std::memory_order_relaxed);
            if (host > 1.0)
                bpm = host;
        }

        return juce::jlimit (20.0, 400.0, bpm);
    }

    double PatternSequencer::computeStepSamples() const noexcept
    {
        const double bpm = getEffectiveTempo();
        return sampleRate * 60.0 / bpm / 4.0; // one 16th note
    }

    //==============================================================================
    void PatternSequencer::process (int numSamples, juce::MidiBuffer& midiOut) noexcept
    {
        applyPendingReset (midiOut);

        const auto mode = (SyncMode) syncMode.load (std::memory_order_relaxed);
        const bool hostIsRolling = hostPlaying.load (std::memory_order_relaxed);

        if (hostIsRolling)
            manualPlayWhileHostStopped.store (false, std::memory_order_relaxed);

        // In Host mode the transport follows the host *and* the local PLAY
        // request (or an explicit manual preview while the host is stopped),
        // so STOP still works and PLAY does not fight the DAW.
        const bool wantRunning = (mode == SyncMode::Host)
                                    ? (playRequested.load (std::memory_order_relaxed)
                                       && (hostIsRolling
                                           || manualPlayWhileHostStopped.load (std::memory_order_relaxed)))
                                    : playRequested.load (std::memory_order_relaxed);

        if (wantRunning != running.load (std::memory_order_relaxed))
        {
            running.store (wantRunning, std::memory_order_relaxed);

            if (wantRunning)
            {
                // Always start on beat one: a 303 pattern has no free-running phase.
                samplesUntilStep = 0.0;
                nextStepIndex = 0;
                gateSamplesRemaining = 0.0;
                heldBySlide = false;
                activeNote = -1;
                currentStep.store (0, std::memory_order_relaxed);
            }
            else
            {
                releaseActiveNote (midiOut, 0);
            }
        }

        if (! running.load (std::memory_order_relaxed) || numSamples <= 0)
            return;

        // No lock on the audio thread: the published snapshot is read with a
        // sequence counter, so a preempted message thread cannot stall audio.
        const Pattern pattern = readPublishedPattern();
        const int length = juce::jlimit (1, Pattern::maxSteps, pattern.length);

        for (int i = 0; i < numSamples; ++i)
        {
            if (samplesUntilStep <= 0.0)
            {
                const double interval = triggerStep (nextStepIndex, pattern, midiOut, i);

                currentStep.store (nextStepIndex, std::memory_order_relaxed);
                samplesUntilStep += juce::jmax (1.0, interval);

                // Shrinking the pattern mid-bar must fall back to the first
                // step instead of skipping into the middle of the bar.
                if (nextStepIndex + 1 >= length)
                    nextStepIndex = 0;
                else
                    ++nextStepIndex;
            }

            if (! heldBySlide && gateSamplesRemaining > 0.0)
            {
                gateSamplesRemaining -= 1.0;
                if (gateSamplesRemaining <= 0.0)
                    releaseActiveNote (midiOut, i);
            }

            samplesUntilStep -= 1.0;
        }
    }

    double PatternSequencer::triggerStep (int stepIndex, const Pattern& pattern,
                                          juce::MidiBuffer& midiOut, int sampleOffset) noexcept
    {
        const auto& step = pattern[stepIndex];
        const double stepSamples = computeStepSamples();
        const double swing = (double) swingAmount.load (std::memory_order_relaxed);
        const bool evenStep = (stepIndex % 2) == 0;

        // Swing lengthens the on-beat and shortens the off-beat, so odd steps
        // land late while every pair still adds up to two 16ths.
        const double interval = stepSamples * (evenStep ? (1.0 + swing * 0.5)
                                                        : (1.0 - swing * 0.5));

        // A previous note whose gate was about to expire must not be mistaken
        // for a legato overlap (fractional gate lengths can leave a remainder).
        if (activeNote >= 0 && ! heldBySlide)
            releaseActiveNote (midiOut, sampleOffset);

        if (! step.gate)
        {
            releaseActiveNote (midiOut, sampleOffset);
            return interval;
        }

        const int note = juce::jlimit (0, 127, step.note);
        const int velocity = step.accent ? 110 : 80;

        if (activeNote >= 0)
        {
            // Slide: the new note starts while the old one is still held, which
            // is what makes the engine glide instead of retriggering.
            midiOut.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) velocity), sampleOffset);
            midiOut.addEvent (juce::MidiMessage::noteOff (1, activeNote), sampleOffset);
        }
        else
        {
            midiOut.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) velocity), sampleOffset);
        }

        activeNote = note;
        heldBySlide = step.slide;
        // +1 sample because the countdown starts on the same sample as the
        // note-on: a 50% gate of N samples releases exactly N samples later.
        gateSamplesRemaining = step.slide ? 0.0 : interval * 0.5 + 1.0;

        return interval;
    }

    void PatternSequencer::publishCurrentLocked() noexcept
    {
        // Seqlock write: odd -> data -> even. The audio thread retries when it
        // observes a change, which is cheap and lock free for a POD pattern.
        publishSequence.fetch_add (1, std::memory_order_acq_rel);
        publishedPattern = patterns[(size_t) juce::jlimit (0, numPatterns - 1,
                                                           currentPatternIndex.load (std::memory_order_relaxed))];
        publishSequence.fetch_add (1, std::memory_order_release);
    }

    Pattern PatternSequencer::readPublishedPattern() const noexcept
    {
        for (;;)
        {
            const auto before = publishSequence.load (std::memory_order_acquire);

            if ((before & 1u) != 0u)
                continue;                       // writer in progress

            Pattern snapshot = publishedPattern;

            if (publishSequence.load (std::memory_order_acquire) == before)
                return snapshot;                // no writer touched it meanwhile
        }
    }

    void PatternSequencer::applyPendingReset (juce::MidiBuffer& midiOut) noexcept
    {
        if (! resetRequested.exchange (false, std::memory_order_acq_rel))
            return;

        releaseActiveNote (midiOut, 0);
        samplesUntilStep = 0.0;
        nextStepIndex = 0;
        currentStep.store (0, std::memory_order_relaxed);
    }

    void PatternSequencer::releaseActiveNote (juce::MidiBuffer& midiOut, int sampleOffset) noexcept
    {
        if (activeNote >= 0)
        {
            midiOut.addEvent (juce::MidiMessage::noteOff (1, activeNote), sampleOffset);
            activeNote = -1;
        }

        gateSamplesRemaining = 0.0;
        heldBySlide = false;
    }

    //==============================================================================
    Pattern PatternSequencer::getPattern (int index) const
    {
        const juce::SpinLock::ScopedLockType lock (patternLock);
        return patterns[(size_t) juce::jlimit (0, numPatterns - 1, index)];
    }

    void PatternSequencer::setPattern (int index, const Pattern& pattern)
    {
        auto copy = pattern;
        copy.sanitise();

        const juce::SpinLock::ScopedLockType lock (patternLock);
        patterns[(size_t) juce::jlimit (0, numPatterns - 1, index)] = copy;
        publishCurrentLocked();
    }

    Step PatternSequencer::getStep (int patternIndex, int stepIndex) const
    {
        const juce::SpinLock::ScopedLockType lock (patternLock);
        return patterns[(size_t) juce::jlimit (0, numPatterns - 1, patternIndex)]
                   [juce::jlimit (0, Pattern::maxSteps - 1, stepIndex)];
    }

    void PatternSequencer::setStep (int patternIndex, int stepIndex, const Step& step)
    {
        auto copy = step;
        copy.note = juce::jlimit (Pattern::lowestNote, Pattern::highestNote, copy.note);

        const juce::SpinLock::ScopedLockType lock (patternLock);
        patterns[(size_t) juce::jlimit (0, numPatterns - 1, patternIndex)]
                [juce::jlimit (0, Pattern::maxSteps - 1, stepIndex)] = copy;
        publishCurrentLocked();
    }

    void PatternSequencer::updateStep (int patternIndex, int stepIndex,
                                       const std::function<void (Step&)>& edit)
    {
        const juce::SpinLock::ScopedLockType lock (patternLock);
        auto& step = patterns[(size_t) juce::jlimit (0, numPatterns - 1, patternIndex)]
                             [juce::jlimit (0, Pattern::maxSteps - 1, stepIndex)];
        edit (step);
        step.note = juce::jlimit (Pattern::lowestNote, Pattern::highestNote, step.note);
        publishCurrentLocked();
    }

    void PatternSequencer::clearPattern (int index)
    {
        Pattern p;
        for (auto& s : p.steps)
        {
            s.gate = false;
            s.note = 36;
            s.accent = false;
            s.slide = false;
        }
        p.length = Pattern::maxSteps;
        setPattern (index, p);
    }

    void PatternSequencer::initPattern (int index)
    {
        setPattern (index, makeDefaultPattern (index));
    }

    void PatternSequencer::copyPattern (int from, int to)
    {
        setPattern (to, getPattern (from));
    }

    void PatternSequencer::setLength (int index, int newLength)
    {
        const juce::SpinLock::ScopedLockType lock (patternLock);
        patterns[(size_t) juce::jlimit (0, numPatterns - 1, index)].length
            = juce::jlimit (1, Pattern::maxSteps, newLength);
        publishCurrentLocked();
    }

    int PatternSequencer::getLength (int index) const
    {
        const juce::SpinLock::ScopedLockType lock (patternLock);
        return patterns[(size_t) juce::jlimit (0, numPatterns - 1, index)].length;
    }

    void PatternSequencer::setCurrentPattern (int index)
    {
        currentPatternIndex.store (juce::jlimit (0, numPatterns - 1, index),
                                   std::memory_order_relaxed);

        const juce::SpinLock::ScopedLockType lock (patternLock);
        publishCurrentLocked();
    }

    void PatternSequencer::nextPattern()
    {
        setCurrentPattern ((getCurrentPattern() + 1) % numPatterns);
    }

    void PatternSequencer::previousPattern()
    {
        setCurrentPattern ((getCurrentPattern() + numPatterns - 1) % numPatterns);
    }

    void PatternSequencer::setSwing (float amount01) noexcept
    {
        swingAmount.store (juce::jlimit (0.0f, 0.66f, amount01), std::memory_order_relaxed);
    }

    //==============================================================================
    juce::ValueTree PatternSequencer::toValueTree() const
    {
        juce::ValueTree root ("SEQUENCER");
        root.setProperty ("currentPattern", getCurrentPattern(), nullptr);
        root.setProperty ("swing", (double) getSwing(), nullptr);
        root.setProperty ("syncMode", (int) getSyncMode(), nullptr);

        // Copy the bank under the lock, then build the tree outside it: the
        // audio thread must never wait for ValueTree allocations.
        std::array<Pattern, (size_t) numPatterns> snapshot;

        {
            const juce::SpinLock::ScopedLockType lock (patternLock);
            snapshot = patterns;
        }

        for (int i = 0; i < numPatterns; ++i)
        {
            auto patternTree = snapshot[(size_t) i].toValueTree();
            patternTree.setProperty ("index", i, nullptr);
            root.appendChild (patternTree, nullptr);
        }

        return root;
    }

    void PatternSequencer::restoreFromValueTree (const juce::ValueTree& tree)
    {
        const bool usable = tree.isValid() && tree.hasType ("SEQUENCER");

        // Parse outside the lock, swap in under it.
        std::array<Pattern, (size_t) numPatterns> parsed;

        for (int i = 0; i < numPatterns; ++i)
            parsed[(size_t) i] = makeDefaultPattern (i);

        if (usable)
        {
            for (int i = 0; i < tree.getNumChildren(); ++i)
            {
                const auto child = tree.getChild (i);
                if (! child.hasType ("PATTERN"))
                    continue;

                const int index = (int) child.getProperty ("index", i);
                if (juce::isPositiveAndBelow (index, numPatterns))
                    parsed[(size_t) index] = Pattern::fromValueTree (child);
            }
        }

        {
            const juce::SpinLock::ScopedLockType lock (patternLock);
            patterns = parsed;
            publishCurrentLocked();
        }

        setCurrentPattern (usable ? (int) tree.getProperty ("currentPattern", 0) : 0);
        setSwing (usable ? (float) (double) tree.getProperty ("swing", 0.0) : 0.0f);
        setSyncMode ((SyncMode) (usable ? (int) tree.getProperty ("syncMode", 0) : 0));

        // A preset load must not inherit the previous pattern's clock phase or
        // a note held by the outgoing bank: ask the audio thread to reset.
        resetRequested.store (true, std::memory_order_release);
    }

    //==============================================================================
    juce::String PatternSequencer::getNoteName (int midiNote)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F",
                                       "F#", "G", "G#", "A", "A#", "B" };

        const int clamped = juce::jlimit (0, 127, midiNote);
        const int octave = clamped / 12 - 1;
        return juce::String (names[clamped % 12]) + juce::String (octave);
    }

    bool PatternSequencer::writeMidiFile (const juce::File& file, int patternIndex, double bpm) const
    {
        const auto pattern = getPattern (patternIndex);
        constexpr int ticksPerQuarter = 960;
        constexpr int stepTicks = ticksPerQuarter / 4; // 16th note

        juce::MidiMessageSequence sequence;
        sequence.addEvent (juce::MidiMessage::tempoMetaEvent (
                               (int) std::llround (60000000.0 / juce::jlimit (20.0, 400.0, bpm))),
                           0.0);

        for (int i = 0; i < juce::jlimit (1, Pattern::maxSteps, pattern.length); ++i)
        {
            const auto& step = pattern[i];
            if (! step.gate)
                continue;

            const double onTick = i * stepTicks;
            const double lengthTicks = step.slide ? stepTicks : stepTicks / 2;
            const int velocity = step.accent ? 110 : 80;

            sequence.addEvent (juce::MidiMessage::noteOn (1, step.note, (juce::uint8) velocity), onTick);
            sequence.addEvent (juce::MidiMessage::noteOff (1, step.note), onTick + lengthTicks);
        }

        sequence.updateMatchedPairs();

        juce::MidiFile midiFile;
        midiFile.setTicksPerQuarterNote (ticksPerQuarter);
        midiFile.addTrack (sequence);

        // Write next to the target, then move: a failed export must never
        // destroy the file the user already had.
        auto temp = file.getParentDirectory()
                        .getNonexistentChildFile (".r303_export", ".mid", false);

        {
            juce::FileOutputStream stream (temp);
            if (! stream.openedOk())
                return false;

            if (! midiFile.writeTo (stream))
            {
                temp.deleteFile();
                return false;
            }
        }

        if (! temp.moveFileTo (file))
        {
            file.deleteFile();
            if (! temp.moveFileTo (file))
            {
                temp.deleteFile();
                return false;
            }
        }

        return true;
    }
}
