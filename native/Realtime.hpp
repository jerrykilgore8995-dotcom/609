#pragma once

#include "Controls.hpp"
#include "BassTransient.hpp"
#include "RealtimeTables.hpp"
#include "SpectralCorrection.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <numbers>

namespace mkii::native {

class Realtime {
public:
    using Frame=std::array<double,2>;

    void prepare(double sampleRate) {
        rate=sampleRate;
        bassTransient.prepare(rate);
        preampLeak=std::exp(-2*std::numbers::pi*.18/rate);
        transformerGain=-std::expm1(-2*std::numbers::pi*14.9953891/rate);
        biasFollow=-std::expm1(-1/(rate*0.00940118177));
        limitGainAverage=-std::expm1(-1/(rate*.003));
        limitTrimRelease=-std::expm1(-1/(rate*.00025));
        channels={};
        previousGain={};
        constexpr double baseRate=44100;
        const double dc=-std::expm1(-2*std::numbers::pi*1.76037857/rate);
        const double harmonicDc=-std::expm1(-2*std::numbers::pi*9/rate);
        // Bilinear-transform the reference's two high-frequency sections
        // for the current host clock. At 44.1 kHz this is an identity.
        sections[0]=retune({.87855588,-.20367940,-.04843757,-.12000398,.04963765},rate/baseRate);
        sections[1]=retune({1.,-.22176785,.05458820,-.48687517,.04769670},rate/baseRate);
        highFrequency=fitAudioShelf();
        const double initPeriod=rate/(2*1000);
        for (auto& ch:channels) {
            ch.spectral.prepare(rate);
            ch.dc=dc;
            ch.harmonicDc=harmonicDc;
            ch.period=initPeriod;
        }
        detectorDrop=std::exp(-1/(rate*.0006));
        limitAverage=-std::expm1(-1/(rate*.00412868));
        slowAverage=-std::expm1(-1/(rate*.006));
        pulseFollow=-std::expm1(-1/(rate*.00023480));
        detectorLow=-std::expm1(-2*std::numbers::pi*230/rate);
        transientQuick=-std::expm1(-1/(rate*.00175));
        transientSlow=-std::expm1(-1/(rate*.026));
        initialized=false;
        for (auto& ch:channels) ch.charge=.582;
        control(defaults);
    }

