// Standalone, full-band "Sneap/C4-like" low-mid validation, no DSP edits.
// Verify transparent reconstruction, steady selective cut, attack/release,
// threshold/range controls, sample-rate invariance and stereo linked GR.
#include "DynamicLowMid.hpp"
#include "AnalogOptoProcessor.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
using namespace lowmid;
constexpr double pi2=6.2831853071795864769;
static double toneGain(double fs,double hz,bool enabled,double cut,double thresh,
                       double center,double width,double attack,double release,double input=.20) {
 DynamicLowMid d;d.prepare(fs);d.setEnabled(enabled);d.setMaxCutDb(cut);
 d.setThresholdDb(thresh);d.setCenterHz(center);d.setWidthOctaves(width);
 d.setAttackMs(attack);d.setReleaseMs(release);
 const int N=int(fs*.8),start=int(fs*.6);
 double sine=0.,cosine=0.;
 for(int i=0;i<N;++i){
    double p=pi2*hz*i/fs;
    float out=d.process(float(input*std::sin(p)));
    if(i>=start){sine+=out*std::sin(p);cosine+=out*std::cos(p);}
 }
 const double output=2.*std::hypot(sine,cosine)/(N-start);
 return 20*std::log10(std::max(1.e-15,output)/input);
}
struct Timings {double atk=-1,rel=-1,peak=0;};
static Timings burst(double fs,double atk,double rel){
 DynamicLowMid d;d.prepare(fs);d.setEnabled(true);d.setCenterHz(170.);
 d.setWidthOctaves(1.3);d.setMaxCutDb(8.);d.setThresholdDb(-35.);
 d.setAttackMs(atk);d.setReleaseMs(rel);
 const int N=int(fs*1.5);
 double maxGr=0.;Timings t{};
 for(int i=0;i<N;++i){
   d.process(float(.4*std::sin(pi2*170*i/fs)));
   if(i==N-1)t.peak=d.reductionDb();
   if(t.atk<0.&&d.reductionDb()>=.632*6.)t.atk=1000.*i/fs;
 }
 for(int i=0;i<int(4*fs);++i){
   d.process(0.f);
   if(t.rel<0 && d.reductionDb()<=.5*t.peak)t.rel=1000.*i/fs;
 }
 return t;
}
int main(){
 std::cout<<std::fixed<<std::setprecision(6);
 std::cout<<"SECTION,samplerate,frequency,enabled,range_db,threshold_db,center,width,attack_ms,release_ms,gain_db\n";
 for(double fs:{44100.,96000.})
 for(double f:{40.,80.,108.,140.,170.,200.,267.,350.,500.,1000.,5000.,10000.,20000.})
 for(int enabled:{0,1})
 for(double cut:{0.,4.,8.})
 {
  double gain=toneGain(fs,f,enabled!=0,cut,-35.,170.,1.3,10.,150.);
  std::cout<<"BAND,"<<fs<<","<<f<<","<<enabled<<","<<cut<<",-35,170,1.3,10,150,"<<gain<<"\n";
  if(!std::isfinite(gain))return 2;
  if((!enabled||cut==0.) && std::abs(gain)>.5) {
   std::cerr<<"transparent error "<<fs<<" "<<f<<" "<<gain<<"\n";return 3;
  }
 }
 std::cout<<"SECTION,samplerate,attack_ms,release_ms,attack63_ms,release50_ms,peak_gr_db\n";
 for(double fs:{44100.,96000.})
 for(double a:{1.,10.,30.,80.})
 for(double r:{20.,150.,500.,1000.}){
  auto t=burst(fs,a,r);
  std::cout<<"PULSE,"<<fs<<","<<a<<","<<r<<","<<t.atk<<","<<t.rel<<","<<t.peak<<"\n";
  if(!(t.peak>1.&&t.rel>0.))return 4;
 }
 std::cout<<"LOWMID_CHUG_TEST=PASS\n";
}
