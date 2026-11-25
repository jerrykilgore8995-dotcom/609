#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../AnalogOptoProcessor.hpp"
#if defined(NICKEL_OPTO_MKII)
#include "OptoOversampling.hpp"
#endif
#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

namespace {
#if defined(NICKEL_OPTO_MKII)
constexpr bool isMkII = true;
constexpr auto productName = "Nickel Opto Mk II";
constexpr auto panelName = "NICKEL   /   OPTO MK II";
#else
constexpr bool isMkII = false;
constexpr auto productName = "Nickel Opto";
constexpr auto panelName = "NICKEL   /   OPTO";
#endif
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
    add("attack","Opto Attack (ms)",10,100,15,.1f);
    add("release","Opto Release (ms)",50,2000,250,1);
    add("makeup","Makeup Gain",-12,18,0,.1f);
    add("output","Output Trim",-24,24,0,.1f);
    add("mix","Dry / Wet Mix (%)",0,100,100,.1f);
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
          juce::ParameterID{"mode",1},"Operating Mode",
          juce::StringArray{"Compressor","Limiter"},0));
    p.add(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{"cell",1},"Optical Cell",
          juce::StringArray{"NSL-32","VTL5C2","T4B-style","VTL5C1"},0));
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
    std::atomic<float> meterGr{0.f};
    // Read-only display telemetry; no change to the DSP signal path.
    std::atomic<float> inputPeak{0.f};
    std::array<std::atomic<float>,2> lowGr{{0.f,0.f}};
    juce::String const getName() const override {return productName;}
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
    void prepareToPlay(double rate,int maxSamples) override {
#if defined(NICKEL_OPTO_MKII)
        oversamplingBlockSize = std::size_t(std::max(1, maxSamples));
        oversampler = makeOptoOversampler(std::size_t(getTotalNumInputChannels()),
                                          oversamplingBlockSize);
        setLatencySamples(juce::roundToInt(oversampler->getLatencyInSamples()));
        rate *= double(oversampler->getOversamplingFactor());
#else
        juce::ignoreUnused(maxSamples);
#endif
        mono.prepare(rate,isMkII);stereo.prepare(rate,isMkII);
        if constexpr (isMkII) {
            inputGain.reset(rate,.01);
            makeupGain.reset(rate,.01);
            outputGain.reset(rate,.01);
            wetMix.reset(rate,.01);
            setGainTargets(true);
        }
    }
    void releaseResources() override {}
#if defined(NICKEL_OPTO_MKII)
    void processBlockBypassed(juce::AudioBuffer<float>& audio,juce::MidiBuffer&) override {
        juce::ScopedNoDenormals noDenormals;
        processOversampled(audio, true);
    }
#endif
    void processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer&) override {
        juce::ScopedNoDenormals noDenormals;
        const int samples = audio.getNumSamples();
        if (samples == 0 || audio.getNumChannels() == 0) return;
        float blockPeak = 0.f;
        for (int ch = 0; ch < audio.getNumChannels(); ++ch)
            blockPeak = std::max(blockPeak, audio.getMagnitude(ch, 0, samples));
        inputPeak.store(blockPeak,std::memory_order_relaxed);

#if defined(NICKEL_OPTO_MKII)
        processOversampled(audio, false);
#else
        processAudio(audio.getWritePointer(0),
                     isMono() ? nullptr : audio.getWritePointer(1), samples);
#endif
    }
