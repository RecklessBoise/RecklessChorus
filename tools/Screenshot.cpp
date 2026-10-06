// Renders the editor offscreen to a PNG (used for the README screenshot).
// Usage: RecklessScreenshot <output.png> [scale] [preset index] [save]

#include "PluginEditor.h"
#include "PluginProcessor.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;

    if (argc < 2)
    {
        std::cerr << "usage: RecklessScreenshot <output.png> [scale] [preset]\n";
        return 1;
    }

    const auto scale = argc > 2 ? juce::String (argv[2]).getFloatValue() : 1.0f;
    const auto preset = argc > 3 ? juce::String (argv[3]).getIntValue() : 0;

    RecklessChorusProcessor processor;
    processor.setCurrentProgram (preset);
    processor.setEditorWidth (juce::roundToInt ((float) RecklessChorusProcessor::kDefaultEditorWidth * scale));
    processor.prepareToPlay (48000.0, 512);

    // Run a little audio so the LFO has a phase to show.
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    for (int i = 0; i < 20; ++i)
        processor.processBlock (buffer, midi);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    if (auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0)))
    {
        for (int i = 0; i < 30; ++i)
            view->tick (1.0 / 60.0, true);

        // Optional 4th argument "save": show the save-preset dialog.
        if (argc > 4 && juce::String (argv[4]) == "save")
            if (auto* save = dynamic_cast<SaveButton*> (view->findChildWithID ("save")); save != nullptr && save->onClick)
                save->onClick();
    }

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    juce::File output (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    output.deleteFile();
    juce::FileOutputStream stream (output);
    juce::PNGImageFormat png;
    if (! stream.openedOk() || ! png.writeImageToStream (image, stream))
        return 2;

    std::cout << "wrote " << output.getFullPathName() << " (" << image.getWidth() << "x" << image.getHeight() << ")\n";
    return 0;
}
