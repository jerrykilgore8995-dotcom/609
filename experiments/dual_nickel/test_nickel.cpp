#include "NickelTransformer.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>
static constexpr double pi=3.14159265358979323846;
struct Spectrum {
    double fundamental=0,thd=0,peak=0,mean=0;
};
Spectrum measure(double freq,double dbu,bool highHeadroom) {
    nickel::Transformer transformer;
    nickel::CoreParams core;
    if(highHeadroom) {
        core.turns=2600;core.coreArea=5e-4;core.Ms=7e5;
        core.a=30;core.k=40;core.c=.30;core.sourceR=10;
        core.primaryLeak=20e-6;core.secondaryLeak=20e-6;
        core.receiverR=15;core.receiverC=10e-12;
    }
    constexpr double fs=44100;
    constexpr int samples=44100;
    transformer.prepare(fs,core);
    // Convert line-level dBu to normalized digital samples;
    // 0 dBFS peak is calibrated to +24 dBu RMS sine.
    double vrms=0.775*std::pow(10.0,dbu/20.0);
    double amplitude=std::sqrt(2.0)*vrms/nickel::fullScalePeakVolts;
    std::vector<double> y(samples);
    for(int n=0;n<samples*2;++n) {
        const double in=amplitude*std::sin(2*pi*freq*n/fs);
        double v=transformer.process(float(in));
        assert(std::isfinite(v));
        if(n>=samples)y[n-samples]=v;
    }
    Spectrum out;
    auto harmonic=[&](int k) {
        if(freq*k>=fs/2)return 0.0;
        double re=0,im=0;
        for(int n=0;n<samples;++n) {
            const double a=2*pi*k*freq*n/fs;
            re+=y[n]*std::cos(a);
            im+=y[n]*std::sin(a);
        }
        return 2*std::hypot(re,im)/samples;
    };
    out.fundamental=harmonic(1);
    double powers=0;
    for(int k=2;k<=20 && k*freq<fs/2;++k) {
        auto a=harmonic(k);
        powers+=a*a;
    }
    out.thd=out.fundamental>1e-12?std::sqrt(powers)/out.fundamental:0;
    for(auto v:y) {
        out.peak=std::max(out.peak,std::abs(v));
        out.mean+=v/samples;
    }
    return out;
}
int main() {
    // Evaluate self-contained input/output stages with physically meaningful
    // levels. All figures are *model results*, NOT measured transformer data.
    std::cout<<std::fixed<<std::setprecision(7);
    for(const bool clean:{false,true}) {
        for(const double freq:{20.0,30.0,50.0,100.0,1000.0,5000.0,10000.0}) {
            for(const double level:{-18.0,4.0,16.0,24.0}) {
                const auto s=measure(freq,level,clean);
                if(!std::isfinite(s.fundamental)||!std::isfinite(s.thd)
                   ||s.fundamental<=1.e-9 ||s.peak>100)
                    return 2;
                // Fail on gross magnetic instability or unphysical
                // distortion in nominal-level clean output, not just NaNs.
                if(clean && level<=4.0) {
                    if(s.thd>0.002 || s.peak>0.2)
                        return 4;
                }
                if(clean && level==4.0 && freq==1000.0 && s.thd>0.0005)
                    return 5;
                if(!clean && level==4.0 && freq==50.0 && s.thd>0.05)
                    return 6;
                std::cout<<(clean?"OUTPUT":"INPUT")<<" "
                         <<freq<<"Hz "<<level<<"dBu"
                         <<" fundamental="<<s.fundamental
                         <<" THD="<<s.thd*100<<"%"
                         <<" peak="<<s.peak
                         <<" DC="<<s.mean<<"\n";
            }
        }
    }
    nickel::DualTransformerGain stage;
    stage.prepare(44100);
    stage.setInputDriveDb(12);
    stage.setInterstageVolumeDb(3);
    stage.setMakeupDb(6);
    stage.setLinked(true);
    assert(stage.linked());
    for(int n=0;n<44100;++n) {
        const float signal=float(.05*std::sin(2*pi*100*n/44100));
        const float y=stage.processOutput(stage.processInput(signal));
        if(!std::isfinite(y))return 3;
    }
    stage.setLinked(false);
    assert(!stage.linked());
    // Per-sample CPU benchmark representative of this standalone DSP only,
    // not a completed 609 VST3 or any host/plugin overhead.
    nickel::Transformer input;
    input.prepare(44100,{});
    volatile float sink=0;
    const auto start=std::chrono::steady_clock::now();
    for(int n=0;n<44100;++n) {
        float s=float(.1*std::sin(2*pi*100*n/44100));
        sink=input.process(s);
    }
    const auto sec=std::chrono::duration<double>(
        std::chrono::steady_clock::now()-start).count();
    std::cout<<"MONO 44100 samples "<<sec<<"s equivalent/512="
             <<sec*512.0/44100.0*1000<<"ms\n";
    (void)sink;
    return 0;
}
