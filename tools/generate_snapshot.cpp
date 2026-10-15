#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    Replicant303AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    if (editor != nullptr)
    {
        editor->setSize(1080, 590);
        editor->setVisible(true);

        juce::Image img(juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
        juce::Graphics g(img);
        editor->paintEntireComponent(g, true);

        juce::File outputFile("/Users/danimanx/code/Replicant303/assets/replicant303_ui.png");
        outputFile.getParentDirectory().createDirectory();
        outputFile.deleteFile();

        juce::FileOutputStream stream(outputFile);
        if (stream.openedOk())
        {
            juce::PNGImageFormat png;
            png.writeImageToStream(img, stream);
            std::cout << "[SUCCESS] Saved snapshot to: " << outputFile.getFullPathName().toRawUTF8() << std::endl;
        }
        else
        {
            std::cerr << "[ERROR] Could not open output stream for: " << outputFile.getFullPathName().toRawUTF8() << std::endl;
            return 1;
        }
    }
    return 0;
}
