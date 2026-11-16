#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../native/Client.hpp"

#include <atomic>
#include <cmath>
#include <numbers>

namespace {
using juce::String;
struct Parameter {
    const char* id;
    const char* title;
    juce::StringArray values;
};
const std::array<Parameter,9> controls{{
    {"ct","Compress Threshold",{"−20","−18","−16","−14","−12","−10","−8","−6","−4","−2","0","+2","+4","+6","+8","+10"}},
    {"cr","Compress Ratio",{"1.5:1","2:1","3:1","4:1","6:1"}},
    {"cm","Compress Recovery",{"100","400","800","1500","a1","a2"}},
    {"cg","Compress Gain",{"0","2","4","6","8","10","12","14","16","18","20"}},
    {"lt","Limit Threshold",{"+4","+4.5","+5","+5.5","+6","+6.5","+7","+7.5","+8","+8.5","+9","+9.5","+10","+10.5","+11","+11.5","+12","+12.5","+13","+13.5","+14","+14.5","+15"}},
    {"lr","Limit Recovery",{"50","100","200","800","a1","a2"}},
    {"ce","Compress In",{}},{"le","Limit In",{}},{"fast","Fast Attack",{}}
}};
const std::array<const char*,6> modes{"in","stereo","power","external","remote_in","remote_stereo"};
const std::array<const char*,6> modeTitles{"Bypass In","Stereo Link","Power","External Control","Remote Bypass In","Remote Stereo Link"};
String parameterId(int channel,const char* name) {return String(channel+1)+"_"+name;}

juce::File runtimePath() {
    static const int anchor=0;
    HMODULE module=nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&anchor),&module)) return {};
    std::array<wchar_t,32768> path{};
    const auto count=GetModuleFileNameW(module,path.data(),static_cast<DWORD>(path.size()));
    if (!count || count>=path.size()) return {};
    return juce::File(String(path.data())).getParentDirectory().getParentDirectory()
        .getChildFile("Resources").getChildFile("609-native.dll");
}

class Compressor final : public juce::AudioProcessor {
public:
    Compressor():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
                                                 .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
                 state(*this,nullptr,"Settings",parameters()) {
        const auto runtime=runtimePath().getFullPathName();
        engine.open(runtime.toWideCharPointer());
        for (int ch=0;ch<2;++ch) for (int i=0;i<9;++i)
            channelParams[ch][i]=state.getRawParameterValue(parameterId(ch,controls[i].id));
        for (int i=0;i<6;++i) modeParams[i]=state.getRawParameterValue(modes[i]);
    }
    const String getName() const override {return "609";}
    bool acceptsMidi() const override {return false;}
    bool producesMidi() const override {return false;}
    bool isMidiEffect() const override {return false;}
    double getTailLengthSeconds() const override {return 5.0;}
    int getNumPrograms() override {return 1;}
    int getCurrentProgram() override {return 0;}
    void setCurrentProgram(int) override {}
    const String getProgramName(int) override {return {};}
    void changeProgramName(int,const String&) override {}
    bool hasEditor() const override {return true;}
    juce::AudioProcessorEditor* createEditor() override;
    bool isBusesLayoutSupported(const BusesLayout& layout) const override {
        const auto input=layout.getMainInputChannelSet();
        return input==layout.getMainOutputChannelSet() &&
               (input==juce::AudioChannelSet::mono() || input==juce::AudioChannelSet::stereo());
    }
    void prepareToPlay(double rate,int block) override {
        ready=engine.prepare(rate,getTotalNumInputChannels(),block);
        setLatencySamples(ready?engine.latency():0);
    }
    void releaseResources() override {engine.stop();ready=false;}
    void processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer&) override {process(audio,false);}
    void processBlockBypassed(juce::AudioBuffer<float>& audio,juce::MidiBuffer&) override {process(audio,true);}
    void getStateInformation(juce::MemoryBlock& data) override {copyXmlToBinary(*state.copyState().createXml(),data);}
    void setStateInformation(const void* data,int size) override {
        if (auto xml=getXmlFromBinary(data,size);xml && xml->hasTagName(state.state.getType()))
            state.replaceState(juce::ValueTree::fromXml(*xml));
    }
    juce::AudioProcessorValueTreeState state;
    std::array<std::atomic<float>,2> meters{};
    std::atomic<bool> readyForPlayback{true};
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout parameters() {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        for (int ch=0;ch<2;++ch) for (int i=0;i<9;++i) {
            const auto& setting=controls[i];
            const juce::ParameterID id{parameterId(ch,setting.id),1};
            const auto title=String("Channel ")+String(ch+1)+" "+setting.title;
            if (setting.values.isEmpty()) layout.add(std::make_unique<juce::AudioParameterBool>(id,title,false));
            else layout.add(std::make_unique<juce::AudioParameterChoice>(id,title,setting.values,
                mkii::native::defaults[ch*9+i]-1));
        }
        for (int i=0;i<6;++i) layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{modes[i],1},modeTitles[i],i==0 || i==2 || i==4));
        return layout;
    }
    void process(juce::AudioBuffer<float>& audio,bool dry) {
        if (!ready || !audio.getNumSamples()) return;
        mkii::native::Controls positions;
        for (int ch=0;ch<2;++ch) for (int i=0;i<9;++i)
            positions[ch*9+i]=static_cast<int>(channelParams[ch][i]->load())+(i<6?1:0);
        const bool remote=modeParams[3]->load()>0.5f;
        positions[18]=modeParams[remote?4:0]->load()>0.5f;
        positions[19]=modeParams[remote?5:1]->load()>0.5f;
        positions[20]=modeParams[2]->load()>0.5f;
        const bool complete=engine.process(audio.getArrayOfWritePointers(),audio.getNumSamples(),positions,isNonRealtime(),dry);
        readyForPlayback.store(complete && engine.failedSolves()==0);
        const auto current=engine.readMeters();
        for (int i=0;i<2;++i) meters[i].store(current[i]);
    }
    mkii::native::Client engine;
    std::array<std::array<const std::atomic<float>*,9>,2> channelParams{};
    std::array<const std::atomic<float>*,6> modeParams{};
    bool ready=false;
};