    void control(const Controls& controls, int attackMode=1) {
        if (initialized && controls==previousControls && attackMode==previousAttackMode) return;
        initialized=true;
        previousControls=controls;
        previousAttackMode=attackMode;
        linked=controls[19]!=0;
        active=controls[18] && controls[20];
        for (int i=0;i<2;++i) {
            auto& ch=channels[i];
            const int off=i*9;
            ch.compress=controls[off+6]!=0;
            ch.limit=controls[off+7]!=0;
            const bool fast=controls[off+8]!=0;
            const int ct=std::clamp(controls[off],1,16)-1;
            const int cr=std::clamp(controls[off+1],1,5)-1;
            const int cm=std::clamp(controls[off+2],1,6)-1;
            const int cg=std::clamp(controls[off+3],1,11)-1;
            const int lt=std::clamp(controls[off+4],1,23)-1;
            const int lr=std::clamp(controls[off+5],1,6)-1;
            ch.ct=-20+ct*2;
            ch.lt=4+lt*.5;
            ch.spectral.configure(static_cast<int>(ch.ct),cr+1,ch.lt,lr,fast);
            bassTransient.configure(i,cr+1,ct+1,cm+1);
            ch.makeupDb=ch.compress?measured::makeup[cg]:0;
            const auto& compressor=measured::compressor[cr][ct];
            ch.limiter=measured::limiter[fast][lt];
            constexpr std::array compRelease{.190,.744,1.480,2.785,.35,.69};
            constexpr std::array limRelease{.022,.047,.0965,.396,.25,.44};
            ch.voltRelease=1/(rate*compRelease[cm]);
            ch.autoRelease=cm>=4;
            ch.chargeDeco=1/(rate*(cm==4?1.28:2.55));
            ch.attack=1/(rate*pulseAttack);
            ch.attackSpeed=attackMode==0?2.5:attackMode==2?.5:1.;
            ch.autoFast=-std::expm1(-1/(rate*(cm==4?.100:.050)));
            ch.autoSlow=-std::expm1(-1/(rate*(cm==4?2.0:5.0)));
            ch.memoryRise=-std::expm1(-1/(rate*.19));
            for (std::size_t k=0;k<ch.chargeCurve.size();++k) {
                const double reduction=std::max(0.0f,compressor[k]);
                const auto voltage=voltsForGain(reduction);
                const double db=ch.ct+measured::compLevels[k];
                const auto duty=std::clamp((voltage-.582)/(24-voltage)*(pulseAttack/(.190*pulseScale)),0.,.45);
                const double level=std::exp((db-reduction-24-.16933453)*.11512925464970228);
                double low=-pulseSoft,high=1;
                for (int i=0;i<26;++i) {
                    const auto middle=(low+high)*.5;
                    const auto estimated=(rectifiedSine(middle)-rectifiedSine(middle+pulseSoft))/pulseSoft;
                    if (estimated>duty) low=middle;
                    else high=middle;
                }
                ch.chargeCurve[k]={voltage,level*(low+high)*.5,level*pulseSoft};
            }
            std::size_t first=0;
            while (first+1<ch.chargeCurve.size() &&
                   ch.chargeCurve[first+1][0] <= voltsForGain(.072)) ++first;
            for (std::size_t k=0;k<first;++k) ch.chargeCurve[k]=ch.chargeCurve[first];
            for (std::size_t k=first+1;k<ch.chargeCurve.size();++k) {
                ch.chargeCurve[k][0]=std::max(ch.chargeCurve[k][0],ch.chargeCurve[k-1][0]+1e-6);
                ch.chargeCurve[k][1]=std::max(ch.chargeCurve[k][1],ch.chargeCurve[k-1][1]);
            }
            ch.lim.setup(rate,fast?.00065:.00142,limRelease[lr],lr>=4?(lr==4?.88:1.29):0);
        }
    }

