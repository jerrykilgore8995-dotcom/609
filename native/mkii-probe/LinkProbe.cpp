#include "Circuit.hpp"
#include <atomic>
#include <chrono>
#include <complex>
#include <cstdio>
#include <immintrin.h>
#include <numbers>
#include <stdexcept>
#include <thread>

namespace {
using namespace mkii::native;
constexpr int rate=44100, hostBlock=512, frames=172*hostBlock;
constexpr double hostPeriod=1000.0*hostBlock/rate;
using Frame=Solver::Frame;

void ftz() {_mm_setcsr(_mm_getcsr()|0x8040);}
struct Results {
    std::vector<Frame> output;
    std::vector<double> times,envelope;
    unsigned fails=0,refinements=0;
};
Results render(const std::vector<Frame>& input,const Controls& settings,int batch,bool simplify,bool tone) {
    Solver solver[]{Solver(0,simplify),Solver(1,simplify)};
    for (auto& lane:solver)
        if (!lane.prepare(rate)||!lane.usesGeneratedSolve()||!lane.usesGeneratedResidual())
            throw std::runtime_error("Generated channel solver unavailable");
    Results result;
    result.output.resize(input.size());
    if (tone) result.envelope.resize(input.size());
    struct alignas(64) ChannelState {std::atomic<unsigned> done{0};double cv[2]{};};
    ChannelState states[2];
    for (int ch=0;ch<2;++ch) states[ch].cv[0]=solver[ch].linkVoltage();
    const auto sample=[&](int ch,int n) {
        auto& local=states[ch];
        auto& peer=states[1-ch];
        auto& lane=solver[ch];
        lane.setPeerLinkVoltage(peer.cv[n%2]);
        result.output[n][ch]=lane.step(input[n],settings)[ch];
        if (tone && ch==0) result.envelope[n]=lane.limiterEnvelope();
        local.cv[1-n%2]=lane.linkVoltage();
        local.done.store(n+1,std::memory_order_release);
        for (unsigned spin=0;peer.done.load(std::memory_order_acquire)<static_cast<unsigned>(n+1);++spin) {
            _mm_pause();
            if ((spin&1023)==1023) std::this_thread::yield();
        }
    };
    std::thread right([&] {ftz();for (int n=0;n<frames;++n) sample(1,n);});
    for (int start=0;start<frames;start+=batch) {
        const auto tick=std::chrono::steady_clock::now();
        for (int n=start;n<start+batch;++n) sample(0,n);
        result.times.push_back(std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now()-tick).count());
    }
    right.join();
    for (const auto& lane:solver) {
        result.fails+=lane.failures();
        result.refinements+=lane.refinedSamples();
    }
    return result;
}

