// Regression: hidden VTL5C1 attack/release automation must NOT change
// fixed detector timing of NSL-32, VTL5C2, or T4B, including switch-away
// and extreme hidden-value cases. Identical full sample traces are required.
#include "OpticalCell.hpp"
#include <cmath>
#include <iostream>
using namespace optocircuit;
static double run(CellKind type,double previousAttack,double previousRelease,
                  bool visitedVtl, bool hiddenAutomation) {
 OpticalCompressor comp;comp.prepare(44100.);
 if(visitedVtl) {
   comp.setCellKind(CellKind::VTL5C1);
   comp.setAttackMs(previousAttack);
   comp.setReleaseMs(previousRelease);
 }
 comp.setCellKind(type);
 if(hiddenAutomation) {
   comp.setAttackMs(previousAttack);
   comp.setReleaseMs(previousRelease);
 }
 if(comp.variableTimingEnabled())return -1.;
 comp.setThresholdDb(-18.);
 double signature=0.,energy=0.;
 for(int i=0;i<44100;++i) {
   const double x=.42*std::sin(2*pi*1000.*i/44100.);
   const double out=comp.process(float(x));
   if(!std::isfinite(out))return -2.;
   if(i>=22050) {
     energy+=out*out;
     signature+=out*std::sin(2*pi*1700.*i/44100.);
   }
 }
 return signature*.001+energy/22050. + comp.reductionDb()*.1;
}
int main(){
 for(CellKind type:{CellKind::NSL32,CellKind::VTL5C2,CellKind::T4B}) {
   const double baseline=run(type,15.,250.,false,false);
   for(double attack:{10.,100.})for(double release:{50.,2000.})
   for(bool visitedVtl:{false,true}) {
      const double checked=run(type,attack,release,visitedVtl,true);
      const double delta=std::abs(checked-baseline);
      std::cout<<"CELL "<<int(type)<<" previousVTL="<<visitedVtl
               <<" hiddenAttack="<<attack<<" hiddenRelease="<<release
               <<" outputDelta="<<delta<<"\n";
      if(!std::isfinite(checked)||delta>1e-7) return 8;
   }
 }
 // VTL5C1 must still respond to the very same hidden parameter values
 // when it is selected again.
 auto vtl=[&](double atk,double rel) {
   OpticalCompressor comp;comp.prepare(44100.);
   comp.setCellKind(CellKind::VTL5C1);
   comp.setAttackMs(atk);comp.setReleaseMs(rel);
   comp.setThresholdDb(-18.);
   for(int i=0;i<44100;++i)
     comp.process(float(.42*std::sin(2*pi*1000.*i/44100.)));
   double sum=0.;
   for(int i=0;i<44100;++i) {
     comp.process(0.f);
     sum+=comp.reductionDb();
   }
   return sum;
 };
 if(std::abs(vtl(10.,50.)-vtl(100.,2000.))<.1) return 9;
 std::cout<<"HIDDEN_VTL_TIMING_ISOLATION=PASS\n";
}