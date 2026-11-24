#pragma once
// Option C: clean input driver -> existing transformer -> optical attenuator
// (integration point) -> complementary BJT makeup/output buffer.
// Experimental discrete circuit equations, NOT measured transistor hardware.
// No nonlinear waveshaper/harmonic injection. Output waveform is obtained by
// solving parameterized BJT emitter currents against coupling-cap and output loads.\n// Published 2N5088/2N5087 SPICE cards are in 2N5088_2N5087_models.lib.\n// This is a reduced real-time BJT approximation, not a full SPICE simulator.
#include <algorithm>
#include <cmath>
#include "NickelTransformer.hpp"
namespace nickel {
struct OutputStageParams {
    double rail=15.0;            // +/-15 V rails
    double biasVbe=0.73;         // nominal base bias per transistor
    double saturationMargin=1.5; // safe initial voltage limit per rail
    double thermalVoltage=0.02585;
    // BJT IS, BF, IKF, VAF, RB/RC and junction C are device-specific.\n    // bias is set by the actual driver resistor network, not by the .model card.
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
        inputCapV=outputCapV=daV=0.;
    }
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
        auto transistorCurrent=[&](double overdrive,double collectorToEmitter,
                                  const SpiceBjtAudioParams& transistor) noexcept {
            // Real-time Gummel-Poon-derived forward-active approximation:
            // individual Is, finite beta, beta rolloff IKF, Early VAF, internal
            // resistances. Explicit full-SPICE reverse junction/charge equations
            // are NOT used here; reference cards are retained for calibration.
            if(overdrive<=0.0)return 0.0;
            const double vt=std::max(.005,p.thermalVoltage);
            const double re=std::max(.01,p.emitterR+transistor.re
                                      +transistor.rb/std::max(1.,transistor.bf));
            const double early=std::clamp(1.+std::max(0.,collectorToEmitter)/
                                          std::max(1.,transistor.vaf),1.,2.);
            double i=std::clamp(overdrive/re,0.,.2);
            for(int j=0;j<8;++j) {
                const double z=std::clamp((overdrive-i*re)/vt,-40.,35.);
                const double g=transistor.is*std::exp(z)*early;
                const double knee=std::max(1e-6,transistor.ikf);
                const double factor=1.0+i/knee;
                const double beta=transistor.bf/factor;
                const double correction=1.0+1.0/std::max(1.,beta);
                const double modeled=g*correction/factor;
                const double f=i-modeled;
                const double dg=modeled*(-re/vt - 1.0/(knee*factor))
                    +g/(factor*transistor.bf*knee);
                const double df=1.0-dg;
                const double step=std::clamp(f/std::max(1e-12,df),-.1,.1);
                i=std::clamp(i-step,0.,.2);
                if(std::abs(step)<1.e-12)break;
            }
            return i;
        };
        // KCL for follower output node. Note cap load is calculated as an
        // implicit backward-Euler capacitor companion, not an EQ or saturator.
        auto kcl=[&](double node) noexcept {
            const double npn=transistorCurrent(upper-node);
            const double pnp=transistorCurrent(node-lower);
            const double loadCurrent=(node-outputCapV)/capR;
            return npn-pnp-loadCurrent;
        };
        const double lim=std::max(1.,p.rail-p.saturationMargin);
        double low=-lim,high=lim;
        // Bracketed KCL solve, no unbounded nonlinear exponential.
        for(int j=0;j<24;++j) {
            const double mid=(low+high)*.5;
            if(kcl(mid)>0.)low=mid;else high=mid;
        }
        const double node=(low+high)*.5;
        const double current=(node-outputCapV)/capR;
        outputCapV+=dt/co*current;
        const double out=current*p.outputLoadR;
        return std::isfinite(out)?float(out/fullScalePeakVolts):0.0f;
    }
private:
    OutputStageParams p{};
    double fs=44100.,inputCapV=0.,outputCapV=0.,daV=0.;
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
    void setLinked(bool x) noexcept {linked=x;}\n    void setOutputLoadOhms(double r) noexcept {driver.setOutputLoadOhms(r);}\n    double outputLoadOhms() const noexcept {return driver.outputLoadOhms();}
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
