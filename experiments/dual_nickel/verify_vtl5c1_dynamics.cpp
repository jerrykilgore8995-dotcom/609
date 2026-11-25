// VTL5C1 cell-specific validation: fine static transfer and true optical
// gain-reduction timing. Reports, never silently substitutes knob values.
#include "OpticalCell.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>
using namespace optocircuit;
constexpr double fs=44100.,pk=17.3677;
double tone(double dbu,int i) {
 return std::sqrt(2.)*.775*dB(dbu)/pk*std::sin(2*pi*1000.*i/fs);
}
double steady(double level,double threshold,double atk=15.,double rel=250.) {
 OpticalCompressor c;c.prepare(fs);c.setCellKind(CellKind::VTL5C1);
 c.setThresholdDb(threshold);c.setAttackMs(atk);c.setReleaseMs(rel);
 const int count=int(.75*fs),start=int(.55*fs);
 double total=0.;
 for(int i=0;i<count;++i) {
   const double y=c.process(float(tone(level,i)));
   if(i>=start)total+=y*y;
 }
 return toDb(std::sqrt(total/(count-start))*pk/.775);
}
struct Timing {double gr=0,a63=-1.,release50=-1.,release10=-1.;};
Timing burst(double attack,double release,double level,double threshold,double holdMs) {
 OpticalCompressor c;c.prepare(fs);c.setCellKind(CellKind::VTL5C1);
 c.setThresholdDb(threshold);c.setAttackMs(attack);c.setReleaseMs(release);
 const int hold=int(holdMs*.001*fs);
 std::vector<double> history(hold);
 for(int i=0;i<hold;++i) {
   c.process(float(tone(level,i)));
   history[i]=c.reductionDb();
 }
 Timing r;r.gr=history.back();
 for(int i=0;i<hold;++i)
    if(r.a63<0. && history[i]>=.632*r.gr)r.a63=1000.*i/fs;
 for(int i=0;i<int(5*fs);++i) {
    c.process(0.f);
    const double gr=c.reductionDb();
    if(r.release50<0.&&gr<=.5*r.gr)r.release50=1000.*i/fs;
    if(r.release10<0.&&gr<=.1*r.gr)r.release10=1000.*i/fs;
 }
 return r;
}
int main(){
 std::cout<<std::fixed<<std::setprecision(5);
 std::cout<<"SECTION,threshold_dbu,input_dbu,output_dbu,GR_db,local_ratio\n";
 for(double threshold:{-24.,-18.,-12.}) {
   double oldLevel=0.,oldOut=0.;bool first=true;
   for(int level=-12;level<=24;level+=2){
      const double out=steady(level,threshold);
      const double ratio=first?0.:2./(out-oldOut);
      std::cout<<"KNEE,"<<threshold<<","<<level<<","<<out<<","
        <<level-out<<","<<ratio<<"\n";
      first=false;oldLevel=level;oldOut=out;
   }
 }
 std::cout<<"SECTION,threshold_dbu,level_dbu,attack_setting_ms,release_setting_ms,burst_ms,GR_db,attack63_ms,release50_ms,release10_ms\n";
 for(double level:{0.,4.,12.})
 for(double threshold:{-24.,-18.,-12.})
 for(double attack:{10.,15.,30.,50.,100.}) {
   Timing r=burst(attack,250.,level,threshold,1500.);
   std::cout<<"ATTACK,"<<threshold<<","<<level<<","<<attack<<",250,1500,"
         <<r.gr<<","<<r.a63<<","<<r.release50<<","<<r.release10<<"\n";
 }
 for(double level:{0.,4.,12.})
 for(double threshold:{-24.,-18.,-12.})
 for(double release:{50.,100.,250.,500.,1000.,2000.}) {
   Timing r=burst(15.,release,level,threshold,1500.);
   std::cout<<"RELEASE,"<<threshold<<","<<level<<",15,"<<release<<",1500,"
         <<r.gr<<","<<r.a63<<","<<r.release50<<","<<r.release10<<"\n";
 }
}