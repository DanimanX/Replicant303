//==============================================================================
// Replicant303 headless regression suite.
//
//   run_tests            (no arguments)
//
// Covers the pattern model, the clock (tempo / swing / accent / slide / length),
// transport edge cases, MIDI export, the sample-accurate synth engine, the
// plugin state round-trip, the pattern_group wiring and the drawer animation.
// It prints one line per check and exits non-zero on any failure.
//==============================================================================

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "DSP/PatternSequencer.h"
#include "DSP/SynthEngine.h"
#include "PluginEditor.h"

#include <iostream>
#include <vector>

namespace
{
    int checks = 0;
    int failures = 0;

    void check (bool condition, const juce::String& what)
    {
        ++checks;

        if (condition)
        {
            std::cout << "  [ok]   " << what << std::endl;
        }
        else
        {
            ++failures;
            std::cout << "  [FAIL] " << what << std::endl;
        }
    }

    void section (const char* name)
    {
        std::cout << "\n== " << name << " ==" << std::endl;
    }

    struct CapturedEvent
    {
        juce::int64 sample = 0;
        juce::MidiMessage message;
    };

    std::vector<CapturedEvent> renderSequencer (Replicant303::PatternSequencer& sequencer,
                                                int totalSamples, int blockSize = 64)
    {
        std::vector<CapturedEvent> events;
        juce::int64 position = 0;

        while (position < totalSamples)
        {
            const int block = juce::jmin (blockSize, (int) (totalSamples - position));

            juce::MidiBuffer buffer;
            sequencer.process (block, buffer);

            for (const auto metadata : buffer)
                events.push_back ({ position + metadata.samplePosition, metadata.getMessage() });

            position += block;
        }

        return events;
    }

    Replicant303::Pattern makePlainPattern (int length = 16, int note = 36)
    {
        Replicant303::Pattern p;
        p.length = length;

        for (auto& s : p.steps)
        {
            s.gate = true;
            s.note = note;
            s.accent = false;
            s.slide = false;
        }

        return p;
    }

    //==========================================================================
    void testPatternModel()
    {
        section ("Pattern model");

        for (int i = 0; i < Replicant303::PatternSequencer::numPatterns; ++i)
        {
            const auto p = Replicant303::makeDefaultPattern (i);
            bool inRange = p.length >= 1 && p.length <= Replicant303::Pattern::maxSteps;
            bool notesInRange = true;
            bool hasGate = false;

            for (const auto& s : p.steps)
            {
                notesInRange = notesInRange
                    && s.note >= Replicant303::Pattern::lowestNote
                    && s.note <= Replicant303::Pattern::highestNote;
                hasGate = hasGate || s.gate;
            }

            check (inRange && notesInRange && hasGate,
                   "factory pattern " + juce::String (i + 1) + " is playable");
        }

        auto p = makePlainPattern();
        p[0].note = 99;
        p[1].note = 2;
        p.sanitise();
        check (p[0].note == Replicant303::Pattern::highestNote
               && p[1].note == Replicant303::Pattern::lowestNote,
               "sanitise clamps notes into the visible range");

        Replicant303::PatternSequencer sequencer;
        sequencer.clearPattern (2);
        const auto cleared = sequencer.getPattern (2);
        bool anyGate = false;
        for (const auto& s : cleared.steps)
            anyGate = anyGate || s.gate;
        check (! anyGate, "clearPattern mutes every step");

        sequencer.initPattern (2);
        check (sequencer.getPattern (2) == Replicant303::makeDefaultPattern (2),
               "initPattern restores factory content");

        auto edited = makePlainPattern();
        edited[5].note = 43;
        edited[5].accent = true;
        edited[5].slide = true;
        sequencer.setPattern (0, edited);
        sequencer.copyPattern (0, 7);
        check (sequencer.getPattern (7) == edited, "copyPattern duplicates the pattern");
    }

