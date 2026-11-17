#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../native/Realtime.hpp"
#include "../native/PlaybackClock.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <complex>
#include <iostream>
#include <cmath>
#include <numbers>
#include <string_view>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace {
bool setControl(juce::AudioProcessor& plugin,const juce::String& name,float value) {
    for (auto* parameter:plugin.getParameters()) {
        if (parameter->getName(100)!=name) continue;
        parameter->setValueNotifyingHost(value);
        return true;
    }
    std::cerr<<"VST3 parameter not found: "<<name<<'\n';
    return false;
}

bool checkSupplyNoise(juce::AudioProcessor& plugin) {
    constexpr int rate=44100,blockSize=512,settle=8192;
    constexpr int fft=4096,pinkBin=93;
    constexpr double tau=2*std::numbers::pi;
    if (!setControl(plugin,"Stereo Link",0)) return false;
    for (int channel=1;channel<=2;++channel) {
        const auto prefix="Channel "+juce::String(channel)+" ";
        if (!setControl(plugin,prefix+"Limit In",0) || !setControl(plugin,prefix+"Compress In",0)) return false;
    }

    const auto render=[&](bool enabled,std::array<double,rate>& samples) {
        if (!setControl(plugin,"PSU Noise",enabled?1.0f:0.0f)) return false;
        plugin.prepareToPlay(rate,blockSize);
        juce::AudioBuffer<float> block(2,blockSize);
        juce::MidiBuffer midi;
        for (int frame=0;frame<settle+rate;frame+=blockSize) {
            block.clear();
            plugin.processBlock(block,midi);
            for (int i=0;i<blockSize && frame+i<settle+rate;++i)
                if (frame+i>=settle) samples[frame+i-settle]=block.getSample(0,i);
        }
        plugin.releaseResources();
        return true;
    };
    auto dry=std::make_unique<std::array<double,rate>>();
    auto wet=std::make_unique<std::array<double,rate>>();
    if (!render(false,*dry) || !render(true,*wet)) return false;
    for (int i=0;i<rate;++i) (*wet)[i]-=(*dry)[i];

    const auto humLevel=[&](int frequency) {
        std::complex<double> sum{};
        for (int i=0;i<rate;++i)
            sum+=(*wet)[i]*std::polar(1.0,-tau*frequency*i/rate);
        return 20*std::log10(2*std::abs(sum)/rate);
    };
    double binPower=0;
    for (int block=0;block<rate/fft;++block) {
        std::complex<double> sum{};
        for (int i=0;i<fft;++i) {
            const double angle=tau*i/fft;
            const double hann=(1-std::cos(angle))*.5;
            sum+=(*wet)[block*fft+i]*std::polar(hann,-pinkBin*angle);
        }
        binPower+=std::norm(sum*(4.0/fft));
    }
    const auto pink=10*std::log10(binPower/(rate/fft));
    const auto hum60=humLevel(60),hum120=humLevel(120);
    std::cout<<"PSU noise at 1 kHz "<<pink<<" dBFS, 60 Hz "<<hum60<<" dBFS, 120 Hz "<<hum120<<" dBFS\n";
    return std::abs(pink+96)<3.5 && std::abs(hum60+78)<.6 && std::abs(hum120+86)<.7;
}
std::array<std::complex<double>,20> measureSpectrum(juce::AudioProcessor& plugin,double frequency,double dbu) {
    constexpr int rate=44100,blockSize=512,duration=26460,window=8820;
    const double omega=2*std::numbers::pi*frequency/rate;
    const double level=std::pow(10.,(dbu-24.)/20);
    juce::AudioBuffer<float> block(2,blockSize);
    juce::MidiBuffer midi;
    plugin.prepareToPlay(rate,blockSize);
    std::array<std::complex<double>,20> spectrum{};
    for (int frame=0;frame<duration;frame+=blockSize) {
        for (int i=0;i<blockSize;++i) {
            const auto x=static_cast<float>(level*std::sin(omega*(frame+i)));
            block.setSample(0,i,x);
            block.setSample(1,i,x);
        }
        plugin.processBlock(block,midi);
        for (int i=0;i<blockSize && frame+i<duration;++i) {
            if (frame+i<duration-window) continue;
            const auto y=static_cast<double>(block.getSample(0,i));
            const auto root=std::polar(1.,-omega*(frame+i));
            auto oscillator=root;
            for (auto& bin:spectrum) {bin+=y*oscillator;oscillator*=root;}
        }
    }
    plugin.releaseResources();
    return spectrum;
}

