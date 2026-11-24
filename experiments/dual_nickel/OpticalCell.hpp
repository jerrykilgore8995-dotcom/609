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
// Three distinct optical assemblies: a calibrated NSL-32 reference,
// a manufacturer-datasheet VTL5C2, and an EL-driven dual-CdS T4B-style unit.
enum class CellKind : int { NSL32=0, VTL5C2=1, T4B=2 };
constexpr double pi=3.1415926535897932384626433832795;
inline double dB(double x) noexcept { return std::pow(10.0,x/20.0); }
inline double toDb(double x) noexcept {return 20.0*std::log10(std::max(x,1.e-16));}
inline double tauRC(double seconds,double sampleTime) noexcept {
    return sampleTime/(std::max(1.e-7,seconds)+sampleTime);
}
struct OptoParts {
    double rSeries=10000.0, rReceive=47000.0, cellDark=1e6;
    double rAt20mA=500.0, ledReferenceA=0.020, maxLedA=.025;
    double ledIs=2.0e-18, ledN=2., thermalV=.02585;
    double ledSeriesOhms=220., ledSupply=12.;
    // A discrete 2N5088 shunt clamp across the LED driver's finite source
    // impedance. Divider from +15 V biases the limiter's emitter.
    // High transient LED current turns the clamp on gradually through
    // the transistor's physical B-E junction; no digital GR or LED cap.
    double ledSourceOhms=47., ledLimiterRail=15.;
    double ledLimiterTopOhms=117000.,ledLimiterBottomOhms=15000.;
    double ledLimiterIs=5.911e-15,ledLimiterN=1.;

