#pragma once
// Analog-opto circuit engine. NSL-32 LED/CdS cell topology and published
// reference points (500 ohms at 20 mA; dark 500 kohms after 10 s;
// conductance rise 3.5 ms, decay-to-100k ~500 ms) form initial parameters.
// Actual transfer curve, history and program timing require hardware fitting.
// No artificial harmonic injection, bypass gain multiplier, or digital
// envelope gain stage: audio is solved through a resistive LDR shunt network.
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace optocircuit {
constexpr double pi=3.1415926535897932384626433832795;
inline double dB(double x) noexcept { return std::pow(10.0,x/20.0); }
inline double toDb(double x) noexcept {return 20.0*std::log10(std::max(x,1.e-16));}
inline double tauRC(double seconds,double sampleTime) noexcept {
    return sampleTime/(std::max(1.e-7,seconds)+sampleTime);
}
struct OptoParts {
    double rSeries=10000.0, rReceive=47000.0, cellDark=5e5;
    double rAt20mA=500.0, ledReferenceA=0.020, maxLedA=.025;
    double ledIs=2.0e-18, ledN=2., thermalV=.02585;
    double ledSeriesOhms=220., ledSupply=12.;
    double attackCellSeconds=.0035, recoveryCellSeconds=.50;
    double chargeFast=.45, conductanceExponent=1.0;
    double rectifierIs=2.e-9, rectifierN=1.7;
    double detectorSeriesR=100., detectorLoadR=100000.;
    double detectorCapF=100e-9;  // physical coupling high-pass
    double detectorCapESR=0.25;
    double detectorPotR=100000., detectorPotWiper=0.5;
    double detectorHoldCapF=1e-6;
    double ledDriverRin=10000., ledDriverRfeedback=17500.; // detector amplifier gain
};
class HighPassCircuit {
public:
    void prepare(double hz,const OptoParts& part) noexcept {fs=hz;p=part;reset();}
    void reset() noexcept {capV=0.;}
    void setCornerHz(double hz) noexcept {corner=std::clamp(hz,20.,500.);}
    double process(double v) noexcept {
        // Variable resistance potentiometer in series with the input RC,
        // loaded by detector resistance; potentiometer is never an ideal EQ.
        const double load=std::max(100.,p.detectorLoadR);
        const double physicalCap=std::max(1e-9,p.detectorCapF);
        // Pot is the shunt resistance of the passive C-R high-pass.
        // Receiver's finite input resistance loads the pot in parallel.
        const double src=p.detectorSeriesR+p.detectorCapESR;
        const double target=1./(2.*pi*corner*physicalCap)-src;
        const double pot=std::clamp(
            target*load/std::max(1.,load-target),100.,p.detectorPotR);
        const double shunt=1./(1./load+1./pot);
        const double dt=1./fs;
        const double cur=(v-capV)/(src+shunt+dt/physicalCap);
        capV+=dt*cur/physicalCap;
        return cur*shunt;
    }
private:
    OptoParts p{};
    double capV=0.,fs=44100.,corner=120.;
};
// Physically attenuating LED/LDR resistor network, with optical light states.
// Light source LED Shockley current is solved by bisection against its supply,
// series resistor and applied control-voltage driver. Only cell conductance
// alters the audio KCL. No synthetic THD function.
class LdrCell {
public:
    void prepare(double sampleRate,OptoParts part={}) noexcept {
        p=part;fs=std::max(1000.,sampleRate);reset();
    }
    void reset() noexcept {
        lightFast=lightSlow=illumination=0.;
        audioCapV=0.; lastResistance=p.cellDark; lastGain=1.;
    }
    double ledCurrent(double control) const noexcept {
        const double voltage=std::clamp(control,0.,p.ledSupply);
        if(voltage<=0.)return 0.;
        const double vt=std::max(.01,p.ledN*p.thermalV);
        double lo=0.,hi=p.maxLedA;
        for(int i=0;i<24;++i) {
            const double current=.5*(lo+hi);
            const double vf=vt*std::log1p(current/p.ledIs);
            const double f=vf+current*p.ledSeriesOhms-voltage;
            if(f>0.)hi=current;else lo=current;
        }
        return .5*(lo+hi);
    }
    double process(double audioVolts,double ledControlVolts) noexcept {
        const double current=ledCurrent(ledControlVolts);
        const double lightTarget=std::pow(std::max(0.,current)/
                            std::max(1e-8,p.ledReferenceA),
                            p.conductanceExponent);
        // Two physical photoconductive relaxation states. Their light-memory
        // evolution is slower when discharging, with the slow trap population
        // additionally dependent on accumulated illumination.
        const double dt=1./fs;
        const bool charging=lightTarget>illumination;
        const double tFast=charging?p.attackCellSeconds:
            std::max(p.recoveryCellSeconds*.13,.006);
        const double tSlow=charging?p.attackCellSeconds*2.:
            p.recoveryCellSeconds*(1.+std::min(2.,lightSlow));
        lightFast+=tauRC(tFast,dt)*(lightTarget-lightFast);
        lightSlow+=tauRC(tSlow,dt)*(lightTarget-lightSlow);
        illumination=std::max(0.,p.chargeFast*lightFast+
                                   (1.-p.chargeFast)*lightSlow);
        const double gdark=1./p.cellDark;
        const double gfull=1./p.rAt20mA;
        const double conductance=gdark+(gfull-gdark)*illumination;
        lastResistance=1./std::max(gdark,conductance);
        // KCL: output node connected to voltage source through Rseries,
        // LDR to ground, and receiver impedance to ground.
        const double sourceG=1./p.rSeries;
        const double loadG=1./p.rReceive;
        const double v=audioVolts*sourceG/
                       (sourceG+loadG+1./lastResistance);
        const double clean=sourceG/(sourceG+loadG+gdark);
        lastGain=(v/(std::abs(audioVolts)>1e-15?audioVolts:1.))/clean;
        return v/clean; // normalize only fixed passive insertion loss
    }
    double resistance() const noexcept {return lastResistance;}
    double gainReduction() const noexcept {
        const double g=1./p.rSeries,r=1./p.rReceive,gd=1./p.cellDark;
        const double on=g/(g+r+1./lastResistance);
        const double off=g/(g+r+gd);
        return std::max(0.,-toDb(on/off));
    }
private:
    OptoParts p{};
    double fs=44100.,lightFast=0.,lightSlow=0.,illumination=0.,
           audioCapV=0.,lastResistance=5e5,lastGain=1.;
};
// Detector consists of physical AC coupling, Shockley-like rectifier,
// and asymmetric charge/discharge of an actual RC storage capacitor.
// Attack and release change R; the capacitor is retained.
class OpticalDetector {
public:
    void prepare(double sr,OptoParts part={}) noexcept {
        p=part;fs=std::max(1000.,sr);filter.prepare(fs,p);reset();
    }
    void reset() noexcept {filter.reset(); storageV=0.;}
    void setSidechainHpF(bool value) noexcept {hpEnabled=value;}
    void setCornerHz(double x) noexcept {filter.setCornerHz(x);}
    void setAttackMs(double v) noexcept {attackMs=std::clamp(v,1.,100.);}
    void setReleaseMs(double v) noexcept {releaseMs=std::clamp(v,50.,2000.);}
    double process(double volts) noexcept {
        const double ac=hpEnabled?filter.process(volts):volts;
        // Symmetric diode bridge: solve diode current against detector
        // source resistor and hold-cap voltage. The bridge is off if the
        // applied amplitude cannot overcome its diode forward drop.
        const double applied=std::abs(ac);
        const double vt=p.rectifierN*.02585;
        const double rCharge=std::max(10.,attackMs*.001/
                                   std::max(1e-9,p.detectorHoldCapF));
        const double rRelease=std::max(10.,releaseMs*.001/
                                    std::max(1e-9,p.detectorHoldCapF));
        const double dt=1./fs;
        double charging=0.;
        if(applied>storageV) {
            double lo=0.,hi=applied/rCharge;
            for(int n=0;n<12;++n) {
                const double current=.5*(lo+hi);
                const double vf=2*vt*std::log1p(current/p.rectifierIs);
                if(current*rCharge+vf+storageV>applied)hi=current;
                else lo=current;
            }
            charging=.5*(lo+hi);
        }
        // Capacitor KCL; discharge through release resistor.
        storageV=std::max(0.,storageV+
              dt*(charging-storageV/rRelease)/p.detectorHoldCapF);
        return storageV;
    }
    double voltage() const noexcept {return storageV;}
private:
    OptoParts p{};
    HighPassCircuit filter;
    double fs=44100.,storageV=0.,attackMs=15.,releaseMs=250.;
    bool hpEnabled=false;
};
class OpticalCompressor {
public:
    void prepare(double hz,OptoParts part={}) noexcept {
        p=part;fs=hz;detector.prepare(fs,p);cell.prepare(fs,p);reset();
    }
    void reset() noexcept {detector.reset();cell.reset();ledControl=lastOutput=0.;}
    void setAttackMs(double x) noexcept {detector.setAttackMs(x);}
    void setReleaseMs(double x) noexcept {detector.setReleaseMs(x);}
    void setSidechainHpF(bool x) noexcept {detector.setSidechainHpF(x);}
    void setSidechainHz(double x) noexcept {detector.setCornerHz(x);}
    void setThresholdDb(double x) noexcept {threshold=std::clamp(x,-45.,18.);}
    void setRatio(double x) noexcept {ratio=std::clamp(x,1.5,8.);}
    void setDetectorDrive(double x) noexcept {detectorDrive=std::clamp(x,.1,20.);}
    float process(float x,float sidechain) noexcept {
        const double detected=detector.process(double(sidechain)*
                                                  17.3677*detectorDrive);
        const double thresholdV=.775*dB(threshold);
        // Detector amplifier drives physical LED through a 220R resistor.
        // Ratio adjusts analog loop gain, not a gain multiplier on audio.
        const double above=std::max(0.,detected-thresholdV);
        // Noninverting detector driver gain is determined by an actual
        // feedback/input resistor ratio and ratio-switch feedback factor.
        const double amplifierGain=p.ledDriverRfeedback/
                                    std::max(1.,p.ledDriverRin);
        const double ratioFeedback=1.+(ratio-1.)*.25;
        const double driven=std::clamp(above*amplifierGain*ratioFeedback,
                                     0.,p.ledSupply);
        ledControl=driven;
        const double y=cell.process(double(x)*17.3677,ledControl);
        lastOutput=y/17.3677;
        return float(lastOutput);
    }
    // A physical feedback compressor senses its attenuator output. The
    // previous output sample is fed to the RC detector (one-sample loop delay).
    float process(float x) noexcept {return process(x,float(lastOutput));}
    float lastAttenuatorOutput() const noexcept {return float(lastOutput);}
    double reductionDb() const noexcept {return cell.gainReduction();}
    double ldrOhms() const noexcept {return cell.resistance();}
    double detectorVolts() const noexcept {return detector.voltage();}
private:
    OptoParts p{};
    double fs=44100.,threshold=-18.,ratio=3.,detectorDrive=1.,ledControl=0.,lastOutput=0.;
    OpticalDetector detector;
    LdrCell cell;
};
} // namespace optocircuit
