#pragma once
// One active low-mid C4-like multiband dynamics stage. This is an original
// three-band LR4 crossover and a compressor operating only on the mid band.
// It does NOT reproduce copyrighted Waves code or claim factory C4 matching.
// Four cascaded 2nd-order Butterworth sections form Linkwitz-Riley 4th order.
// Low-band phase is aligned through the second split's LR all-pass sum.
#include <algorithm>
#include <cmath>
namespace lowmid {
constexpr double pi=3.14159265358979323846;
struct Biquad {
    double b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
    void configure(double fs,double hz,bool highPass) noexcept {
        const double w=2*pi*std::clamp(hz,15.,fs*.43)/fs;
        const double cs=std::cos(w),sn=std::sin(w);
        const double alpha=sn*.7071067811865476;
        const double a0=1+alpha;
        b0=(highPass?(1+cs):(1-cs))/(2*a0);
        b1=(highPass?-(1+cs):(1-cs))/a0;
        b2=b0;a1=-2*cs/a0;a2=(1-alpha)/a0;
    }
    void reset() noexcept {z1=z2=0;}
    double process(double x) noexcept {
        const double y=b0*x+z1;
        z1=b1*x-a1*y+z2;
        z2=b2*x-a2*y;
        return y;
    }
};
struct LR4 {
    Biquad first,second;
    void prepare(double fs,double hz,bool hp) noexcept {
        first.configure(fs,hz,hp);second.configure(fs,hz,hp);reset();
    }
    void reset() noexcept {first.reset();second.reset();}
    double process(double x) noexcept {return second.process(first.process(x));}
};
class DynamicLowMid {
public:
    void prepare(double sampleRate) noexcept {
        fs=std::max(1000.,sampleRate); rebuild();reset();
    }
    void reset() noexcept {
        lpLo.reset();hpLo.reset();lpHi.reset();hpHi.reset();
        lpAlign.reset();hpAlign.reset();
        detectorLo.reset();detectorHi.reset(); rms=0.;gain=1.;gr=0.;
    }
    void setEnabled(bool b) noexcept {enabled=b;}
    void setCenterHz(double hz) noexcept {
        const double v=std::clamp(hz,80.,400.);
        if(std::abs(center-v)>1e-4){center=v;rebuild();}
    }
    void setWidthOctaves(double octaves) noexcept {
        const double v=std::clamp(octaves,.5,2.5);
        if(std::abs(width-v)>1e-4){width=v;rebuild();}
    }
    void setThresholdDb(double db) noexcept {threshold=std::clamp(db,-50.,12.);}
    void setRatio(double r) noexcept {ratio=std::clamp(r,1.0,20.);}
    void setMaxCutDb(double v) noexcept {maxCut=std::clamp(v,0.,12.);}
    void setAttackMs(double ms) noexcept {attack=std::clamp(ms,1.,80.)*.001;}
    void setReleaseMs(double ms) noexcept {release=std::clamp(ms,20.,1000.)*.001;}
    double reductionDb() const noexcept {return gr;}
    double lowCorner() const noexcept {return lo;}
    double highCorner() const noexcept {return hi;}
    double detectBand(double x) noexcept {
        return detectorHi.process(detectorLo.process(x));
    }
    float process(float sample,double linkedDetector=0.,bool external=false) noexcept {
        if(!enabled)return sample;
        const double x=double(sample);
        const double low=lpLo.process(x);
        const double above=hpLo.process(x);
        const double mid=lpHi.process(above);
        const double high=hpHi.process(above);
        // Align low-band phase with upper splitter; otherwise summing the
        // branches creates notches unrelated to the intended dynamic band.
        const double lowAligned=lpAlign.process(low)+hpAlign.process(low);
        const double det=external?linkedDetector:mid;
        const double power=det*det;
        // In MANUAL release the user's setting is a gain-reduction
        // recovery reference, not a second RMS hold. The old topology
        // cascaded TWO release envelopes and used a logarithmic power
        // threshold, causing ~5x longer 50%-GR recovery than the label.
        //
        // The detector retains the attack smoothing for low-frequency
        // waveform immunity, but discharges promptly on falling energy.
        // Gain reduction itself receives the user release time constant.
        // This is a C4-inspired manual mode, NOT proprietary Waves ARC.
        const double detectorTau=power>rms?attack:
                                  std::min(.005,release*.05);
        rms+=(power-rms)*(1-std::exp(-1./(fs*std::max(1e-5,detectorTau))));
        const double lev=10*std::log10(std::max(1e-15,rms));
        const double over=std::max(0.,lev-threshold);
        const double reduction=std::min(maxCut,over*(1-1./ratio));
        const double target=std::pow(10.,-reduction/20.);
        // On release evolve the gain-reduction envelope in dB, with
        // a knob-defined half-recovery time. Applying smoothing in
        // linear gain would make the nominal time level-dependent.
        const double current=-20.*std::log10(std::max(1e-12,gain));
        const bool recovering=reduction<current;
        const double tau=recovering?release/std::log(2.):attack;
        const double coeff=1.-std::exp(-1./(fs*std::max(1e-5,tau)));
        gr=std::max(0.,current+coeff*(reduction-current));
        gain=std::pow(10.,-gr/20.);
        return float(lowAligned+gain*mid+high);
    }
private:
    void rebuild() noexcept {
        lo=std::max(30.,center*std::pow(2.,-width*.5));
        hi=std::min(fs*.35,center*std::pow(2.,width*.5));
        lpLo.prepare(fs,lo,false);hpLo.prepare(fs,lo,true);
        lpHi.prepare(fs,hi,false);hpHi.prepare(fs,hi,true);
        lpAlign.prepare(fs,hi,false);hpAlign.prepare(fs,hi,true);
        detectorLo.prepare(fs,lo,true);detectorHi.prepare(fs,hi,false);
    }
    double fs=44100.,center=170.,width=1.3,lo=108.,hi=267.,
           threshold=-22.,ratio=4.,maxCut=4.,attack=.01,release=.15,
           rms=0.,gain=1.,gr=0.;
    bool enabled=false;
    LR4 lpLo,hpLo,lpHi,hpHi,lpAlign,hpAlign,detectorLo,detectorHi;
};
} // namespace lowmid
