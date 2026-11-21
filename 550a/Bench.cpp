#include "Stimulus.hpp"
#include "Render.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <numbers>

int main(int argc, char** argv) {
    if (argc != 16) return 2;
    const auto integer = [&argv](int i) { return std::atoi(argv[i]); };
    const int rate = integer(1);
    const int samples = integer(2);
    const int channels = integer(3);
    const double frequency = std::atof(argv[4]);
    const double amplitude = std::atof(argv[5]);
    auto controls = mkii::native::defaults;
    for (int i = 0; i < 10; ++i) controls[i] = integer(6 + i);
    if (rate <= 0 || samples <= 0 || frequency < 0) return 2;
    auto circuit = std::make_unique<api550a::Render>();
    if (!circuit->prepare(rate, channels)) return 3;
    const auto sweep = frequency == 0 ? api550a::sweepStimulus(rate, amplitude) : std::vector<double>{};
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < samples; ++i) {
        const double t = static_cast<double>(i) / rate;
        const double y = frequency == 0 ? sweep[static_cast<std::size_t>(i % api550a::sweepPeriod)]
                       : amplitude * std::sin(2 * std::numbers::pi * frequency * t);
        const auto frame = circuit->step({y, channels == 2 ? (frequency == 0 ? -y : y) : 0.0}, controls);
        std::printf("%.14g\n", frequency == 0 && channels == 2 ? (frame[0]-frame[1])*.5 : frame[0]);
    }
    const auto ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    std::fprintf(stderr, "%u %d %.5f\n", circuit->failures(), circuit->latency(), ms);
    return circuit->failures() ? 1 : 0;
}