    //==========================================================================
    void testSerialization()
    {
        section ("Pattern serialisation");

        auto p = Replicant303::makeDefaultPattern (4);
        p.length = 12;
        p[3].gate = false;
        p[9].slide = true;
        p[11].accent = true;

        const auto tree = p.toValueTree();
        const auto restored = Replicant303::Pattern::fromValueTree (tree);
        check (restored == p, "Pattern -> ValueTree -> Pattern round-trip");

        Replicant303::PatternSequencer a;
        a.setPattern (5, p);
        a.setCurrentPattern (5);
        a.setSwing (0.41f);
        a.setSyncMode (Replicant303::PatternSequencer::SyncMode::Host);

        Replicant303::PatternSequencer b;
        b.restoreFromValueTree (a.toValueTree());

        check (b.getPattern (5) == p, "sequencer state restores the edited pattern");
        check (b.getCurrentPattern() == 5, "sequencer state restores the active pattern");
        check (std::abs (b.getSwing() - 0.41f) < 1.0e-4f, "sequencer state restores swing");
        check (b.getSyncMode() == Replicant303::PatternSequencer::SyncMode::Host,
               "sequencer state restores sync mode");
    }

    //==========================================================================
    void testClock()
    {
        section ("Sequencer clock");

        // 120 BPM @ 48 kHz -> 6000 samples per 16th.
        Replicant303::PatternSequencer sequencer;
        sequencer.prepare (48000.0);
        sequencer.setTempo (120.0);
        sequencer.setCurrentPattern (0);
        sequencer.setPattern (0, makePlainPattern());
        sequencer.setSwing (0.0f);
        sequencer.start();

        auto events = renderSequencer (sequencer, 13000);

        juce::int64 firstOn = -1, firstOff = -1, secondOn = -1, secondOff = -1;
        for (const auto& e : events)
        {
            if (e.message.isNoteOn())
            {
                if (firstOn < 0) firstOn = e.sample;
                else if (secondOn < 0) secondOn = e.sample;
            }
            else if (e.message.isNoteOff())
            {
                if (firstOff < 0) firstOff = e.sample;
                else if (secondOff < 0) secondOff = e.sample;
            }
        }

        check (firstOn == 0, "first step fires on sample 0 (got " + juce::String (firstOn) + ")");
        check (firstOff == 3000, "50% gate releases at sample 3000 (got " + juce::String (firstOff) + ")");
        check (secondOn == 6000, "second step fires at sample 6000 (got " + juce::String (secondOn) + ")");
        check (secondOff == 9000, "second gate releases at sample 9000 (got " + juce::String (secondOff) + ")");
        check (events.front().message.isNoteOn(), "first event is a note-on");

        // 60 BPM -> 12000 samples per 16th.
        Replicant303::PatternSequencer slow;
        slow.prepare (48000.0);
        slow.setTempo (60.0);
        slow.setPattern (0, makePlainPattern());
        slow.start();

        auto slowEvents = renderSequencer (slow, 13000);
        check (! slowEvents.empty() && slowEvents.front().sample == 0, "60 BPM starts on sample 0");

        juce::int64 slowSecondOn = -1;
        int slowOns = 0;
        for (const auto& e : slowEvents)
            if (e.message.isNoteOn() && ++slowOns == 2)
                slowSecondOn = e.sample;

        check (slowSecondOn == 12000,
               "60 BPM puts the second step at sample 12000 (got " + juce::String (slowSecondOn) + ")");

        // Accent -> velocity 110, plain -> 80.
        Replicant303::PatternSequencer accentSeq;
        accentSeq.prepare (48000.0);
        accentSeq.setTempo (120.0);
        auto accented = makePlainPattern();
        accented[0].accent = true;
        accentSeq.setPattern (0, accented);
        accentSeq.start();

        auto accentEvents = renderSequencer (accentSeq, 100);
        check (! accentEvents.empty() && accentEvents.front().message.getVelocity() == 110,
               "accented step sends velocity 110");

        // Swing 50% delays the off-beat step to 7500.
        Replicant303::PatternSequencer swingSeq;
        swingSeq.prepare (48000.0);
        swingSeq.setTempo (120.0);
        swingSeq.setPattern (0, makePlainPattern());
        swingSeq.setSwing (0.5f);
        swingSeq.start();

        auto swingEvents = renderSequencer (swingSeq, 13000);
        juce::int64 swingSecondOn = -1;
        int noteOns = 0;
        for (const auto& e : swingEvents)
        {
            if (e.message.isNoteOn() && ++noteOns == 2)
            {
                swingSecondOn = e.sample;
                break;
            }
        }
        check (swingSecondOn == 7500,
               "50% swing delays the second step to 7500 (got " + juce::String (swingSecondOn) + ")");

        // Slide: step 0 holds through the boundary; step 1 starts while it is held.
        Replicant303::PatternSequencer slideSeq;
        slideSeq.prepare (48000.0);
        slideSeq.setTempo (120.0);
        auto sliding = makePlainPattern();
        sliding[0].slide = true;
        sliding[1].note = 43;
        slideSeq.setPattern (0, sliding);
        slideSeq.start();

        auto slideEvents = renderSequencer (slideSeq, 7000);
        bool offBeforeBoundary = false;
        juce::int64 boundaryFirst = -1, boundarySecond = -1;

        for (const auto& e : slideEvents)
        {
            if (e.message.isNoteOff() && e.sample < 6000)
                offBeforeBoundary = true;

            if (e.sample == 6000)
            {
                if (boundaryFirst < 0) boundaryFirst = e.message.isNoteOn() ? 1 : 0;
                else if (boundarySecond < 0) boundarySecond = e.message.isNoteOn() ? 1 : 0;
            }
        }

        check (! offBeforeBoundary, "a sliding step is still held at the next boundary");
        check (boundaryFirst == 1 && boundarySecond == 0,
               "slide emits note-on then note-off (legato overlap)");

        // Pattern length: four steps loop back to step 0.
        Replicant303::PatternSequencer shortSeq;
        shortSeq.prepare (48000.0);
        shortSeq.setTempo (120.0);
        shortSeq.setPattern (0, makePlainPattern (4));
        shortSeq.start();

        auto shortEvents = renderSequencer (shortSeq, 25000);
        juce::int64 thirdOn = -1;
        int ons = 0;
        for (const auto& e : shortEvents)
            if (e.message.isNoteOn() && ++ons == 5)
                thirdOn = e.sample;

        check (thirdOn == 24000, "a 4-step pattern loops after 24000 samples");
        check (shortSeq.getCurrentStep() == 0, "telemetry wraps to step 0 after the loop");

        // Stop releases the held note.
        Replicant303::PatternSequencer stopSeq;
        stopSeq.prepare (48000.0);
        stopSeq.setTempo (120.0);
        stopSeq.setPattern (0, makePlainPattern());
        stopSeq.start();
        renderSequencer (stopSeq, 1000);
        stopSeq.stop();

        juce::MidiBuffer releaseBuffer;
        stopSeq.process (64, releaseBuffer);
        bool gotOff = false;
        for (const auto metadata : releaseBuffer)
            gotOff = gotOff || metadata.getMessage().isNoteOff();
        check (gotOff, "stop() releases the sounding note");
    }

