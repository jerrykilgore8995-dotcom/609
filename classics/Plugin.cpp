#include <juce_audio_processors/juce_audio_processors.h>
#include "../native/Pipeline.hpp"
#include "Panel.hpp"

namespace {
using mkii::native::Controls;
using juce::String;
constexpr bool original2254 = CLASSIC_MODEL == 2254;
constexpr auto product = original2254 ? "2254 Nodal" : "2264 Nodal";

constexpr float degrees(float angle) { return angle * classics::pi / 180.f; }

struct Scale {
    float first;
    float last;
};

// The rotary pointers use the same angular endpoints as the panel markings.
constexpr Scale limitLevel2254{degrees(-120), degrees(120)};
constexpr Scale recovery2254{degrees(-42), degrees(42)};
constexpr Scale ratio2254{degrees(-56), degrees(56)};
constexpr Scale limitLevel2264{degrees(-75), degrees(255)};
constexpr Scale recovery2264{degrees(30), degrees(180)};
constexpr Scale compressLevel2264{degrees(-15), degrees(210)};
constexpr Scale makeup2264{degrees(-60), degrees(90)};
constexpr Scale ratio2264{degrees(120), degrees(240)};

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
    {"cm", "Compress Recovery", original2254 ? juce::StringArray{"400","800","1500","Auto"}
                                            : juce::StringArray{"100","400","800","1500","A1","A2"}},
    {"cg", "Makeup", original2254 ? juce::StringArray{"0","2","4","6","8","10"}
                                     : juce::StringArray{"0","2","4","6","8","10","12","14","16","18","20"}},
    {"lt", "Limit Threshold", original2254 ?
        juce::StringArray{"+4","+4.5","+5","+5.5","+6","+6.5","+7","+7.5","+8","+8.5","+9","+9.5","+10","+10.5","+11","+11.5","+12"} :
        juce::StringArray{"+4","+4.5","+5","+5.5","+6","+6.5","+7","+7.5","+8","+8.5","+9","+9.5",
                          "+10","+10.5","+11","+11.5","+12","+12.5","+13","+13.5","+14","+14.5","+15"},
        8},
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
        for (int ch=0;ch<2;++ch) {
            circuitIn[ch]=state.getRawParameterValue(id(ch,"in"));
            auxiliary[ch]=state.getRawParameterValue(id(ch,original2254?"deess":"gang"));
        }
        if (original2254) link = state.getRawParameterValue("link");
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
    juce::AudioProcessorEditor* createEditor() override;

