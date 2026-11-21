#include "Circuit.hpp"
#include <array>
#include <cmath>
#include <complex>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <string_view>
using namespace mkii::native;
struct Stage {const char* name; int a,b;};
constexpr std::array<Stage,8> stages{{
{"input_T2_primary",4,5},
{"T2_secondary",12,14},
{"bridge_input",17,18},
{"T1_secondary",43,-1},
{"amp_output_before_C16",52,-1},
{"T3_primary_after_C16",92,-1},
{"T3_output",7,9},
{"T2_secondary_single_leg",12,-1}
}};
int main(int argc,char** argv) {
    const int frequency=argc>1?std::atoi(argv[1]):1000;
    const int rate=argc>2?std::atoi(argv[2]):44100;
    const double dbu=argc>3?std::atof(argv[3]):4;
    if(frequency>=rate/2)return 2;
    Solver solver(0);
    if(!solver.prepare(rate))return 3;
    auto controls=defaults;
    constexpr double tau=2*std::numbers::pi;
    const double amplitude=std::pow(10.,(dbu-24.)/20.);
    const int duration=rate*6/10, window=rate/5;
    std::array<std::complex<double>,stages.size()> spectrum{};
    for(int n=0;n<duration;++n) {
        const double phase=tau*frequency*n/rate;
        const double input=amplitude*std::sin(phase);
        solver.step({input,0},controls);
        if(n>=duration-window) {
            const auto oscillator=std::polar(1.,-phase);
            for(std::size_t i=0;i<stages.size();++i) {
                const auto& t=stages[i];
                const double v=solver.diagnosticNode(t.a)-(t.b<0?0.:solver.diagnosticNode(t.b));
                spectrum[i]+=v*oscillator;
            }
        }
    }
    std::cout<<std::setprecision(14);
    for(std::size_t i=0;i<stages.size();++i)
        std::cout<<stages[i].name<<" "<<2*std::abs(spectrum[i])/window<<" "
                 <<std::arg(spectrum[i])*180/std::numbers::pi<<"\n";
    std::cerr<<"frequency "<<frequency<<" rate "<<rate<<" failures "<<solver.failures()
             <<" refined "<<solver.refinedSamples()<<"\n";
    return solver.failures()?4:0;
}
