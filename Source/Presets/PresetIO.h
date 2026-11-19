#pragma once

#include "PresetBank.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace Replicant303::PresetIO
{
    /** Marker embedded in a text meta-event. A .midi preset is a normal
        Standard MIDI File (notes, tempo, track name) plus this metadata, so it
        plays in any DAW and still round-trips every Replicant 303 parameter. */
    inline constexpr const char* metadataPrefix = "R303PRESET/1:";

    /** Upper bound accepted by readMidiFile. A preset is a 16-step pattern and
        one metadata event; this leaves room for a generous plain MIDI pattern
        while refusing anything large enough to stall the importer. */
    inline constexpr int maxPresetFileBytes = 4 * 1024 * 1024;

    juce::String toJson (const Preset& preset);
    bool fromJson (const juce::String& json, Preset& preset);

    /** Writes a preset as a .midi file. Returns false if the file cannot be
        written; an existing file is only replaced after a successful write. */
    bool writeMidiFile (const Preset& preset, const juce::File& file);

    /** Reads a .midi preset. Files without Replicant 303 metadata are accepted
        as plain MIDI patterns: notes map to the 16-step grid, velocity >= 100
        becomes an accent and full-length notes become slides. Files larger than
        maxPresetFileBytes are refused without being parsed. */
    bool readMidiFile (const juce::File& file, Preset& preset);
}
