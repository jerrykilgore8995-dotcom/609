// Physical sidechain resistor calibration near +4 dBu, no GR/ratio target DSP.
// This test is independent of the input transformer: its input is the opto input.
#include "OpticalCell.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
constexpr double pi=3.14159265358979323846,fs=44100.,fspeak=17.3677;
static double level(double in,int kind,double preGain,double driverRf,double tap=0.) {
  optocircuit::OptoParts p{};
  if(kind==0){p.nslCompPreampRf=(preGain-1.)*p.detectorPreampRg;p.ledCompFeedbackR=driverRf;}
  if(kind==1){p.vtlCompPreampRf=(preGain-1.)*p.detectorPreampRg;p.vtlCompFeedbackR=driverRf;}
  if(kind==2){p.elCompDriveRf=(preGain-1.)*p.elCompDriveRg;
      p.elCompFeedbackTapR=tap*p.elCompInputTapR/(1.-tap);}
  optocircuit::OpticalCompressor c;
  c.prepare(fs,p);c.setCellKind(static_cast<optocircuit::CellKind>(kind));
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
  std::cout<<"cell,detector_gain,driver_feedback_ohms,pre_tap,threshold_dbu,input_dbu,gr_db,local_ratio\n";
  for(int kind=0;kind<3;kind++)
  for(double gain:(kind==2?std::initializer_list<double>{1.,1.4,1.8,2.3}:
                              std::initializer_list<double>{2.,3.,4.,6.,8.}))
  for(double feedback:(kind==2?std::initializer_list<double>{.0,.01,.02,.035}:
                                  std::initializer_list<double>{1000.,1500.,2000.,2500.,3000.})){
    double prevIn=0.,prevOut=0.;bool first=true;
    for(double in:{-4.,0.,2.,4.,6.,8.,10.,12.,16.,20.}) {
      double out=level(in,kind,gain,kind==2?0:feedback,kind==2?feedback:0.);
      double ratio=first?0.:(in-prevIn)/(out-prevOut);
      std::cout<<(kind==0?"NSL32":kind==1?"VTL5C2":"T4B")<<","<<gain<<","
               <<(kind==2?0:feedback)<<","<<(kind==2?feedback:0.)<<",-18,"
               <<in<<","<<in-out<<","<<ratio<<"\n";
      prevIn=in;prevOut=out;first=false;
    }
  }
}
