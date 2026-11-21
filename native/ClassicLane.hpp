#pragma once

#include "Circuit.hpp"

#include <numbers>

namespace mkii::native {

class ClassicLane {
public:
    explicit ClassicLane(int channelIndex):solver(channelIndex),channel(channelIndex) {}

    static int factorFor(double rate) {
        return rate<=48000 ? 3 : rate<132300 ? 2 : 1;
    }

    static constexpr int filterRadius=48;
    static int latencyFor(double rate) { return factorFor(rate)>1 ? 2*filterRadius : 0; }

    bool prepare(double rate) {
        factor=factorFor(rate);
        input.fill(0);
        output.fill(0);
        if (factor>1) {
            const int center=filterRadius*factor;
            tapCount=2*center+1;
            for (int k=0;k<tapCount;++k) {
                const double x=double(k-center)/factor;
                const double angle=std::numbers::pi*double(k-center)/center;
                const double window=.42+.5*std::cos(angle)+.08*std::cos(2*angle);
                taps[k]=window*(x==0 ? 1.0 : std::sin(std::numbers::pi*x)/(std::numbers::pi*x));
            }
            for (int phase=0;phase<factor;++phase) {
                double dc=0;
                for (int k=phase;k<tapCount;k+=factor) dc+=taps[k];
                for (int k=phase;k<tapCount;k+=factor) taps[k]/=dc;
            }
        }
        return solver.prepare(rate*factor);
    }

    Solver::Frame step(Solver::Frame frame,const Controls& controls) {
        if (factor==1) return solver.step(frame,controls);
        input.push(frame[channel]);
        Solver::Frame result{};
        for (int phase=0;phase<factor;++phase) {
            Solver::Frame interpolated{};
            double value=0;
            for (int k=phase,i=0;k<tapCount;k+=factor,++i) value+=taps[k]*input[i];
            interpolated[channel]=value;
            output.push(solver.step(interpolated,controls)[channel]);
            if (phase==0) {
                double sum=0;
                for (int k=0;k<tapCount;++k) sum+=taps[k]*output[k];
                result[channel]=sum/factor;
            }
        }
        return result;
    }

    double linkVoltage() const { return solver.linkVoltage(); }
    void setPeerLinkVoltage(double volts) { solver.setPeerLinkVoltage(volts); }
    auto meterVolts() const { return solver.meterVolts(); }
    unsigned failures() const { return solver.failures(); }

private:
    static constexpr int maxTaps=2*filterRadius*3+1;

    template<std::size_t size>
    struct History {
        std::array<double,2*size> samples{};
        std::size_t head=0;

        void fill(double value) { samples.fill(value); head=0; }
        void push(double value) {
            if (head==0) head=size;
            --head;
            samples[head]=samples[head+size]=value;
        }
        double operator[](std::size_t age) const { return samples[head+age]; }
    };

    Solver solver;
    int channel;
    int factor=1,tapCount=0;
    std::array<double,maxTaps> taps{};
    History<2*filterRadius+1> input;
    History<maxTaps> output;
};
}
