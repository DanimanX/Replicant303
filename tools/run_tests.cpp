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
#include "Presets/PresetBank.h"
#include "Presets/PresetIO.h"
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
    void testPresetVault()
    {
        section ("Preset vault");

        Replicant303::PresetBank bank;

        check (Replicant303::PresetBank::numBanks == 8
               && Replicant303::PresetBank::presetsPerBank == 64,
               "the vault is eight banks of 64 presets");

        bool legacyIntact = true;
        for (int b = 0; b < Replicant303::PresetBank::numBanks; ++b)
        {
            const auto& slotZero = bank.getPreset (b, 0);
            legacyIntact = legacyIntact
                           && slotZero.pattern == Replicant303::makeDefaultPattern (b);
        }
        check (legacyIntact, "slot 01 of every letter is the original factory pattern");

        check (bank.getPreset (0, 0).name == "Amber Line"
               && bank.getPreset (7, 0).name == "Last Replicant",
               "the legacy names survive the vault");

        auto generatedA = Replicant303::PresetBank::generatePreset (3, 17, 0);
        auto generatedB = Replicant303::PresetBank::generatePreset (3, 17, 0);
        check (generatedA == generatedB, "the preset generator is deterministic");

        auto seeded = Replicant303::PresetBank::generatePreset (3, 17, 0xdecafbad);
        check (seeded != generatedA, "a different seed generates a different preset");

        bool playable = true;
        bool namesUnique = true;

        for (int b = 0; b < Replicant303::PresetBank::numBanks; ++b)
        {
            juce::StringArray names;

            for (int slot = 0; slot < Replicant303::PresetBank::presetsPerBank; ++slot)
            {
                const auto preset = Replicant303::PresetBank::makeFactoryPreset (b, slot);
                names.addIfNotAlreadyThere (preset.name);

                bool hasGate = false;

                for (int i = 0; i < Replicant303::Pattern::maxSteps; ++i)
                {
                    const auto& step = preset.pattern[i];
                    playable = playable
                               && step.note >= Replicant303::Pattern::lowestNote
                               && step.note <= Replicant303::Pattern::highestNote;
                    hasGate = hasGate || step.gate;
                }

                playable = playable
                           && hasGate
                           && preset.pattern.length >= 1
                           && preset.pattern.length <= Replicant303::Pattern::maxSteps
                           && preset.tempo >= 40.0f && preset.tempo <= 300.0f
                           && preset.cutoff >= 25.0f && preset.cutoff <= 4500.0f;
            }

            namesUnique = namesUnique && names.size() == Replicant303::PresetBank::presetsPerBank;
        }

        check (playable, "all 512 factory presets are playable and in range");
        check (namesUnique, "every bank has 64 distinct preset names");

        //----------------------------------------------------------------------
        auto custom = bank.getPreset (2, 9);
        custom.name = "Test Circuit";
        custom.cutoff = 1234.0f;
        custom.pattern[0].note = 43;
        custom.pattern[0].accent = true;

        bank.setPreset (2, 9, custom);
        bank.setSelectedSlot (2, 9);
        check (bank.getPreset (2, 9) == custom, "setPreset stores a user override");
        check (bank.isOverridden (2, 9), "the overridden slot is flagged");
        check (! bank.isOverridden (2, 8), "neighbouring slots stay factory");

        auto state = bank.toValueTree();
        Replicant303::PresetBank restored;
        restored.restoreFromValueTree (state);
        check (restored.getPreset (2, 9) == custom,
               "user overrides survive the preset-bank state round-trip");
        check (! restored.isOverridden (2, 8),
               "factory slots are not serialised as overrides");
        check (restored.getSelectedSlot (2) == 9,
               "the selected preset slot survives the preset-bank state round-trip");

        restored.restoreFromValueTree (juce::ValueTree());
        check (restored.getPreset (2, 9) == Replicant303::PresetBank::makeFactoryPreset (2, 9),
               "a missing bank state restores factory content");
        check (restored.getSelectedSlot (2) == 0,
               "a missing bank state resets the selected slot");

        //----------------------------------------------------------------------
        const auto jsonPreset = Replicant303::PresetBank::makeFactoryPreset (5, 23);
        const auto json = Replicant303::PresetIO::toJson (jsonPreset);
        Replicant303::Preset fromJson;
        check (Replicant303::PresetIO::fromJson (json, fromJson) && fromJson == jsonPreset,
               "preset JSON round-trip");

        auto quotedPreset = jsonPreset;
        quotedPreset.name = "Acid \"Quote\" // 303";
        Replicant303::Preset quotedRoundTrip;
        check (Replicant303::PresetIO::fromJson (Replicant303::PresetIO::toJson (quotedPreset),
                                                 quotedRoundTrip)
                   && quotedRoundTrip == quotedPreset,
               "preset JSON escapes quotes in the name");

        auto tempDirectory = juce::File::getSpecialLocation (juce::File::tempDirectory);
        auto presetFile = tempDirectory.getNonexistentChildFile ("r303_preset_test", ".midi", false);

        check (Replicant303::PresetIO::writeMidiFile (jsonPreset, presetFile),
               "preset writes as .midi");
        check (presetFile.existsAsFile() && presetFile.getSize() > 0,
               "the .midi preset file is not empty");

        Replicant303::Preset filePreset;
        check (Replicant303::PresetIO::readMidiFile (presetFile, filePreset),
               "preset reads back from .midi");
        check (filePreset == jsonPreset, ".midi preset round-trip preserves the full patch");

        {
            juce::MidiFile midiFile;
            juce::FileInputStream input (presetFile);
            check (midiFile.readFrom (input), "the preset is a valid Standard MIDI File");

            bool foundMetadata = false;
            bool foundTempo = false;

            if (const auto* track = midiFile.getTrack (0))
            {
                for (int i = 0; i < track->getNumEvents(); ++i)
                {
                    const auto& message = track->getEventPointer (i)->message;

                    if (message.isTextMetaEvent()
                        && message.getTextFromTextMetaEvent().startsWith (
                               Replicant303::PresetIO::metadataPrefix))
                        foundMetadata = true;

                    if (message.isTempoMetaEvent())
                        foundTempo = true;
                }
            }

            check (foundMetadata && foundTempo,
                   ".midi preset embeds Replicant metadata and a tempo event");
        }

        presetFile.deleteFile();

        //----------------------------------------------------------------------
        // A plain MIDI file (no metadata) imports as a pattern.
        Replicant303::PatternSequencer sequencer;
        auto plain = makePlainPattern (4);
        plain[0].accent = true;
        plain[1].note = 43;
        plain[1].slide = true;
        sequencer.setPattern (0, plain);

        auto plainFile = tempDirectory.getNonexistentChildFile ("r303_plain_midi", ".mid", false);
        check (sequencer.writeMidiFile (plainFile, 0, 128.0), "plain MIDI export succeeds");

        Replicant303::Preset plainPreset;
        check (Replicant303::PresetIO::readMidiFile (plainFile, plainPreset),
               "a plain MIDI file imports as a preset");
        check (plainPreset.pattern.length == 4, "plain MIDI import keeps a 4-step length");
        check (plainPreset.pattern[0].gate && plainPreset.pattern[0].accent,
               "plain MIDI import derives the accent from velocity");
        check (plainPreset.pattern[1].note == 43 && plainPreset.pattern[1].slide,
               "plain MIDI import derives notes and slides");
        check (std::abs (plainPreset.tempo - 128.0f) < 0.5f,
               "plain MIDI import reads the tempo");
        plainFile.deleteFile();

        auto invalidFile = tempDirectory.getNonexistentChildFile ("r303_not_midi", ".midi", false);
        invalidFile.replaceWithText ("this is not a midi file");
        Replicant303::Preset invalidPreset;
        check (! Replicant303::PresetIO::readMidiFile (invalidFile, invalidPreset),
               "invalid .midi files are rejected");
        invalidFile.deleteFile();
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
    struct MockPlayHead final : public juce::AudioPlayHead
    {
        double bpm = 140.0;
        bool playing = false;
        double ppq = 0.0;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (bpm);
            info.setIsPlaying (playing);
            info.setPpqPosition (ppq);
            return info;
        }
    };

    //==========================================================================
    void testEditorDrawer()
    {
        section ("Editor drawer & zoom");

        Replicant303AudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        check (editor != nullptr, "editor is created");

        if (editor == nullptr)
            return;

        auto* concrete = dynamic_cast<Replicant303AudioProcessorEditor*> (editor.get());
        check (concrete != nullptr, "editor has the expected concrete type");

        if (concrete == nullptr)
            return;

        check (editor->isResizable(), "editor window is resizable");
        check (editor->getWidth() == 1080 && editor->getHeight() == 590,
               "panel starts collapsed at 100% zoom (1080x590 px)");

        concrete->setPatternDrawerOpen (true, false);
        check (editor->getHeight() == 590 + PatternDrawer::drawerHeight,
               "drawer opens to the full panel + drawer height without animation");

        concrete->setPatternDrawerOpen (false, false);
        check (editor->getHeight() == 590, "drawer closes back to 590 px");

        concrete->setPatternDrawerOpen (true, true);
        check (concrete->isPatternDrawerOpen(), "drawer reports the open state");

        juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
        check (editor->getHeight() == 590 + PatternDrawer::drawerHeight,
               "animated drawer reaches the full height");

        concrete->setPatternDrawerOpen (false, true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
        check (editor->getHeight() == 590, "animated drawer closes completely");

        // Zoom between 50% and 300%
        concrete->setZoomFactor (0.50f);
        check (std::abs (concrete->getZoomFactor() - 0.50f) < 1.0e-3f,
               "setZoomFactor(0.50) sets 50% zoom");
        check (editor->getWidth() == 540 && editor->getHeight() == 295,
               "50% zoom scales editor to 540x295 px");

        concrete->setZoomFactor (3.00f);
        check (std::abs (concrete->getZoomFactor() - 3.00f) < 1.0e-3f,
               "setZoomFactor(3.00) sets 300% zoom");
        check (editor->getWidth() == 3240 && editor->getHeight() == 1770,
               "300% zoom scales editor to 3240x1770 px");

        concrete->setZoomFactor (0.10f);
        check (std::abs (concrete->getZoomFactor() - 0.50f) < 1.0e-3f,
               "zoom below 50% clamps to 50%");

        concrete->setZoomFactor (5.00f);
        check (std::abs (concrete->getZoomFactor() - 3.00f) < 1.0e-3f,
               "zoom above 300% clamps to 300%");

        concrete->setZoomFactor (1.50f);
        concrete->setPatternDrawerOpen (true, false);
        check (editor->getWidth() == 1620
                   && editor->getHeight() == juce::roundToInt ((590 + PatternDrawer::drawerHeight) * 1.50f),
               "opening drawer at 150% zoom preserves 150% scale");

        concrete->setPatternDrawerOpen (false, false);
        concrete->setZoomFactor (1.00f);
        check (editor->getWidth() == 1080 && editor->getHeight() == 590,
               "returning to 100% zoom restores 1080x590 px");

        // Waveform bank dropdown: opening it must not move the window, and a
        // selection must reach the parameter the audio thread reads.
        concrete->setWaveformMenuOpen (true);
        check (concrete->isWaveformMenuOpen(), "the waveform bank drops down");
        check (editor->getHeight() == 590,
               "the waveform bank overlays the panel instead of resizing it");

        concrete->selectWaveformIndex (26);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (80);

        check (! concrete->isWaveformMenuOpen(), "picking a shape closes the bank");
        check (juce::roundToInt (processor.apvts.getRawParameterValue ("waveform")->load()) == 26,
               "picking a shape writes the waveform parameter");

        // Loading the H bank moves the TRACK knob to group 3. The timer used to
        // snap the active pattern back to group*2 (pattern F); it must leave
        // the explicitly loaded letter H alone.
        processor.loadPreset (7, 5);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        check (processor.getSequencer().getCurrentPattern() == 7,
               "a loaded preset is not snapped back by the TRACK knob");
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
                                           "dist_mix", "ui_theme", "ui_zoom" };

        bool allPresent = true;
        for (const auto& id : required)
            allPresent = allPresent && processor.apvts.getParameter (id) != nullptr;
        check (allPresent, "all parameters are registered");

        check (processor.getSequencer().getSyncMode()
                   == Replicant303::PatternSequencer::SyncMode::Host,
               "processor defaults to Host sync mode so DAW tempo is followed on load");

        // Verify DAW tempo adoption on load, tempo change, and preset load
        {
            Replicant303AudioProcessor dawProcessor;
            MockPlayHead playHead;
            playHead.bpm = 142.0;
            playHead.playing = false;
            dawProcessor.setPlayHead (&playHead);
            dawProcessor.prepareToPlay (48000.0, 512);

            check (std::abs (dawProcessor.getSequencer().getEffectiveTempo() - 142.0) < 0.1,
                   "prepareToPlay adopts the DAW tempo on plugin load");
            check (std::abs (dawProcessor.apvts.getRawParameterValue ("tempo")->load() - 142.0f) < 0.2f,
                   "tempo parameter matches DAW BPM on plugin load");

            playHead.bpm = 165.0;
            juce::AudioBuffer<float> block (2, 512);
            juce::MidiBuffer midi;
            dawProcessor.processBlock (block, midi);

            check (std::abs (dawProcessor.getSequencer().getEffectiveTempo() - 165.0) < 0.1,
                   "sequencer follows DAW tempo changes in processBlock");
            check (std::abs (dawProcessor.apvts.getRawParameterValue ("tempo")->load() - 165.0f) < 0.2f,
                   "tempo parameter updates when DAW BPM changes");

            dawProcessor.loadPreset (3, 10);
            check (std::abs (dawProcessor.getSequencer().getEffectiveTempo() - 165.0) < 0.1,
                   "loading a preset while synced to DAW preserves DAW tempo");
            check (std::abs (dawProcessor.apvts.getRawParameterValue ("tempo")->load() - 165.0f) < 0.2f,
                   "tempo parameter stays locked to DAW tempo across preset loads");

            dawProcessor.setPlayHead (nullptr);
        }

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

        //----------------------------------------------------------------------
        // Preset vault integration
        //----------------------------------------------------------------------
        auto& presetBank = processor.getPresetBank();
        const auto expected = presetBank.getPreset (1, 5);

        processor.loadPreset (1, 5);
        check (processor.getSequencer().getCurrentPattern() == 1,
               "loading a preset selects its letter");
        check (processor.getSequencer().getPattern (1) == expected.pattern,
               "loading a preset installs its pattern");

        const auto loadedCutoff = processor.apvts.getRawParameterValue ("cutoff")->load();
        const auto loadedTempo = processor.apvts.getRawParameterValue ("tempo")->load();
        check (std::abs (loadedCutoff - expected.cutoff) < 0.6f,
               "loading a preset restores the filter cutoff");
        check (std::abs (loadedTempo - expected.tempo) < 0.6f,
               "loading a preset restores the tempo");
        check (std::abs (processor.getSequencer().getSwing() - expected.swing) < 1.0e-4f,
               "loading a preset restores swing");

        // STORE captures the live edit into the selected slot.
        if (auto* cutoff = processor.apvts.getParameter ("cutoff"))
            cutoff->setValueNotifyingHost (cutoff->convertTo0to1 (2222.0f));

        processor.storeCurrentPreset (7, 12, "Live Circuit");
        check (presetBank.getPreset (7, 12).name == "Live Circuit",
               "STORE names the user preset");
        check (presetBank.isOverridden (7, 12), "STORE marks the slot as an override");
        check (std::abs (presetBank.getPreset (7, 12).cutoff - 2222.0f) < 0.6f,
               "STORE captures the live cutoff");

        // EXPORT -> IMPORT round-trip through a real .midi file.
        auto presetFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getNonexistentChildFile ("r303_processor_preset", ".midi", false);

        processor.loadPreset (2, 9);
        const auto exportedPattern = processor.getSequencer().getPattern (2);

        if (auto* cutoff = processor.apvts.getParameter ("cutoff"))
            cutoff->setValueNotifyingHost (cutoff->convertTo0to1 (1800.0f));

        check (processor.exportPresetToMidi (presetFile, 2, 9),
               "EXPORT writes a .midi preset");

        processor.loadPreset (0, 0);
        check (processor.importPresetFromMidi (presetFile, 4, 17),
               "IMPORT reads a .midi preset");
        check (processor.getSequencer().getCurrentPattern() == 4,
               "IMPORT selects the destination letter");
        check (processor.getSequencer().getPattern (4) == exportedPattern,
               "IMPORT restores the exported pattern");
        check (std::abs (presetBank.getPreset (4, 17).cutoff - 1800.0f) < 0.7f,
               "IMPORT restores the exported voice");
        presetFile.deleteFile();

        // GEN generates, stores and loads a new variation.
        const auto beforeGenerate = presetBank.getPreset (6, 33);
        processor.generatePreset (6, 33, 0xabcdef01u);
        check (presetBank.isOverridden (6, 33), "GEN stores the generated slot");
        check (processor.getSequencer().getCurrentPattern() == 6,
               "GEN selects the destination letter");
        check (processor.getSequencer().getPattern (6) == presetBank.getPreset (6, 33).pattern,
               "GEN loads the generated pattern");
        check (presetBank.getPreset (6, 33) != beforeGenerate,
               "GEN produces a new variation");

        // Overrides travel in the plugin state, factory slots do not.
        juce::MemoryBlock vaultState;
        processor.getStateInformation (vaultState);

        Replicant303AudioProcessor vaultRestored;
        vaultRestored.setStateInformation (vaultState.getData(), (int) vaultState.getSize());

        check (vaultRestored.getPresetBank().getPreset (7, 12).name == "Live Circuit",
               "stored presets survive the plugin state round-trip");
        check (vaultRestored.getPresetBank().getSelectedSlot (1) == 5,
               "the selected preset slot survives the plugin state round-trip");
        check (vaultRestored.getPresetBank().getSelectedSlot (6) == 33,
               "the generated slot stays selected after a state reload");
        check (vaultRestored.getPresetBank().getPreset (4, 17)
                   == presetBank.getPreset (4, 17),
               "imported presets survive the plugin state round-trip");
        check (vaultRestored.getPresetBank().getPreset (6, 33)
                   == presetBank.getPreset (6, 33),
               "generated presets survive the plugin state round-trip");
        check (! vaultRestored.getPresetBank().isOverridden (0, 1),
               "factory slots are regenerated instead of serialised");
    }
}


