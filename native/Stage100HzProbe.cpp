// Diagnostic-only voltage sampling of true nodal 33609 stages at 100 Hz.
// Transformers are LINEAR in the JD source deck; no hysteresis/saturation model.
#include "Circuit.hpp"
#include <array>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <vector>
#include <algorithm>
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    const double level=std::atof(argv[1]), rate=88200., freq=100.;
    const int samples=int(.8*rate),window=int(.2*rate), first=samples-window;
    mkii::native::Solver solver(0);
    if(!solver.prepare(rate))return 3;
    auto controls=mkii::native::defaults;
    controls[19]=0;controls[6]=0;controls[7]=0;
    std::array<std::vector<double>,8> waves;
    for(auto& a:waves)a.reserve(window);
    const double peak=std::pow(10.,(level-24.)/20.);
    for(int n=0;n<samples;++n) {
        const double time=n/rate;
        auto output=solver.step({peak*std::sin(2*std::numbers::pi*freq*time),0},controls)[0];
        if(n<first)continue;
        const auto v=solver.diagnosticVoltages();
        waves[0].push_back(v[0]-v[1]); // T2 secondary
        waves[1].push_back(v[2]-v[3]); // bridge signal input
        waves[2].push_back(v[4]); // T1 secondary
        waves[3].push_back(v[5]); // amplifier output before DC blocking
        waves[4].push_back(v[6]); // signal at T3 input after DC-block
        waves[5].push_back(output); // final loaded output
        waves[6].push_back(v[2]); // positive bridge branch
        waves[7].push_back(v[3]); // negative bridge branch
    }
    const std::array<const char*,8> names{"T2_secondary","diode_bridge_PADH_minus_PADL","T1_secondary","EX10340C_AMP_OUT","T3_primary_LINESEND","loaded_output","bridge_PADH","bridge_PADL"};
    std::cout<<std::setprecision(10);
    for(int j=0;j<8;++j) {
        auto& wave=waves[j];std::array<std::complex<double>,50> h{};
        for(int i=0;i<window;++i){
            auto z=std::polar(1.,-2*std::numbers::pi*freq*double(i)/rate);
            auto b=z;
            for(auto& x:h){x+=wave[i]*b;b*=z;}
        }
        const double fundamental=2*std::abs(h[0])/window;
        double thd2=0,thd20=0,thd50=0;
        for(int k=2;k<=50;++k){
            const double r=std::abs(h[k-1])/std::abs(h[0]);
            if(k<=2)thd2+=r*r;
            if(k<=20)thd20+=r*r;
            thd50+=r*r;
        }
        const auto [mn,mx]=std::minmax_element(wave.begin(),wave.end());
        auto dBc=[&](int k){return 20*std::log10(std::max(1e-14,std::abs(h[k-1])/std::abs(h[0])));};
        std::cout<<level<<','<<names[j]<<','<<fundamental<<','<<100*std::sqrt(thd20)
                 <<','<<100*std::sqrt(thd50)<<','<<dBc(2)<<','<<dBc(3)<<','<<dBc(4)
                 <<','<<*mn<<','<<*mx<<','<<solver.failures()<<'\n';
    }
    return solver.failures()?4:0;
}
