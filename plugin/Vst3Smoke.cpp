#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../native/Client.hpp"
#include "../native/PlaybackClock.hpp"

#include <array>
#include <chrono>
#include <iostream>
#include <cmath>
#include <numbers>
#include <string_view>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace {
bool checkDelayedDry(const juce::String& dll) {
    constexpr int rate=44100,blockSize=512,blocks=192;
    mkii::native::Client reference;
    if (!reference.open(dll.toWideCharPointer()) || !reference.prepare(rate,2,blockSize)) return false;
    const auto latency=reference.latency();
    const auto sample=[](int frame,int channel) {
        return frame<0?0.0f:0.0001f*float((frame*(17+channel*4))%1021-510);
    };
    juce::AudioBuffer<float> block(2,blockSize);
    int overruns=0;
    for (int frame=0;frame<blocks*blockSize;frame+=blockSize) {
        for (int channel=0;channel<2;++channel)
            for (int i=0;i<blockSize;++i)
                block.setSample(channel,i,sample(frame+i,channel));
        if (!reference.process(block.getArrayOfWritePointers(),blockSize,mkii::native::defaults,false,true))
            ++overruns;
        for (int channel=0;channel<2;++channel)
            for (int i=0;i<blockSize;++i)
                if (block.getSample(channel,i)!=sample(frame+i-latency,channel)) return false;
    }
    std::cout<<"Delayed dry output stayed aligned across "<<overruns<<" overloaded callbacks\n";
    return true;
}

bool setControl(juce::AudioProcessor& plugin,const juce::String& name,float value) {
    for (auto* parameter:plugin.getParameters()) {
        if (parameter->getName(100)!=name) continue;
        parameter->setValueNotifyingHost(value);
        return true;
    }
    std::cerr<<"VST3 parameter not found: "<<name<<'\n';
    return false;
}
}