private:
#if defined(NICKEL_OPTO_MKII)
    void processOversampled(juce::AudioBuffer<float>& audio, bool bypassed) {
        if (!oversampler) return;
        juce::dsp::AudioBlock<float> hostBlock(audio);
        for (std::size_t offset = 0; offset < hostBlock.getNumSamples();
              offset += oversamplingBlockSize) {
            auto part = hostBlock.getSubBlock(offset, std::min(oversamplingBlockSize,
                                                  hostBlock.getNumSamples() - offset));
            auto internal = oversampler->processSamplesUp(part);
            if (!bypassed)
                processAudio(internal.getChannelPointer(0),
                             isMono() ? nullptr : internal.getChannelPointer(1),
                             int(internal.getNumSamples()));
            oversampler->processSamplesDown(part);
        }
    }
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    std::size_t oversamplingBlockSize = 0;
#endif
    void processAudio(float* l, float* r, int n) {
        const auto v=[&](const char* name) { return value(name); };
        if constexpr (isMkII) setGainTargets(false);
        // Every GUI control affects the actual signal path, not a preset
        // abstraction or display-only parameter.
        const auto apply=[&](auto& dsp) {
            if constexpr (!isMkII) {
                dsp.setInputDb(v("drive"));
                dsp.setMakeupDb(v("makeup"));
                dsp.setOutputTrimDb(v("output"));
                dsp.setLinkedAutoGain(v("link")>.5);
            }
            dsp.setThresholdDb(v("threshold"));
            dsp.setFeedforward(v("ff")>.5);
            dsp.setLimiter(v("mode")>.5);
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
        const float wetAmount=juce::jlimit(0.f,1.f,float(v("mix")*.01));
        const auto mixForSample = [&](auto& dsp) {
            if constexpr (isMkII) {
                dsp.setLinearGains(inputGain.getNextValue(), makeupGain.getNextValue(),
                                   outputGain.getNextValue());
                return wetMix.getNextValue();
            }
            return wetAmount;
        };
        if(!r) {
            apply(mono);
            for(int i=0;i<n;++i) {
                const float mix=mixForSample(mono);
                const float dry=l[i];
                const float wet=mono.process(dry);
                l[i]=(1.f-mix)*dry+mix*wet;
            }
            gr[0].store(float(mono.opticalGr()),std::memory_order_relaxed);
            meterGr.store(float(mono.meterGr()),std::memory_order_relaxed);
            lowGr[0].store(float(mono.lowMidGr()),std::memory_order_relaxed);
            gr[1].store(0.f,std::memory_order_relaxed);
            lowGr[1].store(0.f,std::memory_order_relaxed);
        } else {
            apply(stereo);
            for(int i=0;i<n;++i) {
                const float mix=mixForSample(stereo);
                const float dryL=l[i],dryR=r[i];
                stereo.process(l[i],r[i]);
                l[i]=(1.f-mix)*dryL+mix*l[i];
                r[i]=(1.f-mix)*dryR+mix*r[i];
            }
            gr[0].store(float(stereo.grLeft()),std::memory_order_relaxed);
            gr[1].store(float(stereo.grRight()),std::memory_order_relaxed);
            meterGr.store(float(stereo.meterGr()),std::memory_order_relaxed);
            lowGr[0].store(float(stereo.lowGrLeft()),std::memory_order_relaxed);
            lowGr[1].store(float(stereo.lowGrRight()),std::memory_order_relaxed);
        }
    }
public:
    void getStateInformation(juce::MemoryBlock& data) override {
        if(auto xml=state.copyState().createXml())copyXmlToBinary(*xml,data);
    }
    void setStateInformation(const void* data,int size) override {
        if(auto xml=getXmlFromBinary(data,size);
           xml && xml->hasTagName(state.state.getType()))
            state.replaceState(juce::ValueTree::fromXml(*xml));
    }
private:
    float value(const char* id) const noexcept {
        return state.getRawParameterValue(id)->load(std::memory_order_relaxed);
    }
    void setGainTargets(bool initialize) noexcept {
        const auto set = [initialize](auto& ramp, float target) {
            if (initialize) ramp.setCurrentAndTargetValue(target);
            else ramp.setTargetValue(target);
        };
        const float drive = value("drive");
        const float trim = value("output") - (value("link") > .5f ? drive : 0.f);
        set(inputGain, juce::Decibels::decibelsToGain(drive));
        set(makeupGain, juce::Decibels::decibelsToGain(value("makeup")));
        set(outputGain, juce::Decibels::decibelsToGain(trim));
        set(wetMix, value("mix") * .01f);
    }
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>
        inputGain{1.f}, makeupGain{1.f}, outputGain{1.f};
    juce::SmoothedValue<float> wetMix{1.f};
    analogopto::MonoEngine mono;
    analogopto::StereoEngine stereo;
};

