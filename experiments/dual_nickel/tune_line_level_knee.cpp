// Evaluate resistor-switched two-detector analog LED soft knees.
// Nothing here sets a numerical compression ratio in the DSP.
#include "OpticalCell.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
constexpr double pi=3.14159265358979323846,fs=44100.,fspeak=17.3677;
static double measure(double in,int kind,double gain,double rf,double bias) {
  optocircuit::OptoParts p{};
  if(kind==0) {
    p.nslLeadDetectorRf=(gain-1.)*p.leadDetectorRg;
    p.nslLeadDriverRf=rf;p.nslLeadClampBiasV=bias;
  } else {
    p.vtlLeadDetectorRf=(gain-1.)*p.leadDetectorRg;
    p.vtlLeadDriverRf=rf;p.vtlLeadClampBiasV=bias;
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
 std::cout<<"cell,lead_detector_gain,lead_feedback_ohms,clamp_bias_volts,threshold_dbu,input_dbu,gr_db,interval_ratio\n";
 for(int kind=0;kind<2;++kind)
 for(double gain:{2.,4.,6.,8.})
 for(double rf:{2000.,4000.,8000.,12000.})
 for(double bias:{.8,1.2,1.6}) {
   double oldIn=0.,oldOut=0.;bool first=true;
   for(double in:{-8.,-4.,0.,2.,4.,6.,8.,10.,12.,16.,20.}) {
     const double out=measure(in,kind,gain,rf,bias);
     const double ratio=first?0.:(in-oldIn)/(out-oldOut);
     std::cout<<(kind==0?"NSL32":"VTL5C2")<<","<<gain<<","<<rf<<","<<bias<<",-18,"
              <<in<<","<<in-out<<","<<ratio<<"\n";
     oldIn=in;oldOut=out;first=false;
   }
 }
}
