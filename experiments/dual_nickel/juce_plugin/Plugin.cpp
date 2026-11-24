#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../AnalogOptoProcessor.hpp"
#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

namespace {
using Layout=juce::AudioProcessorValueTreeState::ParameterLayout;
using SliderAttachment=juce::AudioProcessorValueTreeState::SliderAttachment;
using ButtonAttachment=juce::AudioProcessorValueTreeState::ButtonAttachment;
using ComboAttachment=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
Layout makeParameters(){
    Layout p;
    const auto add=[&](const char* id,const char* title,float low,float high,
                       float initial,float interval=0.01f) {
        p.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{id,1},title,
            juce::NormalisableRange<float>(low,high,interval),initial));
    };
    add("drive","Input Drive", -18,18,0,.1f);
    add("threshold","Opto Threshold",-45,18,-18,.1f);
    add("ratio","Opto Ratio",1.5f,8,3,.1f);
    add("attack","Opto Attack (ms)",10,100,15,.1f);
    add("release","Opto Release (ms)",50,2000,250,1);
    add("makeup","Makeup Gain",-12,18,0,.1f);
    add("output","Output Trim",-24,24,0,.1f);
    add("hpfhz","Sidechain HPF (Hz)",40,300,120,1);
    add("pmcenter","Low-Mid Center (Hz)",80,400,170,1);
    add("pmwidth","Low-Mid Width (oct)",.5f,2.5f,1.3f,.01f);
    add("pmrange","Low-Mid Max Cut (dB)",0,12,4,.1f);
    add("pmthreshold","Low-Mid Threshold",-50,12,-25,.1f);
    add("pmattack","Low-Mid Attack (ms)",1,80,10,.1f);
    add("pmrelease","Low-Mid Release (ms)",20,1000,150,1);
    p.add(std::make_unique<juce::AudioParameterBool>(
          juce::ParameterID{"hpf",1},"Sidechain HPF",false));
    p.add(std::make_unique<juce::AudioParameterBool>(
          juce::ParameterID{"palm",1},"Dynamic Low-Mid",false));
    p.add(std::make_unique<juce::AudioParameterBool>(
          juce::ParameterID{"link",1},"Linked Input/Output",true));
    p.add(std::make_unique<juce::AudioParameterBool>(
          juce::ParameterID{"ff",1},"Feedforward Detector",false));
    p.add(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{"cell",1},"Optical Cell",
          juce::StringArray{"NSL-32","VTL5C2","T4B-style"},0));
    p.add(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{"load",1},"Output Load",
          juce::StringArray{"600 ohm","10k ohm"},1));
    return p;
}
}

