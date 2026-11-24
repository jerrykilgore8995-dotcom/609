#pragma once
// Option C: clean input driver -> existing transformer -> optical attenuator
// (integration point) -> complementary BJT makeup/output buffer.
// Experimental discrete circuit equations, NOT measured transistor hardware.
// No nonlinear waveshaper/harmonic injection. Output waveform is obtained by
// solving parameterized BJT emitter currents against coupling-cap and output loads.
// Published 2N5088/2N5087 SPICE cards are in 2N5088_2N5087_models.lib.
// This is a reduced real-time BJT approximation, not a full SPICE simulator.
#include <algorithm>
#include <cmath>
#include "NickelTransformer.hpp"
namespace nickel {
struct SpiceBjtAudioParams {
    double is, bf, ikf, vaf, rb, re, rc, cje, cjc, tf;
};
// Core model parameters transcribed from published SPICE cards, not
// independently calibrated against physical transistor samples.
inline constexpr SpiceBjtAudioParams npn2N5088{
    5.911e-15,1122.,0.01492,62.37,10.,0.15,1.61,
    4.973e-12,4.017e-12,821.7e-12
};
inline constexpr SpiceBjtAudioParams pnp2N5087{
    6.734e-15,254.1,0.1962,45.7,10.,0.,1.67,
    7.5e-12,6.2e-12,467.8e-12
};
struct OutputStageParams {
    double rail=15.0;            // +/-15 V rails
    double biasVbe=0.73;         // nominal base bias per transistor
    double saturationMargin=1.5; // safe initial voltage limit per rail
    double thermalVoltage=0.02585;
    // BJT IS, BF, IKF, VAF, RB/RC and junction C are device-specific.
    // bias is set by the actual driver resistor network, not by the .model card.
    double emitterR=22.0;       // NPN/PNP local degeneration
    double inputCapF=47e-6, inputESR=0.4, inputLeakR=2e6;
    double inputSourceR=100.0, inputLoadR=47000.0;
    double outputCapF=100e-6, outputESR=0.15;
    double outputLoadR=10000.0; // switchable 600 / 10000 ohms
    double dielectricFraction=0.01, dielectricTime=0.15;
};
class DiscreteOutputDriver {
public:
    void prepare(double rate,OutputStageParams params={}) noexcept {
        fs=std::max(1000.,rate);
        p=params; reset();
    }
    void reset() noexcept {
        inputCapV=outputCapV=daV=previousNode=0.;
    }
    void setOutputLoadOhms(double r) noexcept {
        p.outputLoadR=(r<=1500.0)?600.0:10000.0;
    }
    double outputLoadOhms() const noexcept {return p.outputLoadR;}
    // DC-bias and signal-preserving cap network, with series ESR and a
    // slow first-order dielectric absorption branch (approximation).
    float process(float normalized) noexcept {
        const double dt=1./fs;
        const double vin=double(normalized)*fullScalePeakVolts;
        const double ri=std::max(1.,p.inputSourceR+p.inputLoadR+p.inputESR);
        const double ci=std::max(1.e-9,p.inputCapF);
        const double iIn=(vin-inputCapV)/(ri+dt/ci);
        inputCapV+=dt/ci*iIn;
        const double signal=iIn*p.inputLoadR;
        // Slow dielectric polarization responds to capacitor voltage,
        // changing the effective charge/history under large excursions.
        const double frac=std::clamp(p.dielectricFraction,0.,0.1);
        const double oldDa=daV;
        daV+=(inputCapV-daV)*dt/(std::max(1.e-3,p.dielectricTime)+dt);
        const double base=signal-frac*(daV-oldDa);
        const double upper=std::clamp(base+p.biasVbe,-p.rail,p.rail);
        const double lower=std::clamp(base-p.biasVbe,-p.rail,p.rail);
        const double rc=std::max(1.,p.outputLoadR+p.outputESR);
        const double co=std::max(1.e-9,p.outputCapF);
        const double capR=rc+dt/co;
        struct CurrentSlope {double i=0.0, slope=0.0;};
        auto transistorCurrent=[&](double overdrive,double vce,
                    const SpiceBjtAudioParams& d) noexcept -> CurrentSlope {
            // Log-domain solution of reduced Gummel-Poon forward-active
            // branch with series emitter resistance and beta roll-off.
            // Solving logarithms avoids repeated exponential overflows.
            const double vt=std::max(.005,p.thermalVoltage);
            const double re=std::max(.01,p.emitterR+d.re
                           +d.rb/std::max(1.,d.bf));
            const double knee=std::max(1e-6,d.ikf);
            const double early=std::clamp(1.+std::max(0.,vce)/
                                     std::max(1.,d.vaf),1.,2.);
            const double initial=std::max(1e-13,
                 (overdrive-0.62)/re+std::max(0.,overdrive)*1e-5);
            double i=std::clamp(initial,1e-13,0.2);
            for(int n=0;n<8;++n) {
                const double inverseBeta=(1.+i/knee)/std::max(1.,d.bf);
                const double corr=1.+inverseBeta;
                const double f=std::log(i/d.is)+i*re/vt
                   +std::log1p(i/knee)-std::log(early)
                   -std::log(corr)-overdrive/vt;
                const double df=1./i+re/vt+1./(knee+i)
                   -1./(knee*std::max(1.,d.bf)*corr);
                double proposal=i-f/df;
                if(!std::isfinite(proposal))proposal=i*0.5;
                // Safeguard positivity and fast convergence for low currents.
                proposal=std::clamp(proposal,i*.2,i*4.);
                proposal=std::clamp(proposal,1e-16,.2);
                if(std::abs(proposal-i)<1e-13) {i=proposal;break;}
                i=proposal;
            }
            const double beta=d.bf/(1.+i/knee);
            const double corr=1.+1./std::max(1.,beta);
            const double deriv=1./i+re/vt+1./(knee+i)
                        -1./(knee*std::max(1.,d.bf)*corr);
            return {i,1./(vt*std::max(1e-12,deriv))};
        };
        auto kcl=[&](double node) noexcept {
            const auto npn=transistorCurrent(upper-node,p.rail-node,npn2N5088);
            const auto pnp=transistorCurrent(node-lower,p.rail+node,pnp2N5087);
            const double loadCurrent=(node-outputCapV)/capR;
            const double f=npn.i-pnp.i-loadCurrent;
            const double df=-npn.slope-pnp.slope-1./capR;
            return std::pair<double,double>{f,df};
        };
        const double lim=std::max(1.,p.rail-p.saturationMargin);
        double lo=-lim,hi=lim;
        double node=std::clamp(previousNode,lo,hi);
        bool converged=false;
        for(int j=0;j<12;++j) {
            const auto value=kcl(node);
            if(!std::isfinite(value.first)||!std::isfinite(value.second))
                 return 0.0f;
            if(std::abs(value.first)<1e-10) {converged=true;break;}
            if(value.first>0.)lo=node;else hi=node;
            double next=node-value.first/value.second;
            if(!std::isfinite(next)||next<=lo||next>=hi)
                next=.5*(lo+hi);
            node=next;
        }
        if(!converged) {
            // Rare bounded fallback, still no free-running unstable Newton.
            for(int j=0;j<22;++j) {
                const double v=kcl(node).first;
                if(v>0.)lo=node;else hi=node;
                node=.5*(lo+hi);
            }
        }
        previousNode=node;
        const double current=(node-outputCapV)/capR;
        outputCapV+=dt/co*current;
        const double out=current*p.outputLoadR;
        return std::isfinite(out)?float(out/fullScalePeakVolts):0.0f;
    }
private:
    OutputStageParams p{};
    double fs=44100.,inputCapV=0.,outputCapV=0.,daV=0.,previousNode=0.;
};

// The optical cell is deliberately NOT substituted with an arbitrary
// compressor here: processOpticalOutput receives the future circuit's output.
// The input transformer is the only transformer; output is all BJT.
class OptionCSignalPath {
public:
    void prepare(double rate) noexcept {
        CoreParams in{};
        in.Ms=5.e5;in.a=100.;in.k=150.;in.sourceR=50.;
        transformer.prepare(rate,in);
        driver.prepare(rate);
    }
    void reset() noexcept {transformer.reset();driver.reset();}
    void setInputDriveDb(double x) noexcept {drive=std::clamp(x,-18.,18.);}
    void setMakeupDb(double x) noexcept {makeup=std::clamp(x,-12.,18.);}
    void setOutputTrimDb(double x) noexcept {trim=std::clamp(x,-24.,24.);}
    void setLinked(bool x) noexcept {linked=x;}
    void setOutputLoadOhms(double r) noexcept {driver.setOutputLoadOhms(r);}
    double outputLoadOhms() const noexcept {return driver.outputLoadOhms();}
    float processInput(float x) noexcept {
        return transformer.process(float(x*dbGain(drive)));
    }
    float processOpticalOutput(float x) noexcept {
        return float(driver.process(float(x*dbGain(makeup)))*
                     dbGain(trim-(linked?drive:0.)));
    }
private:
    Transformer transformer;
    DiscreteOutputDriver driver;
    double drive=0.,makeup=0.,trim=0.;
    bool linked=true;
};
} // namespace nickel
