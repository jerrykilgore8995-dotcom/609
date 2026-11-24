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
static double steady(double hz,double dbu,
                     double attack=15.,double release=250.,bool hpf=false,
                     double threshold=-18.){
    optocircuit::OpticalCompressor c;c.prepare(fs);
    c.setThresholdDb(threshold);
    c.setAttackMs(attack);c.setReleaseMs(release);
    c.setSidechainHpF(hpf);c.setSidechainHz(120.);
    for(int i=0;i<44100*2;++i)
        c.process(tone(i,hz,dbu));
    return c.reductionDb();
}
int main(){
    std::cout<<std::fixed<<std::setprecision(5);
      for(const double hz:{50.,100.,1000.,5000.}){
        for(const double dbu:{-18.,-8.,0.,4.,8.,12.,18.}){
          const double gr=steady(hz,dbu);
          std::cout<<"OPTO_STOCK hz="<<hz
                   <<" dbu="<<dbu<<" gr_db="<<gr<<"\n";
          if(!std::isfinite(gr)||gr<0||gr>40.)return 2;
        }
      }
    for(double hz:{100.,1000.}) {
        double before=steady(hz,4.);
        double filtered=steady(hz,4.,15.,250.,true);
        std::cout<<"HPF_TRANSFER hz="<<hz<<" off="<<before
                 <<" on="<<filtered<<"\n";
        if(!std::isfinite(filtered) || filtered>before+0.05)return 3;
    }
    for(double th:{-18.,-25.,-30.,-35.,-40.,-45.}) {
        const double gr100=steady(100.,4.,15.,250.,false,th);
        const double gr1k=steady(1000.,4.,15.,250.,false,th);
        const double gr8=steady(1000.,8.,15.,250.,false,th);
        std::cout<<"THRESHOLD threshold_dbu="<<th
                 <<" gr_100Hz_plus4="<<gr100
                 <<" gr_1k_plus4="<<gr1k
                 <<" gr_1k_plus8="<<gr8<<std::endl;
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
