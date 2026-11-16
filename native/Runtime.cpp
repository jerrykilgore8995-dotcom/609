#include "Pipeline.hpp"

#include <new>

namespace {
using mkii::native::Api;
using mkii::native::Controls;
using mkii::native::Pipeline;

Pipeline& engine(void* instance) {
    return *static_cast<Pipeline*>(instance);
}

const Api runtime{
    mkii::native::apiVersion,
    []() -> void* {
        try {return new Pipeline;}
        catch (...) {return nullptr;}
    },
    [](void* instance) {delete static_cast<Pipeline*>(instance);},
    [](void* instance,double rate,int channels,int hostBlock) -> int {
        try {return engine(instance).prepare(rate,channels,hostBlock);}
        catch (...) {engine(instance).stop();return 0;}
    },
    [](void* instance) {engine(instance).stop();},
    [](void* instance,float* const* audio,int count,const Controls* settings,int offline,int dry) -> int {
        return engine(instance).process(audio,count,*settings,offline!=0,dry!=0);
    },
    [](void* instance) {return engine(instance).failedSolves();},
    [](void* instance,mkii::native::MeterReadings* meters) {*meters=engine(instance).readMeters();}
};
}

extern "C" __declspec(dllexport) const mkii::native::Api* dbx609_native_api() {
    return &runtime;
}
