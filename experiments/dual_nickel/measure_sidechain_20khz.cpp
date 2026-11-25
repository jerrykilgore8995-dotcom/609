// Diagnostic, no code changes: high-frequency response of SIDECHAIN FOLLOWER
// separated from diode storage. A transistor buffer with HPF bypassed
// should not lose multiple dB at 20k solely because of numerical iteration.
#include "OpticalCell.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
using namespace optocircuit;
constexpr double pi=3.1415926535897932384626433832795;
int main() {
 std::cout<<std::fixed<<std::setprecision(7);
 std::cout<<"section,sample_rate,peak_input_V,frequency_hz,hpf_enabled,"
 "follower_gain_db,detector_direct_V,detector_post_follower_V,solver_failures\n";
 for(double fs:{44100.,96000.})
 for(double peak:{.5,2.,6.,10.})
 for(bool hpf:{false,true})
 for(double freq:{100.,1000.,5000.,10000.,15000.,20000.}) {
   SidechainFollower follower;follower.prepare(fs);
   follower.setEnabled(hpf);follower.setCutoffHz(100.);
   OpticalDetector direct,downstream;direct.prepare(fs);downstream.prepare(fs);
   direct.setAttackMs(15.);direct.setReleaseMs(250.);
   downstream.setAttackMs(15.);downstream.setReleaseMs(250.);
   double x2=0.,y2=0.;
   const int count=int(fs*.7),start=int(fs*.5);
   for(int i=0;i<count;++i) {
      const double x=peak*std::sin(2*pi*freq*i/fs);
      const double y=follower.process(x);
      direct.process(x);downstream.process(y);
      if(i>=start){x2+=x*x;y2+=y*y;}
   }
   std::cout<<"FOLLOWER,"<<fs<<","<<peak<<","<<freq<<","
     <<int(hpf)<<","<<10.*std::log10(std::max(1e-20,y2)/std::max(1e-20,x2))
     <<","<<direct.voltage()<<","<<downstream.voltage()
     <<","<<follower.failures()<<"\n";
 }
}