#pragma once

#include <chrono>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

namespace mkii::native {

// Test harness pacing. Windows' ordinary Sleep can batch multiple 128-sample
// blocks if the system timer is coarse, unlike the DAW's periodic callback.
class PlaybackClock {
public:
    using Clock=std::chrono::steady_clock;
#ifdef _WIN32
    PlaybackClock() {
        constexpr DWORD highResolution=0x00000002;
        timer=CreateWaitableTimerExW(nullptr,nullptr,highResolution,TIMER_MODIFY_STATE|SYNCHRONIZE);
    }
    ~PlaybackClock() {if (timer) CloseHandle(timer);}
#endif
    void waitUntil(Clock::time_point deadline) {
#ifdef _WIN32
        if (timer) {
            const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(deadline-Clock::now()).count();
            if (ns<=0) return;
            LARGE_INTEGER due{};
            due.QuadPart=-((ns+99)/100);
            if (SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE)) {
                WaitForSingleObject(timer,INFINITE);
                return;
            }
        }
#endif
        std::this_thread::sleep_until(deadline);
    }

private:
#ifdef _WIN32
    HANDLE timer=nullptr;
#endif
};
}