class NickelOptoProcessor final : public juce::AudioProcessor {
public:
    NickelOptoProcessor():
        AudioProcessor(BusesProperties()
            .withInput("Input",juce::AudioChannelSet::stereo(),true)
            .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
        state(*this,nullptr,"NickelOptoState",makeParameters()) {}
    juce::AudioProcessorValueTreeState state;
    std::array<std::atomic<float>,2> gr{{0.f,0.f}};
    std::array<std::atomic<float>,2> lowGr{{0.f,0.f}};
    juce::String const getName() const override {return "Nickel Opto";}
    bool acceptsMidi() const override {return false;}
    bool producesMidi() const override {return false;}
    bool isMidiEffect() const override {return false;}
    double getTailLengthSeconds() const override {return 3.;}
    int getNumPrograms() override {return 1;}
    int getCurrentProgram() override {return 0;}
    void setCurrentProgram(int) override {}
    juce::String const getProgramName(int) override {return {};}
    void changeProgramName(int,const juce::String&) override {}
    bool hasEditor() const override {return true;}
    juce::AudioProcessorEditor* createEditor() override;
    bool isBusesLayoutSupported(const BusesLayout& layout) const override {
        const auto x=layout.getMainInputChannelSet();
        return x==layout.getMainOutputChannelSet() &&
            (x==juce::AudioChannelSet::mono()||x==juce::AudioChannelSet::stereo());
    }
    bool isMono() const noexcept {return getTotalNumInputChannels()==1;}
    void prepareToPlay(double rate,int) override {
        mono.prepare(rate);stereo.prepare(rate);
    }
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer&) override {
        juce::ScopedNoDenormals noDenormals;
        const auto v=[&](const char* name)->double {
            const auto* atom=state.getRawParameterValue(name);
            return atom ? atom->load(std::memory_order_relaxed):0.0;
        };
        // Every GUI control affects the actual signal path, not a preset
        // abstraction or display-only parameter.
        const auto apply=[&](auto& dsp) {
            dsp.setInputDb(v("drive"));
            dsp.setMakeupDb(v("makeup"));
            dsp.setOutputTrimDb(v("output"));
            dsp.setLinkedAutoGain(v("link")>.5);
            dsp.setThresholdDb(v("threshold"));
            dsp.setRatio(v("ratio"));
            dsp.setFeedforward(v("ff")>.5);
            dsp.setOptoCell(int(v("cell")));
            dsp.setAttackMs(v("attack"));
            dsp.setReleaseMs(v("release"));
            dsp.setScHpf(v("hpf")>.5);
            dsp.setScHpfHz(v("hpfhz"));
            dsp.setPalmMute(v("palm")>.5);
            dsp.setPalmCenterHz(v("pmcenter"));
            dsp.setPalmBandwidthOct(v("pmwidth"));
            dsp.setPalmRangeDb(v("pmrange"));
            dsp.setPalmThresholdDb(v("pmthreshold"));
            dsp.setPalmAttackMs(v("pmattack"));
            dsp.setPalmReleaseMs(v("pmrelease"));
            dsp.setLoadOhms(v("load")<.5?600.:10000.);
        };
        const int n=audio.getNumSamples();
        if(isMono()) {
            apply(mono);
            float* l=audio.getWritePointer(0);
            for(int i=0;i<n;++i)l[i]=mono.process(l[i]);
            gr[0].store(float(mono.opticalGr()),std::memory_order_relaxed);
            lowGr[0].store(float(mono.lowMidGr()),std::memory_order_relaxed);
            gr[1].store(0.f,std::memory_order_relaxed);
            lowGr[1].store(0.f,std::memory_order_relaxed);
        } else if(audio.getNumChannels()>=2) {
            apply(stereo);
            auto* l=audio.getWritePointer(0);
            auto* r=audio.getWritePointer(1);
            for(int i=0;i<n;++i)stereo.process(l[i],r[i]);
            gr[0].store(float(stereo.grLeft()),std::memory_order_relaxed);
            gr[1].store(float(stereo.grRight()),std::memory_order_relaxed);
            lowGr[0].store(0.f,std::memory_order_relaxed);
            lowGr[1].store(0.f,std::memory_order_relaxed);
        }
    }
    void getStateInformation(juce::MemoryBlock& data) override {
        if(auto xml=state.copyState().createXml())copyXmlToBinary(*xml,data);
    }
    void setStateInformation(const void* data,int size) override {
        if(auto xml=getXmlFromBinary(data,size);
           xml && xml->hasTagName(state.state.getType()))
            state.replaceState(juce::ValueTree::fromXml(*xml));
    }
private:
    analogopto::MonoEngine mono;
    analogopto::StereoEngine stereo;
};

