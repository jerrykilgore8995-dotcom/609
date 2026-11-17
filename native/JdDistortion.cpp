#include "Circuit.hpp"
#include "Realtime.hpp"

#include <array>
#include <complex>
#include <iostream>
#include <numbers>
#include <string_view>

int main(int argc,char** argv) {
    using namespace mkii::native;
    constexpr int rate=44100,settling=rate*7/10,window=rate/5;
    const bool calibrated=argc==2 && std::string_view(argv[1])=="--calibrated";
    if (argc!=1 && !calibrated) return 2;
    struct Case {
        const char* name;
        double inputDb;
        std::array<int,8> channel;
    };
    const std::array cases{
        Case{"through",9,{1,1,1,1,9,1,0,0}},
        Case{"compressor",0,{2,5,3,11,9,1,1,0}},
        Case{"limiter_4",22,{1,1,1,1,1,4,0,1}},
        Case{"limiter_8",22,{1,1,1,1,9,4,0,1}},
        Case{"limiter_15",22,{1,1,1,1,23,4,0,1}},
        Case{"hot_compressor",24,{16,3,1,1,9,1,1,0}}
    };

    std::cout<<"Case\tSource\tOutput dBu\tTHD percent\n";
    bool passed=true;
    for (const auto& test:cases) {
        auto settings=defaults;
        std::copy(test.channel.begin(),test.channel.end(),settings.begin());
        Solver reference;
        if (!reference.prepare(rate)) return 3;
        Realtime realtime;
        realtime.prepare(rate);
        realtime.control(settings);
        realtime.setCalibrated(calibrated);

        constexpr double omega=2*std::numbers::pi*1000/rate;
        const auto level=std::pow(10.,(test.inputDb-24)/20);
        std::array<std::array<std::complex<double>,20>,2> spectrum{};
        for (int i=0;i<settling+window;++i) {
            const auto input=level*std::sin(omega*i);
            const auto nodal=reference.step({input,0},settings);
            const auto bounded=realtime.step({input,0});
            if (i<settling) continue;
            const auto root=std::polar(1.,-omega*i);
            auto oscillator=root;
            for (unsigned k=0;k<20;++k) {
                spectrum[0][k]+=nodal[0]*oscillator;
                spectrum[1][k]+=bounded[0]*oscillator;
                oscillator*=root;
            }
        }
        passed=passed && reference.failures()==0;
        for (unsigned path=0;path<spectrum.size();++path) {
            const auto& bins=spectrum[path];
            const double magnitude=std::abs(bins[0]);
            double sum=0;
            for (unsigned k=1;k<bins.size();++k) sum+=std::norm(bins[k]);
            const auto output=24+20*std::log10(2*magnitude/window);
            const auto thd=100*std::sqrt(sum)/magnitude;
            std::cout<<test.name<<'\t'<<(path?"bounded":"nodal")<<'\t'<<output<<'\t'<<thd<<'\n';
            passed=passed && std::isfinite(thd) && std::isfinite(output);
            if (!calibrated) continue;
            const std::string_view name=test.name;
            if (name=="limiter_8") passed=passed && thd>=.36 && thd<=.48 && std::abs(output-8)<=.20;
            if (name=="through") passed=passed && thd<=.075 && std::abs(output-9)<=.16;
            if (name=="compressor") passed=passed && thd<=.20;
        }
    }
    return passed?0:4;
}
