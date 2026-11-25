#pragma once
// Experimental physically coupled circuit with a Jiles-Atherton-style magnetic
// state and chowdsp_wdf output parasitic network. NOT calibrated hardware.
// 1:1 transformer; source, winding copper, both leakage inductors, a nonlinear
// magnetizing branch, finite core loss, shunt capacitance and external loading
// are solved together with an implicit voltage/current balance at each sample.
// WDF is used for an additional winding/receiver network. A fully coupled WDF
// multiport model remains separate future work.
#include <chowdsp_wdf/chowdsp_wdf.h>
#include <algorithm>
#include <cmath>
#include <utility>

namespace nickel {
inline double dbGain(double db) noexcept { return std::pow(10.0, db / 20.0); }
constexpr double fullScalePeakVolts = 17.3677; // +24 dBu rms sine digital reference
constexpr double mu0 = 1.2566370614359173e-6;

struct CoreParams {
    double turns=1400, coreArea=3.0e-4, pathLength=0.12;
    double Ms=5.0e5, a=100.0, k=150.0, c=0.20, alpha=1.0e-4;
    double sourceR=50, primaryR=40, secondaryR=40, loadR=10000;
    double primaryLeak=50e-6, secondaryLeak=50e-6;
    double shuntC=100e-12, coreLossR=1e6;
    double receiverR=30, receiverC=22e-12;
};

// Magnetic state uses reversible + irreversible magnetization, domain wall
// pinning, a Langevin anhysteretic magnetization, and history-dependent motion.
// This is a simplified numerical J-A formulation; it has NOT been fitted or
// verified against a particular alloy's measured B/H minor loops.
class MagneticCore {
public:
    struct State { double H=0, Mirr=0, M=0, B=0; };
    void prepare(const CoreParams& p) noexcept { params=p; reset(); }
    void reset() noexcept { state_={}; }
    const State& state() const noexcept {return state_;}
    double anhysteretic(double H,double M) const noexcept {
        const double z=(H+params.alpha*M)/std::max(1.e-9,params.a);
        const double L=std::abs(z)<1.e-3
            ? z/3.0-z*z*z/45.0+2*z*z*z*z*z/945.0
            : 1.0/std::tanh(z)-1.0/z;
        return params.Ms*std::clamp(L,-1.0,1.0);
    }
    State evolve(double Htarget) const noexcept {
        State s=state_;
        const double deltaH=Htarget-s.H;
        // Constrain per-step H excursions for stable branch traversal.
        const int n=std::clamp(int(std::ceil(std::abs(deltaH)/5.0)),1,24);
        const double dh=deltaH/n;
        for(int j=0;j<n;++j) {
            const double nextH=s.H+dh;
            double mir=s.Mirr, m=s.M;
            // fixed-point update of reversible/irreversible components
            for(int k=0;k<4;++k) {
                const double Man=anhysteretic(nextH,m);
                const double diff=Man-mir;
                const double direction=dh>=0?1.0:-1.0;
                const double denom=direction*params.k-params.alpha*diff;
                const double derivative=dh*diff>0 && std::abs(denom)>1.e-8
                   ? diff/denom : 0.0;
                mir=std::clamp(s.Mirr+dh*derivative,-params.Ms,params.Ms);
                m=std::clamp((1.0-params.c)*mir+params.c*Man,-params.Ms,params.Ms);
            }
            s.H=nextH;s.Mirr=mir;s.M=m;
        }
        s.B=mu0*(s.H+s.M);
        return s;
    }
    void commit(const State& s) noexcept {state_=s;}
private:
    CoreParams params{};
    State state_{};
};

class WdfReceiver {
    using R=chowdsp::wdft::ResistorT<double>;
    using C=chowdsp::wdft::CapacitorT<double>;
    R r{30.0};
    C c{22e-12};
    chowdsp::wdft::WDFSeriesT<double,R,C> series{r,c};
    chowdsp::wdft::PolarityInverterT<double,decltype(series)> inverter{series};
    chowdsp::wdft::IdealVoltageSourceT<double,decltype(inverter)> source{inverter};
public:
    void prepare(double sampleRate,double ohms,double farads) noexcept {
        r.setResistanceValue(std::max(.001,ohms));
        c.setCapacitanceValue(std::max(1.e-14,farads));
        c.prepare(sampleRate);
    }
    double process(double v) noexcept {
        source.setVoltage(v);
        source.incident(inverter.reflected());
        inverter.incident(source.reflected());
        return chowdsp::wdft::voltage<double>(c);
    }
};

// Transformer-coupled, emitter-degenerated 2N5088 receiver. The secondary
// references the bias point through the bias network. Current drawn by the
// base and the input capacitance is stamped into the winding equations.
class LoadedReceiver {
public:
    struct Point { double baseCurrent, baseSlope, emitterVolts; };