class Panel final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit Panel(Compressor& owner):AudioProcessorEditor(owner),processor(owner) {
        for (int ch=0;ch<2;++ch) for (int i=0;i<9;++i) {
            const auto id=parameterId(ch,controls[i].id);
            if (i<6) {
                auto& knob=knobs[ch][i];
                knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
                knob.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
                knob.setPopupDisplayEnabled(true,true,this);
                knob.setLookAndFeel(&face);
                knob.setName(controls[i].title);
                addAndMakeVisible(knob);
                dialBindings.push_back(std::make_unique<DialBinding>(processor.state,id,knob));
            } else bindToggle(switches[ch][i-6],id,controls[i].title);
        }
        for (int i=0;i<4;++i) bindToggle(chassis[i],modes[i],modeTitles[i]);
        setSize(1200,220);
        startTimerHz(30);
    }
    ~Panel() override {
        for (auto& row:knobs) for (auto& knob:row) knob.setLookAndFeel(nullptr);
        for (auto& row:switches) for (auto& sw:row) sw.setLookAndFeel(nullptr);
        for (auto& sw:chassis) sw.setLookAndFeel(nullptr);
    }
    void resized() override {
        constexpr std::array x{388,608,463,536,131,206};
        constexpr std::array sx{675,276,314};
        for (int ch=0;ch<2;++ch) {
            const int y=66+95*ch;
            for (int i=0;i<6;++i) knobs[ch][i].setBounds(x[i]-24,y-24,48,48);
            for (int i=0;i<3;++i) switches[ch][i].setBounds(sx[i]-10,y-17,20,34);
        }
        for (int i=0;i<4;++i) chassis[i].setBounds(i<2?704:1046,34+92*((i<2?i%2:1-i%2)),59,61);
    }
    void paint(juce::Graphics& g) override {
        const auto dark=juce::Colour(0xff22272c), light=juce::Colour(0xff363e45);
        g.setGradientFill({light,0,0,dark,0,220,false});
        g.fillAll();
        g.setColour(juce::Colour(0xff14171a));
        for (int x:{20,1152}) for (int y:{9,195}) g.fillRoundedRectangle(float(x),float(y),28,16,8);
        for (int x:{81,1116}) for (int y:{68,154}) {
            g.setColour(juce::Colour(0xff42464a));g.fillEllipse(float(x),float(y),13,13);
            g.setColour(dark);g.drawLine(float(x+2),float(y+8),float(x+11),float(y+5),1.2f);
        }
        for (int ch=0;ch<2;++ch) {
            const int y=7+95*ch;
            g.setColour(juce::Colour(0xff92979a));
            g.drawRect(juce::Rectangle<float>(97,float(y),243,98),0.7f);
            g.drawRect(juce::Rectangle<float>(344,float(y),348,98),0.7f);
            const int knobY=66+95*ch;
            dialScale(g,131,knobY,"threshold dBu",controls[4].values,2);
            dialScale(g,206,knobY,"recovery ms",controls[5].values,1);
            dialScale(g,388,knobY,"threshold dBu",controls[0].values,2);
            dialScale(g,463,knobY,"recovery ms",controls[2].values,1);
            dialScale(g,536,knobY,"gain",controls[3].values,2);
            dialScale(g,608,knobY,"ratio",controls[1].values,1);
            label(g,"Limit",254,knobY+19,44,10,9);
            label(g,"In",254,knobY+29,44,10,9);
            label(g,"attack",292,knobY-43,44,10,10);
            label(g,"fast",292,knobY-30,44,10,9);
            label(g,"slow",292,knobY+20,44,10,9);
            label(g,"compress",649,knobY+20,52,10,9);
            label(g,"in",649,knobY+30,52,10,9);
            label(g,"LIMIT "+String(ch+1),163,y+83,92,15,13);
            label(g,"COMPRESS "+String(ch+1),439,y+83,128,15,13);
        }
        meter(g,780,25,processor.meters[0].load(),1);
        meter(g,905,84,processor.meters[1].load(),2);
        label(g,"33609/JD",769,176,106,13,12);
        label(g,"PRECISION STEREO",769,190,106,13,11);
        label(g,"LIMITER/COMPRESSOR",769,203,118,13,11);
    }
