#include "DiscreteOutputDriver.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>
#include <chrono>
#include <array>
constexpr double pi=3.14159265358979323846;
constexpr double fs=44100.0;
struct TestResult {double gainDb=0.,thd=0.,peak=0.,phase=0.;};
TestResult measure(double freq,double dbu,double load=10000.,double cap=100.e-6) {
    nickel::DiscreteOutputDriver d;
    nickel::OutputStageParams config{};
    config.outputLoadR=load;config.outputCapF=cap;
    d.prepare(fs,config);
    constexpr int n=11025;
    const double amp=std::sqrt(2.)*.775*std::pow(10.,dbu/20.)/nickel::fullScalePeakVolts;
    std::vector<double> y(n);
    for(int i=0;i<2*n;++i){
        float s=d.process(float(amp*std::sin(2*pi*freq*i/fs)));
        if(!std::isfinite(s))std::exit(4);
        if(i>=n)y[i-n]=s;
    }
    TestResult result;
    auto harmonic=[&](int h) {
        double si=0.,co=0.;
        for(int i=0;i<n;++i) {
            const double a=2*pi*h*freq*i/fs;
            si+=y[i]*std::sin(a);co+=y[i]*std::cos(a);
        }
        return std::pair<double,double>{2*si/n,2*co/n};
    };
    const auto first=harmonic(1);
    const double fundamental=std::hypot(first.first,first.second);
    if(fundamental<=1.e-12)std::exit(5);
    double pow=0.;
    for(int h=2;h<=20 && h*freq<fs/2;++h) {
        const auto v=harmonic(h);
        pow+=v.first*v.first+v.second*v.second;
    }
    result.gainDb=20*std::log10(fundamental/amp);
    result.thd=100*std::sqrt(pow)/fundamental;
    result.phase=180/pi*std::atan2(first.second,first.first);
    for(auto v:y)result.peak=std::max(result.peak,std::abs(v));
    return result;
}
int main(){
    std::cout<<std::fixed<<std::setprecision(6);
    bool pass=true;
    for(double load:{10000.,600.})for(double level:{-18.,4.,12.,18.,22.})
        for(double frequency:{20.,50.,100.,1000.,5000.,10000.}) {
            const auto v=measure(frequency,level,load);
            std::cout<<"driver load="<<load<<" level_dbu="<<level
                     <<" hz="<<frequency<<" gain_db="<<v.gainDb
                     <<" thd_percent="<<v.thd<<" phase_deg="<<v.phase
                     <<" peak="<<v.peak<<"\n";
            if(!std::isfinite(v.gainDb)||!std::isfinite(v.thd))pass=false;
            // For +4 dBu 10k line use, require transparent nominal behavior.
            if(load==10000. && level==4.) {
                if(std::abs(v.gainDb)>1.0 || v.thd>0.2)pass=false;
            }
        }
    // Verify that coupling capacitor actually changes LF/phase behavior.
    const auto small=measure(20.,4.,600.,1.e-6);
    const auto large=measure(20.,4.,600.,100.e-6);
    std::cout<<"CAP_COMPARE 20Hz 600R 1uF gain="<<small.gainDb
             <<" phase="<<small.phase<<" 100uF gain="<<large.gainDb
             <<" phase="<<large.phase<<"\n";
    if(!(large.gainDb>small.gainDb+3.0))pass=false;
    nickel::OptionCSignalPath chain;
    chain.prepare(fs);chain.setInputDriveDb(12);chain.setMakeupDb(0);
    double signal=0;
    for(int i=0;i<8192;++i) {
        const float s=float(.1*std::sin(2*pi*100*i/fs));
        const float z=chain.processOpticalOutput(chain.processInput(s));
        if(!std::isfinite(z))pass=false;
        signal+=z*z;
    }
    if(signal<1.e-12)pass=false;
    nickel::DiscreteOutputDriver bench;bench.prepare(fs);
    float value=0;
    const auto start=std::chrono::steady_clock::now();
    for(int i=0;i<44100;++i)
        value=bench.process(float(.1*std::sin(2*pi*100*i/fs)));
    const auto duration=std::chrono::duration<double>(
        std::chrono::steady_clock::now()-start).count();
    std::cout<<"DRIVER_CPU_MS_PER_512="<<duration*1000*512/44100<<" output="<<value<<"\n";
    std::cout<<"OPTION_C_TEST="<<(pass?"PASS":"FAIL")<<"\n";
    return pass?0:1;
}
