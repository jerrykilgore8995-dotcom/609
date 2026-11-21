#pragma once

#include <array>

namespace mkii::presets {

struct Setting {
    const char* group;
    const char* name;
    int threshold;       // dBu
    int ratio;           // 1 = 1.5:1, 5 = 6:1
    int recovery;        // 1–4 = fixed, 5–6 = a1/a2
    int makeup;          // dB
    int attack;          // 0 = JD, 1 = 7.5 ms, 2 = 15 ms
    bool compress = true;
    int limitThreshold = 0; // Twice dBu. Zero switches the limiter out.
    int limitRecovery = 2;
    bool fast = true;
};

// Published settings and original applications are identified in docs/presets.md.
inline constexpr std::array<Setting, 30> bank{{
    {"Mix Bus", "Subtle Stereo Glue",          4, 1, 5, 0, 0},
    {"Mix Bus", "Classic Stereo Glue",         2, 2, 5, 2, 0},
    {"Mix Bus", "Rock Mix Punch",              0, 4, 5, 2, 2},
    {"Mix Bus", "Tempo Glue 400",               0, 3, 2, 2, 1},
    {"Mix Bus", "Slow Song 800",                0, 3, 3, 2, 1},
    {"Mix Bus", "Dense Mix",                   -2, 4, 2, 4, 0},
    {"Mix Bus", "Slow Auto Mix",                2, 2, 6, 2, 2},

    {"Drums", "CLA Parallel Drums",             0, 2, 1, 8, 0},
    {"Drums", "Rock Kit Punch",                -4, 4, 1, 4, 2},
    {"Drums", "Metal Kit Control",             -4, 4, 1, 4, 0},
    {"Drums", "Parallel Drum Crush",           -8, 5, 1, 8, 0},
    {"Drums", "Parallel Drum Weight",          -6, 4, 1, 6, 0},
    {"Drums", "Room Mic Sustain",              -6, 5, 5, 6, 1},
    {"Drums", "Overhead Glue",                  0, 2, 2, 2, 1},
    {"Drums", "Kick and Snare Punch",          -2, 4, 1, 4, 2},
    {"Drums", "Sampled Drum and Loop Bus",     -4, 3, 2, 4, 0},

    {"Vocals", "Lead Vocal Peak Catch",         2, 4, 1, 2, 0},
    {"Vocals", "Lead Vocal Auto Level",        -2, 3, 5, 4, 0},
    {"Vocals", "Tracking Vocal to Tape",        0, 3, 2, 2, 0},
    {"Vocals", "Stereo Vocal Package",          0, 2, 5, 2, 1},
    {"Vocals", "Backing Vocal Bus",            -2, 2, 3, 2, 1},

    {"Instruments", "Bass Guitar Level",       -2, 3, 2, 4, 0},
    {"Instruments", "Bass Peak Control",        0, 4, 1, 2, 0},
    {"Instruments", "Keys Bus",                 0, 2, 5, 2, 1},
    {"Instruments", "Acoustic Guitar Level",   -2, 2, 2, 2, 1},
    {"Instruments", "Electric Guitar Bus",      0, 2, 3, 2, 2},

    {"Mastering", "Transformer Pass",          10, 1, 5, 0, 0, false},
    {"Mastering", "Light Mastering",            6, 1, 5, 0, 0},
    {"Mastering", "Fast Mastering Catch",       6, 1, 1, 0, 0, true, 30, 1},
    {"Mastering", "Limiter Insurance",          4, 2, 5, 2, 0, true, 30, 2},
}};

constexpr std::array<int, 9> detents(const Setting& p) {
    return {(p.threshold+20)/2, p.ratio-1, p.recovery-1, p.makeup/2,
            p.limitThreshold ? p.limitThreshold-8 : 8, p.limitRecovery-1,
            p.compress, p.limitThreshold != 0, p.fast && p.limitThreshold != 0};
}

static_assert([] {
    constexpr std::array upper{15,4,5,10,22,5,1,1,1};
    for (const auto& preset : bank) {
        const auto values = detents(preset);
        for (std::size_t i=0; i<upper.size(); ++i)
            if (values[i]<0 || values[i]>upper[i]) return false;
        if (preset.threshold%2 || preset.makeup%2 || preset.attack<0 || preset.attack>2) return false;
    }
    return true;
}());

}
