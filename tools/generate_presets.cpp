//==============================================================================
// Replicant303 preset generator.
//
//   generate_presets [output-directory] [--all]
//
// With no arguments it writes the eight factory presets that predate the
// vault into ./presets/<A-H>/01_<name>.midi, which are the examples checked
// into the repository. --all writes the complete 8 x 64 factory bank.
//
// The generator shares PresetBank::generatePreset with the plugin's GEN
// button, so the files on disk can never drift from the in-plugin bank.
//==============================================================================

#include "Presets/PresetBank.h"
#include "Presets/PresetIO.h"

#include <iostream>

namespace
{
    juce::String sanitiseFileName (const juce::String& presetName)
    {
        auto safe = presetName.replaceCharacter (' ', '_');
        safe = safe.retainCharacters ("abcdefghijklmnopqrstuvwxyz"
                                      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                      "0123456789_-");

        return safe.isEmpty() ? juce::String ("Preset") : safe;
    }

    juce::String makeFileName (const Replicant303::Preset& preset, int slot)
    {
        return juce::String (slot + 1).paddedLeft ('0', 2)
             + "_" + sanitiseFileName (preset.name) + ".midi";
    }

    void printUsage()
    {
        std::cout << "Usage: generate_presets [output-directory] [--all]\n"
                  << "\n"
                  << "  (no flags)  write the 8 factory examples into <dir>/A..H\n"
                  << "  --all       write all 8 x 64 factory presets\n"
                  << "  --help      show this help\n";
    }
}

int main (int argc, char* argv[])
{
    bool writeAll = false;
    juce::File output = juce::File::getCurrentWorkingDirectory().getChildFile ("presets");

    for (int i = 1; i < argc; ++i)
    {
        const juce::String argument (argv[i]);

        if (argument == "--all")
            writeAll = true;
        else if (argument == "--help" || argument == "-h")
        {
            printUsage();
            return 0;
        }
        else if (argument.startsWith ("-"))
        {
            std::cerr << "Unknown option: " << argument << std::endl;
            printUsage();
            return 1;
        }
        else
        {
            output = juce::File::getCurrentWorkingDirectory().getChildFile (argument);
        }
    }

    int written = 0;

    for (int bank = 0; bank < Replicant303::PresetBank::numBanks; ++bank)
    {
        const auto bankLetter = Replicant303::PresetBank::getBankLetter (bank);
        auto bankDirectory = output.getChildFile (bankLetter);

        if (! bankDirectory.createDirectory())
        {
            std::cerr << "Cannot create directory: " << bankDirectory.getFullPathName() << std::endl;
            return 1;
        }

        const int lastSlot = writeAll ? Replicant303::PresetBank::presetsPerBank - 1 : 0;

        for (int slot = 0; slot <= lastSlot; ++slot)
        {
            const auto preset = Replicant303::PresetBank::makeFactoryPreset (bank, slot);
            const auto file = bankDirectory.getChildFile (makeFileName (preset, slot));

            if (! Replicant303::PresetIO::writeMidiFile (preset, file))
            {
                std::cerr << "Cannot write preset: " << file.getFullPathName() << std::endl;
                return 1;
            }

            ++written;
        }
    }

    std::cout << "Wrote " << written << " preset(s) to "
              << output.getFullPathName() << std::endl;

    return 0;
}
