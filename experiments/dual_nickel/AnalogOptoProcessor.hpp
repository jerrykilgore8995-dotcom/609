#pragma once
// End-to-end mono audio engine, shared building block for a stereo linked
// detector. Clean source -> nonlinear nickel transformer -> C4-style low-mid
// dynamic band -> LED/photocell compressor -> discrete BJT makeup/output.
// No extra output transformer. All three analog nonlinearity modules actually
// run; no compressor placeholder or static gain reduction inserted.
#include "NickelTransformer.hpp"
#include "DynamicLowMid.hpp"
#include "OpticalCell.hpp"
#include "DiscreteOutputDriver.hpp"
#include <algorithm>
#include <cmath>
namespace analogopto {
class MonoEngine {
public:
    void prepare(double sampleRate) noexcept {
        fs=sampleRate;
        nickel::CoreParams core{};
        core.Ms=5e5;core.a=100.;core.k=150.;core.sourceR=50.;
        xfmr.prepare(fs,core);
        low.prepare(fs);
        comp.prepare(fs);
        output.prepare(fs);
        reset();
    }
    void reset() noexcept {
        xfmr.reset();low.reset();comp.reset();output.reset();
    }
    void setInputDb(double db) noexcept {drive=std::clamp(db,-18.,18.);}
    void setMakeupDb(double db) noexcept {makeup=std::clamp(db,-12.,18.);}
    void setOutputTrimDb(double db) noexcept {trim=std::clamp(db,-24.,24.);}
    void setLinkedAutoGain(bool link) noexcept {linked=link;}
    void setLoadOhms(double r) noexcept {output.setOutputLoadOhms(r);}
    void setAttackMs(double x) noexcept {comp.setAttackMs(x);}
    void setReleaseMs(double x) noexcept {comp.setReleaseMs(x);}
    void setThresholdDb(double x) noexcept {comp.setThresholdDb(x);}
    void setRatio(double x) noexcept {comp.setRatio(x);}
    void setScHpf(bool enable) noexcept {comp.setSidechainHpF(enable);}
    void setScHpfHz(double hz) noexcept {comp.setSidechainHz(hz);}
    void setPalmMute(bool enable) noexcept {low.setEnabled(enable);}
    void setPalmCenterHz(double hz) noexcept {low.setCenterHz(hz);}
    void setPalmBandwidthOct(double o) noexcept {low.setWidthOctaves(o);}
    void setPalmRangeDb(double d) noexcept {low.setMaxCutDb(d);}
    void setPalmThresholdDb(double d) noexcept {low.setThresholdDb(d);}
    void setPalmAttackMs(double v) noexcept {low.setAttackMs(v);}
    void setPalmReleaseMs(double v) noexcept {low.setReleaseMs(v);}
    double opticalGr() const noexcept {return comp.reductionDb();}
    double lowMidGr() const noexcept {return low.reductionDb();}
    double photocellResistance() const noexcept {return comp.ldrOhms();}
    double outputLoad() const noexcept {return output.outputLoadOhms();}
    float process(float input) noexcept {
        const float magnetic=xfmr.process(float(input*nickel::dbGain(drive)));
        const float band=low.process(magnetic);
        const float attenuated=comp.process(band);
        const float driverOut=output.process(float(attenuated*nickel::dbGain(makeup)));
        return float(driverOut*nickel::dbGain(trim-(linked?drive:0.)));
    }
private:
    double fs=44100,drive=0,makeup=0,trim=0;
    bool linked=true;
    nickel::Transformer xfmr;
    lowmid::DynamicLowMid low;
    optocircuit::OpticalCompressor comp;
    nickel::DiscreteOutputDriver output;
};
} // namespace analogopto
