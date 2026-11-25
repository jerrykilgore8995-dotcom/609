#pragma once
// Passive RC sidechain high-pass feeding an ON Semiconductor 2N5088
// common-collector output stage. Filter is referenced to a stiff bypassed
// bias supply so its shunt potentiometer cannot pull the transistor bias.
// Nonlinear B-E junction and beta-limited base current are stamped directly
// into a two-node implicit KCL solve (base and emitter). No digital HPF or
// signal-dependent GR/timing curve. Manufacturer 2N5088 Is/Bf/Nf/Ise/Ne
// parameters from the onsemi 2N5088-D model in NodalBjtOutput.hpp.
// The emitter is capacitively AC-coupled in the physical topology; we
// remove its solved DC operating point at output, equivalent above ~2Hz.
#include <algorithm>
#include <cmath>
namespace optocircuit {
class SidechainFollower {
public:
    void prepare(double sr) noexcept {fs=std::max(1000.,sr);reset();}
    void reset() noexcept {
        capV=0.;base=.73;emitter=0.;dcEmitter=0.;failed=0;
        // Find actual quiescent emitter potential, not a fabricated 0.7V drop.
        for(int i=0;i<48;++i)solve(0.,false);
        dcEmitter=emitter;
    }
    void setCutoffHz(double hz) noexcept {cutoff=std::clamp(hz,20.,500.);}
    void setEnabled(bool v) noexcept {enabled=v;}
    // Input and output are real volts, with +/-15V analog supply.
    double process(double vin) noexcept {
        solve(std::clamp(vin,-12.,12.),true);
        return std::clamp(emitter-dcEmitter,-12.,12.);
    }
    int failures() const noexcept{return failed;}
private:
    static constexpr double is=5.911e-15, bf=1122.,nf=1.;
    static constexpr double ise=5.911e-15, ne=1.394;
    static constexpr double vt=.02585, cap=.1e-6;
    static constexpr double rbias=5.e6,bias=.73;
    static constexpr double re=15000.,rload=100000.;
    static constexpr double sourceR=100.25;
    double fs=44100.,cutoff=100.,capV=0.,base=.73,emitter=0.,dcEmitter=0.;
    bool enabled=false;
    int failed=0;
    double pot() const noexcept {
        // 100nF capacitor, 100k potentiometer, and shunt of the 5M
        // DC-bias network. The knob selects the physical resistor.
        const double req=1./(2.*3.14159265358979323846*cutoff*cap);
        return std::clamp(req*rbias/std::max(1.,rbias-req),
                          100.,100000.);
    }
    void solve(double signal,bool advance) noexcept {
        const double dt=1./fs;
        const double rpot=enabled?pot():1.e30;
        const double rcap=sourceR+dt/cap;
        double b=base,e=emitter;
        // The high-pass is a physical capacitor connected from the
        // unbiased signal source to the biased base+pot node. In HPF OFF
        // mode, a mechanical bypass contact feeds the base through 100R.
        const double drive=signal+bias;
        const double sourceConductance=enabled?1./rcap:1./sourceR;
        const double oldCap=enabled?capV:0.;
        for(int n=0;n<12;++n){
            const double vbe=std::clamp(b-e,-1.,1.);
            const double eb=std::exp(std::clamp(vbe/(nf*vt),-30.,32.));
            const double en=std::exp(std::clamp(vbe/(ne*vt),-30.,32.));
            const double ib=is*(eb-1.)/bf+ise*(en-1.);
            const double dic=is*eb/(nf*vt);
            const double dib=dic/bf+ise*en/(ne*vt);
            const double ie=is*(eb-1.)+ib;
            const double f0=(b-bias)*(1./rpot+1./rbias)+ib-
                     (drive-b-oldCap)*sourceConductance;
            const double f1=(e+15.)/re+(e-dcEmitter)/rload-ie;
            const double a=1./rpot+1./rbias+dib+sourceConductance;
            const double off=-dib;
            const double d=-dic-dib;
            const double h=1./re+1./rload+dic+dib;
            const double det=a*h-off*d;
            if(!(det>1e-20)||!std::isfinite(det)){++failed;break;}
            const double db=(-f0*h+off*f1)/det;
            const double de=(-a*f1+d*f0)/det;
            // Damped Newton to avoid jumping across the exponential
            // on a high transient input. Compact physics-preserving solve.
            const double step=std::min(1.,.16/std::max(1.e-15,
                                     std::abs(db-de)));
            b=std::clamp(b+step*db,-13.5,13.5);
            e=std::clamp(e+step*de,-14.,13.);
            if(std::max(std::abs(step*db),std::abs(step*de))<1e-9)break;
        }
        base=b;emitter=e;
        if(enabled&&advance){
          const double current=(drive-b-capV)/rcap;
          capV+=dt*current/cap;
        }
        if(!enabled)capV=0.;
    }
};
}
