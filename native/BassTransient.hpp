#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace mkii::native {

class BassTransient {
public:
    using Frame=std::array<double,2>;

    void prepare(double sampleRate) {
        *this={};
        for (std::size_t i=0;i<follow.size();++i)
            follow[i]=-std::expm1(-1000/(sampleRate*milliseconds[i]));
        riseFollow=-std::expm1(-1000/(sampleRate*17));
    }

    void configure(int channel,int ratio,int threshold,int recovery) {
        auto& ch=channels[channel];
        ch.ratioPosition=(ratio-3)*.5;
        ch.thresholdPosition=(std::min(threshold,8)-4)/6.;
        ch.recoveryPosition=std::min(recovery,2)-1.;
    }

    Frame step(Frame input,Frame reduction,Frame compressor,Frame limiter,
               const std::array<bool,2>& enabled,bool linked) {
        const auto left=features(channels[0],input[0],reduction[0]);
        const auto right=features(channels[1],input[1],reduction[1]);
        Frame adjustment{};
        if (linked) {
            const double comp=std::max(compressor[0],compressor[1]);
            const double lim=std::max(limiter[0],limiter[1]);
            if (enabled[0] || enabled[1]) {
                for (std::size_t i=0;i<left.size();++i) {
                    adjustment[0]+=weights[0][i]*((left[i]+right[i])*.5)+
                        weights[1][i]*std::max(left[i],right[i])+weights[2][i]*std::min(left[i],right[i]);
                }
                adjustment[0]*=std::clamp(1+comp-lim,0.,1.);
                adjustment[1]=adjustment[0];
            }
        } else {
            const auto estimate=[&](const auto& feature,int channel) {
                if (!enabled[channel]) return 0.;
                double correction=0;
                for (std::size_t i=0;i<feature.size();++i)
                    correction+=(weights[0][i]+weights[1][i]+weights[2][i])*feature[i];
                return correction*std::clamp(1+compressor[channel]-limiter[channel],0.,1.);
            };
            adjustment={estimate(left,0),estimate(right,1)};
        }
        for (int channel=0;channel<2;++channel) {
            const auto target=.6*std::clamp(adjustment[channel],-2.5,2.5);
            update(channels[channel].correction,target,output);
            adjustment[channel]=channels[channel].correction;
        }
        return adjustment;
    }

private:
    enum Time { inputLow,peak,medium,slow,background,history,bias,polarity,polarityRise,weightedRise,
                signedHistory,longRise,noise,output };
    static constexpr std::array<double,14> milliseconds{.65,3,9,27,68,130,6,13,9,16,12,22,5,2};
    struct Channel {
        double low=0,prior=0,correction=0;
        double e3=0,e9=0,e27=0,e68=0,e130=0,l3=0,l9=0,l27=0;
        double signedMean=0,positive=0,negative=0,rise6=0,rise17=0,swell22=0;
        double weightedSwell=0,weightedPulse=0,rise12=0,jitter=0;
        double ratioPosition=0,thresholdPosition=0,recoveryPosition=0;
    };

    void update(double& state,double input,Time time) const {
        state+=follow[time]*(input-state);
    }

    std::array<double,21> features(Channel& ch,double x,double reduction) const {
        update(ch.low,x,inputLow);
        const auto energy=x*x;
        const auto bass=ch.low*ch.low;
        update(ch.e3,energy,peak);
        update(ch.e9,energy,medium);
        update(ch.e27,energy,slow);
        update(ch.e68,energy,background);
        update(ch.e130,energy,history);
        update(ch.l3,bass,peak);
        update(ch.l9,bass,medium);
        update(ch.l27,bass,slow);
        update(ch.signedMean,ch.low,bias);
        update(ch.positive,std::max(ch.low,0.),polarity);
        update(ch.negative,std::max(-ch.low,0.),polarity);
        const auto amplitude=std::sqrt(ch.l9+1e-7);
        const auto sign=std::clamp(ch.signedMean/amplitude,-1.,1.);
        const auto asymmetry=std::clamp((ch.positive-ch.negative)/amplitude,-1.,1.);
        const auto gate=std::clamp((ch.l9/(ch.e9+1e-8)-.30)/.40,0.,1.)*
                        std::clamp(ch.e9/.0005,0.,1.);
        const auto rise=std::clamp(std::sqrt(ch.e3/(ch.e27+1e-7))-1,0.,3.);
        const auto swell=std::clamp(std::sqrt(ch.e9/(ch.e68+1e-7))-1,0.,3.);
        update(ch.rise6,rise,bias);
        ch.rise17+=riseFollow*(rise-ch.rise17);
        update(ch.swell22,swell,longRise);
        update(ch.weightedSwell,sign*swell,polarityRise);
        update(ch.weightedPulse,sign*rise,weightedRise);
        update(ch.rise12,rise,signedHistory);
        const auto slope=x-ch.prior;
        ch.prior=x;
        update(ch.jitter,slope*slope,noise);
        return {gate,gate*std::clamp(reduction/18,0.,1.5),gate*rise,gate*swell,
                gate*ch.rise6,gate*ch.rise17,gate*ch.swell22,
                gate*sign,gate*ch.weightedSwell,gate*ch.weightedPulse,
                gate*asymmetry,gate*asymmetry*swell,gate*asymmetry*rise,
                gate*std::clamp(std::sqrt(ch.e3/(ch.e9+1e-7))-1,-1.,2.),
                gate*std::clamp(std::sqrt(ch.e27/(ch.e130+1e-7))-1,-1.,2.),
                gate*std::clamp(std::sqrt(ch.l3/(ch.l27+1e-7))-1,-1.,2.),
                gate*ch.rise12*ch.ratioPosition,gate*ch.rise12*ch.thresholdPosition,
                gate*ch.rise12*ch.recoveryPosition,
                gate*std::clamp(std::sqrt(ch.l3/(ch.e9+1e-7)),0.,1.5),
                gate*std::clamp(ch.jitter/(ch.e9+1e-7),0.,.15)*10};
    }

    // Row order is the stereo mean, maximum and minimum of each feature.
    static constexpr std::array<std::array<double,21>,3> weights{{
        {
            -0.07432340, 0.08197903, 0.29893897, 0.12281166, 0.25610974, -0.60691441, 0.23357677,
            -0.05345347, -0.00353534, 0.88717018, 0.14178268, -0.37801722, -0.01514217, 0.08998018,
            0.02794197, -0.24611741, -0.74934448, 0.65656475, -0.04782795, -0.07077459, 0.01485185,
        },
        {
            0.45636453, -0.90872218, 0.10918506, 0.62199657, 0.47406412, -0.27334336, 0.14269857,
            -0.15121764, -0.30404677, -0.17953029, 0.13058034, -0.32147126, 0.09981426, 0.16463989,
            0.17683269, -0.16255238, -1.31088941, -0.40829489, 0.57229125, -0.19969951, 0.17132703,
        },
        {
            -0.60501134, 1.07268025, 0.48869288, -0.37637326, 0.03815536, -0.94048546, 0.32445497,
            0.04431070, 0.29697610, 1.95387065, 0.15298502, -0.43456318, -0.13009861, 0.01532048,
            -0.12094875, -0.32968243, -0.18779955, 1.72142439, -0.66794716, 0.05815033, -0.14162332,
        },
    }};
    std::array<Channel,2> channels{};
    std::array<double,milliseconds.size()> follow{};
    double riseFollow=0;
};

}
