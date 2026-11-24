// Diagnostic ONLY: isolated magnetic input transformer, 2N5088/2N5087
// output electronics, and complete (opto idle) chain. No DSP tuning.
// All FR measurements are reported relative to each stage's own 1kHz gain.
// 0.2s coherent sine measurement (bins 5Hz apart), 0.2s warmup.
// Voltages below fullScalePeakVolts represent RMS dBu level correctly.
#include "AnalogOptoProcessor.hpp"
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <cstdlib>
constexpr double sr=44100.,pi=3.1415926535897932384626433832795;
constexpr int n=8820;
struct Result {
 double gain=0,phase=0,peak=0,thd=0;int failures=0;
};
static double db(double v) {return 20.*std::log10(std::max(v,1e-18));}
static double amplitude(double dbu){
 return .775*std::sqrt(2.)*std::pow(10.,dbu/20.)/
        nickel::fullScalePeakVolts;
}
static Result run(int stage,double load,double dbu,double hz) {
 nickel::Transformer transformer;nickel::NodalOutputDriver driver;
 analogopto::MonoEngine complete;
 if(stage==0) {
     nickel::CoreParams p{};
     p.Ms=5e5;p.a=100.;p.k=150.;p.sourceR=50.;
     transformer.prepare(sr,p);
 } else if(stage==1) {
     driver.prepare(sr);driver.setOutputLoadOhms(load);
 } else {
     complete.prepare(sr);
     complete.setThresholdDb(18.); // diode detector inactive at all tested levels
     complete.setLoadOhms(load);
     complete.setPalmMute(false);
     complete.setInputDb(0.);complete.setMakeupDb(0.);
 }
 double si=0,co=0,si2=0,co2=0,peak=0;
 const double a=amplitude(dbu);
 for(int i=0;i<2*n;++i) {
    double input=a*std::sin(2.*pi*hz*i/sr);
    float output=stage==0?transformer.process(float(input)):
       stage==1?driver.process(float(input)):complete.process(float(input));
    if(!std::isfinite(output)){std::cerr<<"NONFINITE stage="<<stage
         <<" load="<<load<<" dbu="<<dbu<<" hz="<<hz<<" n="<<i<<"\n";std::exit(3);}
    if(i>=n){
      const int k=i-n;const double x=2*pi*hz*k/sr;
      si+=output*std::sin(x);co+=output*std::cos(x);
      if(hz*2.<sr*.5){si2+=output*std::sin(2*x);co2+=output*std::cos(2*x);}
      peak=std::max(peak,std::abs(double(output)));
    }
 }
 const double fundamental=2.*std::hypot(si,co)/n;
 Result r;
 r.gain=db(fundamental/a);
 r.phase=std::atan2(co,si)*180./pi;
 r.peak=peak;
 r.thd=hz*2.<sr*.5?100.*std::hypot(si2,co2)/std::max(1e-16,std::hypot(si,co)):0.;
 r.failures=stage==1?driver.failureCount():0;
 return r;
}
static void frequencySweep(){
 const std::array<double,12> hz{20,50,100,1000,5000,8000,10000,12000,15000,17000,19000,20000};
 std::cout<<"SECTION,stage,load_ohm,level_dbu,hz,gain_db,"
             "relative_1k_db,phase_deg,peak_normalized,K2_percent,solver_failures\n";
 for(int stage=0;stage<3;++stage)
 for(double load:{10000.,600.})
 for(double level:{-18.,4.,12.,18.}) {
   const auto reference=run(stage,load,level,1000.);
   for(double f:hz){
     const auto r=run(stage,load,level,f);
     const double relative=r.gain-reference.gain;
     std::cout<<"FR,"<<stage<<","<<load<<","<<level<<","<<f<<","
              <<r.gain<<","<<relative<<","<<r.phase<<","<<r.peak
              <<","<<r.thd<<","<<r.failures<<"\n";
     if(!std::isfinite(relative)||std::abs(relative)>60.)std::exit(4);
   }
 }
}
static void stability(){
 std::cout<<"SECTION,stage,load_ohm,level_dbu,case,peak,tail_peak,"
             "tail_rms,solver_failures,discontinuity_max\n";
 for(int stage=0;stage<3;++stage)
 for(double load:{10000.,600.})
 for(double level:{4.,18.})
 for(int test=0;test<4;++test){
   nickel::Transformer transformer;nickel::NodalOutputDriver driver;
   analogopto::MonoEngine complete;
   if(stage==0){
      nickel::CoreParams p{};
      p.Ms=5e5;p.a=100.;p.k=150.;p.sourceR=50.;
      transformer.prepare(sr,p);
   } else if(stage==1){
      driver.prepare(sr);driver.setOutputLoadOhms(load);
   } else {
      complete.prepare(sr);complete.setLoadOhms(load);
      complete.setThresholdDb(18.);
   }
   const double a=amplitude(level);
   double peak=0,tailPeak=0,tailPower=0,delta=0,last=0;
   constexpr int count=44100;
   for(int i=0;i<count;++i){
      double x=0;
      switch(test){
        case 0: x=(i==0)?a:0;break; // impulse response, post-pulse decay
        case 1: x=i<4410?a:0;break; // 100ms DC step then silence
        case 2: x=i<4410?a*std::sin(2*pi*18000*i/sr):0;break; // 18k burst
        case 3: x=i<4410?a*std::sin(2*pi*100*i/sr):0;break; // large LF burst
      }
      const float y=stage==0?transformer.process(float(x)):
          stage==1?driver.process(float(x)):complete.process(float(x));
      if(!std::isfinite(y))std::exit(5);
      const double v=double(y);
      peak=std::max(peak,std::abs(v));
      if(i>=22050) {
        tailPeak=std::max(tailPeak,std::abs(v));
        tailPower+=v*v;
      }
      delta=std::max(delta,std::abs(v-last));last=v;
   }
   const double rms=std::sqrt(tailPower/(count-22050));
   const int failures=stage==1?driver.failureCount():0;
   const char* name=test==0?"impulse":test==1?"step":test==2?"18k_burst":"100_burst";
   std::cout<<"STABILITY,"<<stage<<","<<load<<","<<level<<","<<name<<","
        <<peak<<","<<tailPeak<<","<<rms<<","<<failures<<","<<delta<<"\n";
   // The output should not persist in large self-oscillation after silence.
   if(!std::isfinite(rms)||rms>1.5*a)std::exit(6);
 }
}
int main(){
 std::cout<<std::fixed<<std::setprecision(7);
 frequencySweep();stability();
 std::cerr<<"STAGE_FR_AND_STABILITY=PASS\n";
 return 0;
}