    Point evaluate(double baseAc) const noexcept {
        // Solve Vb = Re*(Ie-Iq) + Vt*ln(Ie/Iq) for the emitter current.
        // The collector swing sets the forward-active region of the model.
        const double limitedBase = std::clamp(baseAc, -80., 23.5);
        const double x = limitedBase - emitterOffset;
        const double biasDrop = idleCurrent * emitterResistance;
        double logCurrent = x > -.99 * biasDrop
            ? std::log1p(x / (biasDrop + thermalV)) : std::min(-4., (x + biasDrop) / thermalV);
        for (int iteration = 0; iteration < 5; ++iteration) {
            const double drop = biasDrop * std::exp(logCurrent);
            const double error = drop - biasDrop + thermalV * logCurrent - x;
            if (std::abs(error) < 1.e-9) break;
            logCurrent -= error / (drop + thermalV);
        }
        const double emitterDelta = biasDrop * std::expm1(logCurrent);
        const double emitter = emitterOffset + emitterDelta;
        const double current = (biasDrop + emitterDelta) / emitterResistance;
        // Finite beta, high injection, and Early effect from the 2N5088
        // reference card. The bias current is absorbed at the DC operating point.
        const double early = earlyV + 24. - emitter;
        const double baseCurrent = current * (1. + current / highInjection) * earlyV / (beta * early);
        const double emitterSlope = current * emitterResistance /
                                    (current * emitterResistance + thermalV);
        const double baseSlope = emitterSlope * ((1. + 2. * current / highInjection)
                                 * earlyV / (beta * early * emitterResistance) + baseCurrent / early);
        return {baseCurrent - idleBaseCurrent(), baseAc == limitedBase ? baseSlope : 0., emitter};
    }

