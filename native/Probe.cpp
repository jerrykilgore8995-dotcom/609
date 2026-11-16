#include "Circuit.hpp"

#include <chrono>
#include <cstdio>
#include <numbers>

int main(int argc,char** argv) {
    constexpr double rate=44100;
    mkii::native::Solver solver;
    if (!solver.prepare(rate)) return 2;
    auto controls=mkii::native::defaults;
    controls[6]=argc>1?std::atoi(argv[1]):0;
    controls[7]=argc>2?std::atoi(argv[2]):0;
    controls[1]=argc>3?std::atoi(argv[3]):1;
    const double level=argc>4?std::atof(argv[4]):0;
    const auto start=std::chrono::steady_clock::now();
    constexpr int duration=22050;
    double sine=0,cosine=0;
    for (int n=0;n<duration;++n) {
        const double phase=2*std::numbers::pi*1000*n/rate;
        const double input=std::pow(10.,(level-24)/20)*std::sin(phase);
        const double output=solver.step({input,input},controls)[0];
        if (n>=duration-8820) {
            sine+=output*std::sin(phase);
            cosine+=output*std::cos(phase);
        }
    }
    const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()*512/duration;
    const double dbu=24+20*std::log10(2*std::hypot(sine,cosine)/8820);
    std::printf("%.3f dBu out, meter %.6f V, %.3f ms per 512 stereo, %u solve failures, %u refinements\n",dbu,
                solver.meterVolts()[0],ms,solver.failures(),solver.refinedSamples());
    if (solver.failures()) return 3;
    return ms<512000/rate?0:4;
}