class HardwarePanel final : public juce::Component,private juce::Timer {
public:
    HardwarePanel(NickelOptoProcessor& processor,int channel):
        p(processor),channelIndex(channel) {
        const std::array<std::pair<const char*,const char*>,14> settings{{
          {"drive","INPUT"},{"threshold","THRESHOLD"},{"ratio","RATIO"},
          {"makeup","MAKEUP"},{"attack","ATTACK"},{"release","RELEASE"},
          {"hpfhz","S/C HPF"},{"output","OUTPUT"},{"pmcenter","LOW-MID FREQ"},
          {"pmwidth","BANDWIDTH"},{"pmrange","DYN CUT"},{"pmthreshold","BAND THR"},
          {"pmattack","BAND ATK"},{"pmrelease","BAND REL"}
        }};
        for(const auto& setting:settings) {
            Control c{};
            c.slider=std::make_unique<juce::Slider>();
            c.slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            c.slider->setTextBoxStyle(juce::Slider::TextBoxBelow,false,102,18);
            c.slider->setColour(juce::Slider::rotarySliderFillColourId,
                               juce::Colour(0xffd8ad65));
            c.slider->setColour(juce::Slider::thumbColourId,
                               juce::Colour(0xffeeeeeb));
            c.label=std::make_unique<juce::Label>();
            c.label->setText(setting.second,juce::dontSendNotification);
            c.label->setJustificationType(juce::Justification::centred);
            c.label->setColour(juce::Label::textColourId,
                               juce::Colour(0xffe5e4df));
            addAndMakeVisible(*c.slider);
            addAndMakeVisible(*c.label);
            c.attach=std::make_unique<SliderAttachment>(p.state,setting.first,
                                                         *c.slider);
            controls.push_back(std::move(c));
        }
        const std::array<std::pair<const char*,const char*>,4> switches{{
            {"hpf","SIDECHAIN HPF"},{"palm","LOW-MID ON"},
            {"link","LINK OUTPUT"},{"ff","FEEDFORWARD"}
        }};
        for(const auto& item:switches) {
            Toggle t{};
            t.button=std::make_unique<juce::ToggleButton>(item.second);
            t.button->setColour(juce::ToggleButton::textColourId,
                                juce::Colour(0xffe5e4df));
            addAndMakeVisible(*t.button);
            t.attach=std::make_unique<ButtonAttachment>(p.state,item.first,*t.button);
            toggles.push_back(std::move(t));
        }
        load.addItem("600 ohm",1);
        load.addItem("10 kOhm",2);
        addAndMakeVisible(load);
        loadAttachment=std::make_unique<ComboAttachment>(p.state,"load",load);
        cellModel.addItem("NSL-32",1);
        cellModel.addItem("VTL5C2",2);
        cellModel.addItem("T4B-style",3);
        addAndMakeVisible(cellModel);
        cellAttachment=std::make_unique<ComboAttachment>(p.state,"cell",cellModel);
        startTimerHz(25);
    }
    void resized() override {
        constexpr int startX=28,unit=143;
        for(int i=0;i<int(controls.size());++i) {
            const int row=i/4,column=i%4;
            const int x=startX+column*unit;
            const int y=114+row*121;
            controls[i].label->setBounds(x,y,124,19);
            controls[i].slider->setBounds(x,y+20,124,94);
        }
        for(int i=0;i<int(toggles.size());++i)
            toggles[i].button->setBounds(22+i*148,615,143,26);
        load.setBounds(388,685,174,30);
        cellModel.setBounds(388,654,174,28);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff1c1e22));
        auto b=getLocalBounds().reduced(8);
        g.setColour(juce::Colour(0xff414449));
        g.drawRoundedRectangle(b.toFloat(),9.f,2.f);
        g.setColour(juce::Colour(0xffd6a456));
        g.setFont(juce::FontOptions(24.f,juce::Font::bold));
        g.drawFittedText("NICKEL  •  OPTO",25,16,400,36,
                          juce::Justification::left,1);
        g.setFont(juce::FontOptions(13.f,juce::Font::bold));
        g.drawText(channelIndex==0?"CHANNEL L / MONO":"CHANNEL R",
                   415,24,170,24,juce::Justification::right);
        const double reduction=std::clamp(
            double(p.gr[std::size_t(channelIndex)].load()),0.,20.);
        const juce::Rectangle<float> meter(29,72,540,18);
        g.setColour(juce::Colour(0xff0a0b0d));g.fillRoundedRectangle(meter,4.f);
        g.setColour(juce::Colour(0xffd9a65c));
        g.fillRoundedRectangle(meter.withWidth(float(reduction/20.*meter.getWidth())),3.f);
        g.setColour(juce::Colour(0xffeee7da));
        g.setFont(juce::FontOptions(11.f));
        g.drawText("0",30,92,50,16,juce::Justification::left);
        g.drawText("20 dB GR",490,92,78,16,juce::Justification::right);
        g.setColour(juce::Colour(0xffd6a456));
        g.setFont(juce::FontOptions(14.f,juce::Font::bold));
        g.drawText("OPTO CELL",190,654,190,25,
                   juce::Justification::right);
        g.drawText("600 / 10k OUTPUT LOAD",190,688,190,20,
                   juce::Justification::right);
    }
private:
    void timerCallback() override {repaint();}
    struct Control {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<SliderAttachment> attach;
    };
    struct Toggle {
        std::unique_ptr<juce::ToggleButton> button;
        std::unique_ptr<ButtonAttachment> attach;
    };
    NickelOptoProcessor& p;
    int channelIndex=0;
    std::vector<Control> controls;
    std::vector<Toggle> toggles;
    juce::ComboBox load,cellModel;
    std::unique_ptr<ComboAttachment> loadAttachment,cellAttachment;
};
class NickelOptoEditor final : public juce::AudioProcessorEditor {
public:
    explicit NickelOptoEditor(NickelOptoProcessor& p):
         juce::AudioProcessorEditor(p),processor(p),
         left(std::make_unique<HardwarePanel>(p,0)) {
        addAndMakeVisible(*left);
        if(!p.isMono()) {
            right=std::make_unique<HardwarePanel>(p,1);
            addAndMakeVisible(*right);
        }
        setResizable(false,false);
        setSize(right?1240:620,730);
    }
    void resized() override {
        left->setBounds(0,0,620,730);
        if(right)right->setBounds(620,0,620,730);
    }
    void paint(juce::Graphics& g) override {g.fillAll(juce::Colours::black);}
private:
    NickelOptoProcessor& processor;
    std::unique_ptr<HardwarePanel> left,right;
};
juce::AudioProcessorEditor* NickelOptoProcessor::createEditor() {
    return new NickelOptoEditor(*this);
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new NickelOptoProcessor();
}
