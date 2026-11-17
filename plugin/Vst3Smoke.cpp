#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../native/Client.hpp"
#include "../native/PlaybackClock.hpp"

#include <chrono>
#include <iostream>
#include <cmath>
#include <numbers>
#include <string_view>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

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
        mkii::native::Client reference;
        if (!reference.open(dll.toWideCharPointer()) || !reference.prepare(rate,layout.size(),n)) {
            std::cerr<<"Could not prepare the native runtime at "<<dll<<'\n';
            return 7;
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
        auto expected=input;
        if (!reference.process(expected.getArrayOfWritePointers(),expected.getNumSamples(),mkii::native::defaults,true) ||
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
        std::cout<<(offline?"Offline render, ":"Realtime playback, ")<<layout.size()<<" channels, 44.1 kHz / 512, peak error "<<peakError
                 <<", mismatched samples "<<differingSamples<<", longest callback "
                 <<peakCallback<<" ms\n";
        passed=!differingSamples && (offline || peakCallback<1000.0*n/rate) && passed;
    }
    return passed?0:10;
}