//==============================================================================
// 60-shape VCO bank
//==============================================================================
namespace
{
    void testWaveformBank()
    {
        section ("Waveform bank");

        using namespace Replicant303;

        check (numWaveforms == 60, "the VCO bank exposes 60 shapes");
        check (juce::String (getWaveformName (0)) == "Sawtooth"
                   && juce::String (getWaveformName (1)) == "Square",
               "saw and square keep their original preset indices");

        juce::StringArray names;
        for (int i = 0; i < numWaveforms; ++i)
        {
            names.add (getWaveformName (i));
            check (std::isfinite (evaluateWaveformShape (i, 0.37)),
                   juce::String ("shape ") + juce::String (i) + " evaluates");
        }

        auto unique = names;
        unique.removeDuplicates (false);
        check (unique.size() == names.size(), "every shape has a unique name");

        // Render one second of every shape at 220 Hz / 48 kHz.
        Oscillator303 osc;
        osc.prepare (48000.0);

        bool finite = true;
        bool bounded = true;
        bool audible = true;
        float sawPeak = 0.0f;
        std::vector<float> sawSamples, sineSamples;

        for (int i = 0; i < numWaveforms; ++i)
        {
            osc.setWaveform (static_cast<Waveform> (i));
            osc.setFrequency (220.0f);
            osc.reset();

            double sumSquares = 0.0;
            float peak = 0.0f;

            for (int sample = 0; sample < 24000; ++sample)
            {
                const float v = osc.process();
                finite = finite && std::isfinite (v);
                peak = juce::jmax (peak, std::abs (v));
                sumSquares += (double) v * v;

                if (i == 0 && sample < 4096) sawSamples.push_back (v);
                if (i == 3 && sample < 4096) sineSamples.push_back (v);
            }

            bounded = bounded && peak <= 1.0001f;
            audible = audible && std::sqrt (sumSquares / 24000.0) > 0.10;

            if (i == 0)
                sawPeak = peak;
        }

        check (finite, "all 60 shapes render finite samples");
        check (bounded, "all 60 shapes stay inside [-1, 1]");
        check (audible, "all 60 shapes produce audible energy");
        check (sawPeak > 0.5f, "the saw retains its original level");

        bool shapesDiffer = false;
        for (size_t i = 0; i < sawSamples.size(); ++i)
            shapesDiffer = shapesDiffer || std::abs (sawSamples[i] - sineSamples[i]) > 0.05f;
        check (shapesDiffer, "selecting another shape changes the rendered signal");

        // Anti-aliasing: low notes use the full-band table, high notes fall
        // back to a heavily band-limited mip.
        osc.setWaveform (Waveform::Sawtooth);
        osc.setFrequency (20.0f);
        check (osc.getMipLevel() == 0, "a 20 Hz note plays the full-band table");
        osc.setFrequency (5000.0f);
        check (osc.getMipLevel() > 3, "a 5 kHz note drops to a band-limited mip");

        // Fundamental frequency stays put through the wavetable engine.
        osc.setFrequency (220.0f);
        osc.reset();

        int positiveCrossings = 0;
        float previous = 0.0f;

        for (int sample = 0; sample < 48000; ++sample)
        {
            const float v = osc.process();

            if (sample > 0 && previous < 0.0f && v >= 0.0f)
                ++positiveCrossings;

            previous = v;
        }

        check (std::abs (positiveCrossings - 220) <= 3,
               "the wavetable oscillator tracks the note (220 Hz)");

        // The tables themselves must be band-limited: the full-band saw has
        // 1/n harmonics, while the highest mip keeps a single harmonic.
        {
            const auto& tables = WavetableBank::getInstance();

            auto harmonicAmplitude = [] (const float* table, int harmonic)
            {
                const int N = WavetableBank::tableSize;
                double re = 0.0, im = 0.0;

                for (int n = 0; n < N; ++n)
                {
                    const double angle = 6.283185307179586
                                       * (double) harmonic * (double) n / (double) N;
                    re += (double) table[n] * std::cos (angle);
                    im += (double) table[n] * std::sin (angle);
                }

                return std::sqrt (re * re + im * im) * 2.0 / (double) N;
            };

            const float* sawTable = tables.table ((int) Waveform::Sawtooth, 0);
            const float* lastMip = tables.table ((int) Waveform::Sawtooth,
                                                 WavetableBank::numMips - 1);

            const double h1 = harmonicAmplitude (sawTable, 1);
            const double h2 = harmonicAmplitude (sawTable, 2);
            check (std::abs (h2 / h1 - 0.5) < 0.05,
                   "the saw table carries a 1/n harmonic series");
            check (harmonicAmplitude (lastMip, 2) < 0.02 * harmonicAmplitude (lastMip, 1),
                   "the highest mip keeps a single harmonic (no aliasing)");
        }

        // The parameter exposing the bank must list every shape.
        Replicant303AudioProcessor processor;
        auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
            processor.apvts.getParameter ("waveform"));

