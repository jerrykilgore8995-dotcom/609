#pragma once
// End-to-end mono audio engine, shared building block for a stereo linked
// detector. Clean source -> nonlinear nickel transformer -> C4-style low-mid
// dynamic band -> LED/photocell compressor -> discrete BJT makeup/output.
// No extra output transformer. All three analog nonlinearity modules actually
// run; no compressor placeholder or static gain reduction inserted.
#include "NickelTransformer.hpp"
#include "DynamicLowMid.hpp"
#include "OpticalCell.hpp"
#include "NodalBjtOutput.hpp"
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
    void setFeedforward(bool b) noexcept {comp.setFeedforward(b);}
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
    float lastOpticalOutput() const noexcept {return comp.lastAttenuatorOutput();}
    double outputLoad() const noexcept {return output.outputLoadOhms();}
    float processMagnetic(float input) noexcept {
        return xfmr.process(float(input*nickel::dbGain(drive)));
    }
    float processLowMid(float magnetic,float external,bool useExternal) noexcept {
        return low.process(magnetic,external,useExternal);
    }
    float lowMidDetector(float magnetic) noexcept {
        return float(low.detectBand(magnetic));
    }
    float processBeforeOpto(float input) noexcept {
        const float magnetic=processMagnetic(input);
        return low.process(magnetic);
    }
    float processFromOpto(float prepared,float stereoDetector,
                          bool externallyLinked) noexcept {
        const float attenuated=externallyLinked?
                     comp.process(prepared,stereoDetector):comp.process(prepared);
        const float driverOut=output.process(float(attenuated*nickel::dbGain(makeup)));
        return float(driverOut*nickel::dbGain(trim-(linked?drive:0.)));
    }
    float process(float input) noexcept {
        const float prepared=processBeforeOpto(input);
        return processFromOpto(prepared,prepared,false);
    }
private:
    double fs=44100,drive=0,makeup=0,trim=0;
    bool linked=true;
    nickel::Transformer xfmr;
    lowmid::DynamicLowMid low;
    optocircuit::OpticalCompressor comp;
    nickel::NodalOutputDriver output;
};
// Channel-independent nonlinear audio path, linked RMS/optical detection.
// The detector sees whichever channel has the larger instantaneous amplitude
// (with sign restored for rectification); opposite-phase L/R won't cancel.
class StereoEngine {
public:
    void prepare(double sr) noexcept {left.prepare(sr);right.prepare(sr);}
    void reset() noexcept {left.reset();right.reset();previousDetector=0.;}
    void setInputDb(double v) noexcept {left.setInputDb(v);right.setInputDb(v);}
    void setMakeupDb(double v) noexcept {left.setMakeupDb(v);right.setMakeupDb(v);}
    void setAttackMs(double v) noexcept {left.setAttackMs(v);right.setAttackMs(v);}
    void setReleaseMs(double v) noexcept {left.setReleaseMs(v);right.setReleaseMs(v);}
    void setThresholdDb(double v) noexcept {left.setThresholdDb(v);right.setThresholdDb(v);}
    void setRatio(double v) noexcept {left.setRatio(v);right.setRatio(v);}
    void setFeedforward(bool b) noexcept {feedforward=b;left.setFeedforward(b);right.setFeedforward(b);}
    void setScHpf(bool v) noexcept {left.setScHpf(v);right.setScHpf(v);}
    void setScHpfHz(double v) noexcept {left.setScHpfHz(v);right.setScHpfHz(v);}
    void setPalmMute(bool v) noexcept {left.setPalmMute(v);right.setPalmMute(v);}
    void setPalmCenterHz(double v) noexcept {
        left.setPalmCenterHz(v);right.setPalmCenterHz(v);
    }
    void setPalmBandwidthOct(double v) noexcept {
        left.setPalmBandwidthOct(v);right.setPalmBandwidthOct(v);
    }
    void setPalmRangeDb(double v) noexcept {
        left.setPalmRangeDb(v);right.setPalmRangeDb(v);
    }
    void setPalmThresholdDb(double v) noexcept {
        left.setPalmThresholdDb(v);right.setPalmThresholdDb(v);
    }
    void setPalmAttackMs(double v) noexcept {
        left.setPalmAttackMs(v);right.setPalmAttackMs(v);
    }
    void setPalmReleaseMs(double v) noexcept {
        left.setPalmReleaseMs(v);right.setPalmReleaseMs(v);
    }
    void setLoadOhms(double v) noexcept {left.setLoadOhms(v);right.setLoadOhms(v);}
    void setLinkedAutoGain(bool b) noexcept {
        left.setLinkedAutoGain(b);right.setLinkedAutoGain(b);
    }
    void setOutputTrimDb(double v) noexcept {
        left.setOutputTrimDb(v);right.setOutputTrimDb(v);
    }
    void process(float& l,float& r) noexcept {
        const float ml=left.processMagnetic(l);
        const float mr=right.processMagnetic(r);
        const float dl=left.lowMidDetector(ml);
        const float dr=right.lowMidDetector(mr);
        const float linkedLow=std::abs(dl)>=std::abs(dr)?dl:dr;
        const float pl=left.processLowMid(ml,linkedLow,true);
        const float pr=right.processLowMid(mr,linkedLow,true);
        // Linked feedback is taken after each LDR attenuator, not from
        // its input. Preserve one sample of detector delay for the physical
        // rectifier/storage capacitor loop.
        const float detector=feedforward?
            (std::abs(pl)>=std::abs(pr)?pl:pr):previousDetector;
        l=left.processFromOpto(pl,detector,true);
        r=right.processFromOpto(pr,detector,true);
        const float ol=left.lastOpticalOutput(),or_=right.lastOpticalOutput();
        previousDetector=std::abs(ol)>=std::abs(or_)?ol:or_;
    }
    double grLeft() const noexcept {return left.opticalGr();}
    double grRight() const noexcept {return right.opticalGr();}
    double lowGrLeft() const noexcept {return left.lowMidGr();}
    double lowGrRight() const noexcept {return right.lowMidGr();}
private:
    MonoEngine left,right;
    float previousDetector=0.f;
    bool feedforward=false;
};
} // namespace analogopto
