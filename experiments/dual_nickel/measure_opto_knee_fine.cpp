// Dense true static transfer/knee characterization. NO target ratio
// interpolation; measured output only from the physical optical audio cell.
// Numerical interval ratios computed between adjacent *measured* points.
// Run independently from reset for each cell/threshold/input/HPF.
// Finite source, detector, diode driver and CdS assembly all active.
#include "OpticalCell.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <algorithm>
constexpr double pi=3.14159265358979323846,fs=44100.,pk=17.3677;
static double measure(int kind,double thresh,double dbu,int hp,double& finalGr){
 optocircuit::OpticalCompressor c;
 c.prepare(fs);c.setCellKind(static_cast<optocircuit::CellKind>(kind));
 c.setLimiter(false);c.setFeedforward(false);
 c.setThresholdDb(thresh);c.setAttackMs(15.);c.setReleaseMs(250.);
 c.setSidechainHpF(hp>0);
 if(hp>0)c.setSidechainHz(hp);
 const double a=std::sqrt(2.)*.775*std::pow(10.,dbu/20.)/pk;
 const int total=int(fs*.65),start=int(fs*.5);
 double sum=0;
 for(int i=0;i<total;++i){
  const float y=c.process(float(a*std::sin(2*pi*1000*i/fs)));
  if(i>=start)sum+=double(y)*y;
 }
 finalGr=c.reductionDb();
 return 20.*std::log10(std::max(1e-12,
    std::sqrt(sum/(total-start))*pk/.775));
}
int main(){
 std::cout<<std::fixed<<std::setprecision(6);
 std::cout<<"cell,threshold_dbu,hpf_hz,input_dbu,output_dbu,GR_db,local_ratio,local_slope_out_db_per_db\n";
 for(int kind=0;kind<3;++kind)
 for(double threshold:{-24.,-18.,-12.})
 for(int hp:{0,100}){
  double prevOut=0;bool first=true;
  for(int in=-10;in<=22;++in){
   double actualGr=0;
   const double out=measure(kind,threshold,in,hp,actualGr);
   const double delta=out-prevOut;
   const double ratio=first?-1:(std::abs(delta)<1e-8?999:1./delta);
   std::cout<<(kind==0?"NSL32":kind==1?"VTL5C2":"T4B")
    <<","<<threshold<<","<<hp<<","<<in<<","<<out
    <<","<<in-out<<","<<ratio<<","<<(first?0:delta)<<"\n";
   prevOut=out;first=false;
  }
 }
}
