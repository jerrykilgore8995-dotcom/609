// T4B-only physical CdS trap-population fit. No gain-reduction envelope,
// numerical release knee, or change to EL/attenuator/other optical cells.
#include "OpticalCell.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>
constexpr double fs=44100.,pi=3.14159265358979323846,peak=17.3677;
struct Test {double gr=0., a63=-1.,a90=-1.,r50=-1.,r10=-1.,r1=-1.,r500=0.,r1000=0.,r2000=0.,r5000=0.,r6000=0.;};
static double crossing(const std::vector<double>& v,double target,bool rise){
 if(target<=0.)return -1.;
 for(int i=0;i<int(v.size());++i)
   if(rise?v[i]>=target:v[i]<=target)return double(i)*1000./fs;
 return -1.;
}
static double read(const std::vector<double>& v,int ms){
 return v[std::min(int(v.size())-1,int(ms*fs*.001))];
}
static Test test(double level,int burstMs,const optocircuit::OptoParts& p){
 optocircuit::OpticalCompressor c;
 c.prepare(fs,p);c.setCellKind(optocircuit::CellKind::T4B);
 c.setLimiter(false);c.setThresholdDb(-18.);c.setAttackMs(15.);c.setReleaseMs(250.);
 const int nBurst=int(burstMs*fs*.001),nOff=int(6000.*fs*.001);
 std::vector<double> a,b;a.reserve(nBurst);b.reserve(nOff);
 const double amplitude=.775*std::sqrt(2.)*std::pow(10.,level/20.)/peak;
 for(int n=0;n<nBurst;++n){
   c.process(float(amplitude*std::sin(2.*pi*1000.*n/fs)));
   a.push_back(c.reductionDb());
 }
 for(int n=0;n<nOff;++n){c.process(0.f);b.push_back(c.reductionDb());}
 Test r;r.gr=a.back();
 r.a63=crossing(a,r.gr*.632,true);r.a90=crossing(a,r.gr*.90,true);
 r.r50=crossing(b,r.gr*.50,false);r.r10=crossing(b,r.gr*.10,false);
 r.r1=r.gr>=1.?crossing(b,1.,false):-1.;
 r.r500=read(b,500);r.r1000=read(b,1000);r.r2000=read(b,2000);
 r.r5000=read(b,5000);r.r6000=b.back();
 return r;
}
int main(){
 std::cout<<std::fixed<<std::setprecision(5);
 std::cout<<"t4_trap_weight,t4_trap_charge_s,t4_trap_release_s,"
  "level_dbu,burst_ms,gr_db,attack_63_ms,attack_90_ms,"
  "release_50_ms,release_10_ms,release_to_1db_ms,"
  "remaining_GR_500ms,remaining_GR_1000ms,"
  "remaining_GR_2000ms,remaining_GR_5000ms,remaining_GR_6000ms\n";
 for(double weight:{.012,.08,.15,.25,.35})
 for(double charge:{.30,.65})
 for(double release:{2.8,4.0}){
  optocircuit::OptoParts p;
  p.t4CdSTrapConductanceFraction=weight;
  p.t4CdSTrapChargeSeconds=charge;
  p.t4CdSTrapRecoverySeconds=release;
  for(double level:{12.,20.})
  for(int burst:{150,2000}){
    const auto r=test(level,burst,p);
    std::cout<<weight<<","<<charge<<","<<release<<","<<level<<","
       <<burst<<","<<r.gr<<","<<r.a63<<","<<r.a90<<","
       <<r.r50<<","<<r.r10<<","<<r.r1<<","
       <<r.r500<<","<<r.r1000<<","<<r.r2000<<","
       <<r.r5000<<","<<r.r6000<<"\n";
  }
 }
}
