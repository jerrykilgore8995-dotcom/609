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
        fs=std::max(1000.,sampleRate); updateTiming(); rebuild();reset();
    }
    void reset() noexcept {
        lpLo.reset();hpLo.reset();lpHi.reset();hpHi.reset();
        lpAlign.reset();hpAlign.reset();
        detectorLo.reset();detectorHi.reset(); rms=activityPower=0.;gain=1.;gr=0.;
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
    void setAttackMs(double ms) noexcept {
        const double value = std::clamp(ms,1.,80.)*.001;
        if (value != attack) { attack = value; updateTiming(); }
    }
    void setReleaseMs(double ms) noexcept {
        const double value = std::clamp(ms,20.,1000.)*.001;
        if (value != release) { release = value; updateTiming(); }
    }
    bool isEnabled() const noexcept {return enabled;}
    double reductionDb() const noexcept {return enabled ? gr : 0.;}
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
        return float(lowAligned + gainForPower(detectPower(det)) * mid + high);
    }
    double detectOwnPower(float sample) noexcept {
        return enabled ? detectPower(detectBand(sample)) : 0.;
    }
    float renderLinked(float sample, double power) noexcept {
        if (!enabled) return sample;
        const double low = lpLo.process(sample);
        const double above = hpLo.process(sample);
        const double mid = lpHi.process(above);
        const double high = hpHi.process(above);
        const double lowAligned = lpAlign.process(low) + hpAlign.process(low);
        return float(lowAligned + gainForPower(power) * mid + high);
    }
private:
    double detectPower(double det) noexcept {
        const double power=det*det;
        // In MANUAL release the user's setting is a gain-reduction
        // recovery reference, not a second RMS hold. The old topology
        // cascaded TWO release envelopes and used a logarithmic power
        // threshold, causing ~5x longer 50%-GR recovery than the label.
        //
        // Retain the original asymmetric peak/RMS detector while a low-mid
        // signal is present: its slow discharge is essential to the original
        // -25dB threshold / 170Hz guitar activation.  A SHORT independent
        // 2ms activity sensor notices an actual reduction in program energy.
        // Only then does the detector discharge promptly and let the gain
        // envelope perform a predictable manual-style release. This is
        // C4-inspired, not Waves ARC or a proprietary Waves algorithm.
        activityPower += activityCoeff * (power - activityPower);
        const bool programDropped = activityPower < rms * .20;
        const double coeff = power > rms ? attackCoeff : programDropped ? dropCoeff : releaseCoeff;
        rms += (power - rms) * coeff;
        return rms;
    }
    double gainForPower(double detectorPower) noexcept {
        const double lev=10*std::log10(std::max(1e-15,detectorPower));
        const double over=std::max(0.,lev-threshold);
        const double reduction=std::min(maxCut,over*(1-1./ratio));
        // On release evolve the gain-reduction envelope in dB, with
        // a knob-defined half-recovery time. Applying smoothing in
        // linear gain would make the nominal time level-dependent.
        const double coeff = reduction < gr ? recoveryCoeff : attackCoeff;
        gr = std::max(0., gr + coeff * (reduction - gr));
        gain = std::pow(10., -gr / 20.);
        return gain;
    }
    void updateTiming() noexcept {
        const auto coefficient = [this](double seconds) {
            return 1. - std::exp(-1. / (fs * seconds));
        };
        activityCoeff = coefficient(.002);
        dropCoeff = coefficient(.0015);
        attackCoeff = coefficient(attack);
        releaseCoeff = coefficient(release);
        // Shorten manual gain recovery by the fixed activity detector delay.
        const double manualHalfLife = std::max(.001, release - std::min(.012, release * .60));
        recoveryCoeff = coefficient(manualHalfLife / std::log(2.));
    }
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
           rms=0.,activityPower=0.,gain=1.,gr=0.;
    double activityCoeff=0., dropCoeff=0., attackCoeff=0., releaseCoeff=0., recoveryCoeff=0.;
    bool enabled=false;
    LR4 lpLo,hpLo,lpHi,hpHi,lpAlign,hpAlign,detectorLo,detectorHi;
};
} // namespace lowmid
