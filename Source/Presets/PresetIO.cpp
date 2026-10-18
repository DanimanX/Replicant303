#include "PresetIO.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace Replicant303::PresetIO
{
    namespace
    {
        constexpr int kTicksPerQuarter = 960;
        constexpr int kStepTicks = kTicksPerQuarter / 4;

        int transposeIntoPatternRange (int note) noexcept
        {
            while (note < Pattern::lowestNote)  note += 12;
            while (note > Pattern::highestNote) note -= 12;
            return juce::jlimit (Pattern::lowestNote, Pattern::highestNote, note);
        }

        struct RawNote
        {
            double on = 0.0;
            double off = -1.0;
            int sourceNote = 36;
            int note = 36;
            int velocity = 80;
        };

        bool writeBytesAtomically (const juce::File& file, const juce::MidiFile& midiFile)
        {
            file.getParentDirectory().createDirectory();

            auto temp = file.getParentDirectory()
                            .getNonexistentChildFile (".r303_preset", ".mid", false);

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

    //==============================================================================
    juce::String toJson (const Preset& preset)
    {
        const auto nameJson = juce::JSON::toString (juce::var (preset.name), true);

        juce::String json;
        json << "{\"format\":\"replicant303-preset\",\"version\":1,\"name\":" << nameJson
             << ",\"waveform\":" << preset.waveform
             << ",\"tuning\":" << juce::String ((double) preset.tuning, 6)
             << ",\"cutoff\":" << juce::String ((double) preset.cutoff, 6)
             << ",\"resonance\":" << juce::String ((double) preset.resonance, 6)
             << ",\"envMod\":" << juce::String ((double) preset.envMod, 6)
             << ",\"decay\":" << juce::String ((double) preset.decay, 6)
             << ",\"accent\":" << juce::String ((double) preset.accent, 6)
             << ",\"tempo\":" << juce::String ((double) preset.tempo, 6)
             << ",\"swing\":" << juce::String ((double) preset.swing, 6)
             << ",\"distActive\":" << (preset.distActive ? "true" : "false")
             << ",\"distType\":" << preset.distType
             << ",\"distDrive\":" << juce::String ((double) preset.distDrive, 6)
             << ",\"distTone\":" << juce::String ((double) preset.distTone, 6)
             << ",\"distMix\":" << juce::String ((double) preset.distMix, 6)
             << ",\"pattern\":{\"length\":" << preset.pattern.length
             << ",\"gates\":[";

        for (int i = 0; i < Pattern::maxSteps; ++i)
            json << (preset.pattern[i].gate ? 1 : 0) << (i + 1 < Pattern::maxSteps ? "," : "");

        json << "],\"notes\":[";

        for (int i = 0; i < Pattern::maxSteps; ++i)
            json << preset.pattern[i].note << (i + 1 < Pattern::maxSteps ? "," : "");

        json << "],\"accents\":[";

        for (int i = 0; i < Pattern::maxSteps; ++i)
            json << (preset.pattern[i].accent ? 1 : 0) << (i + 1 < Pattern::maxSteps ? "," : "");

        json << "],\"slides\":[";

        for (int i = 0; i < Pattern::maxSteps; ++i)
            json << (preset.pattern[i].slide ? 1 : 0) << (i + 1 < Pattern::maxSteps ? "," : "");

        json << "]}}";
        return json;
    }

    bool fromJson (const juce::String& json, Preset& preset)
    {
        const auto parsed = juce::JSON::parse (json);
        auto* object = parsed.getDynamicObject();

        if (object == nullptr)
            return false;

        if (object->getProperty ("format").toString() != "replicant303-preset")
            return false;

        const auto patternValue = object->getProperty ("pattern");
        auto* patternObject = patternValue.getDynamicObject();

        if (patternObject == nullptr)
            return false;

        Preset result;
        result.name = object->getProperty ("name").toString();
        result.waveform = (int) object->getProperty ("waveform");
        result.tuning = (float) (double) object->getProperty ("tuning");
        result.cutoff = (float) (double) object->getProperty ("cutoff");
        result.resonance = (float) (double) object->getProperty ("resonance");
        result.envMod = (float) (double) object->getProperty ("envMod");
        result.decay = (float) (double) object->getProperty ("decay");
        result.accent = (float) (double) object->getProperty ("accent");
        result.tempo = (float) (double) object->getProperty ("tempo");
        result.swing = (float) (double) object->getProperty ("swing");
        result.distActive = (bool) object->getProperty ("distActive");
        result.distType = (int) object->getProperty ("distType");
        result.distDrive = (float) (double) object->getProperty ("distDrive");
        result.distTone = (float) (double) object->getProperty ("distTone");
        result.distMix = (float) (double) object->getProperty ("distMix");

        result.pattern.length = (int) patternObject->getProperty ("length");

        if (result.pattern.length <= 0)
            result.pattern.length = Pattern::maxSteps;

        for (auto& step : result.pattern.steps)
        {
            step.gate = true;
            step.note = 36;
            step.accent = false;
            step.slide = false;
        }

        auto readIntArray = [&patternObject] (const char* key, int* destination)
        {
            const auto value = patternObject->getProperty (key);

            if (auto* array = value.getArray())
            {
                const int count = juce::jmin (Pattern::maxSteps, array->size());

                for (int i = 0; i < count; ++i)
                    destination[i] = (int) (*array)[i];
            }
        };

        int notes[Pattern::maxSteps];
        int gates[Pattern::maxSteps];
        int accents[Pattern::maxSteps];
        int slides[Pattern::maxSteps];

        for (int i = 0; i < Pattern::maxSteps; ++i)
        {
            notes[i] = 36;
            gates[i] = 1;
            accents[i] = 0;
            slides[i] = 0;
        }

        readIntArray ("notes", notes);
        readIntArray ("gates", gates);
        readIntArray ("accents", accents);
        readIntArray ("slides", slides);

        for (int i = 0; i < Pattern::maxSteps; ++i)
        {
            auto& step = result.pattern[i];
            step.gate = gates[i] != 0;
            step.note = notes[i];
            step.accent = accents[i] != 0;
            step.slide = slides[i] != 0;
        }

        result.sanitise();
        preset = result;
        return true;
    }

    //==============================================================================
    bool writeMidiFile (const Preset& preset, const juce::File& file)
    {
        auto copy = preset;
        copy.sanitise();

        juce::MidiMessageSequence sequence;

        sequence.addEvent (juce::MidiMessage::textMetaEvent (3, copy.name), 0.0);
        sequence.addEvent (juce::MidiMessage::textMetaEvent (
                               1, juce::String (metadataPrefix) + toJson (copy)), 0.0);
        sequence.addEvent (juce::MidiMessage::tempoMetaEvent (
                               (int) std::llround (60000000.0
                                   / juce::jlimit (40.0f, 300.0f, copy.tempo))), 0.0);

        for (int i = 0; i < juce::jlimit (1, Pattern::maxSteps, copy.pattern.length); ++i)
        {
            const auto& step = copy.pattern[i];

            if (! step.gate)
                continue;

            const double onTick = i * kStepTicks;
            const double lengthTicks = step.slide ? kStepTicks : kStepTicks / 2;
            const int velocity = step.accent ? 110 : 80;

            sequence.addEvent (juce::MidiMessage::noteOn (1, step.note, (juce::uint8) velocity),
                               onTick);
            sequence.addEvent (juce::MidiMessage::noteOff (1, step.note),
                               onTick + lengthTicks);
        }

        sequence.updateMatchedPairs();

        juce::MidiFile midiFile;
        midiFile.setTicksPerQuarterNote (kTicksPerQuarter);
        midiFile.addTrack (sequence);

        return writeBytesAtomically (file, midiFile);
    }

    bool readMidiFile (const juce::File& file, Preset& preset)
    {
        juce::FileInputStream input (file);

        if (! input.openedOk())
            return false;

        juce::MidiFile midiFile;

        if (! midiFile.readFrom (input))
            return false;

        juce::String metadata;
        juce::String trackName;
        double tempoBpm = 125.0;
        std::vector<RawNote> notes;

        for (int trackIndex = 0; trackIndex < midiFile.getNumTracks(); ++trackIndex)
        {
            const auto* track = midiFile.getTrack (trackIndex);

            if (track == nullptr)
                continue;

            for (int i = 0; i < track->getNumEvents(); ++i)
            {
                const auto& message = track->getEventPointer (i)->message;

                if (message.isTextMetaEvent())
                {
                    const auto text = message.getTextFromTextMetaEvent();

                    if (text.startsWith (metadataPrefix))
                        metadata = text.substring ((int) std::strlen (metadataPrefix));
                    else if (message.isTrackNameEvent() && trackName.isEmpty())
                        trackName = text;
                }

                if (message.isTempoMetaEvent())
                {
                    const double secondsPerQuarter = message.getTempoSecondsPerQuarterNote();

                    if (secondsPerQuarter > 0.0)
                        tempoBpm = 60.0 / secondsPerQuarter;
                }

                if (message.isNoteOn (true))
                {
                    if (message.getVelocity() == 0)
                    {
                        // Some MIDI files spell note-off as note-on velocity 0.
                        for (auto it = notes.rbegin(); it != notes.rend(); ++it)
                        {
                            if (it->sourceNote == message.getNoteNumber() && it->off < 0.0)
                            {
                                it->off = message.getTimeStamp();
                                break;
                            }
                        }
                    }
                    else
                    {
                        RawNote note;
                        note.on = message.getTimeStamp();
                        note.sourceNote = message.getNoteNumber();
                        note.note = transposeIntoPatternRange (message.getNoteNumber());
                        note.velocity = message.getVelocity();
                        notes.push_back (note);
                    }
                }
                else if (message.isNoteOff())
                {
                    for (auto it = notes.rbegin(); it != notes.rend(); ++it)
                    {
                        if (it->sourceNote == message.getNoteNumber() && it->off < 0.0)
                        {
                            it->off = message.getTimeStamp();
                            break;
                        }
                    }
                }
            }
        }

        // A Replicant 303 preset round-trips exactly through its metadata.
        if (metadata.isNotEmpty())
        {
            Preset parsed;

            if (fromJson (metadata, parsed))
            {
                if (parsed.name.isEmpty())
                    parsed.name = trackName.isNotEmpty()
                                      ? trackName
                                      : file.getFileNameWithoutExtension();

                parsed.sanitise();
                preset = parsed;
                return true;
            }
        }

        // No metadata (or damaged metadata): treat it as a plain MIDI pattern.
        if (notes.empty())
            return false;

        const int timeFormat = midiFile.getTimeFormat();
        const double stepTicks = timeFormat > 0 ? timeFormat / 4.0 : kStepTicks;

        Preset result;
        result.name = trackName.isNotEmpty() ? trackName : file.getFileNameWithoutExtension();
        result.tempo = (float) juce::jlimit (40.0, 300.0, tempoBpm);
        result.pattern.length = Pattern::maxSteps;

        for (auto& step : result.pattern.steps)
        {
            step.gate = false;
            step.note = 36;
            step.accent = false;
            step.slide = false;
        }

        int lastStep = -1;

        for (const auto& note : notes)
        {
            const int stepIndex = (int) std::llround (note.on / stepTicks);

            if (stepIndex < 0 || stepIndex >= Pattern::maxSteps)
                continue;

            auto& step = result.pattern[stepIndex];
            step.gate = true;
            step.note = note.note;
            step.accent = note.velocity >= 100;
            step.slide = note.off >= 0.0
                         && (note.off - note.on) >= stepTicks * 0.75;
            lastStep = juce::jmax (lastStep, stepIndex);
        }

        if (lastStep < 0)
            return false;

        result.pattern.length = lastStep >= 12 ? Pattern::maxSteps
                                               : juce::jmax (1, lastStep + 1);
        result.sanitise();
        preset = result;
        return true;
    }
}