bool checkRatioThd(juce::AudioProcessor& plugin) {
    if (!setControl(plugin,"Stereo Link",0) || !setControl(plugin,"PSU Noise",0)) return false;
    bool valid=true;
    for (int detent=0;detent<5;++detent) {
        for (int channel=1;channel<=2;++channel) {
            const auto prefix="Channel "+juce::String(channel)+" ";
            if (!setControl(plugin,prefix+"Compress In",1) ||
                !setControl(plugin,prefix+"Limit In",0) ||
                !setControl(plugin,prefix+"Compress Threshold",10.f/15.f) ||
                !setControl(plugin,prefix+"Compress Ratio",detent/4.f) ||
                !setControl(plugin,prefix+"Compress Gain",0) ||
                !setControl(plugin,prefix+"Compress Recovery",0)) return false;
        }
        const auto spectrum=measureSpectrum(plugin,1000,10);
        double distortion=0;
        for (std::size_t k=1;k<spectrum.size();++k) distortion+=std::norm(spectrum[k]);
        const auto thd=100*std::sqrt(distortion)/std::abs(spectrum[0]);
        std::cout<<"Packaged ratio THD "<<detent+1<<" "<<thd<<'\n';
        valid=std::isfinite(thd) && thd>0 && valid;
    }
    return valid;
}

