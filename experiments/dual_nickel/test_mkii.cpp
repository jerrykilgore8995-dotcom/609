#include "AnalogOptoProcessor.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

namespace {
constexpr double sampleRate = 44100.;
constexpr int blockSize = 512;
constexpr std::array names{"NSL-32", "VTL5C2", "T4B", "VTL5C1"};

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

float tone(int sample, double frequency, double level, double phase = 0.) {
    return float(nickel::dbGain(level - 24.) *
        std::sin(2. * std::numbers::pi * frequency * sample / sampleRate + phase));
}

struct Burst {
    double endGr = 0., attackMs = -1., halfRecoveryMs = -1., grAfter500Ms = 0.;
};

Burst measureBurst(int kind, double level, double seconds) {
    optocircuit::OpticalCompressor opto;
    opto.prepare(sampleRate, {}, true);
    opto.setCellKind(static_cast<optocircuit::CellKind>(kind));
    Burst result;
    const int count = int(seconds * sampleRate);
    for (int i = 0; i < count; ++i) {
        opto.process(tone(i, 1000., level));
        if (result.attackMs < 0. && opto.reductionDb() >= 3. * .632)
            result.attackMs = 1000. * i / sampleRate;
    }
    result.endGr = opto.reductionDb();
    for (int i = 1; i <= int(.5 * sampleRate); ++i) {
        opto.process(0.f);
        if (result.halfRecoveryMs < 0. && opto.reductionDb() <= result.endGr / 2.)
            result.halfRecoveryMs = 1000. * i / sampleRate;
    }
    result.grAfter500Ms = opto.reductionDb();
    return result;
}

void checkCellDynamics() {
    std::array<Burst, 4> shortBursts{}, longBursts{};
    for (int kind = 0; kind < 4; ++kind) {
        double lo = -6., hi = 20.;
        for (int iteration = 0; iteration < 13; ++iteration) {
            const double level = .5 * (lo + hi);
            if (measureBurst(kind, level, 1.).endGr > 3.) hi = level;
            else lo = level;
        }
        const double level = .5 * (lo + hi);
        const auto shortBurst = measureBurst(kind, level, .15);
        const auto longBurst = measureBurst(kind, level, 2.);
        shortBursts[kind] = shortBurst;
        longBursts[kind] = longBurst;
        check(std::abs(longBurst.endGr - 3.) < .12, "Could not match 3 dB GR");
        check(longBurst.attackMs > 0. && longBurst.halfRecoveryMs > 0.,
              "Missing attack or release crossing");
        std::cout << names[kind] << " at " << level << " dBu: end GR "
                  << longBurst.endGr << " dB, 63% attack " << longBurst.attackMs
                  << " ms, half release " << longBurst.halfRecoveryMs
                  << " ms, GR at 500 ms after short/long burst "
                  << shortBurst.grAfter500Ms << '/' << longBurst.grAfter500Ms << " dB\n";
    }
    check(longBursts[2].grAfter500Ms > shortBursts[2].grAfter500Ms + .1,
          "T4B sustained exposure should increase the recovery tail");

    for (const auto kind : {optocircuit::CellKind::VTL5C1, optocircuit::CellKind::VTL5C2}) {
        const bool fast = kind == optocircuit::CellKind::VTL5C1;
        optocircuit::LdrCell cell;
        cell.prepare(sampleRate, {}, true);
        cell.select(kind);
        for (const auto& [milliamps, expected] : std::array{
                 std::pair{1., fast ? 20000. : 5500.}, std::pair{10., fast ? 600. : 800.},
                 std::pair{40., 200.}}) {
            cell.reset();
            for (int i = 0; i < int(3. * sampleRate); ++i)
                cell.processAtLedCurrent(0., milliamps * .001);
            check(std::abs(cell.resistance() / expected - 1.) < .02,
                  "Vactrol resistance anchor shifted");
        }
        const int offSamples = int((fast ? .035 : .500) * sampleRate);
        for (int i = 0; i < offSamples; ++i)
            cell.processAtLedCurrent(0., 0.);
        check(cell.resistance() >= 100000., "Vactrol missed datasheet turn-off endpoint");
    }
}

void checkStereo() {
    for (int kind = 0; kind < 4; ++kind) {
        std::array<double, 4> gr{};
        for (std::size_t scenario = 0; scenario < gr.size(); ++scenario) {
            analogopto::StereoEngine engine;
            engine.prepare(sampleRate, true);
            engine.setOptoCell(kind);
            for (int i = 0; i < 22050; ++i) {
                float left = tone(i, 100., 8.);
                float right = scenario == 3 ? 0.f : tone(i, 100., 8.,
                    std::array{0., std::numbers::pi, std::numbers::pi / 2.}[scenario]);
                engine.process(left, right);
                check(std::isfinite(left) && std::isfinite(right), "Non-finite stereo audio");
                check(std::abs(engine.grLeft() - engine.grRight()) < 1.e-10,
                      "Stereo attenuation diverged");
                if (i >= 17640) gr[scenario] += engine.grLeft() / 4410.;
            }
        }
        check(std::abs(gr[0] - gr[1]) < .025, "Polarity affected the stereo link");
        check(std::abs(gr[0] - gr[2]) < .25, "Quadrature shifted the stereo link");
        check(std::abs(gr[0] - gr[3]) < .01, "Single-channel program changed linked threshold");
        std::cout << names[kind] << " linked GR in phase, inverted, quadrature, left only: "
                  << gr[0] << ", " << gr[1] << ", " << gr[2] << ", " << gr[3] << " dB\n";
    }
}

void checkReceiver() {
    nickel::LoadedReceiver receiver;
    const auto idle = receiver.evaluate(0.);
    const auto positive = receiver.evaluate(10.);
    const auto negative = receiver.evaluate(-10.);
    check(std::abs(idle.baseCurrent) < 1.e-14, "Incorrect receiver DC solution");
    check(positive.baseCurrent > 0. && negative.baseCurrent < 0. &&
          std::abs(positive.baseSlope - negative.baseSlope) > 1.e-8,
          "Receiver winding load should change with the operating point");

    for (const bool loaded : {false, true}) {
        for (const double level : {4., 18.}) {
            nickel::Transformer transformer;
            transformer.prepare(sampleRate, {}, loaded);
            double fundamental = 0., secondReal = 0., secondImaginary = 0.;
            for (int i = 0; i < 44100; ++i) {
                const double out = transformer.process(tone(i, 100., level));
                check(std::isfinite(out), "Receiver output should remain finite");
                if (i < 22050) continue;
                const double phase = 2. * std::numbers::pi * 100. * i / sampleRate;
                fundamental += out * std::sin(phase);
                secondReal += out * std::cos(2. * phase);
                secondImaginary += out * std::sin(2. * phase);
            }
            const double k2 = 20. * std::log10(std::hypot(secondReal, secondImaginary)
                                                / std::abs(fundamental));
            std::cout << (loaded ? "Loaded receiver" : "Original receiver") << " K2 at "
                      << level << " dBu / 100 Hz, normalized: " << k2 << " dBFS\n";
        }
    }
}

void reportTiming() {
    for (int kind = 0; kind < 4; ++kind) {
        analogopto::StereoEngine engine;
        engine.prepare(sampleRate, true);
        engine.setOptoCell(kind);
        double checksum = 0.;
        std::array<double, 85> blockMs{};
        for (std::size_t block = 0; block < blockMs.size(); ++block) {
            const auto before = std::chrono::steady_clock::now();
            for (int i = 0; i < blockSize; ++i) {
                const int sample = int(block) * blockSize + i;
                float left = tone(sample, 110., 12.);
                float right = tone(sample, 196., 12.);
                engine.process(left, right);
                checksum += left + right;
            }
            blockMs[block] = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - before).count();
        }
        std::sort(blockMs.begin(), blockMs.end());
        check(std::isfinite(checksum), "Non-finite stereo timing render");
        std::cout << names[kind] << " stereo 512 p50/p95/max milliseconds: "
                  << blockMs[42] << '/' << blockMs[80] << '/' << blockMs.back() << '\n';
    }
}
} // namespace

int main() {
    try {
        std::cout << std::fixed << std::setprecision(3);
        checkStereo();
        checkCellDynamics();
        checkReceiver();
        reportTiming();
        std::cout << "NICKEL_OPTO_MKII=PASS\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
