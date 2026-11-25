// VTL5C1 resistor-switch calibration only.  The CdS material is never
// changed. Sweeps LED shunt-bias divider, an actual resistor in the driver.
#include "OpticalCell.hpp"
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
using namespace optocircuit;
constexpr double fs=44100.,peak=17.3677;
static double output(double clampTop,double feedback,double kneeR,double level,double threshold) {
 OptoParts p;
 p.vtl1CompClampTopR=clampTop;
 p.vtl1CompFeedbackR=feedback;
 p.vtl1CompKneeSeriesR=kneeR;
 OpticalCompressor c;c.prepare(fs,p);c.setCellKind(CellKind::VTL5C1);
 c.setLimiter(false);c.setFeedforward(false);
 c.setThresholdDb(threshold);c.setAttackMs(15.);c.setReleaseMs(250.);
 const double amplitude=std::sqrt(2.)*.775*dB(level)/peak;
 double energy=0.;
 const int n=int(.8*fs),start=int(.60*fs);
 for(int i=0;i<n;++i) {
  const double s=c.process(float(amplitude*std::sin(2*pi*1000.*i/fs)));
  if(i>=start)energy+=s*s;
 }
 return toDb(std::sqrt(energy/(n-start))*peak/.775);
}
int main(){
 std::cout<<std::fixed<<std::setprecision(6);
 std::cout<<"clamp_top_ohms,feedback_ohms,knee_ohms,threshold_dbu,input_dbu,output_dbu,GR_db,interval_ratio\n";
 for(double clamp:{68000.,56000.,47000.,39000.,33000.,27000.,22000.,15000.})
 for(double feedback:{3500.,5000.,7500.})
 for(double threshold:{-18.}) {
   double oldLevel=0,oldOut=0;bool first=true;
   for(double level:{-6.,0.,4.,8.,12.,16.,20.,24.}) {
     const double out=output(clamp,feedback,68000.,level,threshold);
     const double ratio=first?0.:(level-oldLevel)/(out-oldOut);
     std::cout<<clamp<<","<<feedback<<",68000,"<<threshold<<","
              <<level<<","<<out<<","<<level-out<<","<<ratio<<"\n";
     oldLevel=level;oldOut=out;first=false;
   }
 }
}