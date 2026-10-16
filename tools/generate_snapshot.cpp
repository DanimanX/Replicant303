#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

// Internal tool: renders the plugin editor off-screen to a PNG so the panel can
// be reviewed without opening a host. Used for the README preview.
//
//   generate_snapshot [output.png] [themeIndex]
//
// The editor is created with whatever skin is stored in the processor, and when
// themeIndex is supplied the skin is then switched *live* through the public
// ui_theme parameter before rendering - the same path the combo box takes - so
// the resulting image also proves that runtime skin switching works.

int main(int argc, char* argv[])
{
    const juce::File outputFile = (argc > 1)
        ? juce::File::getCurrentWorkingDirectory().getChildFile(argv[1])
        : juce::File::getCurrentWorkingDirectory().getChildFile("replicant303_ui.png");

    juce::ScopedJuceInitialiser_GUI gui;
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