// Exact 609-width (1200 x 247), LA-2A-inspired 3U silverface layout.
// No Teletronix, Universal Audio, or meter trademark / brand artwork.
// All existing parameter IDs are retained for preset compatibility.
class BakeliteLook final : public juce::LookAndFeel_V4 {
public:
    void drawComboBox(juce::Graphics& g,int width,int height,bool,
                      int,int,int,int,juce::ComboBox& box) override {
        const auto metal=juce::Colour(0xff5b5a55);
        g.setColour(juce::Colour(0xff292a2b));
        g.fillRoundedRectangle(0.f,0.f,float(width),float(height),3.f);
        g.setColour(metal);
        g.drawRoundedRectangle(1.f,1.f,float(width-2),float(height-2),3.f,1.5f);
        // Use the same right-side drop-down indicator for mode and cell.
        // The former left-side toggle illustration collided with JUCE's
        // ComboBox text, obscuring the COMPRESS selection.
        juce::Path arrow;
        const float x=float(width)-16.f,y=float(height)*.5f;
        arrow.startNewSubPath(x-4.f,y-2.f);
        arrow.lineTo(x,y+2.f);arrow.lineTo(x+4.f,y-2.f);
        g.setColour(juce::Colour(0xffe0dacb));
        g.strokePath(arrow,juce::PathStrokeType(1.6f));
    }
    void drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,
                          float sliderPos,float start,float end,
                          juce::Slider& slider) override {
        const bool main=slider.getName()=="large";
        const auto rect=juce::Rectangle<float>(float(x),float(y),float(width),float(height));
        const float d=std::min(rect.getWidth(),rect.getHeight())*(main?.85f:.83f);
        const auto face=juce::Rectangle<float>(d,d).withCentre(rect.getCentre());
        const auto center=face.getCentre();
        const float radius=d*.5f,angle=start+sliderPos*(end-start);
        g.setColour(juce::Colour(0x77000000));
        g.fillEllipse(face.translated(2.5f,3.5f));
        g.setColour(juce::Colour(0xffaeaaa1));
        g.fillEllipse(face.expanded(2.f));
        juce::ColourGradient plastic(juce::Colour(0xff444445),
                         center.x-radius*.4f,center.y-radius*.9f,
                         juce::Colour(0xff090a0c),
                         center.x+radius*.65f,center.y+radius,true);
        g.setGradientFill(plastic);g.fillEllipse(face);
        g.setColour(juce::Colour(0xff18191b));
        g.drawEllipse(face.reduced(3.f),2.f);
        g.setColour(juce::Colour(0x448e8a83));
        g.drawEllipse(face.reduced(radius*.16f),1.5f);
        // Machined ivory pointer running radially along the top of the knob.
        const float r0=radius*.30f,r1=radius*.80f;
        juce::Path pointer;
        pointer.startNewSubPath(center.x+std::sin(angle)*r0,
                                center.y-std::cos(angle)*r0);
        pointer.lineTo(center.x+std::sin(angle)*r1,
                       center.y-std::cos(angle)*r1);
        g.setColour(juce::Colour(0xfff3e9d4));
        g.strokePath(pointer,juce::PathStrokeType(main?4.f:2.7f,
                      juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.setColour(juce::Colour(0xff161719));
        g.fillEllipse(juce::Rectangle<float>(radius*.19f,radius*.19f)
                        .withCentre(center));
    }
};
class OptionsPopup final : public juce::Component {
public:
    explicit OptionsPopup(NickelOptoProcessor& processor):p(processor) {
        createKnob("mix","DRY / WET",0,0);
        createKnob("hpfhz","S/C HPF Hz",1,0);
        createKnob("pmcenter","CENTER Hz",0,1);
        createKnob("pmwidth","WIDTH OCT",1,1);
        createKnob("pmrange","MAX CUT dB",2,1);
        createKnob("pmthreshold","BAND THR",3,1);
        createKnob("pmattack","BAND ATTACK",4,1);
        createKnob("pmrelease","BAND RELEASE",5,1);
        createToggle("hpf","SIDECHAIN HPF",0);
        createToggle("palm","LOW-MID ENABLE",1);
        createToggle("link","LINK INPUT/OUTPUT",2);
        createToggle("ff","FEEDFORWARD",3);
        load.addItem("600 OHM",1);load.addItem("10 kOHM",2);
        addAndMakeVisible(load);
        loadAttach=std::make_unique<ComboAttachment>(p.state,"load",load);
        setSize(850,330);
    }
    void resized() override {
        const int xs[]={18,155,292,429,566,703};
        const int ys[]={35,177};
        for(auto& k:knobs) {
            const int x=xs[k.column],y=ys[k.row];
            k.title->setBounds(x,y,127,20);
            k.slider->setBounds(x+26,y+20,78,64);
        }
        for(int i=0;i<int(toggles.size());++i)
            toggles[std::size_t(i)].button->setBounds(20+i*205,296,197,25);
        load.setBounds(575,75,235,28);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff24282d));
        g.setColour(juce::Colour(0xffd2ba8a));
        g.setFont(juce::FontOptions(16.f,juce::Font::bold));
        g.drawText("SIGNAL / SIDECHAIN",18,9,300,26,juce::Justification::left);
        g.drawText("C4-STYLE DYNAMIC LOW-MID",18,146,500,26,juce::Justification::left);
        g.setColour(juce::Colour(0xff6e7277));g.drawLine(15.f,139.f,835.f,139.f);
        g.setColour(juce::Colour(0xffd4d5d0));
        g.setFont(juce::FontOptions(11.f));
        g.drawText("OUTPUT LOAD",575,54,235,18,juce::Justification::centred);
    }
