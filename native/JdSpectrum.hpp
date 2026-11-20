#pragma once

#include "JdSpectrumTables.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace mkii::native {

struct JdSpectrum {
    struct Span {
        int lower;
        double mix;
    };

    template<std::size_t size>
    static Span locate(const double (&axis)[size], double coordinate) {
        const int upper = std::clamp<int>(std::upper_bound(axis, axis + size, coordinate) - axis, 1, size - 1);
        return {upper - 1, std::clamp((coordinate - axis[upper - 1]) /
                       (axis[upper] - axis[upper - 1]), 0., 1.)};
    }

    template<std::size_t size>
    static double coefficient(const float (&points)[size][84], Span drive, Span frequency, int index) {
        constexpr int start[]{0, 38, 76, 82, 84};
        const auto fetch = [&](int band) {
            const int column = start[band] + index;
            if (column >= start[band + 1]) return 0.;
            return std::lerp(double(points[drive.lower][column]),
                             double(points[drive.lower + 1][column]), drive.mix);
        };
        return std::lerp(fetch(frequency.lower), fetch(frequency.lower + 1), frequency.mix);
    }

    static void load(std::array<double, 40>& result, int mode, double hz, double db, int ratio,
                     double compThreshold, double limitThreshold, bool fastLimiter, int count) {
        using namespace measured;
        constexpr double frequency[]{2., 3., 3.69897000434, 4.};
        constexpr double throughDrive[]{-18, 0, 9, 18, 22};
        constexpr double dynamicsDrive[]{-9, -3, 0, 3, 6, 12, 20, 30};
        constexpr double compThresholds[]{-20, -10, 0, 10};
        constexpr double limitThresholds[]{4, 8, 15};
        const auto band = locate(frequency, std::log10(hz));
        const auto fade = std::clamp((hz-55.)/45., 0., 1.) * std::clamp((12000.-hz)/2000., 0., 1.);
        result[0] = result[1] = 0.;
        if (mode == 0) {
            const auto drive = locate(throughDrive, db);
            for (int i = 2; i < 2 * count; ++i)
                result[i] = fade * coefficient(jd_through[0], drive, band, i-2);
        } else if (mode == 1) {
            const auto threshold = locate(compThresholds, compThreshold);
            const auto drive = locate(dynamicsDrive, db-compThreshold);
            const int bank = (ratio-1) * 4 + threshold.lower;
            for (int i = 2; i < 2 * count; ++i)
                result[i] = fade * std::lerp(coefficient(jd_comp[bank], drive, band, i-2),
                                     coefficient(jd_comp[bank+1], drive, band, i-2), threshold.mix);
        } else {
            const auto threshold = locate(limitThresholds, limitThreshold);
            const auto drive = locate(dynamicsDrive, db-limitThreshold);
            const int bank = int(fastLimiter) * 3 + threshold.lower;
            for (int i = 2; i < 2 * count; ++i)
                result[i] = fade * std::lerp(coefficient(jd_limit[bank], drive, band, i-2),
                                     coefficient(jd_limit[bank+1], drive, band, i-2), threshold.mix);
        }
    }
};

}