int main(int argc,char** argv) {
    std::cerr<<std::unitbuf;
    const bool offline=argc==3 && std::string_view(argv[2])=="--offline";
    if (argc!=2 && !offline) {
        std::cerr<<"Expected one VST3 bundle argument\n";
        return 2;
    }
    std::cerr<<"Initializing the VST3 host\n";
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File bundle(juce::String::fromUTF8(argv[1]));
    const auto dll=bundle.getChildFile("Contents").getChildFile("Resources")
        .getChildFile("609-native.dll").getFullPathName();
    juce::AudioPluginFormatManager formats;
    auto format=std::make_unique<juce::VST3PluginFormat>();
    juce::OwnedArray<juce::PluginDescription> descriptions;
    std::cerr<<"Scanning "<<bundle.getFullPathName()<<'\n';
    format->findAllTypesForFile(descriptions,bundle.getFullPathName());
    if (descriptions.size()!=1) {
        std::cerr<<"Found "<<descriptions.size()<<" plug-in classes\n";
        return 3;
    }
    formats.addFormat(format.release());
    juce::String error;
    std::cerr<<"Instantiating the VST3\n";
    auto vst=formats.createPluginInstance(*descriptions[0],44100,512,error);
    if (!vst) {
        std::cerr<<error<<'\n';
        return 4;
    }
    {
        std::cerr<<"Rendering the editor\n";
        std::unique_ptr<juce::AudioProcessor> processor(createPluginFilter());
        std::unique_ptr<juce::AudioProcessorEditor> panel(processor->createEditorIfNeeded());
        if (!panel) return 5;
        const auto image=panel->createComponentSnapshot(panel->getLocalBounds(),true,2);
        const auto path=juce::File::getCurrentWorkingDirectory().getChildFile("609-faceplate.png");
        juce::FileOutputStream png(path);
        if (!png.openedOk() || !juce::PNGImageFormat().writeImageToStream(image,png)) return 5;
    }
    if (offline && !checkDelayedDry(dll)) {
        std::cerr<<"The delayed dry signal drifted during queue overload\n";
        return 12;
    }
    constexpr int rate=44100,n=512,blocks=(6*rate/n);
    bool passed=true;
    for (const auto layout : {juce::AudioChannelSet::mono(),juce::AudioChannelSet::stereo()}) {
        juce::AudioProcessor::BusesLayout buses;
        buses.inputBuses.add(layout);
        buses.outputBuses.add(layout);
        std::cerr<<"Preparing "<<layout.size()<<" channels\n";
        if (!vst->setBusesLayout(buses)) {
            std::cerr<<"VST3 rejected the bus layout\n";
            return 6;
        }
        juce::AudioBuffer<float> input(layout.size(),blocks*n);
        for (int c=0;c<layout.size();++c) {
            for (int i=0;i<blocks*n;++i) {
                const double t=double(i)/rate;
                const double w=2*std::numbers::pi*t;
                const double attack=std::exp(-15*std::fmod(t,0.22));
                input.setSample(c,i,static_cast<float>(
                    0.06*std::sin(78*w+c)+0.05*std::sin(847*w)+attack*
                    (0.15*std::sin(7479*w+c)+0.08*std::sin(15601*w))));
            }
        }
        const std::array<const char*,3> caseNames{"through","linked compressor","linked limiter"};
        const int caseCount=layout.size()==2?3:1;
        for (int testCase=0;testCase<caseCount;++testCase) {
            auto settings=mkii::native::defaults;
            if (layout.size()==2) {
                if (!setControl(*vst,"Stereo Link",testCase!=0?1.0f:0.0f)) return 11;
                settings[19]=testCase!=0;
                for (int ch=0;ch<2;++ch) {
                    const auto prefix=juce::String("Channel ")+juce::String(ch+1)+" ";
                    const bool compress=testCase==1;
                    const bool limit=testCase==2;
                    if (!setControl(*vst,prefix+"Compress In",compress?1.0f:0.0f) ||
                        !setControl(*vst,prefix+"Limit In",limit?1.0f:0.0f) ||
                        !setControl(*vst,prefix+"Compress Ratio",compress?0.75f:0.0f)) return 11;
                    settings[ch*9+1]=compress?4:1;
                    settings[ch*9+6]=compress;
                    settings[ch*9+7]=limit;
                }
            }
            mkii::native::Client reference;
            if (!reference.open(dll.toWideCharPointer()) || !reference.prepare(rate,layout.size(),n)) {
                std::cerr<<"Could not prepare the native runtime at "<<dll<<'\n';
                return 7;
            }
            auto expected=input;
            if (!reference.process(expected.getArrayOfWritePointers(),expected.getNumSamples(),settings,true) ||
                reference.failedSolves()!=0) {
                std::cerr<<"Unpaced render failed. Solver failures: "<<reference.failedSolves()<<'\n';
                return 8;
            }
            reference.stop();
            vst->setNonRealtime(offline);
            vst->prepareToPlay(rate,n);
            if (vst->getLatencySamples()!=reference.latency()) {
                std::cerr<<"VST3 latency "<<vst->getLatencySamples()
                         <<" differs from runtime latency "<<reference.latency()<<'\n';
                return 9;
            }
            juce::AudioBuffer<float> block(layout.size(),n);
            juce::MidiBuffer midi;
            mkii::native::PlaybackClock clock;
            using Clock=mkii::native::PlaybackClock::Clock;
            const auto start=Clock::now();
            unsigned differingSamples=0;
            double peakError=0,peakCallback=0;
            for (int frame=0;frame<blocks*n;frame+=n) {
                for (int c=0;c<layout.size();++c) block.copyFrom(c,0,input,c,frame,n);
                if (!offline) clock.waitUntil(start+std::chrono::duration_cast<Clock::duration>(
                    std::chrono::duration<double>(double(frame)/rate)));
                const auto before=Clock::now();
                vst->processBlock(block,midi);
                peakCallback=std::max(peakCallback,std::chrono::duration<double,std::milli>(Clock::now()-before).count());
                for (int c=0;c<layout.size();++c) {
                    for (int i=0;i<n;++i) {
                        const double value=block.getSample(c,i);
                        const double difference=std::abs(value-expected.getSample(c,frame+i));
                        if (!std::isfinite(value) || difference>2e-5) ++differingSamples;
                        peakError=std::max(peakError,difference);
                    }
                }
            }
            vst->releaseResources();
            std::cout<<(offline?"Offline render, ":"Realtime playback, ")
                     <<caseNames[testCase]<<", "<<layout.size()<<" channels, 44.1 kHz / 512, peak error "<<peakError
                     <<", mismatched samples "<<differingSamples<<", longest callback "
                     <<peakCallback<<" ms\n";
            passed=!differingSamples && (offline || peakCallback<1000.0*n/rate) && passed;
        }
    }
    return passed?0:10;
}