        check (choice != nullptr, "waveform is a choice parameter");
        check (choice != nullptr && choice->choices.size() == numWaveforms,
               "the waveform parameter exposes all 60 shapes");
        check (choice != nullptr && choice->choices[0] == "Sawtooth"
                   && choice->choices[1] == "Square",
               "the waveform parameter keeps the preset-compatible order");

        if (choice != nullptr)
        {
            choice->setValueNotifyingHost (choice->convertTo0to1 (27.0f));
            check (processor.captureCurrentPreset ("Scope").waveform == 27,
                   "a selected shape is captured by the preset system");
        }

        Preset wild;
        wild.waveform = 999;
        wild.sanitise();
        check (wild.waveform == numWaveforms - 1,
               "out-of-range preset shapes clamp to the bank size");

        // The scope's gesture wiring: a click toggles, a double-click opens.
        {
            WaveformSelector selector;
            bool singleClicked = false;
            bool doubleClicked = false;

            selector.onClicked = [&singleClicked] { singleClicked = true; };
            selector.onMenuRequested = [&doubleClicked] { doubleClicked = true; };

            auto makeEvent = [&selector] (int clicks)
            {
                return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(),
                                         juce::Point<float> (10.0f, 10.0f),
                                         juce::ModifierKeys::leftButtonModifier,
                                         1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                         &selector, &selector,
                                         juce::Time::getCurrentTime(),
                                         juce::Point<float> (10.0f, 10.0f),
                                         juce::Time::getCurrentTime(),
                                         clicks, false);
            };

            const auto single = makeEvent (1);
            selector.mouseDown (single);

            const auto doubleClick = makeEvent (2);
            selector.mouseDoubleClick (doubleClick);

            check (singleClicked, "a click on the scope requests the waveform menu");
            check (doubleClicked, "a double-click on the scope opens the waveform bank");
        }
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
    testPresetVault();
    testSynthEngine();
    testEngineEdges();
    testProcessor();
    testWaveformBank();
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
