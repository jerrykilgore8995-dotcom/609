// Full-channel harmonic/phase fingerprint for guitar-bus/bass color.
// Only genuine modeled transformer, optical shunt and transistor circuits.
// THD counts in-band K2..K20; this is NOT a comprehensive aliasing test.
// Input source: coherent sine in 0.25s window at 44.1kHz.
#include "AnalogOptoProcessor.hpp"
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
constexpr double fs=44100,pi=3.14159265358979323846;
static constexpr int n=11025;
static double rmsInput(double dbu) {
    return .775*std::pow(10.,dbu/20.)/nickel::fullScalePeakVolts;
}
int main(){
    std::cout<<std::fixed<<std::setprecision(7);
    std::cout<<"load_ohm,level_dbu,freq_hz,optical,GR_db,"
      "gain_db,phase_deg,THD_percent";
    for(int h=2;h<=20;++h)std::cout<<",K"<<h<<"_dBc";
    std::cout<<"\n";
    for(double load:{10000.,600.})
    for(double dbu:{4.,12.})
    for(double hz:{100.,1000.,5000.,10000.})
    for(bool compress:{false,true}) {
        analogopto::MonoEngine engine;engine.prepare(fs);
        engine.setLoadOhms(load);
        engine.setThresholdDb(compress?-18.:18.);
        engine.setAttackMs(15.);
        engine.setReleaseMs(250.);
        engine.setPalmMute(false);
        const double amplitude=std::sqrt(2.)*rmsInput(dbu);
        std::array<double,21> s{},c{};
        for(int i=0;i<3*n;++i) {
            const double input=amplitude*std::sin(2*pi*hz*i/fs);
            const double output=engine.process(float(input));
            if(!std::isfinite(output))return 3;
            if(i<2*n)continue;
            const int k=i-2*n;
            for(int h=1;h<=20;++h)if(h*hz<fs*.5){
                const double a=2*pi*h*hz*k/fs;
                s[h]+=output*std::sin(a);
                c[h]+=output*std::cos(a);
            }
        }
        double fundamental=std::hypot(s[1],c[1])*2./n;
        const double gain=20*std::log10(std::max(1e-15,fundamental)/amplitude);
        const double phase=std::atan2(c[1],s[1])*180./pi;
        double harmonicPower=0;
        for(int h=2;h<=20;++h)if(h*hz<fs*.5){
            harmonicPower+=s[h]*s[h]+c[h]*c[h];
        }
        const double thd=100*std::sqrt(harmonicPower)/
                          std::max(1e-15,std::hypot(s[1],c[1]));
        std::cout<<load<<","<<dbu<<","<<hz<<","<<(compress?1:0)
             <<","<<engine.opticalGr()<<","<<gain<<","<<phase<<","<<thd;
        for(int h=2;h<=20;++h) {
            if(h*hz>=fs*.5){std::cout<<",";continue;}
            const double val=std::hypot(s[h],c[h]);
            const double dBc=20*std::log10(std::max(1e-15,val)/
                                             std::max(1e-15,std::hypot(s[1],c[1])));
            std::cout<<","<<dBc;
        }
        std::cout<<"\n";
        if(!std::isfinite(gain)||!std::isfinite(thd)||thd>100.)return 4;
    }
    return 0;
}
