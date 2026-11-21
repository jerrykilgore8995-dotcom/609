#include "Realtime.hpp"

#include <array>
#include <cmath>
#include <complex>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <string>

int main() {
    using namespace mkii::native;
    constexpr double rate = 44100;
    constexpr double pi = std::numbers::pi;
    std::string mode;
    int attack, ratio, threshold;
    double frequency, input;
    std::cout << std::setprecision(12);
    while (std::cin >> mode >> attack >> ratio >> threshold >> frequency >> input) {
        auto controls = defaults;
        controls[6] = mode == "comp";
        controls[7] = mode == "limit" || mode == "limit800";
        if (controls[6]) {
            controls[0] = threshold;
            controls[1] = ratio;
        }
        if (controls[7]) {
            controls[4] = threshold;
            controls[8] = attack;
            controls[5] = mode == "limit800" ? 4 : 1;
        }
        Realtime engine;
        engine.prepare(rate);
        engine.control(controls);
        constexpr int length = 30870;
        constexpr int window = 8820;
        const auto peak = std::pow(10., (input - 24.) / 20.);
        const auto rotate = std::polar(1., -2 * pi * frequency / rate);
        auto oscillator = std::complex<double>{1., 0.};
        std::array<std::complex<double>, 20> spectrum{};
        for (int sample = 0; sample < length; ++sample) {
            const auto frame = engine.step({-peak * oscillator.imag(), 0.});
            if (sample >= length - window) {
                auto weight = oscillator;
                for (auto& harmonic : spectrum) {
                    harmonic += frame[0] * weight;
                    weight *= oscillator;
                }
            }
            oscillator *= rotate;
        }
        for (const auto harmonic : spectrum) {
            const auto value = harmonic * (2. / window);
            std::cout << value.real() << ' ' << value.imag() << ' ';
        }
        std::cout << '\n';
    }
}