    Frame step(Frame input) {
        Frame audio{},reductions{},compressor{},limiter{},limiterBand{};
        std::array<bool,2> compressEnabled{};
        std::array<measured::Point,2> distortion{};
        for (int i=0;i<2;++i) {
            auto& ch=channels[i];
            double x=input[i];
            for (double& state:ch.dcHistory) {
                state+=ch.dc*(x-state);
                x-=state;
            }
            for (int section=0;section<2;++section) x=sections[section].apply(x,ch.history[section]);
            audio[i]=x;
            // Recover a pre-transformer signal, then reapply the low-frequency
            // coupling after the time-varying gain in the output stage.
            ch.preampSignal=x+ch.preampMemory;
            ch.preampMemory=preampLeak*ch.preampMemory+transformerGain*x;
            const double amplitude=ch.detect(x,rate,detectorDrop);
            const double level=24+20*std::log10(std::max(amplitude,1e-6));
            const auto squared=x*x;
            ch.limPower+=limitAverage*(squared-ch.limPower);
            ch.slowPower+=slowAverage*(squared-ch.slowPower);
            const auto rms=[](double power){return 24+10*std::log10(std::max(2*power,1e-12));};
            measured::Point l{};
            const auto crest=std::clamp(level-rms(ch.slowPower),0.,12.);
            // The C12/Q2 and C7/Q5 detector stages are sensitive to the low-band
            // envelope and the signed peak history before the C13 charging path.
            ch.lowAudio+=detectorLow*(x-ch.lowAudio);
            ch.lowPower+=transientQuick*(ch.lowAudio*ch.lowAudio-ch.lowPower);
            ch.transientPower+=transientQuick*(squared-ch.transientPower);
            ch.slowTransientPower+=transientSlow*(squared-ch.slowTransientPower);
            const double lowFraction=std::clamp(ch.lowPower/(ch.transientPower+1e-10),0.0,1.0);
            const double powerChange=ch.transientPower/(ch.slowTransientPower+1e-10)-1;
            const double settled=std::clamp((.26-std::abs(powerChange))/.17,0.,1.);
            limiterBand[i]=std::clamp((.20-lowFraction)/.10,0.,1.)*settled;
            const double onset=std::clamp(powerChange,0.,3.);
            const double transient=lowFraction*onset;
            const double feedback=std::exp(-(previousGain[i]+.25983066*crest)*.11512925464970228);
            ch.detectorBias+=(x-ch.detectorBias)*(x>ch.detectorBias?transientQuick:transientSlow);
            const double bassEnvelope=std::sqrt(ch.lowPower);
            const double detectorShift=onset*(-.01552013*ch.lowAudio+
                .19172223*std::max(-ch.lowAudio,0.)+.06584739*bassEnvelope);
            const double weight=std::clamp((lowFraction-.25)*3,0.,1.);
            const auto relativeBass=std::sqrt(ch.lowPower/(ch.bassMemory+1e-8));
            ch.bassMemory+=biasFollow*(ch.lowPower-ch.bassMemory);
            const auto bias=std::clamp(relativeBass-1.,-1.,2.);
            const double detectorInput=x+detectorShift+.04709259*ch.detectorBias*lowFraction+
                (-0.642391231)*weight*(ch.preampSignal-x)+(-0.201981523)*weight*std::sqrt(ch.lowPower)*bias;
            const double detector=std::max(0.0,detectorInput)*feedback;
            double pulse=0;
            if (ch.compress) {
                auto [threshold,width]=pulseThreshold(ch.chargeCurve,ch.charge);
                pulse=std::clamp((detector-threshold)/std::max(width,1e-7),0.,1.);
            }
            const double baseline=ch.autoRelease?std::max(.582,ch.tailCharge):.582;
            ch.charge+= (pulse>0 && (pulse*(24-ch.charge)*ch.attack*pulseScale*std::max(.2,1-.15925579*transient+0.0523322195*weight*bias)>(ch.charge-baseline)*ch.voltRelease) ? ch.attackSpeed : 1.) * (pulse*(24-ch.charge)*ch.attack*pulseScale*std::max(.2,1-.15925579*transient+0.0523322195*weight*bias)-(ch.charge-baseline)*ch.voltRelease);
            ch.charge=std::clamp(ch.charge,.582,18.);
            if (ch.autoRelease) {
                const double target=.582+.61*(ch.charge-.582);
                ch.tailCharge+=(target-ch.tailCharge)*(target>ch.tailCharge?1/(rate*.19):ch.chargeDeco);
            } else ch.tailCharge=.582;
            const auto desired=ch.compress?gainForVolts(ch.charge):0.0;
            if (ch.autoRelease) {
                // Detect the end of the driven burst separately from the old
                // capacitor tail; recover in dB with two independent poles.
                const double target=ch.compress?desired:0.;
                const bool driven=pulse>0.0001;
                ch.recoveryHold=driven?int(rate*.003):std::max(0,ch.recoveryHold-1);
                if (ch.recoveryHold>0 && target>=ch.compSmooth) {
                    ch.compSmooth+=pulseFollow*(target-ch.compSmooth);
                    ch.fastEnvelope=ch.slowEnvelope=ch.compSmooth;
                    ch.programMemory+=(1-ch.programMemory)*ch.memoryRise;
                    ch.releaseWeight=std::clamp(.10+.65*ch.programMemory,.10,.75);
                } else if (ch.recoveryHold>0) {
                    ch.compSmooth+=pulseFollow*(target-ch.compSmooth);
                    ch.fastEnvelope=ch.slowEnvelope=ch.compSmooth;
                } else {
                    ch.fastEnvelope*=1-ch.autoFast;
                    ch.slowEnvelope*=1-ch.autoSlow;
                    ch.compSmooth=std::lerp(ch.fastEnvelope,ch.slowEnvelope,ch.releaseWeight);
                    ch.programMemory*=1-ch.autoSlow;
                }
            } else {
                ch.compSmooth+=pulseFollow*(desired-ch.compSmooth);
                ch.fastEnvelope=ch.slowEnvelope=ch.compSmooth;
            }
            const auto comp=ch.compSmooth;
            if (ch.limit) {
                // Blend the peak and smoothed detector depending on bass content.
                const double sensitivity=(-.57770876)*lowFraction*(std::clamp(ch.period*2/rate,.001,.028)-.001)*100;
                const double term=std::lerp(rms(ch.limPower),level,std::clamp(.88304952+.07024392*lowFraction,0.,1.));
                l=lookup(ch.limiter,measured::limitLevels,term+.01444734+ch.makeupDb-ch.lt+sensitivity-.27890904*transient);
            }
            const double limit=ch.lim.update(ch.limit?std::max(0.f,l.reduction):0);
            ch.limiterReduction=limit;
            compressor[i]=comp;
            limiter[i]=limit;
            compressEnabled[i]=ch.compress;
            reductions[i]=std::max(comp,limit);
            if (ch.limit) distortion[i]=limit>comp?l:measured::Point{};
        }
        if (linked) reductions[0]=reductions[1]=std::max(reductions[0],reductions[1]);
        previousGain=reductions;
        const auto bassCorrection=bassTransient.step(input,reductions,compressor,limiter,compressEnabled,linked);

        Frame result{};
        for (int i=0;i<2;++i) {
            auto& ch=channels[i];
            ch.reduction=active?std::max(0.,reductions[i]-bassCorrection[i]):0;
            ch.makeup+=std::min(1.0,240.0/rate)*(ch.makeupDb-ch.makeup);
            const double levelGain=std::exp((ch.makeup-(active?reductions[i]:0.))*.11512925464970228);
            const double preamp=ch.preampSignal*levelGain;
            const double afterTransformer=preamp-ch.transformerMemory;
            ch.transformerMemory=preampLeak*ch.transformerMemory+transformerGain*afterTransformer;
            const double wet=std::lerp(audio[i]*levelGain,afterTransformer,0.64284315);
            const auto& k=distortion[i];
            const double peak=std::max(ch.peak,1e-5);
            double harmonics=0,evenHarmonics=0;
            if (k.k2Cos!=0 || k.k2Sin!=0 || k.k3Cos!=0 || k.k3Sin!=0) {
                const double h=std::numbers::pi/std::max(ch.period,2.0);
                const double u=std::clamp(audio[i]/peak,-1.12,1.12);
                const double v=std::clamp((audio[i]*std::cos(h)-ch.previous)/(std::sin(h)*peak),-1.12,1.12);
                const double radius=std::min(1.,1.18/std::max(std::hypot(u,v),.5));
                const double sin1=radius*u,cos1=radius*v;
                const double cos2=cos1*cos1-sin1*sin1,sin2=2*sin1*cos1;
                const double cos3=cos1*cos2-sin1*sin2,sin3=sin1*cos2+cos1*sin2;
                evenHarmonics=peak*levelGain*(k.k2Cos*cos2-k.k2Sin*sin2);
                harmonics=peak*levelGain*(k.k2Cos*cos2-k.k2Sin*sin2+k.k3Cos*cos3-k.k3Sin*sin3);
            }
            ch.harmonicMemory+=ch.harmonicDc*(harmonics-ch.harmonicMemory);
            ch.evenMemory+=ch.harmonicDc*(evenHarmonics-ch.evenMemory);
            ch.previous=audio[i];
            const double drive=wet+harmonics-ch.harmonicMemory;
            double evenCorrection=0;
            const double correction=ch.spectral.step(audio[i],drive,ch.compSmooth,ch.limiterReduction,ch.compress,ch.limit,
                !linked || std::abs(limiter[0]-limiter[1])<.15,evenCorrection);
            const auto correctedWet=highFrequency.apply(wet,ch.highFrequencyHistory);
            double output=drive+correction+(correctedWet-wet);
            ch.averagedLimiterGain+=limitGainAverage*(levelGain-ch.averagedLimiterGain);
            const double referencePreamp=ch.preampSignal*ch.averagedLimiterGain;
            const double referenceTransformer=referencePreamp-ch.referenceTransformerMemory;
            ch.referenceTransformerMemory=preampLeak*ch.referenceTransformerMemory+transformerGain*referenceTransformer;
            if (active && ch.limit && !ch.compress && (!linked || !compressEnabled[1-i])) {
                // Isolate the nonlinear residual without changing the limiter's
                // detector, gain trajectory, HF shelf or calibrated bass path.
                const double gr=reductions[i];
                const double gainMotion=std::abs(levelGain-ch.averagedLimiterGain)/std::max(levelGain,1e-6);
                const double stationary=std::clamp((.12-gainMotion)/.07,0.,1.);
                const double target=limiterBand[i]*stationary*std::clamp((gr-3)/3,0.,1.);
                const double mixSpeed=target<ch.limiterTrimMix?limitTrimRelease:limitGainAverage;
                ch.limiterTrimMix+=mixSpeed*(target-ch.limiterTrimMix);
                const double mid=std::clamp((gr-7)/7,0.,1.);
                const double deep=std::clamp((gr-14)/3.65,0.,1.);
                const double odd=std::lerp(std::lerp(.49,.47,mid),.27,deep);
                const double even=std::lerp(std::lerp(.08,.03,mid),.11,deep);
                const double reference=std::lerp(audio[i]*ch.averagedLimiterGain,referenceTransformer,.64284315);
                const double evenResidual=evenHarmonics-ch.evenMemory+evenCorrection;
                output+=ch.limiterTrimMix*((1-odd)*(reference-drive-correction)+(even-odd)*evenResidual);
            } else ch.limiterTrimMix=0;
            result[i]=active?output*std::exp(bassCorrection[i]*.11512925464970228):input[i]*600/640.02;
        }
        return result;
    }

