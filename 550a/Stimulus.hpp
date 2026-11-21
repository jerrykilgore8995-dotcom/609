#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace api550a {
inline constexpr int sweepPeriod = 16384;

inline std::vector<double> sweepStimulus(int rate, double amplitude) {
    constexpr std::array marks{50,100,200,300,400,800,1000,1500,3000,5000,
                               7000,10000,12500,15000,20000};
    std::vector<int> bins;
    const auto bin = [&](double f) {
        return static_cast<int>(std::lround(f*sweepPeriod/rate));
    };
    for (int i = 0; i < 112; ++i)
        bins.push_back(bin(30*std::pow(20000.0/30, static_cast<double>(i)/111)));
    for (int f : marks) bins.push_back(bin(f));
    std::sort(bins.begin(), bins.end());
    bins.erase(std::unique(bins.begin(), bins.end()), bins.end());
    std::vector<double> result(sweepPeriod);
    for (std::size_t k=0; k<bins.size(); ++k) {
        const double t = static_cast<double>(k);
        const double phase = std::numbers::pi*t*(t-1)/static_cast<double>(bins.size());
        const double step = 2*std::numbers::pi*bins[k]/sweepPeriod;
        for (int n=0; n<sweepPeriod; ++n) result[n] += amplitude*std::sin(step*n+phase);
    }
    return result;
}
}
