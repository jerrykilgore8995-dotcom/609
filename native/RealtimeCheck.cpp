#include "Circuit.hpp"
#include "Realtime.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <numbers>
#include <vector>

namespace {
using namespace mkii::native;
constexpr double rate=44100;
constexpr double radians=2*std::numbers::pi;

double peak(double dbu) {return std::pow(10.,(dbu-24)/20);}
double db(double amplitude) {return 20*std::log10(std::max(amplitude,1e-12));}

struct Renderers {
    std::unique_ptr<Solver> full=std::make_unique<Solver>();
    Realtime live;
    Controls controls;
    bool ready;
    explicit Renderers(Controls settings):controls(settings),ready(full->prepare(rate)) {
        live.prepare(rate);
        live.control(controls);
    }
    std::array<Solver::Frame,2> step(Solver::Frame input) {
        return {full->step(input,controls),live.step(input)};
    }
    bool valid() const {return ready && full->failures()==0;}
};

bool tone(const char* label,Controls controls,double level,double frequency,double& delta) {
    Renderers test(controls);
    if (!test.ready) return false;
    double sine[2]{},cosine[2]{};
    constexpr int duration=26460, window=8820;
    for (int n=0;n<duration;++n) {
        const auto phase=radians*frequency*n/rate;
        const auto signal=peak(level)*std::sin(phase);
        const auto output=test.step({signal,signal});
        if (n<duration-window) continue;
        for (int model=0;model<2;++model) {
            sine[model]+=output[model][0]*std::sin(phase);
            cosine[model]+=output[model][0]*std::cos(phase);
        }
    }
    const auto reference=db(2*std::hypot(sine[0],cosine[0])/window)+24;
    const auto realtime=db(2*std::hypot(sine[1],cosine[1])/window)+24;
    delta=realtime-reference;
    std::printf("%s, %.0f Hz, %+.0f dBu: full %+.3f dBu, realtime %+.3f dBu, error %+.3f dB\n",
                label,frequency,level,reference,realtime,delta);
    return test.valid() && std::isfinite(delta);
}

bool burst(int mode,bool cymbals) {
    auto controls=defaults;
    controls[1]=controls[10]=4;
    controls[6]=controls[15]=(mode&1)!=0;
    controls[7]=controls[16]=(mode&2)!=0;
    controls[19]=1;
    Renderers test(controls);
    if (!test.ready) return false;
    std::vector<double> error;
    std::array<std::array<double,2>,2> energy{};
    double squared=0,referenceEnergy=0;
    unsigned rng=912;
    constexpr int window=256;
    for (int n=0;n<88200;++n) {
        const double t=n/rate,w=radians*t;
        Solver::Frame in{};
        for (int ch=0;ch<2;++ch) {
            if (cymbals) {
                const auto envelope=std::exp(-15*std::fmod(t,.22));
                in[ch]=.06*std::sin(149*w+ch)+.02*std::sin(909*w)+
                    envelope*(.21*std::sin(9921*w+.3*ch)+.16*std::sin(17351*w+ch));
            } else {
                rng^=rng<<13;
                rng^=rng>>17;
                rng^=rng<<5;
                const auto envelope=std::exp(-12*std::fmod(t+.015*ch,.19));
                in[ch]=envelope*(.23*std::sin(83*w+ch)+.04*std::sin(1831*w)+
                    .05*(double(rng)/2147483648.-1));
            }
        }
        const auto output=test.step(in);
        if (n<4410) continue;
        for (int ch=0;ch<2;++ch) {
            for (int model=0;model<2;++model) energy[model][ch]+=output[model][ch]*output[model][ch];
            squared+=std::pow(output[1][ch]-output[0][ch],2);
            referenceEnergy+=output[0][ch]*output[0][ch];
        }
        if ((n-4409)%window==0) {
            for (int ch=0;ch<2;++ch) error.push_back(std::abs(db(std::sqrt(energy[1][ch]/window))-
                                                             db(std::sqrt(energy[0][ch]/window))));
            energy={};
        }
    }
    std::sort(error.begin(),error.end());
    const auto p95=error[(95*error.size()+99)/100-1];
    const auto residual=100*std::sqrt(squared/referenceEnergy);
    std::printf("%s, linked mode %d: window RMS level p95 error %.3f dB, waveform residual %.2f percent\n",
                cymbals?"Cymbal-band bursts":"Noisy low-frequency bursts",mode,p95,residual);
    const double envelopeBudget=cymbals?1.1:3.0;
    return test.valid() && std::isfinite(residual) && p95<=envelopeBudget;
}
}

int main() {
    auto controls=defaults;
    bool passed=true;
    for (const auto frequency:{20.,1000.,12000.,20000.}) {
        double delta=0;
        passed=tone("Through",controls,0,frequency,delta) && std::abs(delta)<.05 && passed;
    }
    for (int ratio=1;ratio<=5;++ratio) {
        controls=defaults;
        controls[0]=controls[9]=11;
        controls[1]=controls[10]=ratio;
        controls[6]=controls[15]=1;
        double low=0,high=0;
        const bool validLow=tone("Compressor",controls,0,1000,low);
        const bool validHigh=tone("Compressor",controls,10,1000,high);
        passed=validLow && validHigh && std::max(std::abs(low),std::abs(high))<.3 && std::abs(low-high)<.3 && passed;
    }
    for (int fast:{0,1}) {
        controls=defaults;
        controls[7]=controls[16]=1;
        controls[8]=controls[17]=fast;
        double low=0,high=0;
        const bool validLow=tone("Limiter",controls,10,1000,low);
        const bool validHigh=tone("Limiter",controls,20,1000,high);
        passed=validLow && validHigh && std::max(std::abs(low),std::abs(high))<.2 && std::abs(low-high)<.2 && passed;
    }
    for (int mode:{1,2,3}) {
        passed=burst(mode,true) && passed;
        passed=burst(mode,false) && passed;
    }
    return passed?0:1;
}