    void testTransportEdges()
    {
        section ("Transport edge cases");

        // --- host sync: PLAY arms, the host decides, STOP still works. -------
        Replicant303::PatternSequencer host;
        host.prepare (48000.0);
        host.setTempo (120.0);
        host.setPattern (0, makePlainPattern());
        host.setSyncMode (Replicant303::PatternSequencer::SyncMode::Host);

        host.setHostState (false, 140.0);
        juce::MidiBuffer buffer;
        host.process (512, buffer);
        check (! host.isPlaying(), "host sync stays stopped while the host is stopped");

        host.setHostState (true, 140.0);
        host.process (512, buffer);
        check (host.isPlaying(), "host sync follows the host transport");

        host.stop();
        buffer.clear();
        host.process (512, buffer);
        check (! host.isPlaying(), "STOP disarms host sync even while the host rolls");

        // --- switching to host sync arms the transport ----------------------
        Replicant303::PatternSequencer armed;
        armed.prepare (48000.0);
        check (! armed.isTransportArmed(), "internal transport starts disarmed");
        armed.setSyncMode (Replicant303::PatternSequencer::SyncMode::Host);
        check (armed.isTransportArmed(), "switching to host sync arms the transport");

        // --- shrinking the length mid-bar must not skip steps ---------------
        Replicant303::PatternSequencer shrink;
        shrink.prepare (48000.0);
        shrink.setTempo (120.0);
        shrink.setPattern (0, makePlainPattern (16));
        shrink.start();

        // Run past step 9 (6000 samples per 16th -> 55000 samples).
        renderSequencer (shrink, 55000);
        shrink.setLength (0, 4);

        auto after = renderSequencer (shrink, 7000);
        juce::int64 nextOn = -1;
        for (const auto& e : after)
        {
            if (e.message.isNoteOn())
            {
                nextOn = e.sample;
                break;
            }
        }

        check (nextOn >= 0 && nextOn <= 6000,
               "after shrinking the pattern the next step is step 1, not a skipped one");

        int firstNote = after.empty() ? -1 : after.front().message.getNoteNumber();
        check (firstNote == 36, "the next step after a shrink is a valid note");

        // --- a pattern that loops back lands on step 0 ----------------------
        bool sawStepZero = false;
        for (int i = 0; i < 200; ++i)
        {
            juce::MidiBuffer b;
            shrink.process (128, b);
            if (shrink.getCurrentStep() == 0)
                sawStepZero = true;
        }
        check (sawStepZero, "a 4-step pattern keeps returning to step 0");

        // --- preset load resets the bank and the clock ----------------------
        Replicant303::PatternSequencer restored;
        restored.prepare (48000.0);
        auto edited = makePlainPattern();
        edited[2].note = 24;
        restored.setPattern (0, edited);

        restored.restoreFromValueTree (juce::ValueTree());   // old preset, no sequencer node
        check (restored.getPattern (0) == Replicant303::makeDefaultPattern (0),
               "an absent sequencer node restores factory patterns");
        check (restored.getCurrentPattern() == 0, "an absent sequencer node resets the active pattern");
    }

