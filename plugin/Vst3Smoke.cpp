#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Presets.hpp"
#include "../native/Api.hpp"
#include "../native/Circuit.hpp"
#include "../native/BandlimitedHalf.hpp"
#include "../native/DynamicsProbe.hpp"
#include "../native/PlaybackClock.hpp"

#include <algorithm>
#include <array>
#include <vector>
#include <thread>
#include <chrono>
#include <complex>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <numbers>
#include <string>
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

bool checkPresets(juce::AudioProcessor& plugin, juce::AudioProcessorEditor& editor) {
    juce::ComboBox* box=nullptr;
    for (auto* child : editor.getChildren())
        if (child->getName()=="33609 presets") box=dynamic_cast<juce::ComboBox*>(child);
    if (!box || box->getNumItems()!=static_cast<int>(mkii::presets::bank.size())) return false;
    const auto check=[&](const juce::String& name,float value) {
        for (const auto* parameter : plugin.getParameters())
            if (parameter->getName(100)==name) return std::abs(parameter->getValue()-value)<1e-6f;
        return false;
    };
    constexpr std::array<const char*,9> names{"Compress Threshold","Compress Ratio","Compress Recovery",
        "Compress Gain","Limit Threshold","Limit Recovery","Compress In","Limit In","Fast Attack"};
    constexpr std::array upper{15,4,5,10,22,5,1,1,1};
    for (std::size_t i=0; i<mkii::presets::bank.size(); ++i) {
        const auto& preset=mkii::presets::bank[i];
        box->setSelectedId(static_cast<int>(i)+1,juce::sendNotificationSync);
        const auto values=mkii::presets::detents(preset);
        for (int channel : {1,2}) {
            const auto prefix="Channel "+juce::String(channel)+" ";
            for (std::size_t j=0; j<values.size(); ++j)
                if (!check(prefix+names[j],float(values[j])/upper[j])) return false;
        }
        if (!check("Compressor Attack",preset.attack/2.f)) return false;
    }
    juce::MemoryBlock state;
    plugin.getStateInformation(state);
    std::unique_ptr<juce::AudioProcessor> restored(createPluginFilter());
    restored->setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    std::unique_ptr<juce::AudioProcessorEditor> panel(restored->createEditorIfNeeded());
    if (!panel) return false;
    for (auto* child : panel->getChildren())
        if (const auto* recalled=dynamic_cast<const juce::ComboBox*>(child))
            if (recalled->getName()=="33609 presets" && recalled->getSelectedId()==box->getSelectedId()) {
                std::cout<<"Preset recall and save/restore passed for "<<box->getNumItems()<<" presets\n";
                return true;
            }
    return false;
}

bool traceDynamics(juce::AudioProcessor& plugin, const mkii::native::DynamicsProbe& probe) {
    const auto positions = probe.controls();
    if (!setControl(plugin, "Channel 1 Compress Threshold", (positions[0]-1)/15.f) ||
        !setControl(plugin, "Channel 1 Compress Ratio", (positions[1]-1)/4.f) ||
        !setControl(plugin, "Channel 1 Compress Recovery", (positions[2]-1)/5.f) ||
        !setControl(plugin, "Channel 1 Limit Threshold", (positions[4]-1)/22.f) ||
        !setControl(plugin, "Channel 1 Limit Recovery", (positions[5]-1)/5.f) ||
        !setControl(plugin, "Channel 1 Compress In", float(positions[6])) ||
        !setControl(plugin, "Channel 1 Limit In", float(positions[7])) ||
        !setControl(plugin, "Channel 1 Fast Attack", float(positions[8])) ||
        !setControl(plugin, "Compressor Attack", probe.limiter ? 0.0f : probe.attack/2.f)) return false;
    juce::AudioProcessor::BusesLayout stereo;
    stereo.inputBuses.add(juce::AudioChannelSet::stereo());
    stereo.outputBuses.add(juce::AudioChannelSet::stereo());
    if (!plugin.setBusesLayout(stereo)) return false;

    constexpr int block=512;
    plugin.setNonRealtime(true);
    plugin.prepareToPlay(probe.rate,block);
    juce::AudioBuffer<float> audio(2,block);
    juce::MidiBuffer midi;
    const int frames=int(std::ceil(probe.stop*probe.rate))+1;
    std::cout << std::setprecision(12);
    const int latency=plugin.getLatencySamples();
    for (int offset=0;offset<frames+latency;offset+=block) {
        audio.clear();
        for (int n=0;n<block;++n)
            audio.setSample(0,n,float(probe.input(double(offset+n)/probe.rate)));
        plugin.processBlock(audio,midi);
        for (int n=std::max(0,latency-offset);n<std::min(block,frames+latency-offset);++n)
            std::cout << double(offset+n-latency)/probe.rate << ' ' << audio.getSample(0,n) << '\n';
    }
    plugin.releaseResources();
    return true;
}