    // NSL-32 at ~16mA: 3.5ms to 63% illuminated conductance, and
    // ~500ms to 100k after LED turn-off (not an exponential tau).
    // Datasheet dark resistance is >=500k at 10s, NOT an asymptotic 500k.
    // Separate slow trap population has a much longer charge time.
    // Fast/slow parameters beyond those two datasheet points are provisional
    // fitting values, NOT measured individual-cell curves.
    double attackCellSeconds=.0035, fastDarkRecoverySeconds=.085;
    double trapChargeSeconds=.70, trapRecoverySeconds=1.50;
    double trapConductanceFraction=.005, conductanceExponent=1.0;
    double rectifierIs=2.e-9, rectifierN=1.7;
    double detectorSeriesR=100., detectorLoadR=100000.;
    double detectorCapF=100e-9;  // physical coupling high-pass
    double detectorCapESR=0.25;
    double detectorPotR=100000., detectorPotWiper=0.5;
    double detectorHoldCapF=1e-6;
    double ledDriverRin=10000., ledDriverRfeedback=25000.; // detector amplifier gain
    // Feedforward topology samples before the attenuator and engages a real
    // resistor pad at the detector amplifier input; feedback does not.
    double feedForwardSeriesR=3300., feedForwardShuntR=10000.;
    // A diode shunting the feedforward detector amplifier's feedback
    // resistor produces a genuinely nonlinear, gradual knee at high level.
    // The diode's reference is generated from a +15 V resistor divider.
    double feedForwardDiodeIs=2e-9,feedForwardDiodeN=1.7;
    double feedForwardBiasRail=15.,feedForwardBiasTopR=130000.;
    double feedForwardBiasBottomR=15000.;
    // T4 electroluminescent panel coupling (not an LED). These are
    // explicitly provisional driver parameters until a T4 sample is swept:
    // high-voltage panel amplifier / AC excitation threshold.
    double elPanelVoltageGain=45., elPanelTurnOnVac=35.;
    double elPanelReferenceVac=115.,elPanelCapF=5e-9;
    double elPanelDriveR=47000., elPanelRiseSeconds=.0008;
    double elPanelFallSeconds=.003;

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
        audioCapV=panelAudioV=0.;panelLight=panelVoltage=meterLight=0.; lastResistance=darkResistance(); lastGain=1.;lastLedCurrent=0.;
    }
    void select(CellKind kind) noexcept {
        if(kind==cellKind)return;
        cellKind=kind;reset();
    }
    CellKind selected() const noexcept {return cellKind;}
    double darkResistance() const noexcept {
        return cellKind==CellKind::VTL5C2?1000000.:
               cellKind==CellKind::T4B?10000000.:p.cellDark;
    }
    static double logInterpolate(double x,double x1,double y1,
                                 double x2,double y2) noexcept {
        const double f=std::clamp(
            std::log(std::max(x,1e-12)/x1)/std::log(x2/x1),0.,1.);
        return std::exp(std::log(y1)+f*(std::log(y2)-std::log(y1)));
    }
    double physicalConductance(double milliamps) const noexcept {
        if(cellKind==CellKind::VTL5C2) {
            // Published VTL5C2 light-adapted ON resistances, per datasheet.
            // 1mA: 5.5k, 10mA: 800, 40mA: 200. Between points: log-log
            // interpolation; below 1mA extrapolates power law toward dark.
            double r=0.;
            if(milliamps<=0.)return 1./darkResistance();
            if(milliamps<1.)
                r=5500.*std::pow(1./milliamps,
                     std::log(5500./800.)/std::log(10.));
            else if(milliamps<=10.)
                r=logInterpolate(milliamps,1.,5500.,10.,800.);
            else r=logInterpolate(std::min(40.,milliamps),
                                  10.,800.,40.,200.);
            return 1./std::min(darkResistance(),r);
        }
        // NSL-32 is calibrated around 500 ohms at 20mA.
        return 1./darkResistance()+
            (1./p.rAt20mA-1./darkResistance())*
            std::max(0.,milliamps)/20.;
    }
    double processAtPanelVac(double audioVolts,double panelVac) noexcept {
        // Unlike an LED Vactrol, the T4 EL phosphor responds to AC
        // excitation voltage above its activation threshold. Driver gain
        // feeds the panel through an actual RC electrical network.
        const double dt=1./fs;
        // EL plates are driven by alternating voltage; keep polarity
        // through the panel RC, then convert excitation magnitude to light.
        const double targetVoltage=std::clamp(panelVac,-500.,500.);
        panelVoltage+=tauRC(p.elPanelDriveR*p.elPanelCapF,dt)*
                          (targetVoltage-panelVoltage);
        const double supply=std::abs(panelVoltage);
        const double lightTarget=std::pow(std::max(0.,supply-
                 p.elPanelTurnOnVac)/std::max(1.,p.elPanelReferenceVac-
                 p.elPanelTurnOnVac),2.);
        const double time=lightTarget>panelLight?p.elPanelRiseSeconds:
                                                  p.elPanelFallSeconds;
        panelLight+=tauRC(time,dt)*(lightTarget-panelLight);
        // Separate CdS metering element illuminated by the SAME EL source.
        // Its dark recovery is independent of the audio attenuation cell.
        meterLight+=tauRC(lightTarget>meterLight?.012:.19,dt)*
                             (panelLight-meterLight);
        // The T4 has two photocells; the gain photocell is modeled here.
        // EL and CdS response are separate states, so its recovery is not
        // inherited from an LED.
        return processIllumination(audioVolts,panelLight,0.,true);
    }
    double ledCurrent(double control) const noexcept {
        const double voltage=std::clamp(control,0.,p.ledSupply);
        if(voltage<=0.)return 0.;
        const double ledVt=std::max(.01,p.ledN*p.thermalV);
        const double bjtVt=std::max(.01,p.ledLimiterN*p.thermalV);
        const double reference=p.ledLimiterRail*p.ledLimiterBottomOhms/
            std::max(1.,p.ledLimiterTopOhms+p.ledLimiterBottomOhms);
        double lo=0.,hi=cellKind==CellKind::VTL5C2?.040:p.maxLedA;
        for(int i=0;i<24;++i) {
            const double current=.5*(lo+hi);
            const double ledNode=ledVt*std::log1p(current/p.ledIs)
                                  +current*p.ledSeriesOhms;
            const double vbe=ledNode-reference;
            const double shunt=p.ledLimiterIs*
                std::max(0.,std::exp(std::clamp(vbe/bjtVt,-45.,35.))-1.);
            // Kirchhoff voltage/current relation for the LED, source
            // resistance and shunt transistor. The limiter is not in the
            // audio gain path, only the real optical illumination circuit.
            const double f=ledNode+p.ledSourceOhms*(current+shunt)-voltage;
            if(f>0.)hi=current;else lo=current;
        }
        return .5*(lo+hi);
    }
    double process(double audioVolts,double ledControlVolts) noexcept {
        return processAtLedCurrent(audioVolts,ledCurrent(ledControlVolts));
    }
    // Datasheet calibration injects the specified current DIRECTLY into the
    // optical LED. This isolates the CdS photocell from the compressor's
    // intentionally current-limited LED driver.
    double processAtLedCurrent(double audioVolts,double ledAmps) noexcept {
        const double current=std::clamp(ledAmps,0.,
            cellKind==CellKind::VTL5C2?.040:p.maxLedA);
        lastLedCurrent=current;
        if(cellKind==CellKind::T4B)
            return processAtPanelVac(audioVolts,0.);
        const double normalized=cellKind==CellKind::VTL5C2?
            physicalConductance(current*1000.)/
            physicalConductance(20.):current/p.ledReferenceA;
        return processIllumination(audioVolts,
                   std::max(0.,normalized),current,false);
    }
    double processIllumination(double audioVolts,double intensity,
                               double current,bool isPanel) noexcept {
        const double lightTarget=std::pow(std::max(0.,intensity),
                                          p.conductanceExponent);
        // CdS photoconductance consists of a prompt carrier population
        // and a physically separate slowly occupied trap population.
        // Crucially the slow trap CHARGES over hundreds of milliseconds
        // rather than being fully charged after a short musical transient.
        // The trap's small conductance fraction controls residual recovery
        // without slowing initial conduction or faking an output envelope.
        const double dt=1./fs;
        const double rise=isPanel?.010:
                         cellKind==CellKind::VTL5C2?.0035:p.attackCellSeconds;
        const double fastDecay=isPanel?.065:
                               cellKind==CellKind::VTL5C2?.086:
                                                     p.fastDarkRecoverySeconds;
        const double trapRise=isPanel?.65:
                              cellKind==CellKind::VTL5C2?.45:
                                                    p.trapChargeSeconds;
        const double trapDecay=isPanel?2.8:
                               cellKind==CellKind::VTL5C2?1.0:
                                                    p.trapRecoverySeconds;
        const double tFast=lightTarget>lightFast?rise:fastDecay;
        const double tTrap=lightTarget>lightSlow?trapRise:trapDecay;
        lightFast+=tauRC(tFast,dt)*(lightTarget-lightFast);
        lightSlow+=tauRC(tTrap,dt)*(lightTarget-lightSlow);
        const double w=isPanel?.012:
             cellKind==CellKind::VTL5C2?.002:
             std::clamp(p.trapConductanceFraction,0.,.1);
        illumination=std::max(0.,(1.-w)*lightFast+w*lightSlow);
        const double gdark=1./darkResistance();
        const double gfull=isPanel?1./750.:
                           cellKind==CellKind::VTL5C2?
                           physicalConductance(20.):1./p.rAt20mA;
        const double conductance=gdark+(gfull-gdark)*illumination;
        lastResistance=1./std::max(gdark,conductance);
        // KCL: output node connected to voltage source through Rseries,
        // LDR to ground, and receiver impedance to ground.
        const double sourceG=1./p.rSeries;
        const double loadG=1./p.rReceive;
        // VTL5C2 manufacturer specifies about 5 pF output-cell
        // capacitance. Stamp its finite dynamic admittance alongside
        // the LDR shunt, rather than adding a post-audio filter.
        const double cellCap=cellKind==CellKind::VTL5C2?5.e-12:0.;
        const double capG=cellCap*fs;
        const double v=(audioVolts*sourceG+capG*panelAudioV)/
                       (sourceG+loadG+1./lastResistance+capG);
        panelAudioV=v;
        const double clean=sourceG/(sourceG+loadG+gdark);
        lastGain=(v/(std::abs(audioVolts)>1e-15?audioVolts:1.))/clean;
        return v/clean; // normalize only fixed passive insertion loss
    }
    double resistance() const noexcept {return lastResistance;}
    double actualLedAmps() const noexcept {return lastLedCurrent;}
    double meterPhotocellOhms() const noexcept {
        return cellKind==CellKind::T4B?
           1./(1./darkResistance()+std::max(0.,meterLight)/1200.):
           lastResistance;
    }
    double gainReduction() const noexcept {
        const double g=1./p.rSeries,r=1./p.rReceive,gd=1./darkResistance();
        const double on=g/(g+r+1./lastResistance);
        const double off=g/(g+r+gd);
        return std::max(0.,-toDb(on/off));
    }