    Frame gainReduction() const {return {channels[0].reduction,channels[1].reduction};}

private:
    // C13 is charged by the positive peaks at TR8 and discharged by the
    // selected recovery resistor. The capacitor-to-attenuation law is
    // measured from the reference at steady state with the bridge in circuit.
    static constexpr double pulseAttack=.00605573,pulseScale=.87850081,pulseSoft=1.58256736;
    static constexpr std::array<double,26> states{
        .582,.906,1.7934,2.6358,3.16395,3.5535,3.9016,4.23668,4.56561,4.8899,5.21023,5.52775,
        5.84412,6.16098,6.48050,6.80486,7.13644,7.47785,7.83189,8.2017,8.59082,9.00315,
        9.44322,9.91599,10.4271,11.59141};
    static constexpr std::array<double,26> gains{
        0.,.013,.424,1.365,2.236,3.058,3.934,4.907,5.982,7.150,8.398,9.711,
        11.076,12.484,13.925,15.392,16.880,18.382,19.894,21.412,22.932,24.451,
        25.965,27.471,28.966,31.915};
    static double evaluate(const std::array<double,26>& x,const std::array<double,26>& y,double v) {
        if (v<=x.front()) return y.front();
        const auto j=std::min<std::size_t>(x.size()-1,std::upper_bound(x.begin(),x.end(),v)-x.begin());
        return std::lerp(y[j-1],y[j],(v-x[j-1])/(x[j]-x[j-1]));
    }
    static double voltsForGain(double db) {return evaluate(gains,states,db);}
    static double gainForVolts(double v) {return std::clamp(evaluate(states,gains,v),0.,48.);}
    static double rectifiedSine(double threshold) {
        if (threshold>=1) return 0;
        if (threshold<=-1) return -threshold;
        const double phase=std::acos(threshold);
        return (std::sin(phase)-threshold*phase)/std::numbers::pi;
    }
    static Frame pulseThreshold(const std::array<std::array<double,3>,14>& a,double v) {
        if (v<=a.front()[0]) return {a.front()[1],a.front()[2]};
        for (std::size_t j=1;j<a.size();++j) {
            if (v<a[j][0]) {
                const auto delta=a[j][0]-a[j-1][0];
                const double t=delta>1e-10?(v-a[j-1][0])/delta:1;
                return {std::lerp(a[j-1][1],a[j][1],t),std::lerp(a[j-1][2],a[j][2],t)};
            }
        }
        return {a.back()[1],a.back()[2]};
    }
    struct Biquad {
        double b0=1,b1=0,b2=0,a1=0,a2=0;
        double apply(double x,std::array<double,2>& s) const {
            const double y=b0*x+s[0];
            s[0]=b1*x-a1*y+s[1];
            s[1]=b2*x-a2*y;
            return y;
        }
    };
    Biquad fitAudioShelf() const {
        // The legacy filter fits the nodal solver at audio rate, where the
        // integration step rolls off too early. Match the original circuit's
        // SPICE transfer at 10 and 20 kHz in the audio path.
        const auto amplitude=[this](double frequency) {
            const auto z=std::polar(1.,-2*std::numbers::pi*frequency/rate);
            const auto z2=z*z;
            auto transfer=std::complex<double>(1);
            for (const auto& section:sections)
                transfer*=(section.b0+section.b1*z+section.b2*z2)/(1.+section.a1*z+section.a2*z2);
            return std::abs(transfer);
        };
        const double high=std::min(20000.,.46*rate);
        const double low=high*.5;
        const double reference=amplitude(1000);
        const auto desiredPower=[&](double frequency,double spiceDb) {
            const double gain=reference/amplitude(frequency)*std::pow(10.,spiceDb/20);
            return gain*gain;
        };
        const double lowPower=desiredPower(low,-.24*high/20000);
        const double highPower=desiredPower(high,-.44*high/20000);
        const double lowTan=std::tan(std::numbers::pi*low/rate);
        const double highTan=std::tan(std::numbers::pi*high/rate);
        const double lowTan4=std::pow(lowTan,4),highTan4=std::pow(highTan,4);
        const double denominator=(lowPower-1)/lowTan4-(highPower-1)/highTan4;
        if (denominator<=0 || highPower<=lowPower) return {};
        // Second-order Butterworth shelf with unity DC magnitude.
        const double corner4=(highPower-lowPower)/denominator;
        const double corner=std::sqrt(std::sqrt(corner4));
        const double shelf=std::sqrt(lowPower+(lowPower-1)*corner4/lowTan4);
        const double a0=1+std::sqrt(2.)*corner+corner*corner;
        const double tilt=std::sqrt(2*shelf)*corner;
        return {(shelf+tilt+corner*corner)/a0,2*(corner*corner-shelf)/a0,
                (shelf-tilt+corner*corner)/a0,2*(corner*corner-1)/a0,
                (1-std::sqrt(2.)*corner+corner*corner)/a0};
    }

