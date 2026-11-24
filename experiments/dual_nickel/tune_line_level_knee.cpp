// Search physical detector summing resistors with a fixed gentle rectifier branch.
#include "OpticalCell.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
constexpr double pi=3.14159265358979323846,fs=44100.,fspeak=17.3677;
static double measure(double in,int kind,double mainRf,double preTap) {
  optocircuit::OptoParts p{};
  if(kind==0) {
    p.nslLeadDetectorRf=50000.;p.nslLeadDriverRf=4000.;p.nslLeadClampBiasV=.8;
    p.ledCompFeedbackR=mainRf;
    p.ledCompFeedbackTapR=preTap*p.ledCompInputTapR/(1.-preTap);
  } else {
    p.vtlLeadDetectorRf=50000.;p.vtlLeadDriverRf=4000.;p.vtlLeadClampBiasV=.8;
    p.vtlCompFeedbackR=mainRf;
    p.vtlCompFeedbackTapR=preTap*p.vtlCompInputTapR/(1.-preTap);
  }
  optocircuit::OpticalCompressor c;
  c.prepare(fs,p);c.setCellKind(static_cast<optocircuit::CellKind>(kind));
  c.setLimiter(false);c.setThresholdDb(-18.);
  c.setAttackMs(15.);c.setReleaseMs(250.);
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
 std::cout<<"cell,main_feedback_ohms,pre_post_tap,threshold_dbu,input_dbu,gr_db,interval_ratio\n";
 for(int kind=0;kind<2;++kind)
 for(double feedback:{2000.,3000.,4000.,5000.,6000.})
 for(double tap:{0.,.025,.05,.10,.15,.20}){
   double oldIn=0.,oldOut=0.;bool first=true;
   for(double in:{-4.,0.,2.,4.,6.,8.,10.,12.,16.,20.}) {
     const double out=measure(in,kind,feedback,tap);
     const double ratio=first?0.:(in-oldIn)/(out-oldOut);
     std::cout<<(kind==0?"NSL32":"VTL5C2")<<","<<feedback<<","<<tap<<",-18,"
              <<in<<","<<in-out<<","<<ratio<<"\n";
     oldIn=in;oldOut=out;first=false;
   }
 }
}
