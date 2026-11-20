#include "../native/Pipeline.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>
#include <numbers>

namespace {
using namespace mkii::native;
constexpr int rate=44100;
constexpr int block=512;
constexpr int blocks=100;
using Clock=std::chrono::steady_clock;

bool run(int channels, bool linked) {
    auto engine=std::make_unique<Pipeline>();
    if (!engine->prepare(rate,channels,block)) return false;
    auto settings=defaults;
    settings[19]=linked;
    for (int ch=0; ch<channels; ++ch) {
        const auto offset=9*ch;
        settings[offset]=11;
        settings[offset+1]=4;
        settings[offset+4]=9;
        settings[offset+6]=1;
        settings[offset+7]=1;
    }

    std::array<std::array<float,block>,2> audio{};
    float* buffers[]{audio[0].data(),audio[1].data()};
    std::array<double,blocks> times{};
    double outputPower=0;
    const auto start=Clock::now();
    for (int b=0; b<blocks; ++b) {
        for (int i=0; i<block; ++i) {
            const double t=double(b*block+i)/rate;
            const double a=std::exp(-18.0*std::fmod(t,.17));
            const double w=2*std::numbers::pi*t;
            for (int ch=0; ch<channels; ++ch)
                audio[ch][i]=float((ch==0?1.0:.68)*(.19*std::sin(89*w+ch)
                    +a*(.11*std::sin(822*w)+.13*std::sin(7447*w+ch))));
        }
        const auto before=Clock::now();
        if (!engine->process(buffers,block,settings,true)) return false;
        times[b]=std::chrono::duration<double,std::milli>(Clock::now()-before).count();
        for (int ch=0; ch<channels; ++ch)
            for (float y:audio[ch]) {
                if (!std::isfinite(y)) return false;
                outputPower+=y*y;
            }
    }
    const auto mean=std::chrono::duration<double,std::milli>(Clock::now()-start).count()/
        (blocks-playbackLatency(block)/block);
    std::sort(times.begin(),times.end());
    const auto fails=engine->failedSolves();
    std::printf("%d %s | %.2f ms/512 mean, %.2f ms callback p95, %u solve failures, RMS %.6f\n",
        CLASSIC_MODEL,linked?"linked stereo":"mono",mean,times[94],fails,
        std::sqrt(outputPower/(channels*blocks*block)));
    return fails==0 && outputPower>1e-8;
}
// Emit the solved waveform before host buffering. The analysis uses a centred
// single-cycle fundamental envelope and excludes wall clock scheduling.
int trace(int argc, char** argv) {
    if (argc != 12) return 2;
    const bool limiter = std::atoi(argv[2]) == 1;
    const int ratio = std::atoi(argv[3]), recovery = std::atoi(argv[4]);
    const int threshold = std::atoi(argv[5]), fast = std::atoi(argv[6]);
    const double low = std::atof(argv[7]), high = std::atof(argv[8]);
    const double duration = std::atof(argv[9]), tail = std::atof(argv[10]);
    const double frequency = std::atof(argv[11]);
    if (ratio < 1 || ratio > 5 || recovery < 1 || recovery > 6 ||
        threshold < 1 || threshold > (limiter ? 23 : 16) ||
        !(duration > .05 && duration <= 8 && tail > .05 && tail <= 16 &&
          frequency >= 100 && frequency <= 10000 && low >= -100 && high <= 26 && high > low))
        return 2;
    auto settings = defaults;
    settings[19] = 0;
    settings[6] = !limiter;
    settings[7] = limiter;
    settings[8] = fast != 0;
    settings[limiter ? 4 : 0] = threshold;
    settings[limiter ? 5 : 2] = recovery;
    settings[1] = ratio;
    Solver solver(0);
    if (!solver.prepare(rate)) return 3;
    const double lowAmplitude = std::pow(10.,(low-24)/20.);
    const double highAmplitude = std::pow(10.,(high-24)/20.);
    const double phaseIncrement = 2*std::numbers::pi*frequency/rate;
    std::vector<double> samples(static_cast<std::size_t>((.3+duration+tail)*rate));
    for (std::size_t i=0; i<samples.size(); ++i) {
        const double time = double(i)/rate;
        const auto amplitude = time >= .3 && time < .3+duration ? highAmplitude : lowAmplitude;
        samples[i] = solver.step({amplitude*std::sin(phaseIncrement*i),0},settings)[0];
    }
    for (auto sample:samples) std::printf("%.12g\n",sample);
    std::fprintf(stderr,"%u\n",solver.failures());
    return 0;
}

}

int main(int argc, char** argv) {
    if (argc > 1 && std::string_view(argv[1]) == "--trace") return trace(argc,argv);
    const bool mono=run(1,false);
    const bool stereo=run(2,true);
    return mono && stereo ? 0 : 1;
}
