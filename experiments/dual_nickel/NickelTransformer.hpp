#pragma once
// Experimental circuit-coupled magnetic prototype, NOT calibrated to physical hardware.
// This is a reduced nonlinear magnetizing branch; it does not yet implement Jiles-Atherton
// hysteresis, leakage L, distributed capacitance or a true WDF circuit.
#include <algorithm>
#include <cmath>

namespace nickel {
inline double dbGain(double db) noexcept { return std::pow(10.0,db/20.0); }

class Transformer {
public:
    struct Model {
        double sourceR=50.0, windingR=40.0, loadR=10000.0;
        double magnetizingL=10.0, saturationA=0.015, turns=1.0;
    };
    void prepare(double sampleRate,Model model) noexcept {
        fs=std::max(1000.0,sampleRate);
        m=model;
        m.sourceR=std::max(.01,m.sourceR);
        m.windingR=std::max(.01,m.windingR);
        m.loadR=std::max(1.0,m.loadR);
        m.magnetizingL=std::max(.001,m.magnetizingL);
        m.saturationA=std::max(1.e-8,m.saturationA);
        m.turns=std::max(.01,m.turns);
        reset();
    }
    void reset() noexcept { im=0.0; }
    float process(float x) noexcept {
        const double r=m.sourceR+m.windingR;
        const double g=1.0/r+m.turns*m.turns/m.loadR;
        const double h=1.0/fs;
        double i=im;
        // Backward-Euler nonlinear magnetizing-branch current + source/load KCL.
        for(int k=0;k<8;++k) {
            const double v=(double(x)/r-i)/g;
            const double n=std::clamp(i/m.saturationA,-12.0,12.0);
            const double shape=1.0+n*n;
            const double f=i-im-h/m.magnetizingL*v*shape;
            const double df=1.0-h/m.magnetizingL*(-shape/g+v*2.0*n/m.saturationA);
            if(std::abs(df)<1.e-12) break;
            const double delta=std::clamp(f/df,-.01,.01);
            i=std::clamp(i-delta,-1.0,1.0);
            if(std::abs(delta)<1.e-10) break;
        }
        if(!std::isfinite(i)) {reset();return 0.0f;}
        im=i;
        const double v=(double(x)/r-i)/g;
        const double y=v/m.turns;
        return std::isfinite(y)?float(std::clamp(y,-100.0,100.0)):0.0f;
    }
private:
    Model m{};
    double fs=44100.0,im=0.0;
};

class DualTransformerGain {
public:
    void prepare(double fs) noexcept {
        Transformer::Model in;
        in.magnetizingL=10.0; in.saturationA=.015;
        input.prepare(fs,in);
        Transformer::Model out;
        out.sourceR=10.0;out.windingR=80.0;
        out.magnetizingL=100.0;out.saturationA=.2;
        output.prepare(fs,out);
    }
    void reset() noexcept {input.reset();output.reset();}
    void setInputDriveDb(double x) noexcept {drive=std::clamp(x,-12.0,18.0);}
    void setInterstageVolumeDb(double x) noexcept {middle=std::clamp(x,-24.0,24.0);}
    void setMakeupDb(double x) noexcept {makeup=std::clamp(x,-12.0,24.0);}
    void setOutputTrimDb(double x) noexcept {trim=std::clamp(x,-36.0,24.0);}
    void setLinked(bool x) noexcept {link=x;}
    bool linked() const noexcept {return link;}
    // Place limiter between processInput and processOutput.
    float processInput(float x) noexcept {
        return input.process(float(double(x)*dbGain(drive)));
    }
    float processOutput(float limiterOutput) noexcept {
        const float y=output.process(float(double(limiterOutput)*dbGain(middle+makeup)));
        // Gain compensation AFTER output transformer preserves independent output drive.
        return float(double(y)*dbGain(trim-(link?drive:0.0)));
    }
private:
    Transformer input,output;
    double drive=0.0,middle=0.0,makeup=0.0,trim=0.0;
    bool link=true;
};
} // namespace nickel