bool checkSupplyNoise(juce::AudioProcessor& plugin) {
    constexpr int rate=44100,blockSize=512,settle=8192;
    constexpr int fft=4096,pinkBin=93;
    constexpr double tau=2*std::numbers::pi;
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
std::array<std::complex<double>,20> measureSpectrum(juce::AudioProcessor& plugin,double frequency,double dbu,int rate=44100,int durationTenths=6) {
    constexpr int blockSize=512;
    const int duration=rate*durationTenths/10,window=rate/5;
    const double omega=2*std::numbers::pi*frequency/rate;
    const double level=std::pow(10.,(dbu-24.)/20);
    juce::AudioBuffer<float> block(2,blockSize);
    juce::MidiBuffer midi;
    plugin.prepareToPlay(rate,blockSize);
    std::array<std::complex<double>,20> spectrum{};
    const int latency=plugin.getLatencySamples();
    for (int frame=0;frame<duration+latency;frame+=blockSize) {
        for (int i=0;i<blockSize;++i) {
            const auto x=static_cast<float>(level*std::sin(omega*(frame+i)));
            block.setSample(0,i,x);
            block.setSample(1,i,x);
        }
        plugin.processBlock(block,midi);
        for (int i=0;i<blockSize && frame+i<duration+latency;++i) {
            const int source=frame+i-latency;
            if (source<duration-window) continue;
            const auto y=static_cast<double>(block.getSample(0,i));
            const auto root=std::polar(1.,-omega*source);
            auto oscillator=root;
            for (auto& bin:spectrum) {bin+=y*oscillator;oscillator*=root;}
        }
    }
    plugin.releaseResources();
    return spectrum;
}

bool runJdCase(juce::AudioProcessor& plugin,char** args,int argc) {
    const int rate=argc==19 ? std::stoi(args[15]) : 44100;
    constexpr int blockSize=512;
    constexpr std::array<const char*,9> names{"Compress Threshold","Compress Ratio","Compress Recovery",
        "Compress Gain","Limit Threshold","Limit Recovery","Compress In","Limit In","Fast Attack"};
    constexpr std::array upper{15,4,5,10,22,5,1,1,1};
    for (std::size_t i=0; i<names.size(); ++i)
        if (!setControl(plugin,"Channel 1 "+juce::String(names[i]),std::stof(args[i])/upper[i])) return false;
    if (!setControl(plugin,"Compressor Attack",std::stof(args[9])/2.f)) return false;
    const auto frequency=std::stod(args[10]), low=std::stod(args[11]), high=std::stod(args[12]);
    const auto burst=std::stod(args[13]), stop=std::stod(args[14]);
    plugin.setNonRealtime(true);
    std::cout<<std::setprecision(12);
    if (burst==0.) {
        const auto durationTenths=static_cast<int>(std::ceil(stop*10));
        const auto spectrum=measureSpectrum(plugin,frequency,high,rate,durationTenths);
        for (const auto& bin:spectrum)
            std::cout<<bin.real()*(10./rate)<<' '<<bin.imag()*(10./rate)<<'\n';
        return true;
    }
    plugin.prepareToPlay(rate,blockSize);
    juce::AudioBuffer<float> block(2,blockSize);
    juce::MidiBuffer midi;
    const int frames=static_cast<int>(std::ceil(stop*rate))+1;
    const auto peak=[](double db) {return std::pow(10.,(db-24.)/20.);};
    const auto quiet=peak(low), loud=peak(high);
    const auto omega=2*std::numbers::pi*frequency;
    const int latency=plugin.getLatencySamples();
    for (int frame=0;frame<frames+latency;frame+=blockSize) {
        block.clear();
        for (int i=0;i<blockSize;++i) {
            const auto time=double(frame+i)/rate;
            block.setSample(0,i,static_cast<float>((time>=.45 && time<.45+burst?loud:quiet)*std::sin(time*omega)));
        }
        plugin.processBlock(block,midi);
        for (int i=std::max(0,latency-frame);i<blockSize && frame+i<frames+latency;++i)
            std::cout<<double(frame+i-latency)/rate<<' '<<block.getSample(0,i)<<'\n';
    }
    plugin.releaseResources();
    return true;
}

bool checkThroughResponse(juce::AudioProcessor& plugin) {
    if (!setControl(plugin,"PSU Noise",0) ||
        !setControl(plugin,"Bypass In",1) || !setControl(plugin,"Power",1)) return false;
    for (int channel=1;channel<=2;++channel) {
        const auto prefix="Channel "+juce::String(channel)+" ";
        if (!setControl(plugin,prefix+"Limit In",0) || !setControl(plugin,prefix+"Compress In",0))
            return false;
    }
    bool passed=true;
    for (int rate:{44100,48000,96000}) {
        const double reference=std::abs(measureSpectrum(plugin,1000,0,rate)[0]);
        for (int frequency:{20,100,5000,8000,10000,12000,15000,16000,18000,20000}) {
            const double response=20*std::log10(std::abs(measureSpectrum(plugin,frequency,0,rate)[0])/reference);
            std::cout<<"Through response "<<rate<<" Hz, "<<frequency<<" Hz "<<response<<" dB relative to 1 kHz\n";
            passed=passed && std::isfinite(response) && std::abs(response)<=.5;
        }
    }
    return passed;
}

bool checkRatioThd(juce::AudioProcessor& plugin) {
    if (!setControl(plugin,"PSU Noise",0)) return false;
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

bool checkLimiterDistortion(juce::AudioProcessor& plugin) {
    if (!setControl(plugin,"PSU Noise",0)) return false;
    for (int channel=1;channel<=2;++channel) {
        const auto prefix="Channel "+juce::String(channel)+" ";
        if (!setControl(plugin,prefix+"Compress In",0) ||
            !setControl(plugin,prefix+"Limit In",1) ||
            !setControl(plugin,prefix+"Limit Threshold",8.f/22.f) ||
            !setControl(plugin,prefix+"Limit Recovery",3.f/5.f) ||
            !setControl(plugin,prefix+"Fast Attack",0)) return false;
    }

    bool passed=true;
    // The 40 ohm source needs 0.581 dB more to deliver +22 dBu
    // at the terminated input of the original circuit reference.
    for (const double sourceLevel:{22.,22.581}) {
        const auto spectrum=measureSpectrum(plugin,1000,sourceLevel,44100,9);
        const auto fundamental=std::abs(spectrum[0]);
        double power=0;
        for (std::size_t k=1;k<spectrum.size();++k) power+=std::norm(spectrum[k]);
        const auto thd=100*std::sqrt(power)/fundamental;
        const auto output=24+20*std::log10(2*fundamental/8820);
        const auto k2=20*std::log10(std::abs(spectrum[1])/fundamental);
        const auto k3=20*std::log10(std::abs(spectrum[2])/fundamental);
        std::cout<<"Packaged limiter THD, source "<<sourceLevel<<" dBu: "<<thd
                 <<"%, output "<<output<<" dBu, K2 "<<k2<<" dBc, K3 "<<k3<<" dBc\n";
        passed=passed && thd>=.36 && thd<=.45 && std::abs(output-8)<=.15 && k2<=-64;
    }
    return passed;
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


bool firstBeatProbe(juce::AudioProcessor& plugin) {
    constexpr int rate=44100,blockSize=512,seconds=1,frames=rate*seconds;
    juce::AudioProcessor::BusesLayout stereo;
    stereo.inputBuses.add(juce::AudioChannelSet::stereo());
    stereo.outputBuses.add(juce::AudioChannelSet::stereo());
    if (!plugin.setBusesLayout(stereo)) return false;
    for (int ch=1;ch<=2;++ch) {
        const auto p="Channel "+juce::String(ch)+" ";
        if (!setControl(plugin,p+"Compress In",1.0f) ||
            !setControl(plugin,p+"Limit In",1.0f) ||
            !setControl(plugin,p+"Compress Ratio",.75f) ||
            !setControl(plugin,p+"Compress Threshold",10.f/15.f) ||
            !setControl(plugin,p+"Limit Threshold",8.f/22.f)) return false;
    }
    if (!setControl(plugin,"PSU Noise",0) || !setControl(plugin,"Compressor Attack",0)) return false;
    std::array<std::vector<float>,2> input,offline,realtime;
    for (auto* stream:{&input,&offline,&realtime}) for(auto& lane:*stream) lane.resize(frames);
    for (int n=0;n<frames;++n) {
        const double t=double(n)/rate;
        // Starts immediately on beat one, with a sharp kick, a quieter sustain
        // and another kick at 250 ms. No leading silence to hide a late worker.
        const double envelope=(.48*std::exp(-80*std::fmod(t,.25)) + .03);
        input[0][n]=float(envelope*std::sin(2*std::numbers::pi*150*t));
        input[1][n]=float(envelope*std::sin(2*std::numbers::pi*150*t+.35));
    }
    auto render=[&](bool nonRealtime,std::array<std::vector<float>,2>& dst) {
        plugin.setNonRealtime(nonRealtime);
        plugin.prepareToPlay(rate,blockSize);
        const int latency=plugin.getLatencySamples();
        if(latency!=1024) {std::cerr<<"Unexpected latency: "<<latency<<'\\n';return false;}
        juce::AudioBuffer<float> block(2,blockSize);
        juce::MidiBuffer midi;
        using Clock=std::chrono::steady_clock;
        const auto begin=Clock::now();
        for(int offset=0;offset<frames+latency;offset+=blockSize) {
            for(int i=0;i<blockSize;++i)
                for(int ch=0;ch<2;++ch)
                    block.setSample(ch,i,offset+i<frames?input[ch][offset+i]:0);
            if(!nonRealtime) std::this_thread::sleep_until(begin+
                std::chrono::duration_cast<Clock::duration>(
                   std::chrono::duration<double>(double(offset)/rate)));
            plugin.processBlock(block,midi);
            for(int i=0;i<blockSize;++i) {
                const int source=offset+i-latency;
                if(source>=0 && source<frames)
                    for(int ch=0;ch<2;++ch) dst[ch][source]=block.getSample(ch,i);
            }
        }
        plugin.releaseResources();
        return true;
    };
    if(!render(true,offline) || !render(false,realtime)) return false;
    unsigned mismatches=0,dryFallbacks=0,firstMismatches=0;
    double maxDifference=0;
    for(int ch=0;ch<2;++ch) for(int n=0;n<frames;++n) {
        const double delta=std::abs(double(realtime[ch][n])-offline[ch][n]);
        maxDifference=std::max(maxDifference,delta);
        if(delta>2e-5) {
            ++mismatches;
            if(n<blockSize) ++firstMismatches;
            if(std::abs(realtime[ch][n]-input[ch][n])<2e-5f)++dryFallbacks;
        }
    }
    double firstWindowOffline=0,firstWindowRealtime=0;
    for(int n=0;n<blockSize;++n) for(int ch=0;ch<2;++ch) {
       firstWindowOffline=std::max(firstWindowOffline,std::abs(double(offline[ch][n])));
       firstWindowRealtime=std::max(firstWindowRealtime,std::abs(double(realtime[ch][n])));
    }
    std::cout<<"ONSET 44.1k/512 latency 1024; first 512-sample wet reference peak "
             <<firstWindowOffline<<", realtime peak "<<firstWindowRealtime
             <<", first-window mismatches "<<firstMismatches
             <<", total sample mismatches "<<mismatches
             <<", dry fallback matches "<<dryFallbacks
             <<", maximum difference "<<maxDifference<<'\\n';
    return firstWindowOffline>0.0001;
}


}

int main(int argc,char** argv) {
    std::cerr<<std::unitbuf;
    const bool offline=argc==3 && std::string_view(argv[2])=="--offline";
    const bool firstBeat=argc==3 && std::string_view(argv[2])=="--first-beat";
    const bool dynamics=argc==11 && std::string_view(argv[2])=="--jd-timing";
    const bool jdCase=(argc==18 || argc==19) && std::string_view(argv[2])=="--jd-case";
    if (argc!=2 && !offline && !dynamics && !jdCase && !firstBeat) {
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
    if (jdCase) return runJdCase(*vst,argv+3,argc) ? 0 : 19;
    if (firstBeat) return firstBeatProbe(*vst) ? 0 : 20;
    if (dynamics) {
        for (const auto* parameter : vst->getParameters()) {
            if (parameter->getName(100) == "Compressor Attack" && parameter->getValue() != 0.f) {
                std::cerr << "The VST3 did not default to the JD attack detent\n";
                return 16;
            }
        }
        return traceDynamics(*vst, mkii::native::DynamicsProbe(argv+3)) ? 0 : 17;
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
        if (!checkPresets(*processor,*panel)) {
            std::cerr<<"Preset recall or state restore failed\n";
            return 18;
        }
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
        const std::array<const char*,4> caseNames{"through","dual mono compressor","dual mono limiter","dual mono compressor and limiter"};
        const int caseCount=layout.size()==2?4:1;
        for (int testCase=0;testCase<caseCount;++testCase) {
            auto settings=mkii::native::defaults;
            if (layout.size()==2) {
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
            std::array<mkii::native::Solver,2> lanes{mkii::native::Solver(0),mkii::native::Solver(1)};
            for (int ch=0;ch<layout.size();++ch) {
                auto& lane=lanes[ch];
                if (!lane.prepare(rate*2) || !lane.usesGeneratedSolve() || !lane.usesGeneratedResidual()) return 9;
            }
            auto expected=input;
            for (int frame=0;frame<input.getNumSamples();++frame) {
                const mkii::native::Solver::Frame source{
                    input.getSample(0,frame),layout.size()==2?input.getSample(1,frame):0.0};
                const mkii::native::Solver::Frame midpoint{
                    mkii::native::BandlimitedHalf::interpolate(frame,[&input](std::uint64_t t) {
                        return t<static_cast<std::uint64_t>(input.getNumSamples()) ? double(input.getSample(0,int(t))) : 0.0;
                    }),
                    mkii::native::BandlimitedHalf::interpolate(frame,[&input,&layout](std::uint64_t t) {
                        return layout.size()>1 && t<static_cast<std::uint64_t>(input.getNumSamples()) ? double(input.getSample(1,int(t))) : 0.0;
                    })};
                for (int ch=0;ch<layout.size();++ch) {
                    lanes[ch].step(midpoint,settings);
                    expected.setSample(ch,frame,static_cast<float>(lanes[ch].step(source,settings)[ch]));
                }
            }
            const auto failures=lanes[0].failures()+lanes[1].failures();
            if (failures) {
                std::cerr<<failures<<" nodal reference solves failed\n";
                return 9;
            }
            vst->setNonRealtime(offline);
            vst->prepareToPlay(rate,n);
            const auto latency=vst->getLatencySamples();
            if (latency!=mkii::native::playbackLatency(n)) {
                std::cerr<<"Unexpected nodal playback latency\n";
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
                        const auto source=frame+i-latency;
                        const double difference=std::abs(value-(source>=0?expected.getSample(c,source):0.f));
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
    return passed?0:10;
}