    juce::AudioProcessorValueTreeState& panelState() {return state;}
    std::array<std::atomic<float>,2> reduction{}, inputMeter{}, outputMeter{};

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
            const auto prefix=String(ch==0?"Left ":"Right ");
            layout.add(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID{id(ch,"in"),1},prefix+"Circuit In",true));
            if (original2254) {
                layout.add(std::make_unique<juce::AudioParameterBool>(
                    juce::ParameterID{id(ch,"fast"),1},prefix+"Limiter Fast",false));
                layout.add(std::make_unique<juce::AudioParameterBool>(
                    juce::ParameterID{id(ch,"deess"),1},prefix+"De-Ess",false));
                layout.add(std::make_unique<juce::AudioParameterChoice>(
                    juce::ParameterID{id(ch,"meter"),1},prefix+"Meter",juce::StringArray{"In","Control","Out"},1));
            } else {
                layout.add(std::make_unique<juce::AudioParameterChoice>(
                    juce::ParameterID{id(ch,"gang"),1},prefix+"Control Voltage",juce::StringArray{"A","IND","B"},1));
            }
        }
        if (original2254) layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{"link",1},"Stereo Link",false));
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
                settings[offset+8]=static_cast<int>(controls[ch][8]->load());
                settings[22+ch]=auxiliary[ch]->load()>0.5f;
            }
            settings[24+ch]=circuitIn[ch]->load()>0.5f;
        }
        settings[18]=!bypassed;
        const auto one=auxiliary[0]->load(), two=auxiliary[1]->load();
        const bool linked=original2254 ? link->load()>0.5f : one==two && one!=1;
        settings[19]=linked && buffer.getNumChannels()==2 && settings[24] && settings[25];
        for (int ch=0;ch<buffer.getNumChannels();++ch)
            inputMeter[ch].store(buffer.getRMSLevel(ch,0,buffer.getNumSamples()));
        engine.process(buffer.getArrayOfWritePointers(),buffer.getNumSamples(),
                       settings,isNonRealtime(),bypassed);
        const auto gr=engine.readMeters();
        for (int ch=0;ch<buffer.getNumChannels();++ch) {
            reduction[ch].store(settings[24+ch] && !bypassed ? gr[ch] : 0.f);
            outputMeter[ch].store(buffer.getRMSLevel(ch,0,buffer.getNumSamples()));
        }
    }

    juce::AudioProcessorValueTreeState state;
    mkii::native::Pipeline engine;
    std::array<std::array<const std::atomic<float>*,9>,2> controls{};
    std::array<const std::atomic<float>*,2> circuitIn{}, auxiliary{};
    const std::atomic<float>* link=nullptr;
    bool ready=false;
};
class Plate final : public juce::Component, private juce::Timer {
    using FloatRect=juce::Rectangle<float>;
    using SliderAttachment=juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment=juce::AudioProcessorValueTreeState::ButtonAttachment;
    struct Knob {
        std::unique_ptr<classics::Rotary> slider;
        std::unique_ptr<SliderAttachment> attachment;
    };
    struct Toggle {
        std::unique_ptr<juce::ToggleButton> button;
        std::unique_ptr<ButtonAttachment> attachment;
    };

public:
    static constexpr int width=original2254 ? 540 : 170;
    static constexpr int height=original2254 ? 550 : 830;

