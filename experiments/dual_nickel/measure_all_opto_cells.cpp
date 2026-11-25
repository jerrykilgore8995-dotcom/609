// Four-cartridge optical bench: direct LDR datasheet, transfer/knee/ratio,
// compressor pulse timing, and passive photocell harmonics with/without DC.
// Outputs measured CSV rows. It intentionally does NOT fail on deviations:
// hardware calibration is iterative; the baseline is retained in artifacts.
#include "OpticalCell.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>
using namespace optocircuit;
constexpr double fs=44100.0, twoPi=2.*pi, linePeak=17.3677;
constexpr std::array<CellKind,4> cells{CellKind::NSL32,CellKind::VTL5C2,CellKind::T4B,CellKind::VTL5C1};
static const char* name(CellKind c) {
    switch(c) {
        case CellKind::NSL32:return "NSL32";
        case CellKind::VTL5C2:return "VTL5C2";
        case CellKind::T4B:return "T4B";
        case CellKind::VTL5C1:return "VTL5C1";
    }
    return "unknown";
}
static double inputPeak(double dbu) {return std::sqrt(2.)*.775*dB(dbu)/linePeak;}
static double rmsDb(double rms) {return 20.*std::log10(std::max(1e-12,rms)*linePeak/.775);}
static double transfer(CellKind type,double thresh,double inDbu,double attack,double release,
                       bool limiter=false) {
    OpticalCompressor comp;
    comp.prepare(fs);comp.setCellKind(type);comp.setLimiter(limiter);
    comp.setThresholdDb(thresh);comp.setAttackMs(attack);comp.setReleaseMs(release);
    const int n=int(.75*fs),start=int(.55*fs);
    double acc=0.;
    for(int i=0;i<n;++i) {
        const double sample=inputPeak(inDbu)*std::sin(twoPi*1000.*i/fs);
        const double out=comp.process(float(sample));
        if(i>=start) acc+=out*out;
    }
    return rmsDb(std::sqrt(acc/double(n-start)));
}
static void staticSweep() {
    std::cout<<"SECTION,cell,mode,threshold_dbu,input_dbu,output_dbu,GR_db,interval_ratio,local_slope\n";
    for(auto cell:cells)for(bool limiter:{false,true})
    for(double threshold:{-24.,-18.,-12.}) {
        double prevInput=0.,prevOutput=0.;bool first=true;
        for(double level:{-18.,-12.,-6.,0.,4.,8.,12.,16.,20.}) {
            const double out=transfer(cell,threshold,level,15.,250.,limiter);
            const double slope=first?1.:(out-prevOutput)/(level-prevInput);
            const double ratio=std::abs(slope)<1e-8?999.:1./slope;
            std::cout<<"TRANSFER,"<<name(cell)<<","
              <<(limiter?"limiter":"compressor")<<","
              <<threshold<<","<<level<<","<<out<<","<<level-out<<","
              <<(first?0.:ratio)<<","<<slope<<"\n";
            prevInput=level;prevOutput=out;first=false;
        }
    }
}
// Pulse metrics use optical GR rather than detector voltage; 63% attack,
// 50% and 10% release endpoints are explicitly labeled and are not confused
// with manufacturer LED turn-on / 100k-ohm recovery specifications.
static void pulses() {
    std::cout<<"SECTION,cell,attack_setting_ms,release_setting_ms,level_dbu,"
                "steady_GR_db,attack_63_ms,release_50_ms,release_10_ms\n";
    for(auto cell:cells)for(auto settings: {
        std::array<double,2>{10.,50.}, {15.,250.}, {100.,2000.}
    }) {
        OpticalCompressor comp;
        comp.prepare(fs);comp.setCellKind(cell);comp.setThresholdDb(-18.);
        comp.setAttackMs(settings[0]);comp.setReleaseMs(settings[1]);
        const int hold=int(.8*fs),end=int(3.0*fs);
        std::vector<double> gain(hold);
        for(int i=0;i<hold;++i) {
            comp.process(float(inputPeak(4.)*std::sin(twoPi*1000.*i/fs)));
            gain[i]=comp.reductionDb();
        }
        const double peak=gain.back();
        double attack63=-1.,r50=-1.,r10=-1.;
        for(int i=0;i<hold;++i)if(attack63<0.&&gain[i]>=peak*.63)
            attack63=1000.*i/fs;
        for(int i=0;i<end;++i) {
            comp.process(0.f);
            const double current=comp.reductionDb();
            if(r50<0.&&current<=peak*.5)r50=1000.*i/fs;
            if(r10<0.&&current<=peak*.1)r10=1000.*i/fs;
        }
        std::cout<<"PULSE,"<<name(cell)<<","<<settings[0]<<","
          <<settings[1]<<",4,"<<peak<<","<<attack63<<","
          <<r50<<","<<r10<<"\n";
    }
}
static void datasheet() {
    std::cout<<"SECTION,cell,led_mA,resistance_ohm,recovery_100k_ms\n";
    for(auto cell:{CellKind::NSL32,CellKind::VTL5C2,CellKind::VTL5C1})
    for(double milliamps:{1.,10.,20.,40.}) {
        LdrCell v;v.prepare(fs);v.select(cell);
        for(int i=0;i<int(3.*fs);++i)v.processAtLedCurrent(1.,milliamps/1000.);
        const double r=v.resistance();
        double recovery=-1.;
        if(milliamps==40.)for(int i=0;i<int(1.*fs);++i) {
            v.processAtLedCurrent(0.,0.);
            if(recovery<0.&&v.resistance()>=100000.)
                recovery=1000.*i/fs;
        }
        std::cout<<"DEVICE,"<<name(cell)<<","<<milliamps<<","
            <<r<<","<<recovery<<"\n";
    }
}
static void harmonics() {
    std::cout<<"SECTION,cell,hz,dc_volts,k,amplitude_percent,relative_dbc\n";
    for(auto cell:cells)for(double hz:{100.,1000.,5000.,10000.})
    for(double bias:{0.,2.}) {
        // Freeze illumination to isolate the photocell from the detector.
        // EL panel mode is held at fixed excitation for a constant operating
        // point and is reported separately from true LED-photocell devices.
        LdrCell v;v.prepare(fs);v.select(cell);
        for(int i=0;i<int(fs);++i) {
            if(cell==CellKind::T4B)
                v.processAtPanelVac(0.,120.*std::sqrt(2.)*std::sin(twoPi*1000.*i/fs));
            else v.processAtLedCurrent(0.,.010);
        }
        // For T4B, freezing the panel after initialization is not supported:
        // results are explicitly indicative and include optical modulation.
        const int n=int(fs);
        std::array<double,11> sinSum{},cosSum{};
        for(int i=0;i<n;++i) {
            const double input=bias+2.*std::sin(twoPi*hz*i/fs);
            const double y=(cell==CellKind::T4B)?
                v.processAtPanelVac(input,120.*std::sqrt(2.)*std::sin(twoPi*1000.*i/fs)):
                v.processAtLedCurrent(input,.010);
            for(int k=1;k<=10;++k)if(k*hz<fs*.5) {
                const double theta=twoPi*k*hz*i/fs;
                sinSum[k]+=y*std::sin(theta);
                cosSum[k]+=y*std::cos(theta);
            }
        }
        const double fundamental=std::hypot(sinSum[1],cosSum[1]);
        for(int k=2;k<=10;++k)if(k*hz<fs*.5) {
            const double amplitude=std::hypot(sinSum[k],cosSum[k]);
            const double ratio=amplitude/std::max(1e-14,fundamental);
            std::cout<<"HARMONIC,"<<name(cell)<<","<<hz<<","
              <<bias<<","<<k<<","<<100.*ratio<<","
              <<20.*std::log10(std::max(1e-14,ratio))<<"\n";
        }
    }
}
int main(){
    std::cout<<std::fixed<<std::setprecision(6);
    datasheet();staticSweep();pulses();harmonics();
}