    void reset() noexcept { secondaryV = outputDc = emitterOffset = 0.; }
    void setOutputLoad(double ohms) noexcept {
        outputLoad = ohms;
        emitterResistance = 1. / (1. / emitterR + 1. / outputLoad);
        emitterOffset = outputDc * emitterResistance / outputLoad;
    }
    double terminalVoltage(double sourceV, double windingR, double fs) const noexcept {
        const double capG = inputCap * fs;
        const double sourceG = 1. / windingR;
        const double rhs = sourceG * sourceV + capG * secondaryV;
        const double sumG = sourceG + capG + 1. / biasNetworkR;
        const double unloaded = rhs / sumG;
        // Newton correction to the linear network solution. Below clipping
        // the base current is small relative to the bias-network current.
        const auto point = evaluate(unloaded);
        return unloaded - point.baseCurrent / (sumG + point.baseSlope);
    }
    double commit(double terminal, double fs) noexcept {
        secondaryV = terminal;
        const double emitter = evaluate(terminal).emitterVolts;
        // 10 µF output coupling capacitor, returned through the next stage.
        outputDc += (emitter - outputDc) / (1. + fs * 10.e-6 * outputLoad);
        constexpr double smallSignalCorrection = 1. + thermalV / 24.;
        return (emitter - outputDc) * smallSignalCorrection;
    }

private:
    // Bias for the full-scale negative swing even as the LDR loads the emitter.
    static constexpr double thermalV = .02585, emitterR = 4700., idleCurrent = 24. / emitterR;
    static constexpr double beta = 1122., highInjection = .01492, earlyV = 62.37;
    static constexpr double biasNetworkR = 10000., inputCap = 122.e-12;
    static constexpr double idleBaseCurrent() noexcept {
        return idleCurrent * (1. + idleCurrent / highInjection) / (beta * (1. + 24. / earlyV));
    }
    double secondaryV = 0., outputDc = 0., emitterOffset = 0.;
    double outputLoad = 57000., emitterResistance = 1. / (1. / emitterR + 1. / 57000.);
};

class Transformer {
public:
    void prepare(double sampleRate,CoreParams params, bool useLoadedReceiver = false) noexcept {
        fs=std::max(1000.0,sampleRate);
        p=params;
        coupledReceiver = useLoadedReceiver;
        core.prepare(p);
        receiver.prepare(fs,p.receiverR,p.receiverC);
        reset();
    }
    void reset() noexcept {
        core.reset();loadedReceiver.reset();primaryCurrent=secondaryCurrent=previousV=0.0;
    }
    float process(float normalizedInput, double receiverLoad = 57000.) noexcept {
        if (coupledReceiver) loadedReceiver.setOutputLoad(receiverLoad);
        const double input=double(normalizedInput)*fullScalePeakVolts;
        const double dt=1.0/fs;
        const double r1=std::max(.01,p.sourceR+p.primaryR);
        const double r2=std::max(.01,p.secondaryR+p.loadR);
        const double induct1=std::max(0.0,p.primaryLeak)/dt;
        const double induct2=std::max(0.0,p.secondaryLeak)/dt;
        const double y1=1.0/(r1+induct1);
        const double y2=1.0/(r2+induct2);
        const double secondaryWindingR = p.secondaryR + induct2;
        const double secondaryHistory = induct2 * secondaryCurrent;
        auto secondary = [&](double voltage) noexcept {
            const double source = voltage + secondaryHistory;
            if (!coupledReceiver)
                return std::pair{source * y2, source * y2 * p.loadR};
            const double terminal = loadedReceiver.terminalVoltage(source, secondaryWindingR, fs);
            return std::pair{(source - terminal) / secondaryWindingR, terminal};
        };
        const double capY=std::max(0.0,p.shuntC)/dt;
        const double lossY=1.0/std::max(1.0,p.coreLossR);
        const double b0=core.state().B;
        const double baseH=core.state().H;
        const double magnetizeScale=p.pathLength/std::max(1.0,p.turns);
        // Unknown H determines V across ideal coupled windings by Faraday's
        // law. KCL solves primary/secondary winding currents and core current
        // simultaneously, including leakage history and capacitive current.
        auto candidate=[&](double H) noexcept {
            const auto s=core.evolve(H);
            const double v=p.turns*p.coreArea*(s.B-b0)/dt;
            const double ip=y1*(input+induct1*primaryCurrent-v);
            const double is=secondary(v).first;
            const double ic=capY*(v-previousV);
            const double f=ip-is-magnetizeScale*H-ic-lossY*v;
            return f;
        };
        double span=5.0;
        double lo=baseH-span,hi=baseH+span;
        double flo=candidate(lo),fhi=candidate(hi);
        for(int i=0;i<12 && std::isfinite(flo) && std::isfinite(fhi)
               && (flo>0)==(fhi>0);++i) {
            span*=2;lo=baseH-span;hi=baseH+span;
            flo=candidate(lo);fhi=candidate(hi);
        }
        if(!std::isfinite(flo)||!std::isfinite(fhi)||(flo>0)==(fhi>0))
            return 0.0f;
        // Fast locally differentiated Newton iterations, *always* bracketed.
        // Unlike unguarded secant/false-position, a convergence check and
        // high-accuracy bisection fallback prevent low-level harmonic glitches.
        const bool increasing=fhi>flo;
        double guess=std::clamp(baseH,lo,hi);
        double f=candidate(guess);
        bool converged=std::isfinite(f) && std::abs(f)<1.e-11;
        for(int i=0;i<9 && !converged;++i) {
            const double eps=std::max(1.e-5,1.e-4*std::abs(guess));
            const double fp=candidate(guess+eps);
            const double slope=coupledReceiver ? (fp-f)/eps
                                               : (fp-candidate(guess-eps))/(2*eps);
            double proposed=std::abs(slope)>1.e-12 && std::isfinite(slope)
                ?guess-f/slope : .5*(lo+hi);
            const double guard=1.e-7*(hi-lo);
            if(!std::isfinite(proposed) || proposed<=lo+guard
               || proposed>=hi-guard)
                proposed=.5*(lo+hi);
            const double nextF=candidate(proposed);
            if(!std::isfinite(nextF))return 0.0f;
            guess=proposed;f=nextF;
            if((f>0)==increasing){hi=guess;fhi=f;}else{lo=guess;flo=f;}
            converged=std::abs(f)<1.e-11;
        }
        if(!converged) {
            for(int i=0;i<30;++i) {
                guess=.5*(lo+hi);
                f=candidate(guess);
                if(!std::isfinite(f))return 0.0f;
                if((f>0)==increasing)hi=guess;
                else lo=guess;
            }
            guess=.5*(lo+hi);
        }
        const auto state=core.evolve(guess);
        core.commit(state);
        const double v=p.turns*p.coreArea*(state.B-b0)/dt;
        primaryCurrent=y1*(input+induct1*primaryCurrent-v);
        const auto [current, terminal] = secondary(v);
        secondaryCurrent=current;
        previousV=v;
        // Secondary load voltage; WDF receiver damps HF winding parasitics.
        const double output=coupledReceiver ? loadedReceiver.commit(terminal, fs)
                                            : receiver.process(terminal);
        return std::isfinite(output)
            ?float(std::clamp(output/fullScalePeakVolts,-100.0,100.0)) : 0.0f;
    }
private:
    CoreParams p{};
    MagneticCore core;
    WdfReceiver receiver;
    LoadedReceiver loadedReceiver;
    bool coupledReceiver = false;
    double fs=44100,primaryCurrent=0,secondaryCurrent=0,previousV=0;
};

class DualTransformerGain {
public:
    void prepare(double fs) noexcept {
        CoreParams input;
        input.Ms=5.e5;input.a=100;input.k=150; input.sourceR=50;
        character.prepare(fs,input);
        CoreParams clean;
        clean.turns=2600;clean.coreArea=5.e-4;
        clean.Ms=7.e5;clean.a=30;clean.k=40;clean.c=.3;
        clean.sourceR=10;clean.primaryR=40;clean.secondaryR=40;
        clean.primaryLeak=20e-6;clean.secondaryLeak=20e-6;
        clean.receiverR=15;clean.receiverC=10.e-12;
        output.prepare(fs,clean);
    }
    void reset() noexcept {character.reset();output.reset();}
    void setInputDriveDb(double v) noexcept {drive=std::clamp(v,-18.,18.);}
    void setInterstageVolumeDb(double v) noexcept {middle=std::clamp(v,-24.,24.);}
    void setMakeupDb(double v) noexcept {makeup=std::clamp(v,-12.,24.);}
    void setOutputTrimDb(double v) noexcept {trim=std::clamp(v,-36.,24.);}
    void setLinked(bool v) noexcept {linked_=v;}
    bool linked() const noexcept {return linked_;}
    float processInput(float v) noexcept {
        return character.process(float(v*dbGain(drive)));
    }
    // Connect Zener limiter between these functions.
    float processOutput(float limiterOutput) noexcept {
        const auto intoOutput=limiterOutput*dbGain(middle+makeup);
        const float transformed=output.process(float(intoOutput));
        return float(transformed*dbGain(trim-(linked_?drive:0.)));
    }
private:
    Transformer character,output;
    double drive=0,middle=0,makeup=0,trim=0;
    bool linked_=true;
};
} // namespace nickel
