// Empirical burst responses for the actual LED/CDS optical gain cell.
// Measures LDR-derived gain reduction, not an abstract detector envelope.
// All tests independent, starting with a dark cell and silence.
// Attack: first threshold crossing of 10%, 50%, 63.2%, 90% of *measured*
// plateau; release: 50% and 10% of GR measured at the end of the burst.
// The reference fraction is defined in dB of gain reduction, not voltage.
// A measured endpoint of NA means the GR never reached the target.
// Levels are the actual signal presented to this optical circuit (dBu).
#include "OpticalCell.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>

constexpr double pi=3.1415926535897932384626433832795;
constexpr double rate=44100.;
constexpr double normalizedPeakVolts=17.3677;
struct Curve {
    double gr10=-1,gr50=-1,gr63=-1,gr90=-1,plateau=0;
    double release50=-1,release10=-1,at20=0,at50=0,at100=0,
           at250=0,at500=0,at1000=0,peak=0;
    double shortEndpoint=0,post100=0,post500=0,post1000=0;
};
static inline float tone(double dbu,int n) {
    const double amp=std::sqrt(2.)*.775*std::pow(10.,dbu/20.)/
                     normalizedPeakVolts;
    return float(amp*std::sin(2*pi*1000.*n/rate));
}
static double crossing(const std::vector<double>& values,double target,
                       bool ascending) {
    if(values.empty())return -1;
    // The first response is at sample 0 -> 0ms.
    for(size_t n=0;n<values.size();++n) {
        if(ascending ? (values[n]>=target):(values[n]<=target))
            return 1000.*double(n)/rate;
    }
    return -1;
}
static double sampleAt(const std::vector<double>& curve,double ms) {
    const auto sample=std::min(curve.size()-1,
        static_cast<size_t>(std::round(ms*.001*rate)));
    return curve[sample];
}
static Curve measure(double dbu,double attack,double release,
                     double burstMs, double recoveryMs=1800,bool feedforward=false) {
    optocircuit::OpticalCompressor cell;
    cell.prepare(rate);
    cell.setThresholdDb(-18.);
    cell.setFeedforward(feedforward);
    cell.setAttackMs(attack);
    cell.setReleaseMs(release);
    const int warmN=int(.25*rate);
    const int burstN=int(burstMs*.001*rate);
    const int releaseN=int(recoveryMs*.001*rate);
    for(int i=0;i<warmN;++i) cell.process(0.f);
    std::vector<double> onset,decay;
    onset.reserve(burstN);decay.reserve(releaseN);
    double peak=0.;
    for(int n=0;n<burstN;++n) {
        const float output=cell.process(tone(dbu,n));
        if(!std::isfinite(output)) std::exit(9);
        const double gr=cell.reductionDb();
        if(!std::isfinite(gr)) std::exit(9);
        peak=std::max(peak,gr);
        onset.push_back(gr);
    }
    const double finalGr=onset.back();
    for(int n=0;n<releaseN;++n) {
        cell.process(0.f);
        const double gr=cell.reductionDb();
        if(!std::isfinite(gr))std::exit(9);
        decay.push_back(gr);
    }
    Curve c;
    c.plateau=finalGr;c.peak=peak;
    c.gr10=crossing(onset,finalGr*.1,true);
    c.gr50=crossing(onset,finalGr*.5,true);
    c.gr63=crossing(onset,finalGr*.632,true);
    c.gr90=crossing(onset,finalGr*.9,true);
    c.release50=crossing(decay,finalGr*.5,false);
    c.release10=crossing(decay,finalGr*.1,false);
    c.at20=sampleAt(onset,20.);
    c.at50=sampleAt(onset,50.);
    c.at100=sampleAt(onset,100.);
    c.at250=sampleAt(onset,250.);
    c.at500=sampleAt(onset,500.);
    c.at1000=sampleAt(onset,1000.);
    c.post100=sampleAt(decay,100.);
    c.post500=sampleAt(decay,500.);
    c.post1000=sampleAt(decay,1000.);
    return c;
}
int main() {
    std::cout<<std::fixed<<std::setprecision(6);
    std::cout<<"experiment,level_dbu,attack_control_ms,release_control_ms,"
       "burst_ms,end_GR_dB,peak_GR_dB,attack_10_ms,attack_50_ms,"
       "attack_63_ms,attack_90_ms,release_50_ms,release_10_ms,"
       "GR_at_20ms,GR_at_50ms,GR_at_100ms,GR_at_250ms,"
       "GR_at_500ms,GR_at_1000ms,GR_after_100ms,GR_after_500ms,"
       "GR_after_1000ms\n";
    int n=0;
    auto print=[&](const char* experiment,double dbu,double attack,
                   double release,double burstMs,const Curve& c){
        std::cout<<experiment<<","<<dbu<<","<<attack<<","<<release<<","
            <<burstMs<<","<<c.plateau<<","<<c.peak<<","<<c.gr10<<","
            <<c.gr50<<","<<c.gr63<<","<<c.gr90<<","<<c.release50<<","
            <<c.release10<<","<<c.at20<<","<<c.at50<<","<<c.at100<<","
            <<c.at250<<","<<c.at500<<","<<c.at1000<<","<<c.post100
            <<","<<c.post500<<","<<c.post1000<<"\n";
        if(!std::isfinite(c.plateau)||!std::isfinite(c.peak))std::exit(6);
        ++n;
    };
    // Level dependency at every main attack position: fixed 250 ms release.
    for(double level:{-8.,0.,4.,8.,12.})
        for(double a:{10.,15.,30.,50.,100.}) {
            const auto c=measure(level,a,250.,1200.);
            print("attack",level,a,250.,1200.,c);
            // Smooth electrical feedback must not overcompress the optical cell.
            if(level>=4. && c.peak-c.plateau>1.5)return 10;
        }
    // Release dependence at fixed 15ms attack. Two exposure durations
    // demonstrate whether slow light-trap population remembers the burst.
    for(double level:{-8.,4.,12.})
        for(double release:{50.,250.,1000.,2000.})
            for(double duration:{150.,1200.}) {
                const auto c=measure(level,15.,release,duration);
                print(duration<200.?"short_release":"long_release",
                      level,15.,release,duration,c);
            }
    // The switch engages a real input divider and nonlinear diode-feedback
    // resistor network. Confirm useful compression rather than sudden
    // hard optical saturation, and no detector/LED transient overshoot.
    double ffPrev=-1.;
    for(double level:{4.,8.,12.}){
        const auto c=measure(level,15.,250.,1200.,1800,true);
        print("feedforward",level,15.,250.,1200.,c);
        if(c.peak-c.plateau>.5)return 11;
        if(level==4. && !(c.plateau>.5 && c.plateau<4.))return 12;
        if(level==8. && !(c.plateau>3. && c.plateau<8.))return 13;
        if(level==12. && !(c.plateau>3. && c.plateau<8.))return 14;
        if(ffPrev>=0. && c.plateau<ffPrev)return 15;
        ffPrev=c.plateau;
    }
    // Regression: after otherwise identical LED level and detector settings,
    // a sustained note must leave more recoverable CdS trap occupancy than
    // a short transient. Compare remaining GR at the same elapsed time.
    const auto memoryShort=measure(12.,15.,250.,150.);
    const auto memoryLong=measure(12.,15.,250.,1200.);
    std::cerr<<"PROGRAM_MEMORY short_500ms_GR="<<memoryShort.post500
             <<" long_500ms_GR="<<memoryLong.post500
             <<" short_1000ms_GR="<<memoryShort.post1000
             <<" long_1000ms_GR="<<memoryLong.post1000<<"\n";
    if(!(memoryLong.post500>memoryShort.post500*1.1
      &&memoryLong.post1000>memoryShort.post1000*1.1))return 16;
    std::cerr<<"BURST_CHARACTERIZATION completed_points="<<n<<"\n";
    return n==52?0:8;
}