    static Biquad retune(Biquad section,double ratio) {
        const double w=(1-ratio)/(1+ratio),w2=w*w;
        const double a=1+section.a1*w+section.a2*w2;
        return {(section.b0+section.b1*w+section.b2*w2)/a,
                (2*w*(section.b0+section.b2)+(1+w2)*section.b1)/a,
                (section.b0*w2+section.b1*w+section.b2)/a,
                (2*w*(1+section.a2)+(1+w2)*section.a1)/a,
                (w2+section.a1*w+section.a2)/a};
    }

    template<std::size_t n>
    static measured::Point lookup(const measured::Point* points,const double (&axis)[n],double level) {
        if (level<=axis[0]) return points[0];
        const auto j=std::upper_bound(axis,axis+n,level)-axis;
        if (j==n) {
            auto result=points[n-1];
            const auto slope=std::clamp((points[n-1].reduction-points[n-2].reduction)/(axis[n-1]-axis[n-2]),0.,1.);
            result.reduction+=static_cast<float>(slope*std::min(24.,level-axis[n-1]));
            return result;
        }
        const auto t=static_cast<float>((level-axis[j-1])/(axis[j]-axis[j-1]));
        const auto& a=points[j-1];
        const auto& b=points[j];
        return {std::lerp(a.reduction,b.reduction,t),std::lerp(a.k2Cos,b.k2Cos,t),
                std::lerp(a.k2Sin,b.k2Sin,t),std::lerp(a.k3Cos,b.k3Cos,t),std::lerp(a.k3Sin,b.k3Sin,t)};
    }

