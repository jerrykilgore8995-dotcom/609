#pragma once

#include "RatioToneTables.hpp"
#include "SpectralTables.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace mkii::native {

class SpectralCorrection {
public:
    void prepare(double sampleRate) {
        rate=sampleRate;
        powerFollow=-std::expm1(-1/(rate*.034));
        dcFollow=-std::expm1(-2*std::numbers::pi*1.1/rate);
        intensityFollow=-std::expm1(-1/(rate*.003));
    }

    void configure(int threshold,int ratio,double limiterThreshold,bool fast) {
        compThreshold=threshold;
        compRatio=ratio;
        limitThreshold=limiterThreshold;
        fastLimiter=fast;
        update=0;
    }

    // Track a coherent sine cycle for the measured low-level residual.
    // Gain reduction and the broadband path are processed independently.
    double step(double input,double wet,double comp,double limit,bool compress,bool limiter) {
        samples=std::min(samples+1,static_cast<int>(rate));
        outputPower+=powerFollow*(wet*wet-outputPower);
        const double prior=previous;
        previous=input;
        if (std::signbit(input)!=std::signbit(prior) && std::abs(input-prior)>1e-9) {
            const double fraction=std::abs(input/(input-prior));
            const double measuredPeriod=samples+previousFraction-fraction;
            previousFraction=fraction;
            samples=0;
            if (measuredPeriod>=2.65 && measuredPeriod<=rate/36) {
                const double error=std::abs(measuredPeriod-period)/measuredPeriod;
                if (error<.018) {
                    stable=std::min(stable+1,8);
                    period+=.25*(measuredPeriod-period);
                } else {
                    stable=0;
                    period=measuredPeriod;
                }
                if (stable>4) {
                    const auto phase=std::numbers::pi/period;
                    cosStep=std::cos(phase);
                    invSinStep=1/std::sin(phase);
                }
            } else stable=0;
        }
        const double target=stable>4 && samples<1.15*period ? 1 : 0;
        intensity+=intensityFollow*(target-intensity);
        if (intensity<1e-4 || period<2.65) return 0;

        const double quadrature=(input*cosStep-prior)*invSinStep;
        const auto amplitude=std::hypot(input,quadrature);
        if (amplitude<1e-7) return 0;
        const double frequency=rate/(2*period);
        if (!update) {
            // Tables are indexed at the 609's measured filter input amplitude,
            // with the small-signal transfer removed from the drive coordinate.
            const int mode=limiter && limit>comp+.02 ? 2 : compress ? 1 : 0;
            const double db=24+20*std::log10(amplitude)+roughFilterLoss(frequency);
            load(mode,frequency,db);
        }
        update=(update+1)%32;
        const double sine=input/amplitude;
        const double cosine=quadrature/amplitude;
        double sinH=sine,cosH=cosine,value=0;
        for (int harmonic=0;harmonic<count;++harmonic) {
            value+=coefficients[2*harmonic]*cosH-coefficients[2*harmonic+1]*sinH;
            const auto next=sinH*cosine+cosH*sine;
            cosH=cosH*cosine-sinH*sine;
            sinH=next;
        }
        const auto correction=intensity*std::sqrt(2*std::max(outputPower,0.))*value;
        dc+=dcFollow*(correction-dc);
        return correction-dc;
    }

private:
    static double roughFilterLoss(double freq) {
        // Native small-signal loss over the frequency range of the residual tables.
        return .0066*std::pow(freq/1000,1.84)+.12*std::pow(20/std::max(freq,20.),2);
    }

    struct Axis {
        int lower;
        double fraction;
    };
    template<std::size_t size>
    static Axis locate(const double (&points)[size],double x) {
        const auto upper=std::clamp<int>(std::upper_bound(points,points+size,x)-points,1,size-1);
        return {upper-1,std::clamp((x-points[upper-1])/(points[upper]-points[upper-1]),0.,1.)};
    }

    void load(int mode,double frequency,double db) {
        using namespace measured;
        const auto fr=locate(spectralFrequencies,frequency);
        const auto drive=locate(spectralLevels[mode],mode==2?db-limitThreshold+8:db);
        const auto lowComp=locate(spectralLevels[1],db-compThreshold-20);
        const auto highComp=locate(spectralLevels[1],db-compThreshold);
        const auto ratioLevel=locate(ratioDrive,db);
        const auto ratioThreshold=locate(ratioThresholds,compThreshold);
        const auto ratioWeight=std::clamp(1-std::abs(std::log2(frequency/1000.))/.48,0.,1.)*
            std::clamp((db+12)/12.,0.,1.);
        const auto ratioResidual=[&](int harmonic) {
            const auto& table=ratioToneResiduals[compRatio-1];
            const auto interpolate=[&](int threshold) {
                return std::lerp(table[threshold][ratioLevel.lower][harmonic],
                                 table[threshold][ratioLevel.lower+1][harmonic],ratioLevel.fraction);
            };
            return std::lerp(interpolate(ratioThreshold.lower),
                             interpolate(ratioThreshold.lower+1),ratioThreshold.fraction);
        };
        const auto fetch=[&](int group,Axis level,int at) {
            const auto& low=spectralResiduals[group][fr.lower];
            const auto& high=spectralResiduals[group][fr.lower+1];
            return std::lerp(std::lerp(low[level.lower][at],low[level.lower+1][at],level.fraction),
                             std::lerp(high[level.lower][at],high[level.lower+1][at],level.fraction),fr.fraction);
        };
        const double fade=std::clamp((12000-frequency)/4000,0.,1.);
        count=std::clamp(int(.49*rate/frequency),0,20);
        for (int at=0;at<2*count;++at) {
            if (mode==1) {
                const auto threshold=std::clamp((compThreshold+20)/20.,0.,1.);
                double lo=fetch(1,lowComp,at);
                if (compRatio<4) lo=std::lerp(fetch(3,lowComp,at),lo,(compRatio-1)/3.);
                else if (compRatio>4) lo=fetch(4,lowComp,at);
                const auto wideband=std::lerp(lo,fetch(2,highComp,at),threshold);
                coefficients[at]=fade*std::lerp(wideband,ratioResidual(at),ratioWeight);
            } else coefficients[at]=fade*fetch(mode==0?0:fastLimiter?6:5,drive,at);
        }
    }

    double rate=44100,period=0,previous=0,previousFraction=0;
    double cosStep=1,invSinStep=0,powerFollow=0,outputPower=0;
    double intensity=0,intensityFollow=0,dc=0,dcFollow=0;
    int samples=0,stable=0,count=0,update=0;
    int compThreshold=0,compRatio=4;
    double limitThreshold=8;
    bool fastLimiter=false;
    std::array<double,40> coefficients{};
};
}
