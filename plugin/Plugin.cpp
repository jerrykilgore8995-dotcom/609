#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../native/Realtime.hpp"
#include "../native/SupplyNoise.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <numbers>

namespace {
using juce::String;
struct Parameter {
    const char* id;
    const char* title;
    juce::StringArray values;
    float sweep=0;
};
const std::array<Parameter,9> controls{{
    {"ct","Compress Threshold",{"-20","-18","-16","-14","-12","-10","-8","-6","-4","-2","0","+2","+4","+6","+8","+10"},4.65f},
    {"cr","Compress Ratio",{"1.5:1","2:1","3:1","4:1","6:1"},2.2f},
    {"cm","Compress Recovery",{"100","400","800","1500","a1","a2"},3.3f},
    {"cg","Compress Gain",{"0","2","4","6","8","10","12","14","16","18","20"},3.6f},
    {"lt","Limit Threshold",{"+4","+4.5","+5","+5.5","+6","+6.5","+7","+7.5","+8","+8.5","+9","+9.5","+10","+10.5","+11","+11.5","+12","+12.5","+13","+13.5","+14","+14.5","+15"},4.65f},
    {"lr","Limit Recovery",{"50","100","200","800","a1","a2"},3.3f},
    {"ce","Compress In",{}},{"le","Limit In",{}},{"fast","Fast Attack",{}}
}};
const std::array<const char*,6> modes{"in","stereo","power","external","remote_in","remote_stereo"};
const std::array<const char*,6> modeTitles{"Bypass In","Stereo Link","Power","External Control","Remote Bypass In","Remote Stereo Link"};
String parameterId(int channel,const char* name) {return String(channel+1)+"_"+name;}

class Compressor final : public juce::AudioProcessor {
public:
    Compressor():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
                                                 .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
                 state(*this,nullptr,"Settings",parameters()) {
        for (int ch=0;ch<2;++ch) for (int i=0;i<9;++i)
            channelParams[ch][i]=state.getRawParameterValue(parameterId(ch,controls[i].id));
        for (int i=0;i<6;++i) modeParams[i]=state.getRawParameterValue(modes[i]);
        noiseParam=state.getRawParameterValue("noise");
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
    void prepareToPlay(double rate,int) override {
        engine.prepare(rate);
        ready=true;
        supplyNoise.prepare(rate);
        setLatencySamples(0);
    }
    void releaseResources() override {ready=false;}
    void processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer&) override {process(audio,false);}
    void processBlockBypassed(juce::AudioBuffer<float>& audio,juce::MidiBuffer&) override {process(audio,true);}
    void getStateInformation(juce::MemoryBlock& data) override {copyXmlToBinary(*state.copyState().createXml(),data);}
    void setStateInformation(const void* data,int size) override {
        if (auto xml=getXmlFromBinary(data,size);xml && xml->hasTagName(state.state.getType()))
            state.replaceState(juce::ValueTree::fromXml(*xml));
    }
    juce::AudioProcessorValueTreeState state;
    std::array<std::atomic<float>,2> meters{};
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
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"noise",1},"PSU Noise",false));
        return layout;
    }
    void process(juce::AudioBuffer<float>& audio,bool dry) {
        if (!ready || !audio.getNumSamples()) return;
        juce::ScopedNoDenormals noDenormals;
        mkii::native::Controls positions;
        for (int ch=0;ch<2;++ch) for (int i=0;i<9;++i)
            positions[ch*9+i]=static_cast<int>(channelParams[ch][i]->load())+(i<6?1:0);
        const bool remote=modeParams[3]->load()>0.5f;
        positions[18]=modeParams[remote?4:0]->load()>0.5f;
        positions[19]=modeParams[remote?5:1]->load()>0.5f;
        positions[20]=modeParams[2]->load()>0.5f;
        engine.control(positions);
        auto* const left=audio.getWritePointer(0);
        auto* const right=audio.getNumChannels()==2?audio.getWritePointer(1):nullptr;
        for (int frame=0;frame<audio.getNumSamples();++frame) {
            const auto wet=engine.step({left[frame],right?right[frame]:0.0});
            if (dry) continue;
            left[frame]=static_cast<float>(wet[0]);
            if (right) right[frame]=static_cast<float>(wet[1]);
        }
        if (dry || !positions[18] || !positions[20]) supplyNoise.silence();
        else supplyNoise.process(audio.getArrayOfWritePointers(),audio.getNumChannels(),audio.getNumSamples(),
                                 noiseParam->load()>0.5f);
        const auto current=engine.gainReduction();
        for (int i=0;i<2;++i) meters[i].store(dry?0.0f:static_cast<float>(current[i]));
    }
    mkii::native::Realtime engine;
    mkii::native::SupplyNoise supplyNoise;
    const std::atomic<float>* noiseParam=nullptr;
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
                const float centre=2*std::numbers::pi_v<float>;
                const float halfSweep=controls[i].sweep*.5f;
                knob.setRotaryParameters(centre-halfSweep,centre+halfSweep,true);
                knob.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
                knob.setPopupDisplayEnabled(true,true,this);
                knob.setLookAndFeel(&face);
                knob.setName(controls[i].title);
                addAndMakeVisible(knob);
                dialBindings.push_back(std::make_unique<DialBinding>(processor.state,id,knob));
            } else bindToggle(switches[ch][i-6],id,controls[i].title);
        }
        for (int i=0;i<4;++i) bindToggle(chassis[i],modes[i],modeTitles[i]);
        noise.setButtonText("PSU NOISE");
        addAndMakeVisible(noise);
        toggleBindings.push_back(std::make_unique<ToggleBinding>(processor.state,"noise",noise));
        setSize(1200,247);
        startTimerHz(30);
    }
    ~Panel() override {
        for (auto& row:knobs) for (auto& knob:row) knob.setLookAndFeel(nullptr);
        for (auto& row:switches) for (auto& sw:row) sw.setLookAndFeel(nullptr);
        for (auto& sw:chassis) sw.setLookAndFeel(nullptr);
    }
    void resized() override {
        constexpr std::array x{387,611,463,536,130,208};
        constexpr std::array sx{679,279,317};
        for (int ch=0;ch<2;++ch) {
            const int y=65+90*ch;
            for (int i=0;i<6;++i) knobs[ch][i].setBounds(x[i]-24,y-24,48,48);
            for (int i=0;i<3;++i) switches[ch][i].setBounds(sx[i]-10,y-17,20,34);
        }
        noise.setBounds(1045,221,112,24);
        for (int i=0;i<4;++i) chassis[i].setBounds(i<2?707:1049,35+91*(i<2?i%2:1-i%2),60,61);
    }
    void paint(juce::Graphics& g) override {
        const auto dark=juce::Colour(0xff202d42), light=juce::Colour(0xff33445f);
        g.setGradientFill({light,0,0,dark,0,220,false});
        g.fillRect(0,0,getWidth(),220);
        g.setColour(juce::Colour(0xff15171a));
        g.fillRect(0,220,getWidth(),getHeight()-220);
        g.setColour(juce::Colour(0xff14171a));
        for (int x:{9,1163}) for (int y:{6,195}) g.fillRoundedRectangle(float(x),float(y),28,18,9);
        for (int x:{62,1125}) for (int y:{59,149}) {
            g.setColour(juce::Colour(0xff42464a));g.fillEllipse(float(x),float(y),13,13);
            g.setColour(dark);g.drawLine(float(x+2),float(y+8),float(x+11),float(y+5),1.2f);
        }
        g.setColour(juce::Colour(0xffd5dce4));
        for (const juce::Rectangle<float> border : {
                 juce::Rectangle<float>{704,8,66,202}, {774,8,265,202}, {1044,8,72,202}})
            g.drawRect(border,0.9f);
        for (int ch=0;ch<2;++ch) {
            const int y=8+102*ch;
            g.setColour(juce::Colour(0xffe1e5e9));
            g.drawRect(juce::Rectangle<float>(88,float(y),251,98),0.8f);
            g.drawRect(juce::Rectangle<float>(345,float(y),354,98),0.8f);
            const int knobY=65+90*ch;
            dialScale(g,130,knobY,"threshold dBu",controls[4],2);
            dialScale(g,208,knobY,"recovery ms",controls[5],1);
            dialScale(g,387,knobY,"threshold dBu",controls[0],2);
            dialScale(g,463,knobY,"recovery ms",controls[2],1);
            dialScale(g,536,knobY,"gain",controls[3],2);
            dialScale(g,611,knobY,"ratio",controls[1],1);
            label(g,"Limit",254,knobY+19,44,10,9);
            label(g,"In",254,knobY+29,44,10,9);
            label(g,"attack",292,knobY-43,44,10,10);
            label(g,"fast",292,knobY-30,44,10,9);
            label(g,"slow",292,knobY+20,44,10,9);
            label(g,"compress",649,knobY+20,52,10,9);
            label(g,"in",649,knobY+30,52,10,9);
            label(g,"LIMIT "+String(ch+1),148,y+83,130,15,11);
            label(g,"COMPRESS "+String(ch+1),445,y+83,154,15,11);
        }
        meter(g,792,26,displayedReduction[0],1);
        meter(g,914,88,displayedReduction[1],2);
        g.setColour(juce::Colour(0xfff1f3f5));
        g.setFont(juce::Font(juce::FontOptions(10.0f,juce::Font::bold)));
        g.drawText("33609/J",793,166,48,13,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xffe04a49));
        g.drawText("DISCRETE",839,166,68,13,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xfff1f3f5));
        g.drawText("PRECISION STEREO",793,180,117,13,juce::Justification::centredLeft);
        g.drawText("LIMITER/COMPRESSOR",793,193,119,14,juce::Justification::centredLeft);
    }
