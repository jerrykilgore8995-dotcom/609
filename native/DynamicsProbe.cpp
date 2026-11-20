#include "DynamicsProbe.hpp"
#include "Realtime.hpp"

#include <iomanip>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 9) return 2;
    const mkii::native::DynamicsProbe probe(argv + 1);
    mkii::native::Realtime engine;
    engine.prepare(probe.rate);
    engine.control(probe.controls(), probe.limiter ? 0 : probe.attack);
    std::cout << std::setprecision(12);
    for (int sample = 0; sample <= std::ceil(probe.stop * probe.rate); ++sample) {
        const double time = double(sample) / probe.rate;
        const auto output = engine.step({probe.input(time), 0.});
        std::cout << time << ' ' << output[0] << '\n';
    }
}