bool checkLimiterEven(juce::AudioProcessor& plugin) {
    for (int channel=1;channel<=2;++channel) {
        const auto prefix="Channel "+juce::String(channel)+" ";
        if (!setControl(plugin,prefix+"Compress In",0) ||
            !setControl(plugin,prefix+"Limit In",1) ||
            !setControl(plugin,prefix+"Limit Threshold",8.f/22.f) ||
            !setControl(plugin,prefix+"Limit Recovery",0) ||
            !setControl(plugin,prefix+"Fast Attack",0)) return false;
    }
    for (int frequency:{65,110,250}) {
        const auto spectrum=measureSpectrum(plugin,frequency,20);
        for (int k=2;k<=20;k+=2) {
            const double db=20*std::log10(std::abs(spectrum[k-1]/spectrum[0]));
            if (!std::isfinite(db)) return false;
            std::cout<<"Packaged limiter even "<<frequency<<" "<<k<<" "<<db<<'\n';
        }
    }
    return true;
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
        juce::AudioBuffer<float> input(layout.size(),blocks*n);
        for (int c=0;c<layout.size();++c) {
            unsigned rng=912+c;
            for (int i=0;i<blocks*n;++i) {
                const double t=double(i)/rate;
                const double w=2*std::numbers::pi*t;
                const double attack=std::exp(-15*std::fmod(t,0.22));
                rng^=rng<<13;
                rng^=rng>>17;
                rng^=rng<<5;
                const double noisyBass=std::exp(-12*std::fmod(t+.015*c,.19))*
                    (.23*std::sin(83*w+c)+.04*std::sin(1831*w)+.05*(double(rng)/2147483648.-1));
                const double cymbal=.06*std::sin(78*w+c)+.05*std::sin(847*w)+
                    attack*(.15*std::sin(7479*w+c)+.08*std::sin(15601*w));
                const double sustained=t<4.1?0:.6309573444801932*std::sin(2*std::numbers::pi*110*(t-4.1));
                input.setSample(c,i,static_cast<float>(t<2?cymbal:t<4?noisyBass:sustained));
            }
        }
        const std::array<const char*,4> caseNames{"through","linked compressor","linked limiter","linked compressor and limiter"};
        const int caseCount=layout.size()==2?4:1;
        for (int testCase=0;testCase<caseCount;++testCase) {
            auto settings=mkii::native::defaults;
            if (layout.size()==2) {
                if (!setControl(*vst,"Stereo Link",testCase!=0?1.0f:0.0f)) return 11;
                settings[19]=testCase!=0;
                for (int ch=0;ch<2;++ch) {
                    const auto prefix=juce::String("Channel ")+juce::String(ch+1)+" ";
                    const bool combined=testCase==3;
                    const bool compress=testCase==1 || combined;
                    const bool limit=testCase==2 || combined;
                    if (!setControl(*vst,prefix+"Compress In",compress?1.0f:0.0f) ||
                        !setControl(*vst,prefix+"Limit In",limit?1.0f:0.0f) ||
                        !setControl(*vst,prefix+"Compress Ratio",compress?0.75f:0.0f) ||
                        !setControl(*vst,prefix+"Compress Threshold",combined?10.f/15.f:0.f) ||
                        !setControl(*vst,prefix+"Compress Gain",combined?.2f:0.f)) return 11;
                    settings[ch*9]=combined?11:1;
                    settings[ch*9+1]=compress?4:1;
                    settings[ch*9+3]=combined?3:1;
                    settings[ch*9+6]=compress;
                    settings[ch*9+7]=limit;
                }
            }
            mkii::native::Realtime reference;
            reference.prepare(rate);
            reference.control(settings);
            auto expected=input;
            for (int frame=0;frame<input.getNumSamples();++frame) {
                const auto y=reference.step({input.getSample(0,frame),layout.size()==2?input.getSample(1,frame):0.0});
                for (int c=0;c<layout.size();++c) expected.setSample(c,frame,static_cast<float>(y[c]));
            }
            vst->setNonRealtime(offline);
            vst->prepareToPlay(rate,n);
            if (vst->getLatencySamples()!=0) {
                std::cerr<<"The realtime path should report zero added samples of latency\n";
                return 9;
            }
            juce::AudioBuffer<float> block(layout.size(),n);
            juce::MidiBuffer midi;
            mkii::native::PlaybackClock clock;
            using Clock=mkii::native::PlaybackClock::Clock;
            const auto start=Clock::now();
            unsigned differingSamples=0,missedDeadlines=0;
            double peakError=0;
            std::array<double,blocks> callbackTimes{};
            const auto timeForFrame=[&](int frame) {
                return start+std::chrono::duration_cast<Clock::duration>(
                    std::chrono::duration<double>(double(frame)/rate));
            };
            for (int frame=0;frame<blocks*n;frame+=n) {
                for (int c=0;c<layout.size();++c) block.copyFrom(c,0,input,c,frame,n);
                if (!offline) clock.waitUntil(timeForFrame(frame));
                const auto before=Clock::now();
                vst->processBlock(block,midi);
                const auto done=Clock::now();
                callbackTimes[frame/n]=std::chrono::duration<double,std::milli>(done-before).count();
                if (!offline && done>timeForFrame(frame+n)) ++missedDeadlines;
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
            std::sort(callbackTimes.begin(),callbackTimes.end());
            const auto p99=callbackTimes[(99*blocks+99)/100-1];
            const auto peakCallback=callbackTimes.back();
            std::cout<<(offline?"Offline render, ":"Realtime playback, ")
                     <<caseNames[testCase]<<", "<<layout.size()<<" channels, 44.1 kHz / 512, peak error "<<peakError
                     <<", mismatched samples "<<differingSamples<<", missed deadlines "<<missedDeadlines
                     <<", callback p99 "<<p99<<" ms, longest callback "<<peakCallback<<" ms\n";
            passed=!differingSamples && !missedDeadlines && (offline || peakCallback<1000.0*n/rate) && passed;
        }
    }
    if (offline && !checkSupplyNoise(*vst)) {
        std::cerr<<"PSU noise spectrum outside calibration tolerance\n";
        return 13;
    }
    if (offline && !(checkRatioThd(*vst) && checkLimiterEven(*vst))) return 12;
    return passed?0:10;
}
