// Non-production JD physics diagnostic: sample seven internal nodal voltages,
// identify main VCTRL ripple & compare with SAME DC VCTRL forced constant.
// Usage: CvCauseProbe comp|limit|both|through 100|1000 input_dBu [fixed_VCTRL]
#include "Circuit.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <vector>
#include <string>
int main(int argc,char** argv) {
    if(argc!=4 && argc!=5) return 2;
    const std::string mode=argv[1];
    const double f=std::atof(argv[2]),dbu=std::atof(argv[3]),rate=88200.;
    const bool clamp=argc==5;
    mkii::native::Solver solver(0);
    if(clamp)solver.diagnosticClampVctrl(std::atof(argv[4]));
    if(!solver.prepare(rate)) {std::cerr<<"prepare failed\n";return 3;}
    auto controls=mkii::native::defaults;
    controls[19]=0; // no stereo link
    controls[6]=(mode=="comp"||mode=="both");
    controls[7]=(mode=="limit"||mode=="both");
    controls[0]=16; // highest compressor threshold, controls are 1-indexed, +10 dBu
    controls[4]=23; // highest limiter threshold, +15 dBu
    controls[1]=3;controls[2]=1;controls[5]=1;controls[3]=1; // no makeup
    const int duration=int(rate*.65),window=int(rate*.2);
    std::array<std::vector<double>,8> w;
    for(auto& v:w)v.reserve(window);
    const double peak=std::pow(10.,(dbu-24.)/20.);
    for(int n=0;n<duration;++n) {
        const double t=double(n)/rate;
        const auto y=solver.step({peak*std::sin(2*std::numbers::pi*f*t),0},controls);
        if(n<duration-window)continue;
        const auto v=solver.diagnosticVoltages();
        w[0].push_back(v[0]-v[1]); // input T2 transformer
        w[1].push_back(v[2]-v[3]); // attenuator bridge signal
        w[2].push_back(v[4]); // main diode-bridge CV
        w[3].push_back(v[5]); // compressor C13
        w[4].push_back(v[6]); // limiter C14
        w[5].push_back(y[0]); // final loaded output normalized
        w[6].push_back(v[2]); // PADH single branch
        w[7].push_back(v[3]); // PADL single branch
    }
    const char* labels[]={"T2_secondary","bridge_PADH_minus_PADL","VCTRL","C13_comp_env","C14_limit_env","output","bridge_PADH","bridge_PADL"};
    std::cout<<std::setprecision(12);
    std::cout<<"mode,freq_hz,input_dBu,clamped,stage,mean_V,peak_to_peak_V,fundamental_peak_V,second_harmonic_peak_V,third_harmonic_peak_V,THD_pct,K2_dBc,K3_dBc,misses\n";
    for(int k=0;k<8;++k){
        const auto& a=w[k];
        double mean=0;
        std::array<std::complex<double>,20> h{};
        for(size_t i=0;i<a.size();++i){
            mean+=a[i];
            auto osc=std::polar(1.,-2*std::numbers::pi*f*i/rate),z=osc;
            for(auto& val:h){val+=a[i]*z;z*=osc;}
        }
        mean/=a.size();
        auto [lo,hi]=std::minmax_element(a.begin(),a.end());
        std::array<double,20> mag{};
        for(int i=0;i<20;++i)mag[i]=2*std::abs(h[i])/a.size();
        double total=0;
        for(int i=1;i<20;++i)total+=mag[i]*mag[i];
        const double thd=100*std::sqrt(total)/std::max(1e-13,mag[0]);
        const auto dbc=[&](int i){return 20*std::log10(std::max(1e-14,mag[i]/std::max(1e-14,mag[0])));};
        std::cout<<mode<<','<<f<<','<<dbu<<','<<(clamp?1:0)<<','<<labels[k]<<','<<mean<<','<<(*hi-*lo)<<','<<mag[0]<<','<<mag[1]<<','<<mag[2]<<','<<thd<<','<<dbc(1)<<','<<dbc(2)<<','<<solver.failures()<<'\n';
    }
    if(solver.failures()){std::cerr<<"nonconvergence "<<solver.failures()<<"\n";return 4;}
    return 0;
}
