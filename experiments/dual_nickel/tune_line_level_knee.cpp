// Switchable physical resistor decks for VTL5C2 two-stage knee calibration.
// Samples pass through the original VTL5C2 optical model, not a fitted ratio curve.
#include "OpticalCell.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
constexpr double pi=3.14159265358979323846,fs=44100.,fspeak=17.3677;
double measure(double in,double leadRf,double tap,double bias,double sumR) {
  optocircuit::OptoParts p{};
  p.vtlLeadDriverRf=leadRf;
  p.vtlCompFeedbackTapR=tap*p.vtlCompInputTapR/(1.-tap);
  p.vtlHighTapBiasV=bias;p.vtlHighTapSeriesR=80000.;
  p.vtlHighTapSummingR=sumR;
  optocircuit::OpticalCompressor c;
  c.prepare(fs,p);c.setCellKind(optocircuit::CellKind::VTL5C2);
  c.setLimiter(false);c.setThresholdDb(-18.);
  c.setAttackMs(15.);c.setReleaseMs(250.);
  constexpr int count=44100,start=33075;
  const double peak=std::sqrt(2.)*.775*std::pow(10.,in/20.)/fspeak;
  double accum=0.;
  for(int i=0;i<count;++i){
    const double y=c.process(float(peak*std::sin(2*pi*1000.*i/fs)));
    if(i>=start)accum+=y*y;
  }
  return 20*std::log10(std::sqrt(accum/(count-start))*fspeak/.775);
}
int main() {
 std::cout<<std::fixed<<std::setprecision(5);
 std::cout<<"cell,lead_feedback_ohms,base_pre_tap,diode_bias_volts,high_sum_ohms,threshold_dbu,input_dbu,gr_db,interval_ratio\n";
 for(double lead:{1200.,2000.,3000.})
 for(double tap:{.10,.15})
 for(double bias:{3.,5.,7.})
 for(double sum:{5000.,10000.,15000.}) {
   double oldIn=0.,oldOut=0.;bool first=true;
   for(double in:{-4.,0.,2.,4.,6.,8.,10.,12.,16.,20.}) {
     const double out=measure(in,lead,tap,bias,sum);
     const double ratio=first?0.:(in-oldIn)/(out-oldOut);
     std::cout<<"VTL5C2,"<<lead<<","<<tap<<","<<bias<<","<<sum
              <<",-18,"<<in<<","<<in-out<<","<<ratio<<"\n";
     oldIn=in;oldOut=out;first=false;
   }
 }
}