private:
    struct Knob {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> title;
        std::unique_ptr<SliderAttachment> attachment;
        int column=0,row=0;
    };
    struct Toggle {
        std::unique_ptr<juce::ToggleButton> button;
        std::unique_ptr<ButtonAttachment> attachment;
    };
    void createKnob(const char* key,const char* title,int column,int row) {
        Knob k;
        k.slider=std::make_unique<juce::Slider>();
        k.slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider->setTextBoxStyle(juce::Slider::TextBoxBelow,false,85,18);
        k.title=std::make_unique<juce::Label>();
        k.title->setText(title,juce::dontSendNotification);
        k.title->setColour(juce::Label::textColourId,juce::Colour(0xffe9e5dc));
        k.title->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*k.title);addAndMakeVisible(*k.slider);
        k.attachment=std::make_unique<SliderAttachment>(p.state,key,*k.slider);
        k.column=column;k.row=row;knobs.push_back(std::move(k));
    }
    void createToggle(const char* key,const char* name,int index) {
        Toggle t;t.button=std::make_unique<juce::ToggleButton>(name);
        t.button->setColour(juce::ToggleButton::textColourId,
                            juce::Colour(0xffe9e5dc));
        addAndMakeVisible(*t.button);
        t.attachment=std::make_unique<ButtonAttachment>(p.state,key,*t.button);
        toggles.push_back(std::move(t));
        juce::ignoreUnused(index);
    }
    NickelOptoProcessor& p;
    std::vector<Knob> knobs;
    std::vector<Toggle> toggles;
    juce::ComboBox load;
    std::unique_ptr<ComboAttachment> loadAttach;
};
class HardwarePanel final : public juce::Component,private juce::Timer {
public:
    explicit HardwarePanel(NickelOptoProcessor& processor):p(processor) {
        addRotary(input,"drive",false);
        addRotary(output,"output",false);
        addRotary(gain,"makeup",true);
        addRotary(peak,"threshold",true); // reverse-mapped to hardware Peak Reduction
        addRotary(attack,"attack",false);
        addRotary(release,"release",false);
        cellModel.addItem("NSL-32",1);
        cellModel.addItem("VTL5C2",2);
        cellModel.addItem("T4B-STYLE",3);
        cellModel.addItem("VTL5C1",4);
        addAndMakeVisible(cellModel);
        cellModel.setLookAndFeel(&knobLook);
        cellModel.setColour(juce::ComboBox::textColourId,juce::Colour(0xfff3eee2));
        cellAttach=std::make_unique<ComboAttachment>(p.state,"cell",cellModel);
        mode.addItem("COMPRESS",1);
        mode.addItem("LIMIT",2);
        addAndMakeVisible(mode);
        mode.setLookAndFeel(&knobLook);
        mode.setColour(juce::ComboBox::textColourId,juce::Colour(0xfff3eee2));
        modeAttach=std::make_unique<ComboAttachment>(p.state,"mode",mode);
        advanced.setButtonText("ADVANCED");
        advanced.onClick=[this] {
            juce::CallOutBox::launchAsynchronously(
                std::make_unique<OptionsPopup>(p),
                advanced.getScreenBounds(),nullptr);
        };
        addAndMakeVisible(advanced);
        startTimerHz(25);
        updateCellVisibility();
    }
    ~HardwarePanel() override {
        input.setLookAndFeel(nullptr);output.setLookAndFeel(nullptr);
        gain.setLookAndFeel(nullptr);peak.setLookAndFeel(nullptr);
        attack.setLookAndFeel(nullptr);release.setLookAndFeel(nullptr);
        mode.setLookAndFeel(nullptr);cellModel.setLookAndFeel(nullptr);
    }
    void resized() override {
        input.setBounds(363,3,58,50);
        output.setBounds(1010,3,58,50);
        gain.setBounds(266,95,126,126);
        peak.setBounds(763,95,126,126);
        mode.setBounds(47,161,165,28);
        cellModel.setBounds(971,96,182,29);
        attack.setBounds(972,153,84,81);
        release.setBounds(1073,153,84,81);
        advanced.setBounds(505,11,170,29);
    }
    void paint(juce::Graphics& g) override {
        const auto ink=juce::Colour(0xff383530);
        // Dark metal top rail deliberately OUTSIDE the classic silverface.
        g.fillAll(juce::Colour(0xff121417));
        g.setColour(juce::Colour(0xffc7b69a));
        g.setFont(juce::FontOptions(11.f,juce::Font::bold));
        g.drawText("INPUT DRIVE",256,16,102,20,juce::Justification::right);
        g.drawText("OUTPUT TRIM",1071,16,111,20,juce::Justification::left);
        g.setFont(juce::FontOptions(13.f,juce::Font::bold));
        g.drawText(panelName,22,10,240,24,juce::Justification::left);
        // Two hardware-style LED bargraphs on the top rail: input peak and optical GR.
        const auto drawBar=[&](const char* title,int y,float fraction,bool grMeter) {
            g.setFont(juce::FontOptions(10.f,juce::Font::bold));
            g.setColour(juce::Colour(0xffcfccc0));
            g.drawText(title,697,y-1,42,14,juce::Justification::centredRight);
            constexpr int segments=18;
            const int lit=juce::jlimit(0,segments,int(std::ceil(fraction*segments)));
            for(int i=0;i<segments;++i) {
                const float x=744.f+float(i)*9.3f;
                const juce::Colour active=i>=16?juce::Colour(0xffe13e32):
                     i>=12?juce::Colour(0xffffcf40):juce::Colour(0xff45d56f);
                g.setColour(i<lit?active:juce::Colour(0xff2b3430));
                g.fillRoundedRectangle(x,float(y),6.8f,9.f,1.1f);
            }
            g.setFont(juce::FontOptions(9.f));
            g.setColour(juce::Colour(0xffb7b6ac));
            const auto value=grMeter
                ?juce::String(juce::jlimit(0.f,20.f,displayGr),1)+" dB"
                :juce::String(juce::Decibels::gainToDecibels(displayInput,-60.f),1)+" dBFS";
            g.drawText(value,919,y-2,83,14,juce::Justification::centredLeft);
        };
        drawBar("INPUT",11,juce::jlimit(0.f,1.f,
            (juce::Decibels::gainToDecibels(displayInput,-60.f)+60.f)/60.f),false);
        drawBar("GR",31,juce::jlimit(0.f,1.f,displayGr/20.f),true);
        g.setColour(juce::Colour(0xff73777c));g.drawLine(0.f,52.f,1200.f,52.f);
        // Brushed silver three-rack-unit style faceplate (no borrowed logos).
        juce::ColourGradient metal(juce::Colour(0xffe9e7df),0.f,55.f,
                                   juce::Colour(0xffbdbbb4),0.f,246.f,false);
        g.setGradientFill(metal);
        g.fillRect(juce::Rectangle<int>(15,56,1170,190));
        g.setColour(juce::Colour(0xff99978f));g.drawRect(15,56,1170,190,1);
        for(int i=0;i<7;++i){
            g.setColour(juce::Colour(0x0a383736));
            g.drawHorizontalLine(62+i*27,16.f,1184.f);
        }
        auto screw=[&](float x,float y) {
            g.setColour(juce::Colour(0xff777872));g.fillEllipse(x-5,y-5,10,10);
            g.setColour(juce::Colour(0xffd9d8ce));g.drawEllipse(x-5,y-5,10,10,1);
            g.setColour(juce::Colour(0xff4b4d4a));g.drawLine(x-2,y+2,x+2,y-2,1);
        };
        for(float x:{28.f,1172.f})for(float y:{69.f,232.f})screw(x,y);
        g.setColour(juce::Colour(0xff803e3e));
        g.setFont(juce::FontOptions(22.f,juce::Font::bold));
        g.drawText("NICKEL",49,82,176,29,juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(11.f));
        g.drawText("OPTICAL LEVELING AMPLIFIER",48,112,220,19,
                    juce::Justification::centredLeft);
        g.setColour(ink);g.setFont(juce::FontOptions(12.f,juce::Font::bold));
        g.drawText("GAIN",271,220,116,18,juce::Justification::centred);
        g.drawText("PEAK REDUCTION",755,220,140,18,juce::Justification::centred);
        // Printed radial dials on the brushed-metal faceplate, separate
        // from the moving Bakelite knobs, like a physical hardware legend.
        for(float cx:{329.f,826.f}) {
            for(int tick=0;tick<=10;++tick) {
                const float angle=juce::degreesToRadians(225.f+270.f*tick/10.f);
                auto at=[&](float radius) {
                    return juce::Point<float>(cx+std::sin(angle)*radius,
                                             158.f-std::cos(angle)*radius);
                };
                const auto near=at(57.f),far=at(tick%5==0?66.f:62.f);
                g.setColour(juce::Colour(0xff555049));
                g.drawLine(near.x,near.y,far.x,far.y,tick%5==0?1.9f:1.f);
            }
        }
        g.setFont(juce::FontOptions(10.f,juce::Font::bold));
        g.setColour(ink);
        g.drawText("0",254,202,26,17,juce::Justification::centred);
        g.drawText("100",378,202,38,17,juce::Justification::centred);
        g.drawText("0",747,202,26,17,juce::Justification::centred);
        g.drawText("100",874,202,38,17,juce::Justification::centred);
        g.setFont(juce::FontOptions(12.f,juce::Font::bold));
        g.drawText("COMPRESS / LIMIT",49,139,165,19,juce::Justification::centred);
        g.drawText("OPTO CELL",970,75,182,17,juce::Justification::centred);
        if(attack.isVisible()) {
            g.drawText("ATTACK",973,137,80,17,juce::Justification::centred);
            g.drawText("RELEASE",1072,137,86,17,juce::Justification::centred);
        } else {
            g.setFont(juce::FontOptions(11.f,juce::Font::italic));
            g.drawText("FIXED PHOTO-OPTICAL TIMING",967,162,198,48,
                       juce::Justification::centred);
        }
        // Classic backlit VU meter with needle calibrated for 0-20 dB GR.
        juce::Rectangle<float> bezel(429.f,74.f,305.f,155.f);
        g.setColour(juce::Colour(0xff292725));g.fillRoundedRectangle(bezel,4.f);
        g.setColour(juce::Colour(0xff76716b));g.drawRoundedRectangle(bezel,4.f,3.f);
        auto glass=bezel.reduced(9.f);
        juce::ColourGradient lamp(juce::Colour(0xfff4dda0),glass.getCentreX(),
                                  glass.getY(),juce::Colour(0xffb88f4f),
                                  glass.getCentreX(),glass.getBottom(),false);
        g.setGradientFill(lamp);g.fillRoundedRectangle(glass,3.f);
        const auto meterCenter=juce::Point<float>(glass.getCentreX(),glass.getBottom()+18.f);
        g.setColour(juce::Colour(0xff625137));
        for(int i=0;i<=20;++i) {
            const float theta=juce::degreesToRadians(200.f+140.f*i/20.f);
            auto point=[&](float rad) {
                return juce::Point<float>(meterCenter.x+std::cos(theta)*rad,
                                           meterCenter.y+std::sin(theta)*rad);
            };
            // Scale lowered and shortened to make the printed dB numbers readable.
            const auto a=point(129.f),b=point(i%2==0?118.f:123.f);
            g.drawLine(a.x,a.y,b.x,b.y,i%2==0?1.7f:1.f);
            if(i%4==0) {
                const auto labelPoint=point(106.f);
                g.setFont(juce::FontOptions(10.f,juce::Font::bold));
                g.setColour(juce::Colour(0xff4d3c28));
                g.drawText(juce::String(i),int(labelPoint.x)-13,
                           int(labelPoint.y)-7,26,14,juce::Justification::centred);
                g.setColour(juce::Colour(0xff625137));
            }
        }
        g.setFont(juce::FontOptions(10.f,juce::Font::bold));
        g.setColour(juce::Colour(0xff554630));
        g.drawText("GAIN REDUCTION  (dB)",int(glass.getX()+65.f),
                   int(glass.getBottom()-20.f),int(glass.getWidth()-130.f),16,
                   juce::Justification::centred);
        const float reduction=juce::jlimit(0.f,20.f,p.meterGr.load(std::memory_order_relaxed));
        const float theta=juce::degreesToRadians(200.f+140.f*reduction/20.f);
        const juce::Point<float> tip(meterCenter.x+std::cos(theta)*113.f,
                                     meterCenter.y+std::sin(theta)*113.f);
        g.setColour(juce::Colour(0xff25221d));g.drawLine(meterCenter.x,
                    meterCenter.y,tip.x,tip.y,2.4f);
        g.fillEllipse(meterCenter.x-5,meterCenter.y-5,10,10);
    }
private:
    void addRotary(juce::Slider& slider,const char* param,bool large) {
        slider.setName(large?"large":"small");
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        if(&slider==&attack || &slider==&release)
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,70,17);
        else
            slider.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        slider.setLookAndFeel(&knobLook);
        slider.setRotaryParameters(juce::degreesToRadians(225.f),
                                   juce::degreesToRadians(495.f),true);
        addAndMakeVisible(slider);
        if(&slider==&peak) {
            // Physical LA-2A-style clockwise Peak Reduction increases GR.
            // Preserve the underlying "threshold" automation ID and its
            // -45..+18 dBu units; only invert the front-panel knob scale.
            slider.setRange(0.,100.,.1);
            peakAttach=std::make_unique<juce::ParameterAttachment>(
                *p.state.getParameter("threshold"),
                [this](float threshold) {
                    peak.setValue((18.-double(threshold))*100./63.,
                                  juce::dontSendNotification);
                },nullptr);
            slider.onDragStart=[this] {if(peakAttach)peakAttach->beginGesture();};
            slider.onValueChange=[this] {
                if(peakAttach)peakAttach->setValueAsPartOfGesture(
                    float(18.-peak.getValue()*.63));
            };
            slider.onDragEnd=[this] {if(peakAttach)peakAttach->endGesture();};
            peakAttach->sendInitialUpdate();
        } else {
            knobAttachments.push_back(
                std::make_unique<SliderAttachment>(p.state,param,slider));
        }
    }
    void updateCellVisibility() {
        const auto* val=p.state.getRawParameterValue("cell");
        const bool variable=val && int(val->load())==3;
        attack.setVisible(variable);release.setVisible(variable);
        attack.setEnabled(variable);release.setEnabled(variable);
    }
    void timerCallback() override {
        updateCellVisibility();
        // Smooth visuals only; no alteration of compression timing or audio.
        const float peak=p.inputPeak.load(std::memory_order_relaxed);
        displayInput=std::max(peak,displayInput*.88f);
        const float grNow=std::max(p.gr[0].load(std::memory_order_relaxed),
                                   p.gr[1].load(std::memory_order_relaxed));
        displayGr=std::max(grNow,displayGr*.83f);
        repaint(430,76,302,152);
        repaint(690,3,370,47);
    }
    NickelOptoProcessor& p;
    float displayInput=0.f,displayGr=0.f;
    BakeliteLook knobLook;
    juce::Slider input,output,gain,peak,attack,release;
    std::vector<std::unique_ptr<SliderAttachment>> knobAttachments;
    std::unique_ptr<juce::ParameterAttachment> peakAttach;
    juce::ComboBox mode,cellModel;
    juce::TextButton advanced;
    std::unique_ptr<ComboAttachment> modeAttach,cellAttach;
};
class NickelOptoEditor final : public juce::AudioProcessorEditor {
public:
    explicit NickelOptoEditor(NickelOptoProcessor& p):
        juce::AudioProcessorEditor(p),panel(std::make_unique<HardwarePanel>(p)) {
        addAndMakeVisible(*panel);
        setResizable(false,false);
        setSize(1200,247); // same window size as the 609 MkV plugin
    }
    void resized() override {panel->setBounds(getLocalBounds());}
private:
    std::unique_ptr<HardwarePanel> panel;
};
juce::AudioProcessorEditor* NickelOptoProcessor::createEditor() {
    return new NickelOptoEditor(*this);
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new NickelOptoProcessor();
}
