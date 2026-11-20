#include "../native/Pipeline.hpp"

#include <chrono>
#include <cstdio>
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
}

int main() {
    const bool mono=run(1,false);
    const bool stereo=run(2,true);
    return mono && stereo ? 0 : 1;
}
