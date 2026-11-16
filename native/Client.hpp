#pragma once

#include "Api.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace mkii::native {

class Client {
public:
    Client()=default;
    Client(const Client&)=delete;
    Client& operator=(const Client&)=delete;
    ~Client() {close();}

    bool open(const wchar_t* path) {
        close();
        module=LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!module) return false;
        const auto load=reinterpret_cast<LoadApi>(GetProcAddress(module,"dbx609_native_api"));
        api=load?load():nullptr;
        if (api && api->version==apiVersion) instance=api->create();
        if (instance) return true;
        close();
        return false;
    }

    bool prepare(double rate,int channels,int hostBlock) {
        if (!instance || !api->prepare(instance,rate,channels,hostBlock)) return false;
        delay=playbackLatency(hostBlock);
        return true;
    }
    void stop() {if (instance) api->stop(instance);}
    int latency() const {return instance?delay:0;}
    unsigned failedSolves() const {return instance?api->failedSolves(instance):0;}
    MeterReadings readMeters() const {
        MeterReadings meters;
        if (instance) api->readMeters(instance,&meters);
        return meters;
    }
    bool process(float* const* audio,int count,Controls settings,bool offline,bool dry=false) {
        return instance && api->process(instance,audio,count,&settings,offline,dry);
    }

private:
    void close() {
        if (instance) api->destroy(instance);
        instance=nullptr;
        delay=0;
        api=nullptr;
        if (module) FreeLibrary(module);
        module=nullptr;
    }
    HMODULE module=nullptr;
    int delay=0;
    void* instance=nullptr;
    const Api* api=nullptr;
};

}
