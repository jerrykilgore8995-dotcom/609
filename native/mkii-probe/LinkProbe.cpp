#include "Circuit.hpp"
#include <barrier>
#include <chrono>
#include <cstdio>
#include <immintrin.h>
#include <numbers>
#include <stdexcept>
#include <thread>

namespace {
using namespace mkii::native;
constexpr int rate=44100, block=512, blocks=172;
using Frame=Solver::Frame;

void ftz() {_mm_setcsr(_mm_getcsr()|0x8040);}
struct Results {
    std::vector<Frame> output;
    std::vector<double> times;
    unsigned fails=0;
};
template<class Render>
void record(Results& run,Render render) {
    for (int start=0;start<blocks*block;start+=block) {
        const auto t=std::chrono::steady_clock::now();
        for (int i=start;i<start+block;++i) render(i);
        run.times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count());
    }
}
Results exact(std::span<const Frame> input,const Controls& settings) {
    Solver solver;
    if (!solver.prepare(rate)||!solver.usesGeneratedSolve()||!solver.usesGeneratedResidual())
        throw std::runtime_error("Exact generated solver unavailable");
    Results result;
    result.output.resize(input.size());
    record(result,[&](int n){result.output[n]=solver.step(input[n],settings);});
    result.fails=solver.failures();
    return result;
}
Results lagged(std::span<const Frame> input,const Controls& settings,double conductance,bool offline) {
    Solver left(0,conductance),right(1,conductance);
    if (!left.prepare(rate)||!right.prepare(rate)||!left.usesGeneratedSolve()||!right.usesGeneratedSolve()||
        !left.usesGeneratedResidual()||!right.usesGeneratedResidual())
        throw std::runtime_error("Lagged generated solvers unavailable");
    Results result;
    result.output.resize(input.size());
    struct alignas(64) Voltages {double value[2]{};};
    Voltages cv[2];
    cv[0].value[0]=left.linkVoltage();
    cv[1].value[0]=right.linkVoltage();
    std::barrier<> sync(2);
    const auto sample=[&](Solver& solver,int channel,int n) {
        solver.setPeerLinkVoltage(cv[1-channel].value[n%2]);
        result.output[n][channel]=solver.step(input[n],settings,offline)[channel];
        cv[channel].value[1-n%2]=solver.linkVoltage();
        sync.arrive_and_wait();
    };
    std::thread other([&] {
        ftz();
        for (int n=0;n<blocks*block;++n) sample(right,1,n);
    });
    record(result,[&](int n){sample(left,0,n);});
    other.join();
    result.fails=left.failures()+right.failures();
    return result;
}
void print(const char* name,Results& result) {
    std::sort(result.times.begin(),result.times.end());
    double total=0;
    for (const auto time:result.times) total+=time;
    const auto over=std::count_if(result.times.begin(),result.times.end(),[](double ms){return ms>1000.0*block/rate;});
    std::printf("%s mean %.3f ms, p99 %.3f ms, max %.3f ms, over budget %d/%d, failed samples %u\n",name,
        total/blocks,result.times[(99*blocks+99)/100-1],result.times.back(),int(over),blocks,result.fails);
}
}

int main() {
    ftz();
    auto settings=defaults;
    settings[1]=settings[10]=4;
    settings[6]=settings[15]=settings[7]=settings[16]=settings[19]=1;
    for (const bool burst:{false,true}) {
        std::vector<Frame> input(blocks*block);
        for (std::size_t n=0;n<input.size();++n) {
            const double t=double(n)/rate,w=2*std::numbers::pi*t;
            const double transient=std::exp(-15*std::fmod(t,.22));
            if (burst) {
                const bool loud=std::fmod(t,.8)>.18 && std::fmod(t,.8)<.52;
                input[n]={float((loud?.65:.04)*std::sin(1000*w)),float(.048*std::sin(790*w))};
            } else for (int ch=0;ch<2;++ch)
                input[n][ch]=float(.06*std::sin(78*w+ch)+.05*std::sin(847*w)+
                    transient*(.15*std::sin(7479*w+ch)+.08*std::sin(15601*w)));
        }
        std::puts(burst?"Asymmetric gain-reduction burst":"Stereo transient stress");
        auto base=exact(input,settings);
        print("Same-sample relay",base);
        struct Variant {const char* name;double conductance;bool offline;};
        for (const auto& variant:{Variant{"10 ohm, full retry",0.1,true},
                Variant{"22 ohm, full retry",0.045,true},Variant{"22 ohm, capped retry",0.045,false}}) {
            auto candidate=lagged(input,settings,variant.conductance,variant.offline);
            print(variant.name,candidate);
            for (int ch=0;ch<2;++ch) {
                double ref=0,test=0,error=0,peak=0;
                int count=0;
                for (std::size_t n=0;n<input.size();++n) {
                    const double t=double(n)/rate;
                    if (burst && (std::fmod(t,.8)<.25 || std::fmod(t,.8)>.50)) continue;
                    const double a=base.output[n][ch], b=candidate.output[n][ch], d=b-a;
                    ref+=a*a;test+=b*b;error+=d*d;peak=std::max(peak,std::abs(d));++count;
                }
                std::printf("Channel %d level delta %+.4f dB, difference %.2f dBFS RMS, peak %.6f FS\n",
                    ch+1,10*std::log10(test/ref),10*std::log10(error/count),peak);
            }
        }
    }
}
