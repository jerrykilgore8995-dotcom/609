#pragma once
#include "Api.hpp"
#include "Circuit.hpp"
#include <atomic>
#include <chrono>
#include <limits>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mkii::native {

class Pipeline {
public:
    ~Pipeline() {
        stop();
#ifdef _WIN32
        if (event) CloseHandle(event);
#endif
    }
    bool prepare(double rate,int inputChannels,int hostBlock) {
        stop();
        if (inputChannels<1 || inputChannels>2 || hostBlock<1 || hostBlock>4096) return false;
        delay=playbackLatency(hostBlock);
        channels=inputChannels;
        if (!solver.prepare(rate)) return false;
#ifdef _WIN32
        if (!event) event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        if (!event) return false;
#endif
        started=true;
        worker=std::thread([this]{run();});
        return true;
    }
    void stop() {
        written.store(stopSignal,std::memory_order_release);
        wake();
        if (worker.joinable()) worker.join();
        written.store(0,std::memory_order_relaxed);
        rendered.store(0,std::memory_order_relaxed);
        failures.store(0,std::memory_order_relaxed);
        started=false;
        position=0;
        meters={};
    }
    bool process(float* const* buffers,int count,const Controls& controls,bool offline=false,bool dry=false) {
        if (!started || count<0) return false;
        bool complete=true;
        for (int offset=0;offset<count;) {
            const int frames=std::min(count-offset,512);
            const auto end=position+frames;
            while (offline && end-rendered.load(std::memory_order_acquire)>capacity)
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            if (end-rendered.load(std::memory_order_acquire)>capacity) return false;
            for (int i=0;i<frames;++i) {
                auto& sample=input[(position+i)&mask];
                sample.values={buffers[0][offset+i],channels>1?buffers[1][offset+i]:0.0f};
                sample.settings=controls;
            }
            written.store(end,std::memory_order_release);
            wake();
            const auto need=end>delay?end-delay:0;
            while (offline && rendered.load(std::memory_order_acquire)<need)
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            const auto available=rendered.load(std::memory_order_acquire);
            for (int i=0;i<frames;++i) {
                const auto time=position+i;
                if (time<delay) {
                    for (int ch=0;ch<channels;++ch) buffers[ch][offset+i]=0;
                    continue;
                }
                const auto source=time-delay;
                const bool ready=source<available;
                const auto index=source&mask;
                if (ready) meters=output[index].meter;
                if (!ready && !dry) complete=false;
                for (int ch=0;ch<channels;++ch)
                    buffers[ch][offset+i]=(ready && !dry)?output[index].values[ch]:input[index].values[ch];
            }
            position=end;
            offset+=frames;
        }
        return complete;
    }
    unsigned failedSolves() const {return failures.load(std::memory_order_relaxed);}
    MeterReadings readMeters() const {return meters;}

private:
    static constexpr std::uint64_t capacity=32768,mask=capacity-1;
    static constexpr auto stopSignal=std::numeric_limits<std::uint64_t>::max();
    struct Input {std::array<float,2> values{};Controls settings=defaults;};
    struct Output {std::array<float,2> values{};MeterReadings meter{};};
    void wake() {
#ifdef _WIN32
        if (event) SetEvent(event);
#endif
    }
    void wait() {
#ifdef _WIN32
        WaitForSingleObject(event,INFINITE);
#else
        std::this_thread::sleep_for(std::chrono::microseconds(75));
#endif
    }
    void run() {
#ifdef _WIN32
        SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_ABOVE_NORMAL);
#endif
        std::uint64_t next=0;
        while (true) {
            const auto available=written.load(std::memory_order_acquire);
            if (available==stopSignal) return;
            if (available==next) {wait();continue;}
            const auto end=std::min(available,next+8);
            for (;next<end;++next) {
                const auto& sample=input[next&mask];
                const auto result=solver.step({sample.values[0],sample.values[1]},sample.settings);
                const auto voltage=solver.meterVolts();
                auto& out=output[next&mask];
                out.values={float(result[0]),float(result[1])};
                for (int ch=0;ch<2;++ch)
                    out.meter[ch]=float(std::max(0.0,voltage[ch]*6.9));
            }
            failures.store(solver.failures(),std::memory_order_relaxed);
            rendered.store(next,std::memory_order_release);
        }
    }
    Solver solver;
    std::array<Input,capacity> input{};
    std::array<Output,capacity> output{};
    alignas(64) std::atomic<std::uint64_t> written{0};
    alignas(64) std::atomic<std::uint64_t> rendered{0};
    std::atomic<unsigned> failures{0};
    std::thread worker;
    std::uint64_t position=0,delay=1536;
    MeterReadings meters{};
    bool started=false;
    int channels=2;
#ifdef _WIN32
    HANDLE event=nullptr;
#endif
};
}
