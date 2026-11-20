#include "Circuit.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <numbers>
#include <cstdlib>
#include <string_view>

namespace {
int transientProbe(int mode,int seconds) {
    using namespace mkii::native;
    constexpr double rate=44100;
    constexpr int blockSize=512;
    if (seconds<1 || seconds>60) return 2;
    const int samples=static_cast<int>(rate)*seconds;
    auto controls=defaults;
    if (mode&1) {
        controls[1]=controls[10]=4;
        controls[6]=controls[15]=1;
    }
    if (mode&2) controls[7]=controls[16]=1;
    controls[19]=(mode&4)!=0;

    using Frame=Solver::Frame;
    std::vector<Frame> input(samples);
    for (int i=0;i<samples;++i) {
        const double t=i/rate,w=2*std::numbers::pi*t;
        const double attack=std::exp(-15*std::fmod(t,0.22));
        for (int ch=0;ch<2;++ch)
            input[i][ch]=static_cast<float>(0.06*std::sin(78*w+ch)+0.05*std::sin(847*w)+attack*
                         (0.15*std::sin(7479*w+ch)+0.08*std::sin(15601*w)));
    }
    Solver solver;
    if (!solver.prepare(rate)) return 2;
    using Clock=std::chrono::steady_clock;
    std::vector<double> blockTimes;
    blockTimes.reserve(samples/blockSize+1);
    double energy=0;
    double elapsed=0;
    for (int frame=0;frame<samples;) {
        const int count=std::min(samples-frame,blockSize);
        const auto start=Clock::now();
        for (int end=frame+count;frame<end;++frame) {
            const auto output=solver.step(input[frame],controls);
            energy+=output[0]*output[0]+output[1]*output[1];
        }
        const double ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
        elapsed+=ms;
        if (count==blockSize) blockTimes.push_back(ms);
        if (frame==blockSize)
            std::printf("First block: solve %s, residual %s, %d matrix updates, %u refinements\n",
                        solver.usesGeneratedSolve()?"generated":"generic",
                        solver.usesGeneratedResidual()?"generated":"generic",
                        solver.operations(),solver.refinedSamples());
    }
    std::sort(blockTimes.begin(),blockTimes.end());
    const auto p99=(99*blockTimes.size()+99)/100-1;
    const double ms=elapsed*blockSize/samples;
    std::printf("Stereo transient mode %d, %d s: %.3f ms mean, %.3f ms p99, %.3f ms max per 512 samples, "
                "%u solve failures, %u refinements, energy %.6f\n",
                mode,seconds,ms,blockTimes[p99],blockTimes.back(),solver.failures(),solver.refinedSamples(),energy);
    if (solver.failures() || !std::isfinite(energy) || energy<=0) return 3;
    return ms<1000*blockSize/rate?0:4;
}
}

int main(int argc,char** argv) {
    if ((argc==3 || argc==4) && std::string_view(argv[1])=="--transient")
        return transientProbe(std::atoi(argv[2]),argc==4?std::atoi(argv[3]):1);
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