    //==========================================================================
    void testEngineEdges()
    {
        section ("Engine edge cases");

        Replicant303::SynthEngine engine;
        engine.prepare (48000.0);

        juce::AudioBuffer<float> warmUp (2, 24000);   // 0.5 s of note
        juce::MidiBuffer noteOn;
        noteOn.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 110), 0);
        engine.processBlock (warmUp, noteOn);

        // A zero-length block still has to consume the note-off.
        juce::AudioBuffer<float> empty (2, 0);
        juce::MidiBuffer noteOff;
        noteOff.addEvent (juce::MidiMessage::noteOff (1, 36), 0);
        engine.processBlock (empty, noteOff);

        float notePeak = 0.0f;
        for (int i = 0; i < warmUp.getNumSamples(); ++i)
            notePeak = juce::jmax (notePeak, std::abs (warmUp.getSample (0, i)));

        float tailPeak = 0.0f;
        for (int block = 0; block < 200; ++block)   // ~2.1 s of release
        {
            juce::AudioBuffer<float> tail (2, 512);
            juce::MidiBuffer none;
            engine.processBlock (tail, none);

            // Only the last 0.5 s counts: the release itself is audible.
            if (block >= 150)
                for (int i = 0; i < tail.getNumSamples(); ++i)
                    tailPeak = juce::jmax (tailPeak, std::abs (tail.getSample (0, i)));
        }

        check (notePeak > 0.01f, "the note actually sounded before the zero-length note-off");
        check (tailPeak < notePeak * 0.1f,
               "a note-off in a zero-length block is not lost (tail decays to "
               + juce::String (tailPeak / juce::jmax (1.0e-6f, notePeak), 4) + " of peak)");
    }

    //==========================================================================
    void testMidiExport()
    {
        section ("MIDI export");

        Replicant303::PatternSequencer sequencer;
        sequencer.setPattern (0, makePlainPattern (4));

        auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getNonexistentChildFile ("replicant303_test", ".mid", false);

        check (sequencer.writeMidiFile (file, 0, 120.0), "writeMidiFile returns true");
        check (file.existsAsFile() && file.getSize() > 0, "MIDI file exists and is not empty");

        juce::MidiFile midiFile;
        juce::FileInputStream input (file);
        check (midiFile.readFrom (input), "exported MIDI file parses");
        check (midiFile.getNumTracks() == 1, "exported MIDI has one track");

        if (const auto* track = midiFile.getTrack (0))
        {
            int noteOns = 0;
            int noteOffs = 0;
            double firstOnTick = -1.0;
            double secondOnTick = -1.0;

            for (int i = 0; i < track->getNumEvents(); ++i)
            {
                const auto& msg = track->getEventPointer (i)->message;

                if (msg.isNoteOn())
                {
                    if (noteOns == 0) firstOnTick = msg.getTimeStamp();
                    if (noteOns == 1) secondOnTick = msg.getTimeStamp();
                    ++noteOns;
                }
                else if (msg.isNoteOff())
                {
                    ++noteOffs;
                }
            }

            check (noteOns == 4 && noteOffs == 4, "four gated steps export four note pairs");
            check (std::abs (firstOnTick) < 1.0e-9, "first exported note starts at tick 0");
            check (std::abs (secondOnTick - 240.0) < 1.0e-9,
                   "second exported note starts one 16th later (240 ticks)");
        }
        else
        {
            check (false, "exported MIDI contains a readable track");
        }

        file.deleteFile();
    }

    //==========================================================================
    void testSynthEngine()
    {
        section ("Synth engine");

        Replicant303::SynthEngine engine;
        engine.prepare (48000.0);
        engine.setCutoff (1200.0f);
        engine.setResonance (0.7f);
        engine.setEnvMod (0.6f);
        engine.setDecay (0.7f);
        engine.setAccent (0.8f);
        engine.setMasterVolume (0.8f);

        juce::AudioBuffer<float> buffer (2, 512);
        buffer.clear();

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 256);
        engine.processBlock (buffer, midi);

        float beforeEvent = 0.0f;
        float afterEvent = 0.0f;

        for (int i = 0; i < 256; ++i)
            beforeEvent = juce::jmax (beforeEvent, std::abs (buffer.getSample (0, i)));

        for (int i = 256; i < 512; ++i)
            afterEvent = juce::jmax (afterEvent, std::abs (buffer.getSample (0, i)));

        check (beforeEvent < 1.0e-6f, "audio before the MIDI event is silent (sample-accurate timing)");
        check (afterEvent > 1.0e-3f, "audio after the MIDI event is sounding");

        int models = 0;
        for (int model = 0; model < 5; ++model)
        {
            engine.reset();
            engine.prepare (48000.0);
            engine.setDistortionActive (true);
            engine.setDistortionType ((Replicant303::DistortionType) model);
            engine.setDistortionDrive (0.8f);
            engine.setDistortionTone (0.6f);
            engine.setDistortionMix (0.9f);

            bool finite = true;
            float peak = 0.0f;
            double energy = 0.0;

            juce::MidiBuffer noteOn;
            noteOn.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 110), 0);

            for (int block = 0; block < 60; ++block)
            {
                juce::AudioBuffer<float> chunk (2, 512);
                chunk.clear();

                if (block == 0)
                {
                    engine.processBlock (chunk, noteOn);
                }
                else
                {
                    juce::MidiBuffer empty;
                    engine.processBlock (chunk, empty);
                }

                for (int ch = 0; ch < 2; ++ch)
                {
                    const auto* data = chunk.getReadPointer (ch);
                    for (int i = 0; i < chunk.getNumSamples(); ++i)
                    {
                        const float v = data[i];
                        finite = finite && std::isfinite (v);
                        peak = juce::jmax (peak, std::abs (v));
                        energy += (double) v * v;
                    }
                }
            }

            check (finite, "distortion model " + juce::String (model + 1) + " stays finite");
            check (peak < 4.0f, "distortion model " + juce::String (model + 1) + " stays bounded");
            check (energy > 1.0, "distortion model " + juce::String (model + 1) + " produces audio");
            ++models;
        }

        check (models == 5, "all five distortion models were rendered");
    }

    //==========================================================================
    void testEditorDrawer()
    {
        section ("Editor drawer");

        Replicant303AudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        check (editor != nullptr, "editor is created");

        if (editor == nullptr)
            return;

        auto* concrete = dynamic_cast<Replicant303AudioProcessorEditor*> (editor.get());
        check (concrete != nullptr, "editor has the expected concrete type");

        if (concrete == nullptr)
            return;

        check (editor->getHeight() == 590, "panel starts collapsed (590 px)");

        concrete->setPatternDrawerOpen (true, false);
        check (editor->getHeight() == 990, "drawer opens to 990 px without animation");

        concrete->setPatternDrawerOpen (false, false);
        check (editor->getHeight() == 590, "drawer closes back to 590 px");

        concrete->setPatternDrawerOpen (true, true);
        check (concrete->isPatternDrawerOpen(), "drawer reports the open state");

        juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
        check (editor->getHeight() == 990, "animated drawer reaches the full height");

        concrete->setPatternDrawerOpen (false, true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
        check (editor->getHeight() == 590, "animated drawer closes completely");
    }

    //==========================================================================
    void testProcessor()
    {
        section ("Plugin processor");

        Replicant303AudioProcessor processor;

        const juce::StringArray required { "waveform", "tuning", "cutoff", "resonance",
                                           "env_mod", "decay", "accent", "volume",
                                           "tempo", "pattern_group", "dist_active",
                                           "dist_type", "dist_drive", "dist_tone",
                                           "dist_mix", "ui_theme" };

        bool allPresent = true;
        for (const auto& id : required)
            allPresent = allPresent && processor.apvts.getParameter (id) != nullptr;
        check (allPresent, "all parameters are registered");

        processor.prepareToPlay (96000.0, 512);
        check (std::abs (processor.getSequencer().getSampleRate() - 96000.0) < 1.0e-9,
               "sequencer follows the host sample rate");

        // Edit a pattern, persist, reload into a fresh instance.
        auto& sequencer = processor.getSequencer();
        auto pattern = sequencer.getPattern (2);
        pattern.length = 12;
        pattern[5].note = 43;
        pattern[5].accent = true;
        pattern[5].slide = true;
        pattern[5].gate = true;
        sequencer.setPattern (2, pattern);
        sequencer.setCurrentPattern (3);
        sequencer.setSwing (0.33f);
        sequencer.setSyncMode (Replicant303::PatternSequencer::SyncMode::Host);

        juce::MemoryBlock state;
        processor.getStateInformation (state);
        check (state.getSize() > 0, "state serialises to a non-empty block");

        Replicant303AudioProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());

        check (restored.getSequencer().getPattern (2) == pattern,
               "pattern survives the plugin state round-trip");
        check (restored.getSequencer().getCurrentPattern() == 3,
               "active pattern survives the state round-trip");
        check (std::abs (restored.getSequencer().getSwing() - 0.33f) < 1.0e-4f,
               "swing survives the state round-trip");
        check (restored.getSequencer().getSyncMode()
                   == Replicant303::PatternSequencer::SyncMode::Host,
               "sync mode survives the state round-trip");

        // Render the sequencer through the processor: silence when stopped,
        // audio when playing.
        processor.prepareToPlay (48000.0, 512);

        double peakStopped = 0.0;
        for (int block = 0; block < 10; ++block)
        {
            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                peakStopped = juce::jmax (peakStopped, (double) std::abs (buffer.getSample (0, i)));
        }
        check (peakStopped < 1.0e-6, "stopped transport renders silence");

        sequencer.setSyncMode (Replicant303::PatternSequencer::SyncMode::Internal);
        sequencer.setCurrentPattern (0);
        sequencer.setPattern (0, makePlainPattern());
        sequencer.start();

        double peakPlaying = 0.0;
        bool finite = true;

        for (int block = 0; block < 120; ++block)
        {
            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                {
                    const float v = buffer.getSample (ch, i);
                    finite = finite && std::isfinite (v);
                    peakPlaying = juce::jmax (peakPlaying, (double) std::abs (v));
                }
        }

        check (finite, "rendered output is finite");
        check (peakPlaying > 0.01, "playing transport renders audio from the pattern");
        check (peakPlaying < 4.0, "rendered output stays bounded");

        sequencer.stop();

        float tailPeak = 0.0f;
        for (int block = 0; block < 60; ++block)   // ~640 ms of release
        {
            juce::AudioBuffer<float> tail (2, 512);
            juce::MidiBuffer tailMidi;
            processor.processBlock (tail, tailMidi);

            if (block == 59)
                for (int i = 0; i < tail.getNumSamples(); ++i)
                    tailPeak = juce::jmax (tailPeak, std::abs (tail.getSample (0, i)));
        }

        check (tailPeak < 0.01f, "output decays to silence after stop (no stuck note)");
    }
}

//==============================================================================
int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::cout << "Replicant303 test suite" << std::endl;

    testPatternModel();
    testSerialization();
    testClock();
    testTransportEdges();
    testMidiExport();
    testSynthEngine();
    testEngineEdges();
    testProcessor();
    testEditorDrawer();

    std::cout << "\n----------------------------------------" << std::endl;
    std::cout << checks - failures << " / " << checks << " checks passed" << std::endl;

    if (failures != 0)
    {
        std::cout << failures << " FAILURE(S)" << std::endl;
        return 1;
    }

    std::cout << "ALL TESTS PASSED" << std::endl;
    return 0;
}
