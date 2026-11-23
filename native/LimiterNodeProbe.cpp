// Read-only circuit diagnostic: +22dBu 1kHz limiter, 88.2kHz solver.
// Comp OUT, makeup bypassed, limiter IN, +8dBu, 800ms recovery.
// Outputs the final 200 ms of exact native nodal voltages for current auditing.
#include "Circuit.hpp"
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numbers>
int main() {
    constexpr double rate=88200, frequency=1000, duration=1.2, window=.2;
    mkii::native::Solver solver(0);
    if (!solver.prepare(rate)) {std::cerr<<"solver.prepare(88200) failed\n";return 4;}
    auto controls=mkii::native::defaults;
    controls[3]=1; // makeup zero, though compressor OFF
    controls[4]=9; // front panel +8 dBu
    controls[5]=4; // 800ms
    controls[6]=0; // compressor OFF
    controls[7]=1; // limiter ON
    controls[8]=0; // slow limiter attack
    controls[19]=0; // no stereo link
    const double peak=std::pow(10.,(22.-24.)/20.);
    std::cout<<std::setprecision(12)
      <<"time_s,output,scv,ldet_v,ql12b,ql12e,attack_tap,c14,release_ref,link,law_top,vctrl,law_knee,ql7e,ql7c,ql11c,padh,padl,t2h,t2l\n";
    const int total=int(duration*rate),start=int((duration-window)*rate);
    for(int n=0;n<total;++n){
        const double t=double(n)/rate;
        const auto y=solver.step({peak*std::sin(2*std::numbers::pi*frequency*t),0.0},controls);
        if(n>=start && (n%2)==0){
            const auto d=solver.limiterDiagnosticVoltages();
            const auto a=solver.diagnosticVoltages();
            std::cout<<t<<','<<y[0];
            for(double x:d)std::cout<<','<<x;
            std::cout<<','<<a[2]<<','<<a[3]<<','<<a[0]<<','<<a[1]<<'\n';
        }
    }
    std::cerr<<"solver_failures="<<solver.failures()<<" subdivisions="<<solver.refinedSamples()<<"\n";
    return solver.failures()?5:0;
}
