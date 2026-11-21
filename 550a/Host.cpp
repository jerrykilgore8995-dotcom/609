#include "Stimulus.hpp"
#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <numbers>
#include <vector>

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    const auto path = juce::SystemStats::getEnvironmentVariable("API550A_VST3_PATH", {});
    const bool snapshot = argc == 3 && juce::String(argv[1]) == "--snapshot";
    if (path.isEmpty() || (!snapshot && argc != 16)) return 2;
    juce::AudioPluginFormatManager formats;
    auto registered = std::make_unique<juce::VST3PluginFormat>();
    auto* format = registered.get();
    formats.addFormat(registered.release());
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format->findAllTypesForFile(descriptions, path);
    if (descriptions.size() != 1) return 3;
    const auto integer = [&argv](int i) { return std::atoi(argv[i]); };
    const int rate = snapshot ? 44100 : integer(1);
    const int channels = snapshot ? 1 : integer(3);
    constexpr int block = 512;
    juce::String error;
    auto plugin = formats.createPluginInstance(*descriptions[0], rate, block, error);
    if (!plugin) {
        std::fprintf(stderr, "%s\n", error.toRawUTF8());
        return 4;
    }
    juce::AudioProcessor::BusesLayout layout;
    const auto set = channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
    layout.inputBuses.add(set);
    layout.outputBuses.add(set);
    if (!plugin->setBusesLayout(layout)) return 5;
    plugin->setNonRealtime(true);
    plugin->prepareToPlay(rate, block);
    if (snapshot) {
        auto editor = std::unique_ptr<juce::AudioProcessorEditor>(plugin->createEditorIfNeeded());
        if (!editor) return 6;
        editor->setVisible(true);
        auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 2);
        juce::FileOutputStream file(juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]));
        return file.openedOk() && juce::PNGImageFormat{}.writeImageToStream(image, file) ? 0 : 7;
    }
    constexpr std::array names{"LF Gain", "MF Gain", "HF Gain", "LF Frequency", "MF Frequency",
                               "HF Frequency", "LF Shelf", "HF Shelf", "Filter", "EQ In"};
    for (std::size_t i = 0; i < names.size(); ++i) {
        bool found = false;
        for (auto* parameter : plugin->getParameters()) {
            if (parameter->getName(100) == names[i]) {
                const auto setting = integer(6 + static_cast<int>(i));
                const float value = i < 3 ? static_cast<float>(setting + 5) / 10
                    : (i < 6 ? static_cast<float>(setting - 1) / 4 : static_cast<float>(setting));
                parameter->setValueNotifyingHost(value);
                found = true;
                break;
            }
        }
        if (!found) return 8;
    }
    const double frequency = std::atof(argv[4]);
    const double amplitude = std::atof(argv[5]);
    const int samples = integer(2);
    juce::AudioBuffer<float> audio(channels, block);
    juce::MidiBuffer midi;
    std::vector<float> result(static_cast<std::size_t>(samples));
    const auto sweep = frequency == 0 ? api550a::sweepStimulus(rate, amplitude) : std::vector<double>{};
    const auto start = std::chrono::steady_clock::now();
    for (int offset = 0; offset < samples; offset += block) {
        audio.clear();
        const int count = std::min(block, samples - offset);
        for (int i = 0; i < count; ++i) {
            const auto n = offset + i;
            const auto y = static_cast<float>(frequency == 0 ? sweep[static_cast<std::size_t>(n % api550a::sweepPeriod)]
                : amplitude * std::sin(2 * std::numbers::pi * frequency * static_cast<double>(n) / rate));
            audio.setSample(0, i, y);
            if (channels == 2) audio.setSample(1, i, frequency == 0 ? -y : y);
        }
        plugin->processBlock(audio, midi);
        for (int i = 0; i < count; ++i) {
            const auto y = audio.getSample(0, i);
            const auto sample = channels == 2 && frequency == 0 ? (y - audio.getSample(1, i))*.5f : y;
            if (!std::isfinite(sample)) return 9;
            result[static_cast<std::size_t>(offset + i)] = sample;
        }
    }
    const auto ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    for (auto sample : result) std::printf("%.12g\n", static_cast<double>(sample));
    std::fprintf(stderr, "0 %d %.5f\n", plugin->getLatencySamples(), ms);
    plugin->releaseResources();
    return 0;
}
