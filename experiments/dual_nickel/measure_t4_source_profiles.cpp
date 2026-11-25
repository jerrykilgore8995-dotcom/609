// Reproducible PROXY signals for MODO Bass, SSD kick, and distorted guitar bus.
// NOT samples from these products. Runs actual T4B EL/CdS circuit in Compressor mode.
// Report distinct transient, sustained, and history-dependent GR for each profile.
#include "OpticalCell.hpp"
#include <cmath>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <limits>
constexpr double fs=44100.,pi=3.14159265358979323846;
constexpr double scale=17.3677;
static double dbu(double level){return .775*std::sqrt(2.)*std::pow(10.,level/20.)/scale;}
static double tail(double t,double seconds){return std::exp(-std::max(0.,t)/seconds);}
struct Metrics {
 double peak=0.,avg=0.,min=100.,first40=0.,first100=0.,middle=0.,late=0.,gap=0.;
 double after100=0.,after500=0.,after1000=0.,after2000=0.,after5000=0.;
};
static double sample(int type,double time,double level,double bpm){
 const double beat=60./bpm;
 const double t=std::max(0.,time);
 const double amp=dbu(level);
 if(type==0) { // articulated bass notes with 80ms gaps and alternating intensity
  const double note=beat*.5;
  const int n=int(t/note);
  const double nt=t-note*n;
  if(nt>note-.08)return 0.;
  const double onset=1.-std::exp(-nt/.003);
  const double envelope=onset*(.48+.52*std::exp(-nt/.18));
  const double velocity=n%4==0?1.0:n%4==1?.8:n%4==2?.7:.9;
  // Fundamental 55Hz plus nonlinear pluck harmonics, not proprietary MODO audio.
  return amp*velocity*envelope*(std::sin(2*pi*55*t)+
     .35*std::sin(2*pi*110*t)*tail(nt,.11)+
     .15*std::sin(2*pi*165*t)*tail(nt,.06))/1.2;
 }
 if(type==1){ // sampled-style kick proxy: dense first 8ms + 60-75Hz tail
   const double interval=beat*.5;
   const double k=t-std::floor(t/interval)*interval;
   const double env=(1.-std::exp(-k/.0015))*std::exp(-k/.11);
   const double freq=57.+42.*std::exp(-k/.018);
   const double body=std::sin(2*pi*(57.*k+42.*(.018)*(1.-std::exp(-k/.018))));
   const double click=.28*std::sin(2*pi*2400*k)*std::exp(-k/.004);
   return amp*env*(body+click);
 }
 // dense double-tracked distorted guitar bus proxy; asymmetric picking
 const double eighth=beat*.5;
 const double ntime=t-std::floor(t/eighth)*eighth;
 const double attack=1.-std::exp(-ntime/.004);
 const double pattern=.72+.28*std::exp(-ntime/.12);
 const double gate=(ntime<eighth-.03?1.:.2);
 const double wave=.48*std::sin(2*pi*110*t)+.33*std::sin(2*pi*220*t)+
                   .20*std::sin(2*pi*330*t)+.12*std::sin(2*pi*440*t)+
                   .07*std::sin(2*pi*770*t);
 return amp*gate*attack*pattern*std::tanh(2.2*wave);
}
static double readAt(const double* values,int n,int which){
 return values[std::clamp(which,0,n-1)];
}
static Metrics measure(int type,double level,double bpm){
 optocircuit::OpticalCompressor c;
 c.prepare(fs);c.setCellKind(optocircuit::CellKind::T4B);
 c.setLimiter(false);c.setFeedforward(false);
 c.setThresholdDb(-18.);c.setAttackMs(15.);c.setReleaseMs(250.);
 constexpr int n=int(2.*fs),off=int(5.*fs);
 Metrics m;double sum=0.;
 double history[6]={}; // summary checkpoints, not DSP timing
 for(int i=0;i<n;++i){
   const double t=i/fs;
   c.process(float(sample(type,t,level,bpm)));
   const double gr=c.reductionDb();
   if(!std::isfinite(gr))std::exit(2);
   sum+=gr;m.peak=std::max(m.peak,gr);m.min=std::min(m.min,gr);
   if(i==int(.04*fs))m.first40=gr;
   if(i==int(.1*fs))m.first100=gr;
   if(i==int(.8*fs))m.middle=gr;
   if(i==int(1.95*fs))m.late=gr;
 }
 m.avg=sum/n;
 m.gap=c.reductionDb();
 for(int i=0;i<off;++i){
  c.process(0.f);
  const double g=c.reductionDb();
  if(i==int(.1*fs))m.after100=g;
  if(i==int(.5*fs))m.after500=g;
  if(i==int(1.*fs))m.after1000=g;
  if(i==int(2.*fs))m.after2000=g;
  if(i==off-1)m.after5000=g;
 }
 return m;
}
int main(){
 std::cout<<std::fixed<<std::setprecision(5);
 std::cout<<"proxy,source_reference_dbu,bpm,GR_peak_dB,GR_avg_dB,GR_min_dB,"
 "GR_at40ms,GR_at100ms,GR_at800ms,GR_at1950ms,GR_end,"
 "GR_after100ms,GR_after500ms,GR_after1000ms,GR_after2000ms,GR_after5000ms\n";
 for(int type=0;type<3;++type)
 for(double level:{4.,8.,12.,16.})
 for(double bpm:{100.,160.}){
  auto m=measure(type,level,bpm);
  std::cout<<(type==0?"MODO_BASS_PROXY":type==1?"SSD_KICK_PROXY":"GUITAR_BUS_PROXY")
    <<","<<level<<","<<bpm<<","<<m.peak<<","<<m.avg<<","<<m.min
    <<","<<m.first40<<","<<m.first100<<","<<m.middle<<","<<m.late
    <<","<<m.gap<<","<<m.after100<<","<<m.after500<<","
    <<m.after1000<<","<<m.after2000<<","<<m.after5000<<"\n";
 }
}
