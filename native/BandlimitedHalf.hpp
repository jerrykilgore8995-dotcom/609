#pragma once
#include <array>
#include <cstdint>
namespace mkii::native {
struct BandlimitedHalf {
    // Windowed-sinc fractional-sample interpolation of a 44.1 kHz
    // bandlimited input at the midpoint between two host samples.
    // All audio is already buffered; 23 future samples can be consumed by
    // the solver worker while maintaining the host's 1024-sample latency.
    static constexpr int future=23;
    static constexpr int past=24;
    static constexpr std::array<double,48> weights{
        -0.00003168001359651743,
        0.00009424479763212624,
        -0.0002039195253396132,
        0.0003802011658270671,
        -0.0006469606303725552,
        0.0010327729168332414,
        -0.0015712436663113086,
        0.002301393893253662,
        -0.003268208756796489,
        0.004523522058296789,
        -0.006127510653540618,
        0.008151240899983224,
        -0.010680999582723885,
        0.01382567194779089,
        -0.0177294538808136,
        0.022594288283613304,
        -0.028721039501845444,
        0.036589447495704676,
        -0.047025985130542985,
        0.06159600405353468,
        -0.083668234243599,
        0.1220334769938722,
        -0.20899584073145952,
        0.6355488118105996,
        0.6355488118105996,
        -0.20899584073145952,
        0.1220334769938722,
        -0.083668234243599,
        0.06159600405353468,
        -0.047025985130542985,
        0.036589447495704676,
        -0.028721039501845444,
        0.022594288283613304,
        -0.0177294538808136,
        0.01382567194779089,
        -0.010680999582723885,
        0.008151240899983224,
        -0.006127510653540618,
        0.004523522058296789,
        -0.003268208756796489,
        0.002301393893253662,
        -0.0015712436663113086,
        0.0010327729168332414,
        -0.0006469606303725552,
        0.0003802011658270671,
        -0.0002039195253396132,
        0.00009424479763212624,
        -0.00003168001359651743
    };
    template<typename Fetch>
    static double interpolate(std::uint64_t frame,Fetch fetch) {
        double sample=0;
        for (int i=0;i<48;++i) {
            const auto index=static_cast<std::int64_t>(frame) - (i-23);
            if (index>=0) sample+=weights[i]*fetch(static_cast<std::uint64_t>(index));
        }
        return sample;
    }
};
}
