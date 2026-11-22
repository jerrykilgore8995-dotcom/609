#include "Circuit.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <string>
int main(int argc,char** argv) {
    if(argc!=8 && argc!=9) return 2;
    const bool limiter=std::string(argv[1])=="limit";
    const int rec=std::atoi(argv[2]), attack=std::atoi(argv[3]);
    const double startDb=std::atof(argv[4]),endDb=std::atof(argv[5]);
    const double step=std::atof(argv[6]),after=std::atof(argv[7]);
    if(rec<1||rec>6) return 3;
    const double rate=(argc==9)?std::atof(argv[8]):44100.;
    mkii::native::Solver solver(0);
    if(!solver.prepare(rate))return 4;
    auto controls=mkii::native::defaults;
    controls[19]=0; // True independent factory channel; do not ground missing stereo peer through 10-ohm link.
    controls[0]=limiter?1:1; // compressor reference -20 dBu
    controls[1]=3;
    controls[2]=rec;
    controls[4]=9; // limiter +8 dBu
    controls[5]=rec;
    controls[6]=!limiter;
    controls[7]=limiter;
    controls[8]=limiter && attack;
    auto input=[](double dBu,double time) {
        return std::pow(10.,(dBu-24.)/20.)*std::sin(2*std::numbers::pi*1000.*time);
    };
    const long total=static_cast<long>((step+after)*rate);
    std::cout<<"sample,time_s,control_voltage_V,QL12E,ATTACK_TAP,RELEASE_REF,QL7C,QL12B,QL7B,ZREF\n"<<std::setprecision(14);
    for(long n=0;n<=total;++n) {
        double t=n/rate;
        solver.step({input(t<step?startDb:endDb,t),0},controls);
        if(n%static_cast<long>(rate/1000)==0||n==static_cast<long>(step*rate)||n==total)
            {auto v=solver.diagnosticVoltages();std::cout<<n<<','<<t<<','<<solver.controlVoltage()[limiter?1:0];for(double x:v)std::cout<<','<<x;std::cout<<'\n';}
    }
    std::cerr<<"misses="<<solver.failures()<<" subdivisions="<<solver.refinedSamples()<<"\n";
    return solver.failures()?5:0;
}
