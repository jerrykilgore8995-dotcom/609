#pragma once

#include "../native/ClassicLane.hpp"

namespace api550a {

using Controls = mkii::native::Controls;

class Render {
public:
    int latency() const { return delay * 2; }

    bool prepare(double rate, int count) {
        if (count != 1 && count != 2) return false;
        channels = count;
        delay = mkii::native::ClassicLane::latencyFor(rate) / 2;
        history.fill(mkii::native::defaults);
        cursor = 0;
        for (int ch = 0; ch < channels; ++ch)
            if (!lanes[ch].prepare(rate)) return false;
        return true;
    }

    mkii::native::Solver::Frame step(mkii::native::Solver::Frame input, const Controls& controls) {
        const auto delayed = delay == 0 ? controls : history[cursor];
        if (delay) {
            history[cursor] = controls;
            if (++cursor == delay) cursor = 0;
        }

        mkii::native::Solver::Frame result{};
        for (int ch = 0; ch < channels; ++ch)
            result[ch] = lanes[ch].step(input, delayed)[ch];
        return result;
    }

    unsigned failures() const {
        unsigned total = 0;
        for (int ch = 0; ch < channels; ++ch) total += lanes[ch].failures();
        return total;
    }

private:
    std::array<mkii::native::ClassicLane, 2> lanes{
        mkii::native::ClassicLane(0), mkii::native::ClassicLane(1)};
    std::array<Controls, mkii::native::ClassicLane::filterRadius> history{};
    int cursor = 0, delay = 0, channels = 0;
};
}
