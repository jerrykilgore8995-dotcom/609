#pragma once
// Dual magnetic-transformer experiment: real Jiles-Atherton state equation coupled
// to a source/termination KCL, followed by an actual chowdsp_wdf RC parasitic network.
// WARNING: parameters are engineering starting points, not fitted hardware measurements.
// Magnetic domain / copper / leakage coupling must be validated before production use.
#include <chowdsp_wdf/chowdsp_wdf.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace nickel {
inline double dbGain(double db) noexcept {return std::pow(10.0,db/20.0);}

struct CoreParams {
    double turns=600.0, area=1.5e-4, path=0.12; // N, m^2, m
    double Ms=5.0e5, a=100.0, k=150.0, c=0.20, alpha=1.0e-4;
    double sourceOhms=50.0, primaryOhms=40.0, secondaryOhms=40.0;
    double loadOhms=10000.0;
    double parasiticR=100000.0, parasiticC=22e-12;
};
class JilesAthertonCore {
public:
    struct State {double H=0.0, Mirr=0.0, M=0.0, B=0.0;};
    explicit JilesAthertonCore(CoreParams p={}):p(p) {}
    void configure(CoreParams params) noexcept {p=params; reset();}
    void reset() noexcept {s={};}
    const State& state() const noexcept {return s;}
    // Langevin magnetisation with series near zero to avoid singularity.
    double anhysteretic(double H,double M) const noexcept {
        const double he=H+p.alpha*M;
        const double z=he/p.a;
        double L;
        if(std::abs(z)<1.e-3) L=z/3.0-z*z*z/45.0+2.0*z*z*z*z*z/945.0;
        else L=1.0/std::tanh(z)-1.0/z;
        return p.Ms*std::clamp(L,-1.0,1.0);
    }
    // Evaluate a *candidate* branch, without committing core state.
    State evolve(double targetH) const noexcept {
        State t=s;
        const double total=targetH-s.H;
        // Subdivide large excursions for minor-loop/path-dependent integration.
        const int count=std::clamp(int(std::ceil(std::abs(total)/20.0)),1,48);
        const double step=total/count;
        for(int n=0;n<count;++n) {
            const double nextH=t.H+step;
            // Fixed-point reversible/irreversible magnetization split.
            double mir=t.Mirr;
            for(int j=0;j<5;++j) {
                const double Man=anhysteretic(nextH,t.M);
                const double diff=Man-mir;
                const double delta=step>=0.0?1.0:-1.0;
                const double denom=delta*p.k-p.alpha*diff;
                // Irreversible motion occurs only toward anhysteretic M.
                const bool active=(step*diff>0.0);
                const double dMdH=active && std::abs(denom)>1.e-9
                     ? diff/denom : 0.0;
                const double nextMir=std::clamp(t.Mirr+step*dMdH,-p.Ms,p.Ms);
                mir=0.5*mir+0.5*nextMir;
            }
            t.Mirr=mir;
            const double Man=anhysteretic(nextH,t.M);
            t.M=std::clamp((1.0-p.c)*mir+p.c*Man,-p.Ms,p.Ms);
            t.H=nextH;
        }
        constexpr double mu0=1.2566370614359173e-6;
        t.B=mu0*(t.H+t.M);
        return t;
    }
    void commit(const State& n) noexcept {s=n;}
private:
    CoreParams p;
    State s{};
};

// WDF electrical parasitic: real chowdsp_wdf nodes, not a hand-coded lowpass.
// Coupled core/source/termination is solved implicitly in process(); WDF models
// a simple downstream winding/capacitive damping stage. The network remains a
// reduced approximation, not a complete multiwinding WDF gyrator transformer.
class WdfParasitic {
    using R=chowdsp::wdft::ResistorT<double>;
    using C=chowdsp::wdft::CapacitorT<double>;
    R resistor{100000.0};
    C capacitor{22e-12};
    chowdsp::wdft::WDFSeriesT<double,R,C> series{resistor,capacitor};
    chowdsp::wdft::IdealVoltageSourceT<double,decltype(series)> source{series};
public:
    void prepare(double fs,double r,double c) noexcept {
        resistor.setResistanceValue(std::max(1.0,r));
        capacitor.setCapacitanceValue(std::max(1.e-14,c));
        capacitor.prepare(fs);
    }
    double process(double x) noexcept {
        source.setVoltage(x);
        source.incident(series.reflected());
        series.incident(source.reflected());
        return chowdsp::wdft::voltage<double>(capacitor);
    }
};