    struct Ballistics {
        double gain=0,memory=0,rise=0,fall=0,tailRise=0,tailFall=0;
        void setup(double rate,double attack,double recovery,double automatic) {
            const auto coefficient=[rate](double seconds) {return -std::expm1(-1/(rate*seconds));};
            rise=coefficient(attack);
            fall=coefficient(recovery);
            tailRise=coefficient(.22);
            tailFall=automatic>0?coefficient(automatic):1;
        }
        double update(double wanted) {
            if (tailFall<1) {
                memory+=(wanted-memory)*(wanted>memory?tailRise:tailFall);
                wanted=std::max(wanted,.72*memory);
            }
            gain+=(wanted-gain)*(wanted>gain?rise:fall);
            return gain;
        }
    };

    struct Channel {
        Ballistics lim;
        SpectralCorrection spectral;
        double limiterReduction=0;
        std::array<std::array<double,3>,14> chargeCurve{};
        double attackSpeed=1.,autoFast=0,autoSlow=0,memoryRise=0,fastEnvelope=0,slowEnvelope=0,programMemory=0,releaseWeight=.1;
        int recoveryHold=0;
        double charge=.582,tailCharge=.582,voltRelease=0,attack=0,compSmooth=0,chargeDeco=0;
        bool autoRelease=false;
        std::array<double,4> dcHistory{};
        std::array<std::array<double,2>,2> history{};
        std::array<double,2> highFrequencyHistory{};
        double dc=0,harmonicDc=0,harmonicMemory=0,evenMemory=0,previous=0;
        double averagedLimiterGain=1,referenceTransformerMemory=0,limiterTrimMix=0;
        double preampSignal=0,preampMemory=0,transformerMemory=0,bassMemory=0;
        double limPower=0,slowPower=0,lowAudio=0,lowPower=0,transientPower=0,slowTransientPower=0,detectorBias=0;
        double peak=0,period=0,prevAbs=0,prevPrevAbs=0,prevSign=0;
        double ct=0,lt=0,makeupDb=0,makeup=0,reduction=0;
        unsigned sincePeak=0,sinceCrossing=0;
        bool compress=false,limit=false;
        const measured::Point* limiter=nullptr;