private:
    static void label(juce::Graphics& g,String text,int x,int y,int w,int h,float size,
                      juce::Colour colour=juce::Colour(0xffeeeee8)) {
        g.setColour(colour);
        g.setFont(juce::Font(juce::FontOptions(size)));
        g.drawText(text,x,y,w,h,juce::Justification::centred);
    }
    static void dialScale(juce::Graphics& g,int x,int y,String name,const juce::StringArray& values,int stride) {
        label(g,name,x-39,y-51,78,15,11);
        const float sweep=values.size()>12?4.65f:2.69f;
        for (int i=0;i<values.size();++i) {
            const float a=(float(i)/float(values.size()-1)-0.5f)*sweep;
            const float dx=std::sin(a),dy=-std::cos(a);
            g.setColour(juce::Colour(0xffeeeee8));
            g.drawLine(x+24*dx,y+24*dy,x+27*dx,y+27*dy,0.75f);
            if (i%stride==0 || i==values.size()-1)
                label(g,values[i],int(x+34*dx)-13,int(y+34*dy)-5,26,11,8.5f);
        }
    }
    static void meter(juce::Graphics& g,int x,int y,float reduction,int ch) {
        label(g,"gain reduction "+String(ch),x,y-18,118,16,11);
        g.setColour(juce::Colour(0xff151918));g.fillRoundedRectangle(float(x),float(y),118,101,2);
        g.setColour(juce::Colour(0xff6b6e65));g.drawRect(x+2,y+2,114,97,1);
        g.setGradientFill({juce::Colour(0xfff9db94),0,float(y+6),juce::Colour(0xffcab784),0,float(y+69),false});
        g.fillRect(x+8,y+9,102,57);
        const auto ink=juce::Colour(0xff363322);
        const juce::Point<float> p(x+59.0f,y+67.0f);
        for (int mark=0;mark<=20;mark+=4) {
            const float a=-1.02f+2.04f*mark/20;
            const juce::Point<float> u(std::sin(a),-std::cos(a));
            g.setColour(ink);g.drawLine({p+u*35.0f,p+u*44.0f},0.8f);
            label(g,String(mark),int(p.x+52*u.x)-8,int(p.y+52*u.y)-5,16,9,8,ink);
        }
        label(g,"dB",x+46,y+52,26,11,8,ink);
        const float a=-1.02f+2.04f*std::clamp(reduction/20.0f,0.0f,1.0f);
        g.setColour(juce::Colour(0xff2c211a));
        g.drawLine({p,p+juce::Point<float>{std::sin(a),-std::cos(a)}*43.0f},1.1f);
        g.setColour(juce::Colour(0xffb1aca0));g.drawEllipse(float(x+53),float(y+78),12,12,1);
        g.drawLine(float(x+55),float(y+88),float(x+63),float(y+80),1);
    }
    class Face final : public juce::LookAndFeel_V4 {
        void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float position,float,float,juce::Slider& slider) override {
            const juce::Point<float> center(x+w*.5f,y+h*.5f);
            const auto dial=juce::Rectangle<float>(43,43).withCentre(center);
            g.setGradientFill({juce::Colour(0xff515254),0,dial.getY(),juce::Colour(0xff121416),0,dial.getBottom(),false});
            g.fillEllipse(dial);
            g.setColour(juce::Colour(0xff8d8d8b));g.drawEllipse(dial.reduced(1.5f),.55f);
            const float sweep=(slider.getName().contains("Threshold")?4.65f:2.69f);
            const float a=(position-.5f)*sweep;
            g.setColour(juce::Colour(0xffe8e6d5));
            const juce::Point<float> direction(std::sin(a),-std::cos(a));
            g.drawLine({center+direction*12.0f,center+direction*19.0f},1.8f);
        }
        void drawToggleButton(juce::Graphics& g,juce::ToggleButton& button,bool,bool pressed) override {
            const auto b=button.getLocalBounds().toFloat();
            g.setColour(juce::Colour(0xff171915));g.fillRoundedRectangle(b,2);
            const bool active=button.getToggleState();
            if (b.getWidth()<24) {
                g.setColour(active?juce::Colour(0xffbfb7a0):juce::Colour(0xff7a766a));
                g.fillRoundedRectangle(b.reduced(4,7).translated(0,active?-4.0f:4.0f),3);
                return;
            }
            const auto interior=b.reduced(4);
            g.setColour(pressed?juce::Colour(0xff867e64):juce::Colour(0xffbdb395));g.fillRect(interior);
            const auto name=button.getName();
            if (name=="Power") {
                if (active) {g.setColour(juce::Colour(0xffe9c47c));g.fillRect(interior.reduced(1));}
                label(g,"POWER",0,20,button.getWidth(),21,10,juce::Colour(0xff25251c));
                return;
            }
            const String top=name=="Bypass In"?"BY PASS":name=="Stereo Link"?"MONO":"EXT";
            const String bottom=name=="Bypass In"?"IN":name=="Stereo Link"?"STEREO":"INT";
            const bool topActive=name=="External Control"?active:!active;
            const bool bottomActive=!topActive;
            g.setColour(juce::Colour(0xffefd199));
            if (topActive) g.fillRect(interior.withTrimmedBottom(interior.getHeight()/2));
            if (bottomActive) g.fillRect(interior.withTrimmedTop(interior.getHeight()/2));
            g.setColour(juce::Colour(0xff514e41));g.drawHorizontalLine(30,5,b.getWidth()-5);
            const auto ink=juce::Colour(0xff25251c);
            label(g,top,0,10,button.getWidth(),18,10,ink);label(g,bottom,0,33,button.getWidth(),18,10,ink);
        }
    };
    using DialBinding=juce::AudioProcessorValueTreeState::SliderAttachment;
    using ToggleBinding=juce::AudioProcessorValueTreeState::ButtonAttachment;
    void bindToggle(juce::ToggleButton& button,const String& id,const String& name) {
        button.setName(name);
        button.setLookAndFeel(&face);
        addAndMakeVisible(button);
        toggleBindings.push_back(std::make_unique<ToggleBinding>(processor.state,id,button));
    }
    void timerCallback() override {repaint();}
    Compressor& processor;
    Face face;
    std::array<std::array<juce::Slider,6>,2> knobs;
    std::array<std::array<juce::ToggleButton,3>,2> switches;
    std::array<juce::ToggleButton,4> chassis;
    std::vector<std::unique_ptr<DialBinding>> dialBindings;
    std::vector<std::unique_ptr<ToggleBinding>> toggleBindings;
};
juce::AudioProcessorEditor* Compressor::createEditor() {return new Panel(*this);}
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {return new Compressor;}
