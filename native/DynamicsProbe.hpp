#pragma once

#include "Controls.hpp"
#include <cmath>
#include <cstdlib>
#include <numbers>
#include <string_view>

namespace mkii::native {
struct DynamicsProbe {
    explicit DynamicsProbe(char** args)
        : limiter(std::string_view(args[0]) == "limit"), attack(std::atoi(args[1])),
          recovery(std::atoi(args[2])), burst(std::atoi(args[3])), stop(std::atof(args[4])),
          high(std::atof(args[5])), threshold(std::atoi(args[6])), ratio(std::atoi(args[7])) {}

    static constexpr int rate = 44100;
    bool limiter;
    int attack, recovery, burst;
    double stop, high;
    int threshold, ratio;

    Controls controls() const {
        auto positions = defaults;
        positions[0] = limiter ? 1 : threshold;
        positions[1] = ratio;
        positions[2] = recovery;
        positions[4] = limiter ? threshold : 9;
        positions[5] = recovery;
        positions[6] = !limiter;
        positions[7] = limiter;
        positions[8] = limiter && attack == 1;
        return positions;
    }

    double input(double time) const {
        const double low = limiter ? 10. : -30.;
        const bool driven = time >= .45 && time < (450 + burst) / 1000.;
        const double dbu = driven ? high : low;
        return std::pow(10., (dbu - 24) / 20) * std::sin(2 * std::numbers::pi * 1000 * time);
    }
};
}
