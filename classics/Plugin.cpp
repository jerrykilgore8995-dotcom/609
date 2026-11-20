#include <juce_audio_processors/juce_audio_processors.h>
#include "../native/Pipeline.hpp"

namespace {
using mkii::native::Controls;
using juce::String;
constexpr bool original2254 = CLASSIC_MODEL == 2254;
constexpr auto product = original2254 ? "2254 Nodal" : "2264 Nodal";

struct Dial {
    const char* id;
    const char* title;
    juce::StringArray labels;
    int initial = 0;
};

const std::array<Dial,8> dials{{
    {"ct", "Compress Threshold",
        {"-20","-18","-16","-14","-12","-10","-8","-6","-4","-2","0","+2","+4","+6","+8","+10"},10},
    {"cr", "Compress Ratio", {"1.5:1","2:1","3:1","4:1","6:1"}},
    {"cm", "Compress Recovery", original2254 ? juce::StringArray{"200","400","800","Auto"}
                                            : juce::StringArray{"100","400","800","1500","A1","A2"}},
    {"cg", "Makeup", original2254 ? juce::StringArray{"0","2","4","6","8","10"}
                                     : juce::StringArray{"0","2","4","6","8","10","12","14","16","18","20"}},
    {"lt", "Limit Threshold", original2254 ?
        juce::StringArray{"+4","+5","+6","+7","+8","+9","+10","+11","+12"} :
        juce::StringArray{"+4","+4.5","+5","+5.5","+6","+6.5","+7","+7.5","+8","+8.5","+9","+9.5",
                          "+10","+10.5","+11","+11.5","+12","+12.5","+13","+13.5","+14","+14.5","+15"},
        original2254 ? 4 : 8},
    {"lr", "Limit Recovery", original2254 ? juce::StringArray{"100","200","800","Auto"}
                                          : juce::StringArray{"50","100","200","800","A1","A2"}},
    {"ce", "Compress In", {}},
    {"le", "Limit In", {}}
}};

String id(int channel, const char* key) {
    return String(channel+1) + "_" + key;
}

class Classic final : public juce::AudioProcessor {
public:
    Classic()
        : AudioProcessor(BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          state(*this, nullptr, "Settings", parameters()) {
        for (int ch=0; ch<2; ++ch) {
            for (std::size_t i=0; i<dials.size(); ++i)
                controls[ch][i]=state.getRawParameterValue(id(ch,dials[i].id));
            if (original2254)
                controls[ch][8]=state.getRawParameterValue(id(ch,"fast"));
        }
        in = state.getRawParameterValue("in");
        link = state.getRawParameterValue("link");
    }

    const String getName() const override { return product; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const String getProgramName(int) override { return {}; }
    void changeProgramName(int, const String&) override {}

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override {
        return new juce::GenericAudioProcessorEditor(*this);
    }

    bool isBusesLayoutSupported(const BusesLayout& layout) const override {
        auto input=layout.getMainInputChannelSet();
        return input==layout.getMainOutputChannelSet() &&
            (input==juce::AudioChannelSet::mono() || input==juce::AudioChannelSet::stereo());
    }

    void prepareToPlay(double rate, int frames) override {
        ready = engine.prepare(rate, getTotalNumInputChannels(), frames);
        setLatencySamples(ready ? mkii::native::playbackLatency(frames) : 0);
    }
    void releaseResources() override { engine.stop(); ready=false; }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override {
        process(buffer, false);
    }
    void processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override {
        process(buffer, true);
    }

    void getStateInformation(juce::MemoryBlock& data) override {
        copyXmlToBinary(*state.copyState().createXml(), data);
    }
    void setStateInformation(const void* data, int size) override {
        if (auto xml=getXmlFromBinary(data,size); xml && xml->hasTagName(state.state.getType()))
            state.replaceState(juce::ValueTree::fromXml(*xml));
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout parameters() {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        for (int ch=0; ch<2; ++ch) {
            for (const auto& dial:dials) {
                const juce::ParameterID key{id(ch,dial.id),1};
                const auto name = String(ch==0 ? "Left " : "Right ")+dial.title;
                if (dial.labels.isEmpty())
                    layout.add(std::make_unique<juce::AudioParameterBool>(key,name,false));
                else
                    layout.add(std::make_unique<juce::AudioParameterChoice>(key,name,dial.labels,dial.initial));
            }
            if (original2254)
                layout.add(std::make_unique<juce::AudioParameterBool>(
                    juce::ParameterID{id(ch,"fast"),1},ch==0?"Left Limiter Fast":"Right Limiter Fast",false));
        }
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"in",1},"Circuit In",true));
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"link",1},"Stereo Link",false));
        return layout;
    }

    void process(juce::AudioBuffer<float>& buffer, bool bypassed) {
        if (!ready) return;
        juce::ScopedNoDenormals guard;
        Controls settings = mkii::native::defaults;
        for (int ch=0; ch<2; ++ch) {
            const auto offset=ch*9;
            for (std::size_t i=0; i<dials.size(); ++i)
                settings[offset+i]=static_cast<int>(controls[ch][i]->load())+(i<6 ? 1 : 0);
            if (original2254) {
                // The original threshold is a continuous trim. Sample it
                // at the centre and every second half-dB circuit detent.
                settings[offset+4]=2*settings[offset+4]-1;
                settings[offset+8]=static_cast<int>(controls[ch][8]->load());
            }
        }
        settings[18]=in->load()>0.5f;
        settings[19]=link->load()>0.5f && buffer.getNumChannels()==2;
        engine.process(buffer.getArrayOfWritePointers(),buffer.getNumSamples(),
                       settings,isNonRealtime(),bypassed || !settings[18]);
    }

    juce::AudioProcessorValueTreeState state;
    mkii::native::Pipeline engine;
    std::array<std::array<const std::atomic<float>*,9>,2> controls{};
    const std::atomic<float>* in=nullptr;
    const std::atomic<float>* link=nullptr;
    bool ready=false;
};
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new Classic;
}
