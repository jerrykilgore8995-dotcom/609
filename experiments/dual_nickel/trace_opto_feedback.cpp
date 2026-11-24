#include "OpticalCell.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
constexpr double fs=44100., pi=3.141592653589793;
int main(){
 std::cout<<std::fixed<<std::setprecision(7);
 std::cout<<"mode,level,attack_ms,time_ms,gr_db,detector_V,led_V,led_mA,ldr_ohms\n";
 for(double dbu:{4.,8.,12.})for(double attack:{1.,15.,100.}){
  optocircuit::OpticalCompressor c;c.prepare(fs);
  c.setThresholdDb(-18.);c.setRatio(3);c.setAttackMs(attack);c.setReleaseMs(250.);
  const double amp=std::sqrt(2.)*.775*std::pow(10.,dbu/20.)/17.3677;
  for(int n=0;n<44100*2;++n) {
   c.process(float(amp*std::sin(2*pi*1000*n/fs)));
   if(n%441==0)
      std::cout<<"FB,"<<dbu<<","<<attack<<","<<n*1000./fs<<","
       <<c.reductionDb()<<","<<c.detectorVolts()<<","
       <<c.ledDriveVolts()<<","<<c.ledAmps()*1000<<","
       <<c.ldrOhms()<<"\n";
  }
 }
}
