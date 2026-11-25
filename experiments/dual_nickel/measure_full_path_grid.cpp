// End-to-end Nickel Opto matrix: transformer -> optional low-mid ->
// physical opto -> BJT output. Coherent-window harmonic/phase measurements.
// Gain normalized against same-condition 1 kHz amplitude; phase reported
// modulo 360 (absolute phase includes sample-rate-dependent delay).
// THD includes only harmonics below Nyquist, never aliases as true K.
#include "AnalogOptoProcessor.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
using analogopto::MonoEngine;
constexpr double fs=44100.,twoPi=6.283185307179586,pk=nickel::fullScalePeakVolts;
constexpr int n=4410;  // 100ms coherent block, 10 Hz bins.
struct Result {
 double gain=0,phase=0,thd=0,gr=0;
 std::array<double,21> harmonics{};
};
static double inputAmplitude(double dbu){
 return std::sqrt(2.)*.775*std::pow(10.,dbu/20.)/pk;
}
static Result measure(int cell,double drive,double level,double threshold,
                      double load,double freq,bool limiter=false) {
 MonoEngine engine;engine.prepare(fs);engine.setOptoCell(cell);
 engine.setInputDb(drive);engine.setThresholdDb(threshold);
 engine.setLimiter(limiter);engine.setFeedforward(false);
 engine.setAttackMs(15.);engine.setReleaseMs(250.);
 engine.setMakeupDb(0.);engine.setOutputTrimDb(0.);
 engine.setLinkedAutoGain(true);engine.setLoadOhms(load);
 engine.setPalmMute(false);
 const double amp=inputAmplitude(level);
 std::array<double,21> sine{},cosine{};
 constexpr int warm=2*n;
 for(int i=0;i<warm+n;++i) {
    const double x=amp*std::sin(twoPi*freq*i/fs);
    const double y=engine.process(float(x));
    if(!std::isfinite(y)) {std::cerr<<"NONFINITE\n";std::exit(3);}
    if(i<warm)continue;
    const int sample=i-warm;
    for(int h=1;h<=20;++h)if(h*freq<fs*.5){
       const double phase=twoPi*h*freq*sample/fs;
       sine[h]+=y*std::sin(phase);
       cosine[h]+=y*std::cos(phase);
    }
 }
 Result r;
 const double fund=std::hypot(sine[1],cosine[1]);
 r.gain=20.*std::log10(std::max(1e-15,2*fund/n)/amp);
 r.phase=std::atan2(cosine[1],sine[1])*180./3.141592653589793;
 r.gr=engine.opticalGr();
 double sum=0.;
 for(int h=2;h<=20;++h)if(h*freq<fs*.5){
   const double mag=std::hypot(sine[h],cosine[h]);
   r.harmonics[h]=20.*std::log10(std::max(1e-15,mag)/std::max(1e-15,fund));
   sum+=mag*mag;
 }else r.harmonics[h]=std::numeric_limits<double>::quiet_NaN();
 r.thd=100.*std::sqrt(sum)/std::max(1e-15,fund);
 return r;
}
static void print(const std::string& label,int cell,double drive,double level,
                  double threshold,double load,double hz,const Result& x,
                  double refGain,double refPhase) {
 double relPhase=std::remainder(x.phase-refPhase,360.);
 std::cout<<label<<","<<cell<<","<<drive<<","<<level<<","<<threshold<<","
  <<load<<","<<hz<<","<<x.gain<<","<<x.gain-refGain<<","
  <<x.phase<<","<<relPhase<<","<<x.gr<<","<<x.thd;
 for(int h=2;h<=20;++h) {
   std::cout<<",";
   if(std::isfinite(x.harmonics[h]))std::cout<<x.harmonics[h];
 }
 std::cout<<"\n";
}
int main(){
 std::cout<<std::fixed<<std::setprecision(7);
 std::cout<<"section,cell,drive_db,input_dbu,threshold_dbu,load_ohm,freq_hz,gain_db,"
             "FR_relative_1khz_db,phase_deg,phase_relative_1khz_deg,opto_GR_db,THD_percent";
 for(int h=2;h<=20;++h)std::cout<<",K"<<h<<"_dBc";
 std::cout<<"\n";
 // VTL5C1 gets full drive/threshold sweep. Three stock modes get
 // representative before/after checks at two drive settings.
 for(int cell=0;cell<4;++cell) {
  const auto drives=cell==3?std::vector<double>{-12.,0.,12.,18.}:
                             std::vector<double>{0.,12.};
  const auto thresholds=cell==3?std::vector<double>{-30.,-18.,-6.,18.}:
                                 std::vector<double>{-18.,18.};
  for(double drive:drives)for(double threshold:thresholds)
  for(double level:{-18.,4.,12.})
  for(double load:{600.,10000.}) {
   const auto ref=measure(cell,drive,level,threshold,load,1000.);
   for(double hz:{100.,1000.,5000.,10000.,15000.,20000.}) {
      const auto r=hz==1000.?ref:measure(cell,drive,level,threshold,load,hz);
      print("FULL",cell,drive,level,threshold,load,hz,r,ref.gain,ref.phase);
   }
  }
 }
 // Transformer-only harmonic fingerprint distinguishes true drive
 // saturation from linked output compensation and downstream GR.
 std::cout<<"# section TRANSFORMER uses cell=-1, threshold=0, load=10000\n";
 for(double drive:{-18.,-12.,0.,6.,12.,18.})
 for(double level:{-18.,4.,12.})
 for(double hz:{100.,1000.,5000.,10000.}){
   nickel::Transformer transformer;
   nickel::CoreParams core{};
   core.Ms=5e5;core.a=100.;core.k=150.;core.sourceR=50.;
   transformer.prepare(fs,core);
   const double amplitude=inputAmplitude(level)*std::pow(10.,drive/20.);
   std::array<double,21> sine{},cosine{};
   for(int i=0;i<3*n;++i) {
     const double y=transformer.process(float(amplitude*std::sin(twoPi*hz*i/fs)));
     if(!std::isfinite(y))return 4;
     if(i<2*n)continue;
     for(int h=1;h<=20;++h)if(h*hz<fs*.5) {
       const double a=twoPi*h*hz*(i-2*n)/fs;
       sine[h]+=y*std::sin(a);cosine[h]+=y*std::cos(a);
     }
   }
   Result r;
   const double fund=std::hypot(sine[1],cosine[1]);
   r.gain=20.*std::log10(std::max(1e-15,2*fund/n)/std::max(1e-15,amplitude));
   r.phase=std::atan2(cosine[1],sine[1])*180./3.141592653589793;
   double power=0;
   for(int h=2;h<=20;++h)if(h*hz<fs*.5){
     const double mag=std::hypot(sine[h],cosine[h]);
     r.harmonics[h]=20.*std::log10(std::max(1e-15,mag)/std::max(1e-15,fund));
     power+=mag*mag;
   }else r.harmonics[h]=std::numeric_limits<double>::quiet_NaN();
   r.thd=100.*std::sqrt(power)/std::max(1e-15,fund);
   print("TRANSFORMER",-1,drive,level,0.,10000.,hz,r,r.gain,r.phase);
 }
 return 0;
}
