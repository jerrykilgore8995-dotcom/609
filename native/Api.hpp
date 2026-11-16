#pragma once
#include "Controls.hpp"
#include <cstdint>

namespace mkii::native {
using MeterReadings = std::array<float,2>;
struct Api {
    std::uint32_t version;
    void* (*create)();
    void (*destroy)(void*);
    int (*prepare)(void*,double,int,int);
    void (*stop)(void*);
    int (*process)(void*,float* const*,int,const Controls*,int,int);
    unsigned (*failedSolves)(void*);
    void (*readMeters)(void*,MeterReadings*);
};
inline constexpr std::uint32_t apiVersion=1;
constexpr int playbackLatency(int hostBlock) {return 3*(hostBlock<512?512:hostBlock);}
using LoadApi=const Api* (*)();
}
