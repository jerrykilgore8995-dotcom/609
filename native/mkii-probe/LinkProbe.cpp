#include "Circuit.hpp"
#include <barrier>
#include <atomic>
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
    unsigned fails=0,refinements=0;
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
Results lagged(std::span<const Frame> input,const Controls& settings) {
    Solver left(0),right(1);
    if (!left.prepare(rate)||!right.prepare(rate)||!left.usesGeneratedSolve()||!right.usesGeneratedSolve()||
        !left.usesGeneratedResidual()||!right.usesGeneratedResidual())
        throw std::runtime_error("Lagged generated solvers unavailable");
    Results result;
    result.output.resize(input.size());
    struct alignas(64) Voltages {double value[2]{};};
    Voltages cv[2];
    cv[0].value[0]=left.linkVoltage();
    cv[1].value[0]=right.linkVoltage();
#if defined(MKII_CODE_SPEED)
    struct alignas(64) SampleCounter {std::atomic<std::uint64_t> done{0};};
    SampleCounter counters[2];
#else
    std::barrier<> sync(2);
#endif
    const auto sample=[&](Solver& solver,int channel,int n) {
        solver.setPeerLinkVoltage(cv[1-channel].value[n%2]);
        result.output[n][channel]=solver.step(input[n],settings)[channel];
        cv[channel].value[1-n%2]=solver.linkVoltage();
#if defined(MKII_CODE_SPEED)
        const auto completed=static_cast<std::uint64_t>(n+1);
        counters[channel].done.store(completed,std::memory_order_release);
        for (unsigned spins=0;counters[1-channel].done.load(std::memory_order_acquire)<completed;++spins) {
            _mm_pause();
            if ((spins&1023)==1023) std::this_thread::yield();
        }
#else
        sync.arrive_and_wait();
#endif
    };
    std::thread other([&] {
        ftz();
        for (int n=0;n<blocks*block;++n) sample(right,1,n);
    });
    record(result,[&](int n){sample(left,0,n);});
    other.join();
    result.fails=left.failures()+right.failures();
    result.refinements=left.refinedSamples()+right.refinedSamples();
    return result;
}
void print(const char* name,Results& result) {
    std::sort(result.times.begin(),result.times.end());
    double total=0;
    for (const auto time:result.times) total+=time;
    std::printf("%s mean %.3f ms, p99 %.3f ms, max %.3f ms, failed samples %u\n",name,
        total/blocks,result.times[(99*blocks+99)/100-1],result.times.back(),result.fails);
}
}

int main() {
    ftz();
#if defined(MKII_CODE_SPEED)
    std::puts("Optimized: spin, aligned storage, vectorized checks");
#else
    std::puts("Baseline: standard barrier, original buffers and scans");
#endif
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
        auto candidate=lagged(input,settings);
        print("10 ohm lagged dual mono",candidate);
        std::uint64_t hash=14695981039346656037ULL;
        for (const auto& frame:candidate.output)
            for (const auto sample:frame) {
                hash^=std::bit_cast<std::uint64_t>(sample);
                hash*=1099511628211ULL;
            }
        std::printf("Output checksum %016llx, refinements %u, failures %u\n",
                    static_cast<unsigned long long>(hash),candidate.refinements,candidate.fails);
    }
}
