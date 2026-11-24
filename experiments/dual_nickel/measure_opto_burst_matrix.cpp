// Actual optical gain-reduction envelope for all three cell selections.
// 1 kHz sine bursts, independent dark-cell starts, Compressor mode.
// Attack percentages use the SAME sustained (2000 ms) GR reference per
// cell and level. A short burst may never reach 63.2/90% (reported NA).
// Release fractions reference GR at burst termination, not the long-burst
// reference. 1 dB endpoint means <=1.0 dB remaining optical GR.
// Each recovery is observed for a full 6000 ms; NA means not reached.
// This is a measurement only: it does not modify DSP, optical behavior,
// cell coefficients, or detector timing.
#include "OpticalCell.hpp"
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>
constexpr double fs=44100.,pi=3.14159265358979323846;
constexpr double fsPeak=17.3677;
struct Result {
 double burstGr=0., peakGr=0., refGr=0.;
 double a10=-1.,a50=-1.,a63=-1.,a90=-1.;
 double r50=-1.,r10=-1.,r1db=-1.;
 double remaining100=0.,remaining500=0.,remaining1000=0.;
 double remaining2000=0.,remaining4000=0.,remaining6000=0.;
};
static int samples(double ms){return int(std::lround(ms*fs/1000.));}
static const char* label(int kind){return kind==0?"NSL32":kind==1?"VTL5C2":"T4B";}
static double tone(double dbu,int n){
 const double peak=std::sqrt(2.)*.775*std::pow(10.,dbu/20.)/fsPeak;
 return peak*std::sin(2.*pi*1000.*double(n)/fs);
}
static double timeCross(const std::vector<double>& values,double threshold,bool upward){
 if(threshold<=0.)return -1.;
 for(size_t i=0;i<values.size();++i)
  if(upward?values[i]>=threshold:values[i]<=threshold)
   return double(i)*1000./fs;
 return -1.;
}
static double atTime(const std::vector<double>& v,double ms){
 const size_t i=std::min(v.size()-1,size_t(samples(ms)));
 return v[i];
}
static Result measure(int kind,double dbu,double burstMs,double referenceGr){
 optocircuit::OpticalCompressor c;
 c.prepare(fs);
 c.setCellKind(static_cast<optocircuit::CellKind>(kind));
 c.setLimiter(false);
 c.setFeedforward(false);
 c.setThresholdDb(-18.);
 c.setAttackMs(15.);
 c.setReleaseMs(250.);
 const int warm=samples(250.),burst=samples(burstMs),recovery=samples(6000.);
 for(int n=0;n<warm;++n)c.process(0.f);
 std::vector<double> attack,release;
 attack.reserve(burst);release.reserve(recovery);
 double peakGr=0.;
 for(int n=0;n<burst;++n){
  c.process(float(tone(dbu,n)));
  const double gr=c.reductionDb();
  if(!std::isfinite(gr))std::exit(2);
  peakGr=std::max(peakGr,gr);
  attack.push_back(gr);
 }
 for(int n=0;n<recovery;++n) {
  c.process(0.f);
  const double gr=c.reductionDb();
  if(!std::isfinite(gr))std::exit(3);
  release.push_back(gr);
 }
 Result r;
 r.burstGr=attack.back();r.peakGr=peakGr;r.refGr=referenceGr;
 // Reference to 2-second steady-burst gain reduction, NOT an artificially
 // short-burst normalization. If not reached, report -1 as NA.
 r.a10=(referenceGr>=.1)?timeCross(attack,.10*referenceGr,true):-1.;
 r.a50=(referenceGr>=.1)?timeCross(attack,.50*referenceGr,true):-1.;
 r.a63=(referenceGr>=.1)?timeCross(attack,.632*referenceGr,true):-1.;
 r.a90=(referenceGr>=.1)?timeCross(attack,.90*referenceGr,true):-1.;
 r.r50=(r.burstGr>=.1)?timeCross(release,.50*r.burstGr,false):-1.;
 r.r10=(r.burstGr>=.1)?timeCross(release,.10*r.burstGr,false):-1.;
 r.r1db=(r.burstGr>=1.)?timeCross(release,1.0,false):-1.;
 r.remaining100=atTime(release,100.);
 r.remaining500=atTime(release,500.);
 r.remaining1000=atTime(release,1000.);
 r.remaining2000=atTime(release,2000.);
 r.remaining4000=atTime(release,4000.);
 r.remaining6000=release.back();
 return r;
}
int main(){
 std::cout<<std::fixed<<std::setprecision(5);
 std::cout<<"cell,mode,threshold_dbu,attack_setting_ms,release_setting_ms,"
  "level_dbu,burst_ms,long_reference_GR_db,burst_end_GR_db,"
  "burst_peak_GR_db,attack_10pct_long_ms,attack_50pct_long_ms,"
  "attack_63pct_long_ms,attack_90pct_long_ms,"
  "release_50pct_ms,release_10pct_ms,release_to_1dB_ms,"
  "remaining_GR_100ms,remaining_GR_500ms,remaining_GR_1000ms,"
  "remaining_GR_2000ms,remaining_GR_4000ms,remaining_GR_6000ms\n";
 for(int kind=0;kind<3;++kind)
  for(double level:{0.,4.,8.,12.,20.}){
   // The 2s reference is computed once and used for every exposure.
   const Result longResult=measure(kind,level,2000.,0.);
   const double ref=longResult.burstGr;
   for(double burst:{30.,150.,500.,2000.}){
    const Result r=measure(kind,level,burst,ref);
    if(kind==2 && burst==2000. && level>=8.) {
      // Physics regression guard: rapid first half recovery while
      // occupied deep CdS traps continue to recover over seconds.
      if(!(r.r50>=40. && r.r50<=125. && r.r10>=500.
             && r.r10<=5000. && r.remaining2000>.10
             && r.a90>=2. && r.a90<=35.))
        return 8;
    }
    std::cout<<label(kind)<<",Compressor,-18,15,250,"
     <<level<<","<<burst<<","<<ref<<","<<r.burstGr<<","<<r.peakGr<<","
     <<r.a10<<","
     <<r.a50<<","<<r.a63<<","<<r.a90<<","
     <<r.r50<<","<<r.r10<<","<<r.r1db<<","
     <<r.remaining100<<","<<r.remaining500<<","<<r.remaining1000<<","
     <<r.remaining2000<<","<<r.remaining4000<<","<<r.remaining6000<<"\n";
   }
  }
 return 0;
}
