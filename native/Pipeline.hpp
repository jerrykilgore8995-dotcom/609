#pragma once
#include "Api.hpp"
#include "Circuit.hpp"
#include "BandlimitedHalf.hpp"
#include <atomic>
#include <chrono>
#include <limits>
#include <semaphore>
#include <thread>
#if defined(__SSE2__) || defined(_M_X64)
#include <xmmintrin.h>
#endif
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
        for (int ch=0;ch<channels;++ch)
            if (!laneSolvers[ch].prepare(rate*internalFactor)) return false;
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
        submitted=returned=0;
        meters={};
    }
    bool process(float* const* buffers,int count,const Controls& controls,bool offline=false,bool dry=false) {
        if (!started || count<0) return false;
        bool complete=true;
        for (int offset=0;offset<count;) {
            const int frames=std::min(count-offset,512);
            const auto end=position+frames;
            const auto firstSource=position>delay?position-delay:0;
            discardBefore(firstSource,rendered.load(std::memory_order_acquire));
            while (offline && submitted+frames-returned>capacity) {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
                discardBefore(firstSource,rendered.load(std::memory_order_acquire));
            }
            const bool canSubmit=submitted+frames-returned<=capacity;
            for (int i=0;i<frames;++i) {
                const auto time=position+i;
                const std::array<float,2> values{buffers[0][offset+i],channels>1?buffers[1][offset+i]:0.0f};
                dryInput[time&mask]=values;
                if (canSubmit) input[(submitted+i)&mask]={values,controls,time};
            }
            if (canSubmit) {
                submitted+=frames;
                written.store(submitted,std::memory_order_release);
                wake();
            } else complete=false;
            if (offline) {
                const auto endSource=end>delay?end-delay:0;
                auto needed=returned;
                while (needed<submitted && input[needed&mask].time<endSource) ++needed;
                while (rendered.load(std::memory_order_acquire)<needed)
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
            const auto available=rendered.load(std::memory_order_acquire);
            for (int i=0;i<frames;++i) {
                const auto time=position+i;
                if (time<delay) {
                    for (int ch=0;ch<channels;++ch) buffers[ch][offset+i]=0;
                    continue;
                }
                const auto source=time-delay;
                discardBefore(source,available);
                const auto index=returned&mask;
                const bool ready=returned<available && input[index].time==source;
                if (ready) meters=output[index].meter;
                if (!ready && !dry) complete=false;
                for (int ch=0;ch<channels;++ch)
                    buffers[ch][offset+i]=(ready && !dry)?output[index].values[ch]:dryInput[source&mask][ch];
                if (ready) ++returned;
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
    struct Input {std::array<float,2> values{};Controls settings=defaults;std::uint64_t time=0;};
    struct Output {std::array<float,2> values{};MeterReadings meter{};};
    void discardBefore(std::uint64_t time,std::uint64_t available) {
        while (returned<available && input[returned&mask].time<time) ++returned;
    }
    void wake() {
#ifdef _WIN32
        if (event) SetEvent(event);
#else
        written.notify_one();
#endif
    }
    void wait([[maybe_unused]] std::uint64_t observed) {
#ifdef _WIN32
        WaitForSingleObject(event,INFINITE);
#else
        written.wait(observed,std::memory_order_acquire);
#endif
    }
    static void prepareWorker() {
#if defined(__SSE2__) || defined(_M_X64)
        constexpr unsigned flushToZero=1u<<15;
        constexpr unsigned denormalsAreZero=1u<<6;
        _mm_setcsr(_mm_getcsr() | flushToZero | denormalsAreZero);
#endif
#ifdef _WIN32
        SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_ABOVE_NORMAL);
#endif
    }
    struct alignas(64) LinkState {
        std::atomic<std::uint64_t> completed{0};
        std::array<double,2> volts{};
    };
    void renderLane(int channel,std::uint64_t first,std::uint64_t end,bool linked) {
        auto& lane=laneSolvers[channel];
        for (auto frame=first;frame<end;++frame) {
            const auto& sample=input[frame&mask];
            const auto index=frame&1;
            auto& local=linkState[channel];
            auto& peer=linkState[1-channel];
            if (linked) lane.setPeerLinkVoltage(peer.volts[index]);
            auto settings=sample.settings;
            settings[19]=linked;
            const auto midpoint=Solver::Frame{
                BandlimitedHalf::interpolate(frame,[this](std::uint64_t t) {return double(input[t&mask].values[0]);}),
                BandlimitedHalf::interpolate(frame,[this](std::uint64_t t) {return double(input[t&mask].values[1]);})};
            lane.step(midpoint,settings);
            const auto result=lane.step({sample.values[0],sample.values[1]},settings);
            auto& out=output[frame&mask];
            out.values[channel]=float(result[channel]);
            out.meter[channel]=float(std::max(0.0,lane.meterVolts()[channel]*6.9));
            if (linked) {
                local.volts[1-index]=lane.linkVoltage();
                const auto completed=frame+1;
                local.completed.store(completed,std::memory_order_release);
                for (unsigned spins=0;peer.completed.load(std::memory_order_acquire)<completed;++spins) {
#if defined(__SSE2__) || defined(_M_X64)
                    _mm_pause();
#endif
                    if ((spins&1023)==1023) std::this_thread::yield();
                }
            }
        }
    }
    void run() {
        prepareWorker();
        std::binary_semaphore begin(0),done(0);
        std::uint64_t first=0,last=0;
        bool linked=false;
        std::thread right;
        if (channels==2) right=std::thread([&] {
            prepareWorker();
            for (;;) {
                begin.acquire();
                if (last==stopSignal) return;
                renderLane(1,first,last,linked);
                done.release();
            }
        });

        std::uint64_t next=0;
        for (;;) {
            const auto available=written.load(std::memory_order_acquire);
            if (available==stopSignal) break;
            const auto ready=available>BandlimitedHalf::future ? available-BandlimitedHalf::future : 0;
            if (ready==next) {wait(available);continue;}
            const auto wantsLink=[&](std::uint64_t frame) {
                const auto& settings=input[frame&mask].settings;
                return channels==2 && settings[19] && settings[20];
            };
            linked=wantsLink(next);
            const auto limit=std::min(ready,next+512);
            auto end=next+1;
            while (end<limit && wantsLink(end)==linked) ++end;
            if (linked) {
                for (int ch=0;ch<2;++ch) {
                    linkState[ch].volts[next&1]=laneSolvers[ch].linkVoltage();
                    linkState[ch].completed.store(next,std::memory_order_relaxed);
                }
            }
            if (right.joinable()) {
                first=next;
                last=end;
                begin.release();
            }
            renderLane(0,next,end,linked);
            if (right.joinable()) done.acquire();
            failures.store(laneSolvers[0].failures()+
                (channels==2 ? laneSolvers[1].failures():0),std::memory_order_relaxed);
            rendered.store(end,std::memory_order_release);
            next=end;
        }
        if (right.joinable()) {
            last=stopSignal;
            begin.release();
            right.join();
        }
    }
    std::array<LinkState,2> linkState{};
    static constexpr int internalFactor=2;
    std::array<Solver,2> laneSolvers{Solver(0),Solver(1)};
    // The DAW clock advances even when the worker queue cannot accept a block.
    std::array<std::array<float,2>,capacity> dryInput{};
    std::array<Input,capacity> input{};
    std::array<Output,capacity> output{};
    alignas(64) std::atomic<std::uint64_t> written{0};
    alignas(64) std::atomic<std::uint64_t> rendered{0};
    std::atomic<unsigned> failures{0};
    std::thread worker;
    std::uint64_t position=0,delay=1536;
    std::uint64_t submitted=0,returned=0;
    MeterReadings meters{};
    bool started=false;
    int channels=2;
#ifdef _WIN32
    HANDLE event=nullptr;
#endif
};
}
