#include "OpticalCell.hpp"
#include <cmath>
#include <iostream>
#include <iomanip>
using namespace optocircuit;
static double measure(double sr,double r,double gain,double level,double hz,int feedforward) {
  OptoParts p;p.elPanelDriveR=r;p.elPanelVoltageGain=gain;
  OpticalCompressor c;c.prepare(sr,p);c.setCellKind(CellKind::T4B);
  c.setThresholdDb(-18.);c.setFeedforward(feedforward!=0);
  const int n=int(sr*.8);
  for(int i=0;i<n;++i){
    const double a=std::sqrt(2.)*.775*dB(level)/17.3677;
    c.process(float(a*std::sin(2*pi*hz*i/sr)));
  }
  return c.reductionDb();
}
int main() {
 std::cout<<std::fixed<<std::setprecision(6);
 std::cout<<"rc_ohms,driver_gain,level_dbu,sample_rate,mode,GR_1khz,GR_5khz,GR_10khz,GR_20khz\n";
 for(double sr:{44100.,96000.})
 for(double r:{220.,150.,100.,68.,47.,22.})
 for(double g:{24.5})
 for(double level:{4.,12.})
 for(int ff:{0,1})
 std::cout<<r<<","<<g<<","<<level<<","<<sr<<","<<ff<<","
  <<measure(sr,r,g,level,1000.,ff)<<","
  <<measure(sr,r,g,level,5000.,ff)<<","
  <<measure(sr,r,g,level,10000.,ff)<<","
  <<measure(sr,r,g,level,20000.,ff)<<"\n";
}