void timing(const Results& result,int batch,bool simplify) {
    const int blocks=batch/hostBlock;
    double sum=0,finished=0;
    unsigned late1536=0,late2560=0;
    for (std::size_t i=0;i<result.times.size();++i) {
        const auto first=i*blocks;
        sum+=result.times[i];
        finished=std::max(finished,(first+blocks-1)*hostPeriod)+result.times[i];
        for (int j=0;j<blocks;++j) {
            late1536+=finished>(first+j+3)*hostPeriod;
            late2560+=finished>(first+j+5)*hostPeriod;
        }
    }
    auto times=result.times;
    std::sort(times.begin(),times.end());
    const auto p99=times[(99*times.size()+99)/100-1];
    std::printf("Stress %s batch %d: %.3f ms mean per 512, %.3f ms batch p99, "
                "modeled late blocks %u/%u at PDC 1536/2560, failures %u, refinements %u\n",
        simplify?"Q12/Q13 static":"full",batch,sum/(frames/hostBlock),p99,
        late1536,late2560,result.fails,result.refinements);
}
void compare(const char* name,const Results& reference,const Results& candidate) {
    double square=0,peak=0,refEnergy=0,candidateEnergy=0;
    for (int n=0;n<frames;++n) for (int ch=0;ch<2;++ch) {
        const auto a=reference.output[n][ch], b=candidate.output[n][ch];
        const auto delta=b-a;
        square+=delta*delta;
        peak=std::max(peak,std::abs(delta));
        refEnergy+=a*a;
        candidateEnergy+=b*b;
    }
    std::printf("%s: %.2f dBFS RMS difference, %.6g FS peak, %+.6f dB level change\n",
        name,10*std::log10(square/(2*frames)),peak,10*std::log10(candidateEnergy/refEnergy));
}
double average(const std::vector<double>& series,int first,int last) {
    double total=0;
    for (int n=first;n<last;++n) total+=series[n];
    return total/(last-first);
}
void quality(const Results& result,bool fast,bool simplified) {
    constexpr int attackAt=rate/4,releaseAt=3*rate/4;
    const auto& cv=result.envelope;
    const auto initial=average(cv,attackAt-rate/20,attackAt);
    const auto reduced=average(cv,releaseAt-rate/20,releaseAt);
    const auto released=average(cv,frames-rate/10,frames);
    const auto crossing=[&](int begin,int end,double start,double target) {
        if (std::abs(target-start)<.0001) return -1.0;
        const auto threshold=start+(target-start)*.6321205588;
        for (int n=begin;n<end;++n)
            if ((cv[n]-threshold)*(target-start)>=0) return 1000.0*(n-begin)/rate;
        return -1.0;
    };
    std::array<std::complex<double>,20> bins{};
    for (int n=releaseAt-rate/5;n<releaseAt;++n) {
        const auto r=std::polar(1.0,-2*std::numbers::pi*1000*n/rate);
        auto w=r;
        for (auto& bin:bins) {bin+=w*result.output[n][0];w*=r;}
    }
    double harmonic=0;
    for (std::size_t i=1;i<bins.size();++i) harmonic+=std::norm(bins[i]);
    std::printf("Limiter %s %s: envelope attack %.3f ms, release %.3f ms, THD %.6f %%, failures %u\n",
        fast?"fast":"slow",simplified?"Q12/Q13 static":"full",
        crossing(attackAt,releaseAt,initial,reduced),crossing(releaseAt,frames,reduced,released),
        100*std::sqrt(harmonic)/std::abs(bins[0]),result.fails);
}
}

int main() {
    ftz();
    auto settings=defaults;
    settings[1]=settings[10]=4;
    settings[6]=settings[15]=settings[7]=settings[16]=settings[19]=1;
    std::vector<Frame> input(frames);
    for (int n=0;n<frames;++n) {
        const double t=double(n)/rate,w=2*std::numbers::pi*t;
        const double attack=std::exp(-15*std::fmod(t,.22));
        for (int ch=0;ch<2;++ch)
            input[n][ch]=float(.06*std::sin(78*w+ch)+.05*std::sin(847*w)+
                attack*(.15*std::sin(7479*w+ch)+.08*std::sin(15601*w)));
    }
    const auto full=render(input,settings,512,false,false);
    timing(full,512,false);
    const auto full1024=render(input,settings,1024,false,false);
    timing(full1024,1024,false);
    compare("1024 vs 512",full,full1024);
    const auto reduced=render(input,settings,512,true,false);
    timing(reduced,512,true);
    compare("Simplified vs full stress",full,reduced);
    settings[6]=settings[15]=0;
    settings[4]=settings[13]=9;
    settings[5]=settings[14]=2;
    for (int n=0;n<frames;++n) {
        const bool burst=n>=rate/4 && n<3*rate/4;
        const double w=2*std::numbers::pi*n/rate;
        input[n]={float((burst?.849:.035)*std::sin(1000*w)),float(.06*std::sin(1000*w))};
    }
    for (const bool fast:{false,true}) {
        settings[8]=settings[17]=fast;
        const auto base=render(input,settings,512,false,true);
        const auto candidate=render(input,settings,512,true,true);
        quality(base,fast,false);
        quality(candidate,fast,true);
        compare("Limiter burst simplified vs full",base,candidate);
    }
}
