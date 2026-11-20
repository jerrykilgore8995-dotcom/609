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
    unsigned fails=0,subdivisions=0,retries=0,searchCaps=0,iterationCaps=0;
};
template<class Render>
void record(Results& run,Render render) {
    for (int start=0;start<blocks*block;start+=block) {
        const auto t=std::chrono::steady_clock::now();
        for (int i=start;i<start+block;++i) render(i);
        run.times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count());
    }
}
Results render(std::span<const Frame> input,const Controls& settings,double conductance,bool offline,int maxIterations,bool widerClose) {
    Solver left(0,conductance,widerClose),right(1,conductance,widerClose);
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
        if (conductance>0) solver.setPeerLinkVoltage(cv[1-channel].value[n%2]);
        result.output[n][channel]=solver.step(input[n],settings,offline,maxIterations)[channel];
        if (conductance>0) {
            cv[channel].value[1-n%2]=solver.linkVoltage();
            sync.arrive_and_wait();
        } else if ((n+1)%block==0) {
            sync.arrive_and_wait();
        }
    };
    std::thread other([&] {
        ftz();
        for (int n=0;n<blocks*block;++n) sample(right,1,n);
    });
    record(result,[&](int n){sample(left,0,n);});
    other.join();
    result.fails=left.failures()+right.failures();
    result.subdivisions=left.refinedSamples()+right.refinedSamples();
    result.retries=left.retryCount()+right.retryCount();
    result.searchCaps=left.lineSearchCount()+right.lineSearchCount();
    result.iterationCaps=left.iterationCapCount()+right.iterationCapCount();
    return result;
}
void print(const char* name,Results& result) {
    std::sort(result.times.begin(),result.times.end());
    double total=0;
    for (const auto time:result.times) total+=time;
    const auto over=std::count_if(result.times.begin(),result.times.end(),[](double ms){return ms>1000.0*block/rate;});
    std::printf("%s mean %.3f ms, p99 %.3f ms, max %.3f ms, over budget %d/%d, failed %u, recovered %u, retries %u, line-search stops %u, iteration-limit stops %u\n",name,
        total/blocks,result.times[(99*blocks+99)/100-1],result.times.back(),int(over),blocks,result.fails,result.subdivisions,result.retries,result.searchCaps,result.iterationCaps);
}
}

int main() {
    std::puts("Real-time only: one 2x retry with 8 iterations, no source continuation or recovery, 4 halvings.");
    ftz();
    auto settings=defaults;
    settings[1]=settings[10]=4;
    settings[6]=settings[15]=settings[7]=settings[16]=settings[19]=1;
    std::vector<Frame> input(blocks*block);
    for (std::size_t n=0;n<input.size();++n) {
        const double t=double(n)/rate,w=2*std::numbers::pi*t;
        const double transient=std::exp(-15*std::fmod(t,.22));
        for (int ch=0;ch<2;++ch)
            input[n][ch]=float(.06*std::sin(78*w+ch)+.05*std::sin(847*w)+
                transient*(.15*std::sin(7479*w+ch)+.08*std::sin(15601*w)));
    }
    for (const double conductance:{0.1,0.045})
        for (const bool widerClose:{false,true})
            for (const int cap:{7,10,12}) {
                auto result=render(input,settings,conductance,false,cap,widerClose);
                char label[128];
                std::snprintf(label,sizeof(label),"%d ohm, %s, cap %d",
                    conductance==0.1?10:22,widerClose?"350/18 mV":"220/25 mV",cap);
                print(label,result);
            }
}
