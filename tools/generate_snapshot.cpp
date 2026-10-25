#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

// Internal tool: renders the plugin editor off-screen to a PNG so the panel can
// be reviewed without opening a host. Used for the README preview.
//
//   generate_snapshot [output.png] [themeIndex] [drawerOpen] [presetIndex]
//                     [wavetableHolo]
//
// The editor is created with whatever skin is stored in the processor, and when
// themeIndex is supplied the skin is then switched *live* through the public
// ui_theme parameter before rendering - the same path the combo box takes - so
// the resulting image also proves that runtime skin switching works.
//
// presetIndex (0..511) loads a vault slot before rendering, so the snapshot
// shows the preset row and the name window exactly as a user would see them.
//
// wavetableHolo (0/1) expands the cyberpunk 3D wavetable hologram. The offscreen
// renderer always uses the scope's software path, which draws the same scene the
// GPU renders in a host, so the screenshot is deterministic on every machine.

int main(int argc, char* argv[])
{
    const juce::File outputFile = (argc > 1)
        ? juce::File::getCurrentWorkingDirectory().getChildFile(argv[1])
        : juce::File::getCurrentWorkingDirectory().getChildFile("replicant303_ui.png");

    juce::ScopedJuceInitialiser_GUI gui;

    // Snapshots are rendered offscreen, where there is no peer to attach an
    // OpenGL context to: pin the scope to its software renderer.
    Replicant303::CyberWavetable3D::setSoftwareRenderingForced (true);

    Replicant303AudioProcessor processor;

    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    if (editor == nullptr)
        return 1;

    editor->setSize(1080, 590);
    editor->setVisible(true);

    if (argc > 2)
    {
        const int requestedIndex = juce::String(argv[2]).getIntValue();

        if (auto* themeParam = processor.apvts.getParameter("ui_theme"))
        {
            const int currentIndex = Replicant303::ThemeManager::getInstance().getCurrentIndex();
            if (requestedIndex != currentIndex)
            {
                std::cout << "[INFO] Switching skin live: " << currentIndex
                          << " -> " << requestedIndex << std::endl;

                themeParam->setValueNotifyingHost(
                    Replicant303::ThemeManager::indexToNormalized(requestedIndex));

                // The ThemeManager broadcasts asynchronously, so let the change
                // message and its repaint reach the editor.
                juce::MessageManager::getInstance()->runDispatchLoopUntil(200);
            }

            const int activeIndex = Replicant303::ThemeManager::getInstance().getCurrentIndex();
            if (activeIndex != requestedIndex)
            {
                std::cerr << "[ERROR] Skin switch failed: expected index " << requestedIndex
                          << " but the ThemeManager reports " << activeIndex << std::endl;
                return 2;
            }
        }
    }

    // Optionally open the pattern drawer (third argument) so the sequencer can
    // be reviewed in the README without opening a host.
    if (argc > 3)
    {
        auto* concrete = dynamic_cast<Replicant303AudioProcessorEditor*> (editor.get());
        if (concrete == nullptr)
        {
            std::cerr << "[ERROR] Editor is not a Replicant303AudioProcessorEditor" << std::endl;
            return 3;
        }

        const int mode = juce::String (argv[3]).getIntValue();

        if (mode == 2)
        {
            // Half-way through the slide animation, to review the reveal.
            concrete->setPatternDrawerOpen (true, true);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (70);
            std::cout << "[INFO] Pattern drawer: mid-animation, height "
                      << editor->getHeight() << std::endl;
        }
        else
        {
            const bool open = mode != 0;
            concrete->setPatternDrawerOpen (open, false);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

            std::cout << "[INFO] Pattern drawer: " << (open ? "open" : "closed") << std::endl;
        }

    }

    // Optionally load a vault slot (fourth argument), so screenshots show the
    // preset row and name window with real content.
    if (argc > 4)
    {
        const int presetIndex = juce::String (argv[4]).getIntValue();
        const int bank = juce::jlimit (0, Replicant303::PresetBank::numBanks - 1,
                                       presetIndex / Replicant303::PresetBank::presetsPerBank);
        const int slot = juce::jlimit (0, Replicant303::PresetBank::presetsPerBank - 1,
                                       presetIndex % Replicant303::PresetBank::presetsPerBank);

        processor.loadPreset (bank, slot);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (250);

        std::cout << "[INFO] Preset: "
                  << Replicant303::PresetBank::getBankLetter (bank).toRawUTF8()
                  << " " << (slot + 1) << " "
                  << processor.getPresetBank().getPreset (bank, slot).name.toRawUTF8()
                  << std::endl;
    }

    // Optionally expand the 3D wavetable hologram (fifth argument).
    auto* concreteEditor = dynamic_cast<Replicant303AudioProcessorEditor*> (editor.get());

    if (concreteEditor != nullptr && argc > 5 && juce::String (argv[5]).getIntValue() != 0)
    {
        concreteEditor->setWavetableHoloOpen (true, false);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
        std::cout << "[INFO] Wavetable hologram: "
                  << (concreteEditor->isWavetableHoloOpen() ? "open" : "closed")
                  << " (" << (concreteEditor->getWavetableScope().isUsingGPU() ? "GPU" : "software")
                  << " renderer)" << std::endl;
    }

    // Feed the scope with a held note so the screenshot shows the live VCO
    // instead of the idle hologram. Nothing is audible: the buffers are local.
    processor.prepareToPlay (48000.0, 512);
    processor.getKeyboardState().noteOn (1, 40, 0.9f);

    for (int block = 0; block < 30; ++block)
    {
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);
    }

    juce::Image img(juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
    juce::Graphics g(img);
    editor->paintEntireComponent(g, true);

    outputFile.getParentDirectory().createDirectory();
    outputFile.deleteFile();

    juce::FileOutputStream stream(outputFile);
    if (stream.openedOk())
    {
        juce::PNGImageFormat png;
        png.writeImageToStream(img, stream);
        std::cout << "[SUCCESS] Saved snapshot to: " << outputFile.getFullPathName().toRawUTF8() << std::endl;
        std::cout << "[INFO] Active skin: " << Replicant303::ThemeManager::getThemeName(
                         Replicant303::ThemeManager::getInstance().getCurrentIndex()).toRawUTF8() << std::endl;
        return 0;
    }

    std::cerr << "[ERROR] Could not open output stream for: " << outputFile.getFullPathName().toRawUTF8() << std::endl;
    return 1;
}