class Transformer {
public:
    void prepare(double sampleRate,CoreParams params) noexcept {
        fs=std::max(1000.0,sampleRate); p=params;
        core.configure(p);
        parasitic.prepare(fs,p.parasiticR,p.parasiticC);
        reset();
    }
    void reset() noexcept {core.reset();}
    float process(float input) noexcept {
        // Balance source + primary copper + reflected secondary loading:
        // Vin = Vp + (Rs+Rp)*(Im + Vp/Rreflected).
        // Im=H*magnetic_path/N ; Vp=N*A*dB/dt.
        // Secondary copper resistance contributes to reflected load.
        const double r=std::max(.01,p.sourceOhms+p.primaryOhms);
        const double load=std::max(1.0,p.loadOhms+p.secondaryOhms);
        const double conductance=1.0+r/load;
        const double currPerH=p.path/std::max(1.0,p.turns);
        const double B0=core.state().B;
        auto residual=[&](double H) {
            const auto candidate=core.evolve(H);
            const double Vflux=p.turns*p.area*fs*(candidate.B-B0);
            const double Vcircuit=(double(input)-r*H*currPerH)/conductance;
            return Vflux-Vcircuit;
        };
        // Bracket continuously around previous state, widen if necessary.
        const double h0=core.state().H;
        double span=100.0,lo=h0-span,hi=h0+span;
        for(int j=0;j<16 && residual(lo)*residual(hi)>0.0;++j) {
            span*=2.0;lo=h0-span;hi=h0+span;
        }
        const double flo=residual(lo),fhi=residual(hi);
        if(!std::isfinite(flo)||!std::isfinite(fhi)||flo*fhi>0.0) return 0.0f;
        // Implicit trapezoid not assumed: this is backward Euler flux update.
        // Robust bounded bisection; avoid divergent Newton steps in hysteresis.
        const bool growing=fhi>flo;
        for(int i=0;i<24;++i) {
            const double mid=0.5*(lo+hi), f=residual(mid);
            if(!std::isfinite(f)) return 0.0f;
            if((f>0.0)==growing) hi=mid; else lo=mid;
        }
        const auto next=core.evolve(.5*(lo+hi));
        core.commit(next);
        const double v=(double(input)-r*next.H*currPerH)/conductance;
        const double out=parasitic.process(v);
        return std::isfinite(out)?float(std::clamp(out,-100.0,100.0)):0.0f;
    }
private:
    double fs=44100.0;
    CoreParams p{};
    JilesAthertonCore core;
    WdfParasitic parasitic;
};
class DualTransformerGain {
public:
    void prepare(double fs) noexcept {
        CoreParams input;
        input.sourceOhms=50; input.Ms=5.e5;input.a=100;input.k=150;
        character.prepare(fs,input);
        CoreParams clean;
        clean.sourceOhms=10;clean.primaryOhms=40;clean.secondaryOhms=40;
        clean.turns=1000;clean.area=2.e-4;clean.a=30;clean.k=40;
        clean.c=.30;clean.parasiticR=150000;clean.parasiticC=10.e-12;
        output.prepare(fs,clean);
    }
    void reset() noexcept {character.reset();output.reset();}
    void setInputDriveDb(double x) noexcept {drive=std::clamp(x,-12.,18.);}
    void setInterstageVolumeDb(double x) noexcept {middle=std::clamp(x,-24.,24.);}
    void setMakeupDb(double x) noexcept {makeup=std::clamp(x,-12.,24.);}
    void setOutputTrimDb(double x) noexcept {trim=std::clamp(x,-36.,24.);}
    void setLinked(bool x) noexcept {link=x;}
    bool linked() const noexcept {return link;}
    float processInput(float x) noexcept {
        return character.process(float(double(x)*dbGain(drive)));
    }
    float processOutput(float limiterOutput) noexcept {
        const float y=output.process(float(double(limiterOutput)*dbGain(middle+makeup)));
        return float(double(y)*dbGain(trim-(link?drive:0.)));
    }
private:
    Transformer character,output;
    double drive=0,middle=0,makeup=0,trim=0;
    bool link=true;
};
} // namespace nickel
