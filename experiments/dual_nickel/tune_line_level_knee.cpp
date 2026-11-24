// Real VTL5C2 diode-tap resistor calibration, optical model unchanged.
#include "OpticalCell.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
constexpr double pi=3.14159265358979323846,fs=44100.,fspeak=17.3677;
double measure(double in,double bias,double rseries,double rsum){
  optocircuit::OptoParts p{};
  p.vtlHighTapBiasV=bias;p.vtlHighTapSeriesR=rseries;
  p.vtlHighTapSummingR=rsum;
  optocircuit::OpticalCompressor c;
  c.prepare(fs,p);c.setCellKind(optocircuit::CellKind::VTL5C2);
  c.setLimiter(false);c.setThresholdDb(-18.);c.setAttackMs(15.);c.setReleaseMs(250.);
  constexpr int count=44100,start=33075;
  const double peak=std::sqrt(2.)*.775*std::pow(10.,in/20.)/fspeak;
  double accum=0.;
  for(int i=0;i<count;i++){
    const double y=c.process(float(peak*std::sin(2*pi*1000.*i/fs)));
    if(i>=start)accum+=y*y;
  }
  return 20*std::log10(std::sqrt(accum/(count-start))*fspeak/.775);
}
int main(){
 std::cout<<std::fixed<<std::setprecision(5);
 std::cout<<"cell,high_tap_bias_volts,series_ohms,summing_ohms,threshold_dbu,input_dbu,gr_db,interval_ratio\n";
 for(double bias:{2.,3.,4.,5.})
 for(double series:{20000.,40000.,80000.})
 for(double sum:{5000.,10000.,20000.}){
   double previousIn=0.,previousOut=0.;bool first=true;
   for(double in:{0.,2.,4.,6.,8.,10.,12.,14.,16.,18.,20.,24.}) {
     const double out=measure(in,bias,series,sum);
     const double ratio=first?0.:(in-previousIn)/(out-previousOut);
     std::cout<<"VTL5C2,"<<bias<<","<<series<<","<<sum<<",-18,"
              <<in<<","<<in-out<<","<<ratio<<"\n";
     previousIn=in;previousOut=out;first=false;
   }
 }
}