        double detect(double sample,double rate,double drop) {
            const double magnitude=std::abs(sample);
            ++sinceCrossing;
            const double sign=sample>=0?1:-1;
            if (sign!=prevSign && magnitude+prevAbs>1e-5) {
                const double p=std::clamp(double(sinceCrossing),rate/40000,rate/36);
                period+=.20*(p-period);
                sinceCrossing=0;
                prevSign=sign;
            }
            double estimate=magnitude;
            if (prevAbs>=magnitude && prevAbs>prevPrevAbs) {
                const double curvature=2*prevAbs-magnitude-prevPrevAbs;
                const double slope=magnitude-prevPrevAbs;
                estimate=std::max(estimate,prevAbs+.125*slope*slope/curvature);
            }
            prevPrevAbs=prevAbs;
            prevAbs=magnitude;
            if (++sincePeak>std::max(rate*.0009,1.22*period)) peak*=drop;
            if (estimate>=peak) {peak=estimate;sincePeak=0;}
            return peak;
        }
    };

    std::array<Biquad,2> sections{};
    Biquad highFrequency;
    std::array<Channel,2> channels{};
    BassTransient bassTransient;
    Frame previousGain{};
    int previousAttackMode=1;
    Controls previousControls{};
    double rate=44100,detectorDrop=0,limitAverage=0,slowAverage=0,pulseFollow=0;
    double detectorLow=0,transientQuick=0,transientSlow=0;
    double limitGainAverage=0,limitTrimRelease=0;
    double preampLeak=0,transformerGain=0,biasFollow=0;
    bool initialized=false,active=true,linked=false;
};
}
