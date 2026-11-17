#pragma once

#include "LimiterEvenBasis.hpp"
#include "LimiterEvenGrid00.hpp"
#include "LimiterEvenGrid01.hpp"
#include "LimiterEvenGrid10.hpp"
#include "LimiterEvenGrid11.hpp"
#include "LimiterEvenTrim0.hpp"
#include "LimiterEvenTrim1.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <numbers>

namespace mkii::native {

// The limiter's small even components evolve as its coupling capacitors
// settle. This path adds the measured complex residual to coherent bass
// tones. The main limiter detector and its odd harmonics remain independent.
class LimiterEvenCorrection {
public:
    void prepare(double sampleRate) {
        *this={};
        rate=sampleRate;
        dcFollow=-std::expm1(-2*std::numbers::pi*1.1/rate);
        silenceLimit=static_cast<int>(rate*.055);
        ageLimit=static_cast<int>(rate*measured::limiterEvenAges[std::size(measured::limiterEvenAges)-1]);
    }

    void configure(int threshold,int recovery,bool fast) {
        using namespace measured;
        detent=threshold;
        setRows(fast,recovery);
        trimRows=fast?&limiterEvenTrim1[recovery]:&limiterEvenTrim0[recovery];
        update=0;
    }

    void observe(double input,bool enabled) {
        if (!enabled) {
            ageSamples=quietSamples=update=0;
            dc=0;
            return;
        }
        if (std::abs(input)>1e-7) {
            quietSamples=0;
            ageSamples=std::min(ageSamples+1,ageLimit);
        } else if (quietSamples<silenceLimit) {
            ++quietSamples;
            if (ageSamples) ageSamples=std::min(ageSamples+1,ageLimit);
        } else {
            ageSamples=update=0;
            dc=0;
        }
    }

    // The oscillator and power estimate come from the existing spectral path.
    double correction(double frequency,double drive,double sine,double cosine,double scale,int count) {
        const auto age=ageSamples/rate;
        if (frequency<34 || frequency>400 || drive<-4 || drive>17 || age<.055) return 0;
        if (!update) load(frequency,drive,age);
        update=(update+1)%32;
        const auto sinTwo=2*sine*cosine;
        const auto cosTwo=cosine*cosine-sine*sine;
        double sinH=sinTwo,cosH=cosTwo,sum=0;
        for (int harmonic=0;harmonic<count/2;++harmonic) {
            sum+=coefficients[2*harmonic]*cosH-coefficients[2*harmonic+1]*sinH;
            const auto next=sinH*cosTwo+cosH*sinTwo;
            cosH=cosH*cosTwo-sinH*sinTwo;
            sinH=next;
        }
        const double sample=sum*scale;
        dc+=dcFollow*(sample-dc);
        return sample-dc;
    }

private:
    using Row=std::int16_t[measured::limiterEvenRank];
    void setRows(bool fast,int recovery) {
        using namespace measured;
        if (fast) rows=recovery<3?limiterEvenGrid10[recovery]:limiterEvenGrid11[recovery-3];
        else rows=recovery<3?limiterEvenGrid00[recovery]:limiterEvenGrid01[recovery-3];
    }

    struct Axis {int lower;double mix;};
    template<std::size_t n>
    static Axis locate(const double (&points)[n],double value) {
        const auto upper=std::clamp<int>(std::upper_bound(points,points+n,value)-points,1,n-1);
        return {upper-1,std::clamp((value-points[upper-1])/(points[upper]-points[upper-1]),0.,1.)};
    }

    void load(double frequency,double drive,double age) {
        using namespace measured;
        const auto freq=locate(limiterEvenFrequencies,frequency);
        // Avoid leaking into neighboring calibration cells due to the tiny
        // gain and frequency estimation errors on coherent sine waves.
        for (double anchor : limiterEvenLevels)
            if (std::abs(drive-anchor)<.035) drive=anchor;
        const auto level=locate(limiterEvenLevels,drive);
        const auto limit=locate(limiterEvenThresholds,4+.5*detent);
        const auto time=locate(limiterEvenAges,age);
        const auto detailLevel=locate(limiterEvenDetailLevels,drive);
        const auto detailMix=std::clamp((12-std::abs(frequency-110)) / 11.7,0.,1.)*
            std::clamp((drive-6)/2,0.,1.);
        const auto frequencyMix=std::clamp((frequency-34)/16,0.,1.)*
            std::clamp((400-frequency)/80,0.,1.);
        const auto driveMix=std::clamp((drive+4)/2,0.,1.)*std::clamp((17-drive)/5,0.,1.);
        const auto ageMix=std::clamp((age-.055)/.035,0.,1.);
        const auto strength=frequencyMix*driveMix*ageMix;

        std::array<double,limiterEvenRank> shape{};
        for (int k=0;k<limiterEvenRank;++k) {
            const auto at=[&](int thresholdIndex,int frequencyIndex) {
                const int offset=(thresholdIndex*8+frequencyIndex)*4+level.lower;
                return std::lerp(double(rows[offset][k]),double(rows[offset+1][k]),level.mix);
            };
            const auto lower=std::lerp(at(limit.lower,freq.lower),at(limit.lower,freq.lower+1),freq.mix);
            const auto upper=std::lerp(at(limit.lower+1,freq.lower),at(limit.lower+1,freq.lower+1),freq.mix);
            const auto coarse=std::lerp(lower,upper,limit.mix);
            const int exact=96+detent*2+detailLevel.lower;
            const auto detail=std::lerp(double(rows[exact][k]),double(rows[exact+1][k]),detailLevel.mix);
            shape[k]=strength*limiterEvenScale[k]*std::lerp(coarse,detail,detailMix);
        }
        // The 110 Hz sweep uses a second fit over the 400–600 ms window.
        // Apply its phase-aligned trim with a gradual onset and decay.
        const auto trimWindow=std::min(std::clamp((age-.25)/.12,0.,1.),
                                       std::clamp((.77-age)/.10,0.,1.));
        for (int h=0;h<20;++h) {
            double value=0;
            for (int k=0;k<limiterEvenRank;++k)
                value+=shape[k]*std::lerp(double(limiterEvenBasis[time.lower][h][k]),
                                           double(limiterEvenBasis[time.lower+1][h][k]),time.mix);
            const auto& trim=(*trimRows)[detent];
            const auto normalized=std::lerp(double(trim[detailLevel.lower][h]),
                                            double(trim[detailLevel.lower+1][h]),detailLevel.mix);
            coefficients[h]=value+trimWindow*detailMix*strength*limiterEvenTrimScale[h]*normalized;
        }
    }

    const Row* rows=measured::limiterEvenGrid00[0];
    using TrimRows=std::int16_t[23][2][20];
    const TrimRows* trimRows=&measured::limiterEvenTrim0[0];
    std::array<double,20> coefficients{};
    double rate=44100,dc=0,dcFollow=0;
    int detent=8,ageSamples=0,quietSamples=0,ageLimit=0,silenceLimit=0,update=0;
};
}