private:
    OptoParts p{};
    double fs=44100.,lightFast=0.,lightSlow=0.,illumination=0.,
           audioCapV=0.,panelAudioV=0.,panelLight=0.,panelVoltage=0.,meterLight=0.,lastResistance=5e5,lastGain=1.,lastLedCurrent=0.;
    CellKind cellKind=CellKind::NSL32;
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
    void setAttackMs(double v) noexcept {attackMs=std::clamp(v,10.,100.);}
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
        // Diode steering: the attack branch charges only during bridge
        // conduction; otherwise the storage capacitor discharges through the
        // release pot. A permanently parallel release shunt incorrectly
        // changes the compressor's steady-state threshold at every setting.
        const double releaseCurrent=charging>1.e-7?0.:storageV/rRelease;
        storageV=std::max(0.,storageV+
              dt*(charging-releaseCurrent)/p.detectorHoldCapF);
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
    void setFeedforward(bool enabled) noexcept {feedforward=enabled;}
    void setCellKind(CellKind kind) noexcept {
        if(cell.selected()==kind)return;
        cell.select(kind);detector.reset();ledControl=lastOutput=0.;
    }
    CellKind selectedCellKind() const noexcept {return cell.selected();}
    float process(float x,float sidechain) noexcept {
        if(cell.selected()==CellKind::T4B) {
            // A T4B is illuminated by an *AC*-excited EL panel, not by
            // rectified DC LED current. Feed the high-voltage driver with
            // sidechain audio (pre- or post-cell, according to FF switch).
            // Threshold controls sidechain amplifier sensitivity.
            const double gain=p.elPanelVoltageGain*
                 dB(-18.-threshold)*(1.+(ratio-1.)*.25);
            const double panelAc=double(sidechain)*17.3677*gain;
            const double y=cell.processAtPanelVac(double(x)*17.3677,
                                                  panelAc);
            lastOutput=y/17.3677;
            return float(lastOutput);
        }
        const double detectorPad=feedforward?
            p.feedForwardShuntR/(p.feedForwardSeriesR+
                                p.feedForwardShuntR):1.0;
        const double detected=detector.process(double(sidechain)*
                                                  17.3677*detectorDrive*
                                                  detectorPad);
        const double thresholdV=.775*dB(threshold);
        // Detector amplifier drives physical LED through a 220R resistor.
        // Ratio adjusts analog loop gain, not a gain multiplier on audio.
        const double above=std::max(0.,detected-thresholdV);
        // Noninverting detector driver gain is determined by an actual
        // feedback/input resistor ratio and ratio-switch feedback factor.
        const double amplifierGain=p.ledDriverRfeedback/
                                    std::max(1.,p.ledDriverRin);
        const double ratioFeedback=1.+(ratio-1.)*.25;
        const double target=std::max(0.,above*amplifierGain*ratioFeedback);
        double source=target;
        if(feedforward) {
            // The detector amplifier's inverting resistor-feedback branch is
            // shunted by a biased diode. KCL: Iin=Vout/Rfeedback+Idiode.
            // This is *not* a synthetic gain curve on the audio channel.
            const double bias=p.feedForwardBiasRail*p.feedForwardBiasBottomR/
                std::max(1.,p.feedForwardBiasTopR+p.feedForwardBiasBottomR);
            const double vt=std::max(.01,p.feedForwardDiodeN*p.thermalV);
            double lo=0.,hi=std::min(target,p.ledSupply);
            for(int n=0;n<18;++n) {
                const double trial=.5*(lo+hi);
                const double diode=p.feedForwardDiodeIs*
                    std::max(0.,std::exp(std::clamp((trial-bias)/vt,-40.,35.))-1.);
                const double voltageEquivalent=trial+p.ledDriverRfeedback*diode;
                if(voltageEquivalent>target)hi=trial;else lo=trial;
            }
            source=.5*(lo+hi);
        }
        ledControl=std::clamp(source,0.,p.ledSupply);
        const double y=cell.process(double(x)*17.3677,ledControl);
        lastOutput=y/17.3677;
        return float(lastOutput);
    }
    // A physical feedback compressor senses its attenuator output. The
    // previous output sample is fed to the RC detector (one-sample loop delay).
    float process(float x) noexcept {return process(x,feedforward?x:float(lastOutput));}
    float lastAttenuatorOutput() const noexcept {return float(lastOutput);}
    double reductionDb() const noexcept {return cell.gainReduction();}
    double ldrOhms() const noexcept {return cell.resistance();}
    double meterCellOhms() const noexcept {return cell.meterPhotocellOhms();}
    double detectorVolts() const noexcept {return detector.voltage();}
    double ledDriveVolts() const noexcept {return ledControl;}
    double ledAmps() const noexcept {return cell.actualLedAmps();}
private:
    OptoParts p{};
    double fs=44100.,threshold=-18.,ratio=3.,detectorDrive=1.,ledControl=0.,lastOutput=0.;
    bool feedforward=false;
    OpticalDetector detector;
    LdrCell cell;
};
} // namespace optocircuit
