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
#include <optional>
namespace analogopto {
class MonoEngine {
public:
    void prepare(double sampleRate, bool useMkII = false) noexcept {
        fs=sampleRate;
        nickel::CoreParams core{};
        core.Ms=5e5;core.a=100.;core.k=150.;core.sourceR=50.;
        xfmr.prepare(fs,core,useMkII);
        low.prepare(fs);
        comp.prepare(fs,{},useMkII);
        if (useMkII) {
            legacyOutput.reset();
        } else {
            if (!legacyOutput) legacyOutput.emplace();
            legacyOutput->prepare(fs);
        }
        reset();
    }
    void reset() noexcept {
        xfmr.reset();low.reset();comp.reset();
        if (legacyOutput) legacyOutput->reset();
    }
    void setInputDb(double db) noexcept {
        drive=std::clamp(db,-18.,18.);
        inputGain=nickel::dbGain(drive);
        updateOutputGain();
    }
    void setMakeupDb(double db) noexcept {makeupGain=nickel::dbGain(std::clamp(db,-12.,18.));}
    void setOutputTrimDb(double db) noexcept {
        trim=std::clamp(db,-24.,24.);
        updateOutputGain();
    }
    void setLinkedAutoGain(bool link) noexcept {linked=link;updateOutputGain();}
    void setLinearGains(double input,double makeup,double outputTrim) noexcept {
        inputGain=input;
        makeupGain=makeup;
        outputGain=outputTrim;
    }
    void setLoadOhms(double r) noexcept {
        if (legacyOutput) legacyOutput->setOutputLoadOhms(r);
    }
    void setAttackMs(double x) noexcept {comp.setAttackMs(x);}
    void setReleaseMs(double x) noexcept {comp.setReleaseMs(x);}
    void setThresholdDb(double x) noexcept {comp.setThresholdDb(x);}
    void setFeedforward(bool b) noexcept {comp.setFeedforward(b);}
    void setLimiter(bool b) noexcept {comp.setLimiter(b);}
    void setOptoCell(int kind) noexcept {
        comp.setCellKind(static_cast<optocircuit::CellKind>(std::clamp(kind,0,3)));
    }
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
    double meterGr() const noexcept {return comp.meterReductionDb();}
    double detectorPreFraction() const noexcept {return comp.detectorPreFraction();}
    double ledCompPreFraction() const noexcept {return comp.ledCompPreFraction();}
    double lowMidGr() const noexcept {return low.reductionDb();}
    double photocellResistance() const noexcept {return comp.ldrOhms();}
    int outputSolverFailures() const noexcept {return legacyOutput ? legacyOutput->failureCount() : 0;}
    float lastOpticalOutput() const noexcept {return comp.lastAttenuatorOutput();}
    float processMagnetic(float input) noexcept {
        // The optional multiband stage has a buffered 47 kΩ input.
        // Otherwise the receiver drives the optical divider directly.
        return xfmr.process(float(input*inputGain), low.isEnabled() ? 47000. : comp.audioInputOhms());
    }
    float processLowMid(float magnetic,float external,bool useExternal) noexcept {
        return low.process(magnetic,external,useExternal);
    }
    float lowMidDetector(float magnetic) noexcept {
        return float(low.detectBand(magnetic));
    }
    double detectLowMidPower(float sample) noexcept {return low.detectOwnPower(sample);}
    float renderLowMid(float sample, double control) noexcept {return low.renderLinked(sample, control);}
    float processBeforeOpto(float input) noexcept {
        const float magnetic=processMagnetic(input);
        return low.process(magnetic);
    }
    float processFromOpto(float prepared,float stereoDetector,
                          bool externallyLinked) noexcept {
        const float attenuated=externallyLinked?
                     comp.process(prepared,stereoDetector):comp.process(prepared);
        return processOutput(attenuated);
    }
    double detectOptical(float audio) noexcept {return comp.detectOwn(audio);}
    double ledCurrentForControl(double control) const noexcept {
        return comp.ledCurrentForControl(control);
    }
    float renderOptical(float audio,double control,double sharedLedCurrent) noexcept {
        return processOutput(comp.render(audio,control,sharedLedCurrent));
    }
    float processOutput(float attenuated) noexcept {
        const double level = attenuated * makeupGain;
        if (legacyOutput)
            return float(legacyOutput->process(float(level)) * outputGain);
        return float(level * outputGain);
    }
    float process(float input) noexcept {
        const float prepared=processBeforeOpto(input);
        return processFromOpto(prepared,prepared,false);
    }
private:
    void updateOutputGain() noexcept {outputGain=nickel::dbGain(trim-(linked?drive:0.));}
    double fs=44100,drive=0,trim=0;
    double inputGain=1., makeupGain=1., outputGain=1.;
    bool linked=true;
    nickel::Transformer xfmr;
    lowmid::DynamicLowMid low;
    optocircuit::OpticalCompressor comp;
    std::optional<nickel::NodalOutputDriver> legacyOutput{std::in_place};
};
// Channel-independent nonlinear audio path, linked RMS/optical detection.
// The detector sees whichever channel has the larger instantaneous amplitude
// (with sign restored for rectification); opposite-phase L/R won't cancel.
class StereoEngine {
public:
    void prepare(double sr, bool useMkII = false) noexcept {
        mkII = useMkII;
        left.prepare(sr,mkII);
        right.prepare(sr,mkII);
    }
    void reset() noexcept {left.reset();right.reset();}
    void setInputDb(double v) noexcept {left.setInputDb(v);right.setInputDb(v);}
    void setMakeupDb(double v) noexcept {left.setMakeupDb(v);right.setMakeupDb(v);}
    void setLinearGains(double input,double makeup,double outputTrim) noexcept {
        left.setLinearGains(input,makeup,outputTrim);
        right.setLinearGains(input,makeup,outputTrim);
    }
    void setAttackMs(double v) noexcept {left.setAttackMs(v);right.setAttackMs(v);}
    void setReleaseMs(double v) noexcept {left.setReleaseMs(v);right.setReleaseMs(v);}
    void setThresholdDb(double v) noexcept {left.setThresholdDb(v);right.setThresholdDb(v);}
    void setFeedforward(bool b) noexcept {feedforward=b;left.setFeedforward(b);right.setFeedforward(b);}
    void setLimiter(bool b) noexcept {limiter=b;left.setLimiter(b);right.setLimiter(b);}
    void setOptoCell(int kind) noexcept {optoCell=kind;left.setOptoCell(kind);right.setOptoCell(kind);}
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
        if (mkII) {
            const double lowControl = std::max(left.detectLowMidPower(ml), right.detectLowMidPower(mr));
            const float pl = left.renderLowMid(ml, lowControl);
            const float pr = right.renderLowMid(mr, lowControl);
            // Both detector paths finish before the control bus is combined.
            // The isolated high-select bus drives identical optical attenuators.
            const double controlL = left.detectOptical(pl);
            const double controlR = right.detectOptical(pr);
            const double control = std::max(controlL, controlR);
            // Matched LED drivers fed by the same voltage draw the same current.
            const double ledCurrent = optoCell == 2 ? -1. : left.ledCurrentForControl(control);
            l = left.renderOptical(pl, control, ledCurrent);
            r = right.renderOptical(pr, control, ledCurrent);
            return;
        }
        const float dl=left.lowMidDetector(ml);
        const float dr=right.lowMidDetector(mr);
        const float linkedLow=std::abs(dl)>=std::abs(dr)?dl:dr;
        const float pl=left.processLowMid(ml,linkedLow,true);
        const float pr=right.processLowMid(mr,linkedLow,true);
        // A physical stereo-linked feedback sum contains both pre- and
        // post-attenuator signals at the SAME audio instant. The former
        // previousDetector path mixed n and n-1, producing fictitious
        // near-Nyquist cancellation at 44.1k. Predict today's attenuated
        // waveforms using last sample's slow optical conductance, then choose
        // the larger-magnitude channel for a properly linked detector.
        const float beforeOpto=std::abs(pl)>=std::abs(pr)?pl:pr;
        const float postL=float(pl*nickel::dbGain(-left.opticalGr()));
        const float postR=float(pr*nickel::dbGain(-right.opticalGr()));
        const float afterOpto=std::abs(postL)>=std::abs(postR)?postL:postR;
        float detector=feedforward?beforeOpto:afterOpto;
        if(!limiter && optoCell!=2 && !feedforward) {
            const double f=left.ledCompPreFraction();
            detector=float(f*beforeOpto+(1.-f)*afterOpto);
        }
        if(optoCell==2 && !feedforward) {
            const double f=left.detectorPreFraction();
            detector=float(f*beforeOpto+(1.-f)*afterOpto);
        }
        l=left.processFromOpto(pl,detector,true);
        r=right.processFromOpto(pr,detector,true);
    }
    double grLeft() const noexcept {return left.opticalGr();}
    double grRight() const noexcept {return right.opticalGr();}
    double meterGr() const noexcept {return left.meterGr();}
    double lowGrLeft() const noexcept {return left.lowMidGr();}
    double lowGrRight() const noexcept {return right.lowMidGr();}
private:
    MonoEngine left,right;
    bool feedforward=false,limiter=false,mkII=false;
    int optoCell=0;
};
} // namespace analogopto