private:
    static void label(juce::Graphics& g,String text,int x,int y,int w,int h,float size,
                      juce::Colour colour=juce::Colour(0xffeeeee8)) {
        g.setColour(colour);
        g.setFont(juce::Font(juce::FontOptions(size)));
        g.drawText(text,x,y,w,h,juce::Justification::centred);
    }
    static void dialScale(juce::Graphics& g,int x,int y,String name,const Parameter& parameter,int stride) {
        label(g,name,x-39,y-44,78,9,8.5f);
        const auto& values=parameter.values;
        for (int i=0;i<values.size();++i) {
            const float a=(float(i)/float(values.size()-1)-0.5f)*parameter.sweep;
            const float dx=std::sin(a),dy=-std::cos(a);
            g.setColour(juce::Colour(0xffeeeee8));
            g.drawLine(x+24*dx,y+24*dy,x+27*dx,y+27*dy,0.75f);
            if (i%stride==0 || i==values.size()-1)
                label(g,values[i],int(x+34*dx)-11,int(y+32*dy)-4,22,8,7.4f);
        }
    }
    static void meter(juce::Graphics& g,int x,int y,float reduction,int ch) {
        label(g,"gain reduction "+String(ch),x,y-16,110,13,9);
        const juce::Rectangle<float> bezel(float(x),float(y),110,98);
        g.setColour(juce::Colour(0x9905080c));
        g.fillRect(bezel.translated(2,3));
        g.setGradientFill({juce::Colour(0xff353638),0,bezel.getY(),
                           juce::Colour(0xff18191b),0,bezel.getBottom(),false});
        g.fillRect(bezel);
        g.setColour(juce::Colour(0xff4c4e50));
        g.drawHorizontalLine(y,x+1.0f,x+109.0f);
        const auto window=juce::Rectangle<int>(x+9,y+10,92,50);
        g.setColour(juce::Colour(0xff4a4b4d));
        g.fillRect(window.expanded(3,3));
        g.setGradientFill({juce::Colour(0xff08090b),0,float(y+10),
                           juce::Colour(0xff202123),0,float(y+58),false});
        g.fillRect(window);
        const auto ink=juce::Colour(0xffeeefed);
        const juce::Point<float> pivot(x+55.0f,y+72.0f);
        constexpr std::array<float,6> angles{-0.86f,-0.51f,-0.15f,0.17f,0.51f,0.86f};
        for (int i=0;i<6;++i) {
            const auto a=angles[i];
            const juce::Point<float> u(std::sin(a),-std::cos(a));
            g.setColour(ink);
            g.drawLine({pivot+u*42.0f,pivot+u*48.0f},0.9f);
            label(g,String(i*4),int(pivot.x+55*u.x)-8,int(pivot.y+55*u.y)-4,16,9,7.5f,ink);
        }
        label(g,"dB",x+44,y+43,22,9,8,ink);
        const float scaled=std::clamp(reduction/4.0f,0.0f,5.0f);
        const int index=std::min(int(scaled),4);
        const float a=std::lerp(angles[index],angles[index+1],scaled-index);
        {
            const juce::Graphics::ScopedSaveState clip(g);
            g.reduceClipRegion(window);
            g.setColour(ink);
            g.drawLine({pivot,pivot+juce::Point<float>{std::sin(a),-std::cos(a)}*53.0f},1.35f);
        }
        g.setColour(juce::Colour(0xff121315));
        g.fillEllipse(float(x+49),float(y+77),12,12);
        g.setColour(juce::Colour(0xff494a4a));
        g.fillEllipse(float(x+50),float(y+78),10,10);
        g.setColour(juce::Colour(0xff171719));
        g.drawLine(float(x+53),float(y+78),float(x+57),float(y+88),1);
    }
    class Face final : public juce::LookAndFeel_V4 {
        void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float position,float start,float end,juce::Slider&) override {
            const juce::Point<float> center(x+w*.5f,y+h*.5f);
            const auto dial=juce::Rectangle<float>(42,42).withCentre(center);
            g.setColour(juce::Colour(0xff111821));
            g.fillEllipse(dial.translated(1,2));
            g.setGradientFill({juce::Colour(0xfff5f7fa),0,dial.getY(),
                               juce::Colour(0xff777f88),0,dial.getBottom(),false});
            g.fillEllipse(dial);
            const auto flutes=dial.reduced(2.0f);
            g.setColour(juce::Colour(0xff9ba4ae));
            g.drawEllipse(flutes,1.3f);
            for (int i=0;i<48;++i) {
                const float theta=2.0f*std::numbers::pi_v<float>*i/48.0f;
                const juce::Point<float> u(std::sin(theta),std::cos(theta));
                g.setColour(i<25?juce::Colour(0xff6c7680):juce::Colour(0xffedf0f2));
                g.drawLine({center+u*18.1f,center+u*20.0f},0.55f);
            }
            const auto cap=dial.reduced(4.1f);
            g.setGradientFill({juce::Colour(0xff3590cb),cap.getX(),cap.getY(),
                              juce::Colour(0xff0b62ab),cap.getRight(),cap.getBottom(),false});
            g.fillEllipse(cap);
            g.setColour(juce::Colour(0xff164573));
            g.drawEllipse(cap,0.65f);
            const float a=start+position*(end-start);
            g.setColour(juce::Colour(0xfff1f5f7));
            const juce::Point<float> direction(std::sin(a),-std::cos(a));
            g.drawLine({center,center+direction*16.0f},1.55f);
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
            const auto frame=b.reduced(1.5f);
            g.setGradientFill({juce::Colour(0xfff0f0e8),0,frame.getY(),
                               juce::Colour(0xff6c706d),0,frame.getBottom(),false});
            g.fillRoundedRectangle(frame,2.5f);
            g.setColour(juce::Colour(0xff424742));
            g.fillRoundedRectangle(frame.reduced(3),2);
            const auto interior=b.reduced(6).translated(0,pressed?1.0f:0.0f);
            const auto name=button.getName();
            const auto drawLens=[&](juce::Rectangle<float> bound,juce::Colour lens,bool lit) {
                auto gradient=juce::ColourGradient(lens.brighter(lit?.27f:.08f),0,bound.getY(),
                                                   lens.darker(.43f),0,bound.getBottom(),false);
                gradient.addColour(.35,lens);
                g.setGradientFill(gradient);
                g.fillRoundedRectangle(bound,1.8f);
                g.setColour(juce::Colours::white.withAlpha(lit?.46f:.30f));
                g.drawHorizontalLine(int(bound.getY()+1),bound.getX()+2,bound.getRight()-2);
                g.setColour(juce::Colours::black.withAlpha(.26f));
                g.drawRoundedRectangle(bound,1.8f,.65f);
            };
            if (name=="Power") {
                drawLens(interior,juce::Colour(active?0xffedb198:0xffc5c6bd),active);
                label(g,"POWER",0,20,button.getWidth(),21,10,juce::Colour(0xff25251c));
                return;
            }
            const String top=name=="Bypass In"?"BYPASS":name=="Stereo Link"?"MONO":"EXT";
            const String bottom=name=="Bypass In"?"IN":name=="Stereo Link"?"STEREO":"INT";
            const bool topActive=name=="External Control"?active:!active;
            const auto topFace=interior.withTrimmedBottom(interior.getHeight()/2+.5f);
            const auto bottomFace=interior.withTrimmedTop(interior.getHeight()/2+.5f);
            if (name=="External Control") {
                drawLens(topFace,juce::Colour(topActive?0xff79e646:0xff527b41),topActive);
                drawLens(bottomFace,juce::Colour(topActive?0xff873e3a:0xffef6d5d),!topActive);
            } else {
                drawLens(topFace,juce::Colour(topActive?0xffeeefd8:0xffc9cac2),topActive);
                drawLens(bottomFace,juce::Colour(topActive?0xffc9cac2:0xffdbeab5),!topActive);
            }
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
    void timerCallback() override {
        static const std::array<juce::Rectangle<int>,2> meterBounds{{
            {792,26,113,101}, {914,88,113,101}
        }};
        for (int channel=0;channel<2;++channel) {
            const auto reduction=processor.meters[channel].load();
            if (std::abs(reduction-displayedReduction[channel])<0.01f) continue;
            displayedReduction[channel]=reduction;
            repaint(meterBounds[channel]);
        }
    }
    std::array<float,2> displayedReduction{};
    Compressor& processor;
    Face face;
    std::array<std::array<juce::Slider,6>,2> knobs;
    std::array<std::array<juce::ToggleButton,3>,2> switches;
    std::array<juce::ToggleButton,4> chassis;
    juce::ToggleButton noise;
    std::vector<std::unique_ptr<DialBinding>> dialBindings;
    std::vector<std::unique_ptr<ToggleBinding>> toggleBindings;
};
juce::AudioProcessorEditor* Compressor::createEditor() {return new Panel(*this);}
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {return new Compressor;}
