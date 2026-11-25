// Non-gating characterization: records deviations but never fails on a
// 10% specification discrepancy. Genuine NaNs/Inf still get surfaced in CSV.
// Runs actual optical circuit, physical RC+2N5088 follower, all three cell models.
// Absolute calibrated levels in dBu, threshold in dBu, frequencies in Hz.
// Separate independent history for every static point. 44.1kHz/0.55s
// settle is a controlled comparison, not a substitute for >2s slow CdS tests.
#include "OpticalCell.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
constexpr double fs=44100.,pi=3.14159265358979323846,peakVolts=17.3677;
inline double amplitude(double dbu) {
 return std::sqrt(2.)*.775*std::pow(10.,dbu/20.)/peakVolts;
}
const char* label(int kind){return kind==0?"NSL32":kind==1?"VTL5C2":"T4B";}
struct StaticPoint{double outDbu=0.,endGr=0.,averageGr=0.,peakGr=0.;};
static StaticPoint steady(int kind,double hz,int hpf,double threshold,double dbu){
 optocircuit::OpticalCompressor c;c.prepare(fs);
 c.setCellKind(static_cast<optocircuit::CellKind>(kind));
 c.setLimiter(false);c.setFeedforward(false);
 c.setThresholdDb(threshold);c.setAttackMs(15.);c.setReleaseMs(250.);
 c.setSidechainHpF(hpf>0);if(hpf>0)c.setSidechainHz(hpf);
 constexpr int n=int(fs*.55),start=int(fs*.40);
 double energy=0.,grsum=0.,maxgr=0.;
 double a=amplitude(dbu);
 for(int i=0;i<n;++i) {
  const auto x=float(a*std::sin(2*pi*hz*i/fs));
  const double y=c.process(x);
  const double g=c.reductionDb();
  if(i>=start){energy+=y*y;grsum+=g;}
  maxgr=std::max(g,maxgr);
 }
 StaticPoint p;
 p.outDbu=20*std::log10(std::max(1e-12,std::sqrt(energy/(n-start))*peakVolts/.775));
 p.endGr=c.reductionDb();p.averageGr=grsum/(n-start);p.peakGr=maxgr;
 return p;
}
struct Timing{double grEnd=0.,att63=-1.,att90=-1.,release50=-1.,release10=-1.,after250=0.,after1000=0.,after2000=0.;};
static Timing burst(int kind,double hz,int hpf,double threshold,double level,int burstMs){
 optocircuit::OpticalCompressor c;c.prepare(fs);
 c.setCellKind(static_cast<optocircuit::CellKind>(kind));
 c.setLimiter(false);c.setFeedforward(false);
 c.setThresholdDb(threshold);c.setAttackMs(15.);c.setReleaseMs(250.);
 c.setSidechainHpF(hpf>0);if(hpf>0)c.setSidechainHz(hpf);
 const int n=int(fs*burstMs/1000.),tail=int(fs*2.);
 const double a=amplitude(level);
 // Relative to the actual burst end GR, with short-burst comparisons
 // explicitly labelled (not treated as true steady-state attack).
 double* response=new double[n];
 for(int i=0;i<n;++i) {
  c.process(float(a*std::sin(2*pi*hz*i/fs)));
  response[i]=c.reductionDb();
 }
 Timing t;t.grEnd=response[n-1];
 if(t.grEnd>=.1)for(int i=0;i<n;++i) {
  const double ms=1000.*i/fs;
  if(t.att63<0&&response[i]>=t.grEnd*.632)t.att63=ms;
  if(t.att90<0&&response[i]>=t.grEnd*.90)t.att90=ms;
 }
 delete[] response;
 for(int i=0;i<tail;++i) {
  c.process(0.f);
  const double g=c.reductionDb(),ms=1000.*i/fs;
  if(t.grEnd>=.1) {
    if(t.release50<0&&g<=t.grEnd*.5)t.release50=ms;
    if(t.release10<0&&g<=t.grEnd*.1)t.release10=ms;
  }
  if(i==int(.25*fs))t.after250=g;
  if(i==int(fs))t.after1000=g;
  if(i==tail-1)t.after2000=g;
 }
 return t;
}
int main(){
 std::cout<<std::fixed<<std::setprecision(5);
 std::cout<<"measurement,cell,freq_hz,hpf_hz,threshold_dbu,input_dbu,"
 "output_dbu,GR_average_db,GR_end_db,GR_peak_db,local_ratio,"
 "burst_ms,attack63_ms,attack90_ms,release50_ms,release10_ms,"
 "GR_after250ms,GR_after1000ms,GR_after2000ms\n";
 int count=0;
 // Select frequencies appropriate to low bass, guitar-mid, and higher
 // harmonic / bite. The frequency sweep is independent of the signals.
 for(int kind=0;kind<3;++kind)
 for(double hz:{55.,220.,1000.})
 for(int hp:{0,80,100,250})
 for(double threshold:{-24.,-18.,-12.}) {
  double prevInput=0.,prevOutput=0.;
  bool first=true;
  for(double level:{-8.,-4.,0.,4.,8.,12.,16.,20.}) {
   const auto p=steady(kind,hz,hp,threshold,level);
   const double diff=p.outDbu-prevOutput;
   const double ratio=first?-1.:(std::abs(diff)<1.e-6?999.:(level-prevInput)/diff);
   std::cout<<"static,"<<label(kind)<<","<<hz<<","<<hp<<","<<threshold
    <<","<<level<<","<<p.outDbu<<","<<p.averageGr<<","
    <<p.endGr<<","<<p.peakGr<<","<<ratio<<",0,-1,-1,-1,-1,-1,-1,-1\n";
   ++count;first=false;prevInput=level;prevOutput=p.outDbu;
  }
 }
 // Attack and release, short vs long program, with both shifted
 // threshold and multiple source levels.
 for(int kind=0;kind<3;++kind)
 for(double hz:{55.,220.,1000.})
 for(int hp:{0,100,250})
 for(double threshold:{-24.,-18.,-12.})
 for(double level:{4.,12.})
 for(int length:{30,150,1000}) {
  const Timing t=burst(kind,hz,hp,threshold,level,length);
  std::cout<<"timing,"<<label(kind)<<","<<hz<<","<<hp<<","<<threshold
   <<","<<level<<",0,0,"<<t.grEnd<<",0,-1,"<<length
   <<","<<t.att63<<","<<t.att90<<","<<t.release50<<","<<t.release10
   <<","<<t.after250<<","<<t.after1000<<","<<t.after2000<<"\n";
  ++count;
 }
 std::cerr<<"AUDIT_COMPLETE points="<<count
  <<" deviations are advisory only; NO 10%-tolerance gating.\n";
}
