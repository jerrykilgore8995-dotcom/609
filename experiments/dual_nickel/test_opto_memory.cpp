// NSL-32 isolated cell dark recovery, datasheet calibration and
// illumination-history tests. This bypasses the compressor detector
// deliberately: it measures the photocell itself, without release-pot
// interaction or the feedback loop hiding its physical time constants.
#include "OpticalCell.hpp"
#include <cmath>
#include <iostream>
#include <iomanip>
#include <algorithm>
constexpr double sampleRate=44100.;
struct Results {
  double initialConductance=0, ledMilliamps=0, rise63ms=-1;
  double off0=0, off100=0, off500=0, off1000=0, off10000=0;
  double resistanceAt500=0, resistanceAt10000=0;
  double rAtEnd=0, off500Conductance=0, off1000Conductance=0;
};
static double driveAtCurrent(optocircuit::LdrCell& cell,double target) {
  double lo=0,hi=12;
  for(int i=0;i<48;++i){
    double mid=.5*(lo+hi);
    if(cell.ledCurrent(mid)>target)hi=mid;else lo=mid;
  }
  return .5*(lo+hi);
}
static Results measure(double ms,double milliamps) {
  optocircuit::LdrCell cell;cell.prepare(sampleRate);
  Results r{};
  const auto current=milliamps*.001;
  const double drive=driveAtCurrent(cell,current);
  r.ledMilliamps=cell.ledCurrent(drive)*1000.;
  const auto targetConductance=1./500.e3+(1./500.-1./500.e3)*
                                    (current/.020);
  const int onSamples=int(ms*.001*sampleRate);
  for(int n=0;n<onSamples;++n) {
    cell.process(1.,drive);
    const double conductance=1./cell.resistance();
    if(r.rise63ms<0 && conductance>=.632*targetConductance)
      r.rise63ms=1000.*n/sampleRate;
  }
  r.rAtEnd=cell.resistance();
  r.off0=1./r.rAtEnd;
  for(int n=0;n<=int(10.*sampleRate);++n) {
    cell.process(1.,0.);
    const double t=double(n)/sampleRate;
    if(n==int(.1*sampleRate))r.off100=cell.resistance();
    if(n==int(.5*sampleRate)){
       r.off500=cell.resistance();r.off500Conductance=1./r.off500;
    }
    if(n==int(1.*sampleRate)){
       r.off1000=cell.resistance();r.off1000Conductance=1./r.off1000;
    }
    if(n==int(10.*sampleRate))r.off10000=cell.resistance();
  }
  return r;
}
int main(){
  std::cout<<std::fixed<<std::setprecision(6);
  std::cout<<"exposure_ms,led_mA,on_R,initial_rise_63_ms,"
               "R_off_100ms,R_off_500ms,R_off_1000ms,R_off_10000ms\n";
  for(double mA:{4.,8.,16.})
    for(double exposure:{150.,500.,1200.,5000.}){
      const Results r=measure(exposure,mA);
      std::cout<<exposure<<","<<r.ledMilliamps<<","<<r.rAtEnd<<","
          <<r.rise63ms<<","<<r.off100<<","<<r.off500<<","
          <<r.off1000<<","<<r.off10000<<"\n";
      if(!std::isfinite(r.off500)||!std::isfinite(r.off10000))
        return 2;
      if(r.off500>=r.off1000 || r.off1000>=r.off10000)
        return 3; // off resistance must grow monotonically
    }
  const auto medium=measure(1200.,16.);
  const auto shortPulse=measure(150.,16.);
  const auto longPulse=measure(5000.,16.);
  // NSL-32 datasheet: 16mA, 3.5ms conductance rise; 500ms
  // decay to 100k after LED off. Explicit tolerances: both are
  // representative specs, not guaranteed min/max limits.
  if(!(medium.rise63ms>2.0&&medium.rise63ms<5.5))return 4;
  if(!(medium.off500>70000.&&medium.off500<150000.))return 5;
  if(!(medium.off10000>450000.))return 6;
  // Prolonged illumination must fill traps so 500ms and 1s later
  // it leaves distinctly more residual light conductance than a
  // short pulse, even though the bright state itself reaches nearly
  // the same final resistance.
  const double ratio500=longPulse.off500Conductance/
                               shortPulse.off500Conductance;
  const double ratio1000=longPulse.off1000Conductance/
                               shortPulse.off1000Conductance;
  std::cerr<<"NSL32_REFERENCE rise_63_ms="<<medium.rise63ms
      <<" resistance_500ms="<<medium.off500
      <<" resistance_10s="<<medium.off10000
      <<" short_to_long_conductance_ratio_500ms="<<ratio500
      <<" ratio_1000ms="<<ratio1000<<"\n";
  if(!(ratio500>1.10&&ratio1000>1.10))return 7;
  std::cerr<<"NSL32_MEMORY=PASS\n";
  return 0;
}
