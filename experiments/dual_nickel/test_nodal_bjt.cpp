#include "NodalBjtOutput.hpp"
#include <cmath>
#include <chrono>
#include <iostream>
constexpr double pi=3.14159265358979323846;
constexpr double fs=44100.;
int main(){
    std::cout.setf(std::ios::fixed);std::cout.precision(7);
    for(double load:{10000.,600.})for(double dbu:{-18.,4.,12.,18.})
    for(double hz:{50.,1000.,5000.}) {
        nickel::NodalOutputDriver d;d.prepare(fs);d.setOutputLoadOhms(load);
        constexpr int n=22050;
        const double a=std::sqrt(2.)*.775*std::pow(10.,dbu/20.)/
                        nickel::fullScalePeakVolts;
        double sin1=0,cos1=0,sin2=0,cos2=0,peak=0;
        for(int i=0;i<2*n;++i) {
            const double v=d.process(float(a*std::sin(2*pi*hz*i/fs)));
            if(!std::isfinite(v))return 2;
            if(i>=n) {
                sin1+=v*std::sin(2*pi*hz*i/fs);
                cos1+=v*std::cos(2*pi*hz*i/fs);
                sin2+=v*std::sin(4*pi*hz*i/fs);
                cos2+=v*std::cos(4*pi*hz*i/fs);
                peak=std::max(peak,std::abs(v));
            }
        }
        const double fund=2.*std::hypot(sin1,cos1)/n;
        const double k2=2.*std::hypot(sin2,cos2)/n;
        const double gain=20.*std::log10(std::max(1e-16,fund)/a);
        const double thd=100.*k2/std::max(1e-16,fund);
        std::cout<<"NODAL load="<<load<<" dbu="<<dbu<<" hz="<<hz
                 <<" gain_db="<<gain<<" K2_percent="<<thd
                 <<" peak="<<peak<<" solver_failures="<<d.failureCount()<<"\n";
        if(dbu==4 && load==10000. &&
           (std::abs(gain)>1.5 || d.failureCount()>200))return 3;
    }
    nickel::NodalOutputDriver d;d.prepare(fs);
    float sink=0;
    const auto start=std::chrono::steady_clock::now();
    for(int n=0;n<44100;++n)
        sink=d.process(float(.05*std::sin(2*pi*100*n/fs)));
    const double elapsed=std::chrono::duration<double>(
          std::chrono::steady_clock::now()-start).count();
    std::cout<<"NODAL_CPU_MS_512="<<elapsed*512./44100*1000.
             <<" failures="<<d.failureCount()<<" sink="<<sink<<"\n";
    return 0;
}