    Plate(Classic& owner,int channel):processor(owner),ch(channel) {
        using enum classics::Rotary::Type;
        if (original2254) {
            rotary("lt",87,416,54,skirt,limitLevel2254);
            rotary("lr",270,420,54,skirt,recovery2254);
            rotary("cm",452,421,54,skirt,recovery2254);
            rotary("cr",453,157,54,skirt,ratio2254);
            rotary("meter",130,298,29,skirt,{degrees(-55),degrees(55)});
            button("in",132,153,64,64,"rocker");
            button("le",231,153,49,49,"toggle");
            button("ce",309,153,49,49,"toggle");
            button("fast",270,286,43,43,"side");
            button("deess",450,290,49,49,"toggle");
        } else {
            rotary("lr",85,352,48,ring,recovery2264);
            rotary("lt",85,352,32,centre,limitLevel2264);
            rotary("cm",85,507,48,ring,recovery2264);
            rotary("ct",85,507,32,centre,compressLevel2264);
            rotary("cr",85,651,48,ring,ratio2264);
            rotary("cg",85,651,32,centre,makeup2264);
            button("le",44,255,31,34,"toggle");
            button("ce",124,255,31,34,"toggle");
            button("in",44,742,31,37,"rocker");
            gang.setButtonText({});
            gang.setTitle("Control Voltage A / IND / B");
            gang.setColour(juce::TextButton::buttonColourId,juce::Colours::transparentBlack);
            gang.setColour(juce::TextButton::buttonOnColourId,juce::Colours::transparentBlack);
            gang.setTooltip("Control Voltage A / IND / B");
            gang.setBounds(106,724,36,35);
            gang.onClick=[this] {
                auto* p=processor.panelState().getParameter(id(ch,"gang"));
                const auto next=(static_cast<int>(p->convertFrom0to1(p->getValue()))+1)%3;
                p->beginChangeGesture();
                p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(next)));
                p->endChangeGesture();
            };
            addAndMakeVisible(gang);
        }
        startTimerHz(30);
    }

    ~Plate() override {
        stopTimer();
        for (auto& control:switches) control.button->setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override {
        using namespace classics;
        const auto light=original2254 ? Colour{0xff494943} : Colour{0xff30322f};
        g.setGradientFill(juce::ColourGradient(light,0,0,light.darker(.30f),
                                              float(width),float(height),false));
        g.fillAll();
        g.setColour(light.brighter(.20f));
        g.drawRect(0,0,width,height,1);
        g.setColour(light.darker(.8f));
        g.drawRect(0,0,width,height,2);
        if (original2254) paint2254(g);
        else paint2264(g);
    }

    void paintOverChildren(juce::Graphics& g) override {
        if (!original2254) {
            using namespace classics;
            auto x=124.f;
            const auto selected=processor.panelState().getRawParameterValue(id(ch,"gang"))->load();
            const auto y=742.f+8.f*(selected-1.f);
            g.setColour(Colour{0xff171817});
            g.fillRoundedRectangle(x-10,728,20,29,2);
            g.setColour(Colour{0xffb7b2a3});
            g.fillRoundedRectangle(x-7,y-3,14,6,1.5f);
            g.setColour(Colour{0xff53514a});
            g.drawHorizontalLine(int(y-2),x-6,x+6);
        }
    }

private:
    void rotary(const char* control,int x,int y,float r,classics::Rotary::Type shape,Scale scale) {
        auto slider=std::make_unique<classics::Rotary>(shape,r,scale.first,scale.last);
        const auto key=id(ch,control);
        auto& state=processor.panelState();
        slider->setTitle(state.getParameter(key)->getName(80));
        slider->setTooltip(slider->getTitle());
        const int d=2*static_cast<int>(r);
        slider->setBounds(x-d/2,y-d/2,d,d);
        addAndMakeVisible(*slider);
        auto attachment=std::make_unique<SliderAttachment>(state,key,*slider);
        knobs.push_back({std::move(slider),std::move(attachment)});
    }

    void button(const char* control,int x,int y,int w,int h,const char* style) {
        auto toggle=std::make_unique<juce::ToggleButton>();
        const auto key=id(ch,control);
        auto& state=processor.panelState();
        toggle->setTitle(state.getParameter(key)->getName(80));
        toggle->setTooltip(toggle->getTitle());
        toggle->setComponentID(style);
        toggle->setLookAndFeel(&switchLook);
        toggle->setBounds(x-w/2,y-h/2,w,h);
        addAndMakeVisible(*toggle);
        auto attachment=std::make_unique<ButtonAttachment>(state,key,*toggle);
        switches.push_back({std::move(toggle),std::move(attachment)});
    }

    void paint2254(juce::Graphics& g) {
        using namespace classics;
        label(g,"LIMITER AND COMPRESSOR",150,19,240,25,16);
        for (int x:{27,513}) for (int y:{21,533}) screw(g,float(x),float(y),10);
        screw(g,270,546,14);
        label(g,"2254",4,492,38,22,13);
        label(g,"dB",43,46,32,19,14);
        label(g,"BY-PASS",94,107,76,22,14);
        label(g,"LIMIT",195,174,72,24,14);
        label(g,"COMPRESS",266,178,86,24,14);
        label(g,"COMPRESS\nRATIO",404,213,96,34,14);
        label(g,"CONTROL",87,247,96,20,14);
        label(g,"IN",96,264,21,20,14);
        label(g,"OUT",150,264,30,20,14);
        label(g,"METER",103,327,64,20,14);
        label(g,"LIMIT\nATTACK",228,221,84,38,14);
        label(g,"100µS",191,277,55,20,13);
        label(g,"1mS",294,277,41,20,13);
        label(g,"DE-ESS",408,317,84,20,14);
        label(g,"LIMIT LEVEL",29,473,116,21,14);
        label(g,"LIMIT\nRECOVERY",221,475,98,37,14);
        label(g,"COMPRESS\nRECOVERY",403,475,98,37,14);
        marks(g,87,416,61,limitLevel2254.first,limitLevel2254.last,
            {"+4","","","","+6","","","","","","","","+10","","","","+12"});
        label(g,"+8",86,335,22,14,12);
        const juce::StringArray recoveryTicks{"","","",""};
        marks(g,270,420,62,recovery2254.first,recovery2254.last,recoveryTicks);
        marks(g,452,421,62,recovery2254.first,recovery2254.last,recoveryTicks);
        label(g,"100mS",182,352,47,17,12);
        label(g,"200mS",215,333,48,17,12);
        label(g,"800mS",278,333,48,17,12);
        label(g,"AUTO",312,352,46,17,12);
        label(g,"400\nmS",371,352,37,31,12);
        label(g,"800mS",395,338,46,17,12);
        label(g,"1·5 S",450,338,34,17,12);
        label(g,"AUTO",494,352,42,17,12);
        marks(g,453,157,64,ratio2254.first,ratio2254.last,{"1·5:1","2:1","3:1","4:1","6:1"});
        const FloatRect frame{19,73,72,270};
        g.setColour(Colour{0xff111210});
        g.fillRect(frame);
        g.setGradientFill(juce::ColourGradient(Colour{0xfffff6d4},21,77,Colour{0xffc6bb9c},82,333,false));
        g.fillRoundedRectangle(frame.reduced(7,10),3);
        const auto dark=Colour{0xff27241b};
        constexpr std::array levelScale{12.f,8.f,4.f,0.f,-4.f,-8.f,-12.f,-16.f};
        constexpr std::array reductionScale{16.f,12.f,8.f,6.f,4.f,3.f,2.f,1.f};
        for (std::size_t i=0;i<levelScale.size();++i) {
            const auto y=113.f+26.f*float(i);
            const auto level=levelScale[i];
            label(g,(level>0?String("+"):String())+String(level,0),26,y-8,27,16,11,dark);
            label(g,String(reductionScale[i],0),55,y-8,19,16,11,dark);
            g.setColour(dark);
            g.drawHorizontalLine(int(y),50,55);
        }
        const auto mode=processor.panelState().getRawParameterValue(id(ch,"meter"))->load();
        const auto dbu=24.f+juce::Decibels::gainToDecibels(std::sqrt(2.f)*displayedSignal);
        const auto gr=std::clamp(displayedReduction,1.f,16.f);
        float y=295.f;
        if (mode==1) {
            for (std::size_t i=1;i<reductionScale.size();++i) {
                if (gr<reductionScale[i]) continue;
                const auto upper=reductionScale[i-1], lower=reductionScale[i];
                y=113.f+26.f*(float(i)-(gr-lower)/(upper-lower));
                break;
            }
        } else {
            y=113.f+182.f*std::clamp((12.f-dbu)/28.f,0.f,1.f);
        }
        g.setColour(Colour{0xff44392a});
        juce::Path pointer;
        pointer.addTriangle(22,y-5,35,y,22,y+5);
        g.fillPath(pointer);
        g.setColour(Colour{0x20ffffff});
        g.drawRoundedRectangle(frame.reduced(3),3,2);
    }

    void paint2264(juce::Graphics& g) {
        using namespace classics;
        label(g,"GAIN REDUCTION",9,24,152,19,13);
        g.setColour(Colour{0xff0d0e0d});
        g.fillRoundedRectangle(8,43,154,155,3);
        g.setColour(Colour{0xffa5a08b});
        g.drawRoundedRectangle(11,46,148,76,3,1);
        g.setGradientFill(juce::ColourGradient(Colour{0xff34362c},10,48,
            Colour{0xff161814},160,122,false));
        g.fillRoundedRectangle(14,49,142,70,2);
        for (int i=0;i<6;++i) {
            const auto angle=-.94f+.376f*float(i);
            const auto tick=polar(85,133,72,angle);
            g.setColour(ink);
            g.drawLine({tick,polar(85,133,65,angle)},1);
            const auto text=polar(85,133,81,angle);
            label(g,String(i*4),text.x-11,text.y-7,22,14,9);
        }
        label(g,"dB",70,97,30,14,9);
        {
            juce::Graphics::ScopedSaveState save(g);
            g.reduceClipRegion(14,49,142,70);
            g.setColour(Colour{0xfff5ebbc});
            const auto a=-.94f+1.88f*std::clamp(displayedReduction/20.f,0.f,1.f);
            g.drawLine({{85,133},polar(85,133,70,a)},1.35f);
        }
        screw(g,85,166,11);
        label(g,"LIMIT",16,211,56,20,11);
        label(g,"COMPRESS",85,211,78,20,11);
        label(g,"OUT",69,228,32,16,10);
        label(g,"IN",69,258,32,16,10);
        g.setColour(ink);
        g.drawHorizontalLine(279,4,166);
        label(g,"LIMIT",3,282,42,16,10);
        juce::StringArray thresholdLimit=dials[4].labels;
        for (int i=1;i+1<thresholdLimit.size();++i)
            thresholdLimit.set(i,i%2 ? String() : thresholdLimit[i].substring(1));
        marks(g,85,352,56,limitLevel2264.first,limitLevel2264.last,thresholdLimit,orange,9);
        marks(g,85,352,56,recovery2264.first,recovery2264.last,{"50","100","200","800","A1","A2"},ink,10);
        label(g,"THRESHOLD",2,421,83,14,10,orange);
        label(g,"(dBm)",13,433,54,12,9,orange);
        label(g,"RECOVERY",88,421,80,14,10);
        label(g,"(mS)",100,433,55,12,9);
        marks(g,85,507,56,compressLevel2264.first,compressLevel2264.last,
            {"−20","","16","","12","","8","","4","","0","","4","","8","+10"},orange,9);
        marks(g,85,507,56,recovery2264.first,recovery2264.last,{"100","400","800","1500","A1","A2"},ink,10);
        label(g,"COMPRESS",1,574,76,15,10);
        label(g,"GAIN",3,589,43,13,10,orange);
        label(g,"RATIO",124,589,44,13,10);
        marks(g,85,651,56,makeup2264.first,makeup2264.last,{"0","","4","","8","","12","","16","","20"},orange,9);
        marks(g,85,651,56,ratio2264.first,ratio2264.last,{"1.5:1","2:1","3:1","4:1","6:1"},ink,9);
        label(g,"BYPASS",5,704,77,17,11);
        label(g,"IN",12,760,34,16,10);
        label(g,"A",147,722,20,12,10);
        label(g,"IND",141,736,26,12,10);
        label(g,"B",147,752,20,12,10);
        screw(g,85,811,11);
        label(g,"2264",5,785,43,17,12);
    }

    void timerCallback() override {
        const auto channel=static_cast<std::size_t>(ch);
        const auto next=processor.reduction[channel].load();
        displayedReduction+=(next-displayedReduction)*(next>displayedReduction?.55f:.18f);
        if (original2254) {
            const auto mode=processor.panelState().getRawParameterValue(id(ch,"meter"))->load();
            const auto signal=mode==0 ? processor.inputMeter[channel].load() : processor.outputMeter[channel].load();
            displayedSignal+=(signal-displayedSignal)*(signal>displayedSignal?.7f:.15f);
        }
        repaint(original2254?juce::Rectangle<int>{18,74,72,266}:juce::Rectangle<int>{8,43,154,155});
        if (!original2254) repaint(106,724,36,35);
    }

    Classic& processor;
    int ch;
    classics::SwitchLook switchLook;
    std::vector<Knob> knobs;
    std::vector<Toggle> switches;
    juce::TextButton gang;
    float displayedReduction=0,displayedSignal=0;
};

class Editor final : public juce::AudioProcessorEditor {
public:
    explicit Editor(Classic& owner):AudioProcessorEditor(owner) {
        const int channels=owner.getTotalNumInputChannels()==1 ? 1 : 2;
        for (int ch=0;ch<channels;++ch) {
            auto panel=std::make_unique<Plate>(owner,ch);
            panel->setBounds(ch*Plate::width,0,Plate::width,Plate::height);
            addAndMakeVisible(*panel);
            panels.push_back(std::move(panel));
        }
        setSize(channels*Plate::width,Plate::height);
    }

private:
    std::vector<std::unique_ptr<Plate>> panels;
};

juce::AudioProcessorEditor* Classic::createEditor() {return new Editor(*this);}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new Classic;
}
