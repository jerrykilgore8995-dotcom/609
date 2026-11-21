#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    juce::ScopedJuceInitialiser_GUI environment;
    const std::unique_ptr<juce::AudioProcessor> processor(createPluginFilter());
    auto layout = processor->getBusesLayout();
    layout.inputBuses.set(0, juce::AudioChannelSet::mono());
    layout.outputBuses.set(0, juce::AudioChannelSet::mono());
    if (!processor->setBusesLayout(layout)) return 3;
    const std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());
    if (!editor) return 4;
    editor->setVisible(true);
    const auto image = editor->createComponentSnapshot(editor->getLocalBounds());
    const juce::File output(juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]));
    const auto stream = output.createOutputStream();
    if (image.isNull() || !stream || !stream->setPosition(0) || stream->truncate().failed()) return 5;
    return juce::PNGImageFormat().writeImageToStream(image, *stream) ? 0 : 6;
}
