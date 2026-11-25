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
constexpr int oversamplingFactor = 4;
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

void checkT4Sensitivity() {
    constexpr double internalRate = oversamplingFactor * sampleRate;
    for (const bool feedforward : {false, true}) {
        optocircuit::OpticalCompressor opto;
        opto.prepare(internalRate, {}, true);
        opto.setCellKind(optocircuit::CellKind::T4B);
        opto.setFeedforward(feedforward);
        opto.setThresholdDb(-18.);
        double first10ms = 0.;
        double settled = 0.;
        for (int i = 0; i < int(internalRate * 2.); ++i) {
            const double phase = 2. * std::numbers::pi * 100. * i / internalRate;
            opto.process(float(nickel::dbGain(12. - 24.) * std::sin(phase)));
            const double gr = opto.reductionDb();
            if (i < int(internalRate * .01)) first10ms = std::max(first10ms, gr);
            if (i >= int(internalRate * 1.9)) settled = std::max(settled, gr);
        }
        check(settled > 4. && settled < 12., "T4B drive should stay in its calibrated range");
        check(first10ms < .7 * settled, "T4B should not hit its steady GR on short peaks");
        std::cout << "T4B 100 Hz +12 dBu, " << (feedforward ? "FF" : "FB")
                  << " first 10 ms/settled peak GR " << first10ms << '/' << settled << " dB\n";
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

void checkReceiverHeadroom() {
    analogopto::MonoEngine engine;
    engine.prepare(sampleRate, true);
    engine.setThresholdDb(18.);
    double fundamentalSin = 0., fundamentalCos = 0.;
    double harmonicSin = 0., harmonicCos = 0.;
    for (int i = 0; i < int(3. * sampleRate); ++i) {
        const double out = engine.process(tone(i, 100., 24.));
        if (i < int(2. * sampleRate)) continue;
        const double phase = 2. * std::numbers::pi * 100. * i / sampleRate;
        fundamentalSin += out * std::sin(phase);
        fundamentalCos += out * std::cos(phase);
        harmonicSin += out * std::sin(28. * phase);
        harmonicCos += out * std::cos(28. * phase);
    }
    const double harmonic = std::hypot(harmonicSin, harmonicCos);
    const double fundamental = std::hypot(fundamentalSin, fundamentalCos);
    const double k28 = 20. * std::log10(harmonic / fundamental);
    check(k28 < -98., "Receiver cutoff at full-scale 100 Hz");
    std::cout << "100 Hz full-scale, K28 normalized: " << k28 << " dBFS\n";
}

void checkIsolatedPhotocellHarmonics() {
    constexpr int samples = 4410; // 100 cycles of the 1 kHz test tone.
    for (int kind = 0; kind < 4; ++kind) {
        const auto measure = [kind](double polarity, double reductionDb) {
            optocircuit::OptoParts parts;
            parts.cellContactAsymmetry *= polarity;
            optocircuit::LdrCell cell;
            cell.prepare(sampleRate, parts, true);
            const auto cellKind = static_cast<optocircuit::CellKind>(kind);
            cell.select(cellKind);
            const bool panel = cellKind == optocircuit::CellKind::T4B;
            const double dark = 1. / cell.darkResistance();
            const double full = panel ? 1. / 750. : cell.physicalConductance(20.);
            const double conductance = 1. / parts.rSeries + 1. / parts.rReceive + dark;
            const double light = conductance * (nickel::dbGain(reductionDb) - 1.) / (full - dark);
            for (int i = 0; i < int(3 * sampleRate); ++i)
                cell.processIllumination(0., light, 0., panel);
            std::array<double, 5> cosine{}, sine{};
            for (int i = -441; i < samples; ++i) {
                const double phase = 2. * std::numbers::pi * 1000. * i / sampleRate;
                const double y = cell.processIllumination(
                    nickel::fullScalePeakVolts * nickel::dbGain(-18.) * std::sin(phase),
                    light, 0., panel);
                if (i < 0) continue;
                for (const int k : {1, 2, 4}) {
                    cosine[k] += y * std::cos(k * phase);
                    sine[k] += y * std::sin(k * phase);
                }
            }
            const double fundamental = std::hypot(cosine[1], sine[1]);
            return std::array{cosine[2] / fundamental,
                std::hypot(cosine[2], sine[2]) / fundamental,
                std::hypot(cosine[4], sine[4]) / fundamental};
        };
        const auto positive = measure(1., 6.);
        const auto negative = measure(-1., 6.);
        const auto symmetric = measure(0., 6.);
        const auto dark = measure(1., 0.);
        const auto normalized = [](double amplitude) { return 20. * std::log10(amplitude) - 18.; };
        check(normalized(positive[1]) > -100. && normalized(positive[1]) < -95.,
              "Illuminated photocell should generate small but measurable K2");
        check(symmetric[1] < 1.e-6 && dark[1] < 1.e-8,
              "Photocell K2 should fall to the baseline with no asymmetry or light");
        check(positive[0] * negative[0] < 0.
              && std::abs(positive[0] + negative[0]) < positive[1] * .05,
              "Reversing contact polarity should reverse K2 phase");
        check(positive[2] > symmetric[2] * 10., "Photocell crossover should produce K4");
        std::cout << names[kind] << " fixed light at 6 dB GR, normalized K2/K4 "
                  << normalized(positive[1]) << '/' << normalized(positive[2]) << " dBFS\n";
    }
}

void checkDigitalOutput() {
    analogopto::MonoEngine engine;
    engine.prepare(sampleRate * oversamplingFactor, true);
    engine.setLinearGains(1., 2., .75);
    check(engine.processOutput(.25f) == .375f, "Incorrect digital makeup/trim gain");
    check(engine.processOutput(-1.f) == -1.5f, "Digital output should preserve peaks above full scale");
}

void reportTiming() {
    for (int kind = 0; kind < 4; ++kind) {
        analogopto::StereoEngine engine;
        engine.prepare(sampleRate * oversamplingFactor, true);
        engine.setOptoCell(kind);
        double checksum = 0.;
        std::array<double, 85> blockMs{};
        for (std::size_t block = 0; block < blockMs.size(); ++block) {
            const auto before = std::chrono::steady_clock::now();
            constexpr int internalBlockSize = blockSize * oversamplingFactor;
            for (int i = 0; i < internalBlockSize; ++i) {
                const int sample = int(block) * internalBlockSize + i;
                float left = tone(sample, 110. / oversamplingFactor, 12.);
                float right = tone(sample, 196. / oversamplingFactor, 12.);
                engine.process(left, right);
                checksum += left + right;
            }
            blockMs[block] = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - before).count();
        }
        std::sort(blockMs.begin(), blockMs.end());
        check(std::isfinite(checksum), "Non-finite stereo timing render");
        std::cout << names[kind] << " stereo 4x engine per host 512 p50/p95/max milliseconds: "
                  << blockMs[42] << '/' << blockMs[80] << '/' << blockMs.back() << '\n';
    }
}
} // namespace

int main() {
    try {
        std::cout << std::fixed << std::setprecision(3);
        checkStereo();
        checkCellDynamics();
        checkT4Sensitivity();
        checkReceiver();
        checkReceiverHeadroom();
        checkIsolatedPhotocellHarmonics();
        checkDigitalOutput();
        reportTiming();
        std::cout << "NICKEL_OPTO_MKII=PASS\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
