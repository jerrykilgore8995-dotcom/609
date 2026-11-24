#include "OpticalCell.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <iomanip>
constexpr double pi=3.14159265358979323846;
constexpr double fs=44100.;
constexpr double voltsPerPeak=17.3677;
static float tone(int n,double hz,double dbu) {
    const double a=.775*std::sqrt(2.)*std::pow(10.,dbu/20.)/voltsPerPeak;
    return float(a*std::sin(2*pi*hz*n/fs));
}
static double steady(double hz,double dbu,double ratio,
                     double attack=15.,double release=250.,bool hpf=false){
    optocircuit::OpticalCompressor c;c.prepare(fs);
    c.setThresholdDb(-18.);c.setRatio(ratio);
    c.setAttackMs(attack);c.setReleaseMs(release);
    c.setSidechainHpF(hpf);c.setSidechainHz(120.);
    for(int i=0;i<44100*2;++i)
        c.process(tone(i,hz,dbu));
    return c.reductionDb();
}
int main(){
    std::cout<<std::fixed<<std::setprecision(5);
    for(const double ratio:{2.,3.,4.,6.}) {
      for(const double hz:{50.,100.,1000.,5000.}){
        for(const double dbu:{-18.,-8.,0.,4.,8.,12.,18.}){
          const double gr=steady(hz,dbu,ratio);
          std::cout<<"OPTO_RATIO ratio="<<ratio<<" hz="<<hz
                   <<" dbu="<<dbu<<" gr_db="<<gr<<"\n";
          if(!std::isfinite(gr)||gr<0||gr>40.)return 2;
        }
      }
    }
    for(double hz:{100.,1000.}) {
        double before=steady(hz,4.,3.);
        double filtered=steady(hz,4.,3.,15.,250.,true);
        std::cout<<"HPF_TRANSFER hz="<<hz<<" off="<<before
                 <<" on="<<filtered<<"\n";
        if(!std::isfinite(filtered) || filtered>before+0.05)return 3;
    }
    for(double a:{1.,8.,15.,50.,100.})for(double r:{50.,250.,1000.}) {
        optocircuit::OpticalCompressor cell;cell.prepare(fs);
        cell.setThresholdDb(-18.);
        cell.setAttackMs(a);cell.setReleaseMs(r);
        for(int i=0;i<44100;++i)cell.process(tone(i,1000.,4.));
        const double gr=cell.reductionDb();
        for(int i=0;i<44100;++i)cell.process(0.f);
        const double remaining=cell.reductionDb();
        std::cout<<"TIMING attack_ms="<<a<<" release_ms="<<r
                 <<" gr_on="<<gr<<" after_1s_silence="<<remaining<<"\n";
        if(!std::isfinite(remaining)||remaining<0.)return 4;
    }
    std::cout<<"OPTO_CHARACTER=PASS\n";
    return 0;
}
