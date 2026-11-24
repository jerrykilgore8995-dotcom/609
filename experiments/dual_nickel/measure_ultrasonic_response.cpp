// 96kHz ultrasonic frequency/solver stability diagnostic, no DSP changes.
// Stage 0 input nickel transformer, stage 1 discrete BJT, stage 2 full chain.
// 0.1s coherent DFT window + 0.1s settle; all frequencies are integer bins.
// Fundamental gain is always relative to the same stage/load/level at 1kHz.
#include "AnalogOptoProcessor.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cstdlib>
constexpr double sampleRate=96000.,pi=3.1415926535897932384626;
constexpr int n=9600;
static double dB(double x){return 20.*std::log10(std::max(1e-18,x));}
struct Measurement {double gain=0,phase=0,peak=0;int failures=0;};
static Measurement measure(int stage,double load,double dbu,double freq){
 nickel::Transformer transformer;nickel::NodalOutputDriver driver;
 analogopto::MonoEngine full;
 if(stage==0){
   nickel::CoreParams p{};p.Ms=5e5;p.a=100;p.k=150;p.sourceR=50;
   transformer.prepare(sampleRate,p);
 } else if(stage==1){
   driver.prepare(sampleRate);driver.setOutputLoadOhms(load);
 } else {
   full.prepare(sampleRate);full.setLoadOhms(load);
   full.setThresholdDb(18.);full.setPalmMute(false);
 }
 const double amplitude=.775*std::sqrt(2.)*
             std::pow(10.,dbu/20.)/nickel::fullScalePeakVolts;
 double sinPart=0,cosPart=0,peak=0;
 for(int i=0;i<2*n;++i){
   const double input=amplitude*std::sin(2*pi*freq*i/sampleRate);
   const float y=stage==0?transformer.process(float(input)):
       stage==1?driver.process(float(input)):full.process(float(input));
   if(!std::isfinite(y))std::exit(2);
   if(i>=n){
      const double x=2*pi*freq*(i-n)/sampleRate;
      sinPart+=y*std::sin(x);cosPart+=y*std::cos(x);
      peak=std::max(peak,std::abs(double(y)));
   }
 }
 const double fundamental=2.*std::hypot(sinPart,cosPart)/n;
 return {dB(fundamental/amplitude),
         std::atan2(cosPart,sinPart)*180./pi,
         peak,stage==1?driver.failureCount():0};
}
int main(){
 std::cout<<std::fixed<<std::setprecision(7);
 std::cout<<"stage,load_ohm,level_dbu,hz,gain_db,relative_1k_db,phase_deg,"
             "peak,solver_failures\n";
 for(int stage=0;stage<3;++stage)
 for(double load:{600.,10000.})
 for(double level:{4.,18.}){
   const auto base=measure(stage,load,level,1000.);
   for(double f:{1000.,10000.,20000.,30000.,40000.,45000.}){
      auto r=measure(stage,load,level,f);
      std::cout<<stage<<","<<load<<","<<level<<","<<f<<","
         <<r.gain<<","<<r.gain-base.gain<<","<<r.phase<<","
         <<r.peak<<","<<r.failures<<"\n";
      if(!std::isfinite(r.gain)||r.failures>0) return 3;
   }
 }
 std::cerr<<"ULTRASONIC_96KHZ=PASS\n";
}
