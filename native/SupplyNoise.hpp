#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <numbers>

namespace mkii::native {

class SupplyNoise {
public:
    void prepare(double sampleRate) {
        rate=sampleRate;
        phase=level=0;
        channels={};
        channels[0].random=0x12a84231;
        channels[1].random=0xcd12d16b;
        phaseStep=2*std::numbers::pi*60/rate;
        fadeStep=1/(rate*0.005);

        const double omega=2*std::numbers::pi*1000/rate;
        const auto delay=std::polar(1.0,-omega);
        std::complex<double> pink{};
        for (std::size_t i=0;i<poles.size();++i) {
            const double frequency=8*std::pow(4.0,double(i));
            const double pole=std::exp(-2*std::numbers::pi*frequency/rate);
            poles[i]={pole, std::sqrt(frequency)*(1-pole)};
            pink+=poles[i].weight/(1.0-pole*delay);
        }
        highpass=firstOrder(40,true);
        lowpass=firstOrder(std::min(15000.0,rate*0.45),false);
        const auto response=[&](Filter filter) {
            return (filter.b0+filter.b1*delay)/(1.0+filter.a1*delay);
        };
        // A Hann-windowed 4096-point FFT calibrated to peak sine amplitude.
        // The uniform generator has variance 1/3.
        constexpr double binGain=0.02209708691207961; // sqrt(4*(1/3)*1.5/4096)
        noiseGain=std::pow(10.0,-96.0/20)/
            (binGain*std::abs(pink*response(highpass)*response(lowpass)));
    }

    void silence() {level=0;}

    void process(float* const* data,int channelCount,int count,bool enabled) {
        if (!enabled && level==0) return;
        constexpr double hum60=0.00012589254117941674;
        constexpr double hum120=0.00005011872336272725;
        constexpr double cycle=2*std::numbers::pi;
        for (int i=0;i<count;++i) {
            level=enabled?std::min(1.0,level+fadeStep):std::max(0.0,level-fadeStep);
            const double hum=hum60*std::sin(phase)+hum120*std::sin(2*phase);
            for (int channel=0;channel<channelCount;++channel) {
                auto& state=channels[channel];
                auto& random=state.random;
                random^=random<<13;
                random^=random>>17;
                random^=random<<5;
                const double white=double(random)*(1.0/2147483648.0)-1;
                double pink=0;
                for (std::size_t k=0;k<poles.size();++k) {
                    state.pink[k]=poles[k].decay*state.pink[k]+poles[k].weight*white;
                    pink+=state.pink[k];
                }
                const double filtered=lowpass.apply(highpass.apply(pink,state.highpass),state.lowpass);
                data[channel][i]+=static_cast<float>(level*(hum+noiseGain*filtered));
            }
            phase+=phaseStep;
            if (phase>=cycle) phase-=cycle;
        }
    }

private:
    struct Pole {double decay=0,weight=0;};
    struct Filter {
        double b0=0,b1=0,a1=0;
        double apply(double input,double& memory) const {
            const double output=b0*input+memory;
            memory=b1*input-a1*output;
            return output;
        }
    };
    struct Channel {
        std::array<double,7> pink{};
        double highpass=0,lowpass=0;
        std::uint32_t random=0;
    };

    Filter firstOrder(double frequency,bool high) const {
        const double k=std::tan(std::numbers::pi*frequency/rate);
        const double b=(high?1:k)/(1+k);
        return {b,high?-b:b,(k-1)/(k+1)};
    }
    std::array<Pole,7> poles{};
    std::array<Channel,2> channels{};
    Filter highpass,lowpass;
    double rate=44100,phase=0,phaseStep=0,level=0,fadeStep=0,noiseGain=0;
};
}
