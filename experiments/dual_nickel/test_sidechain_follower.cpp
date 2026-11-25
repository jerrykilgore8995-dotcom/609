// Bench ON 2N5088 buffered passive RC sidechain HPF, independent of DSP.
// Checks loading independence and unity baseline and tests GR for all three
// optical models at 80 / 100Hz.
#include "OpticalCell.hpp"
#include <iostream>
#include <cmath>
#include <iomanip>
#include <algorithm>
constexpr double fs=44100.,pi=3.14159265358979323846;
double amp(double x){return std::sqrt(2.)*.775*std::pow(10.,x/20.);}
double response(double freq,double corner,bool enabled,double volts=1.){
 optocircuit::SidechainFollower f;
 f.prepare(fs);f.setEnabled(enabled);f.setCutoffHz(corner);
 double re=0,im=0,refs=0,refc=0;
 const int warm=int(fs*.5),n=int(fs*2.);
 for(int i=0;i<warm+n;++i){
  double ph=2*pi*freq*i/fs;
  double x=volts*std::sin(ph);
  double y=f.process(x);
  if(i>=warm){re+=y*std::sin(ph);im+=y*std::cos(ph);
   refs+=x*std::sin(ph);refc+=x*std::cos(ph);}
 }
 return std::hypot(re,im)/std::max(1e-12,std::hypot(refs,refc));
}
double compression(int kind,double hz,double cutoff,bool enabled){
 optocircuit::OpticalCompressor c;c.prepare(fs);
 c.setCellKind(static_cast<optocircuit::CellKind>(kind));
 c.setLimiter(false);c.setFeedforward(false);
 c.setAttackMs(15.);c.setReleaseMs(250.);c.setThresholdDb(-18.);
 c.setSidechainHpF(enabled);c.setSidechainHz(cutoff);
 const double peak=amp(12.)/17.3677;
 double sum=0;
 constexpr int n=int(fs*2.);
 for(int i=0;i<n;++i){
  const float x=float(peak*std::sin(2*pi*hz*i/fs));
  c.process(x);
  if(i>int(fs*1.5))sum+=c.reductionDb();
 }
 return sum/(n-int(fs*1.5)-1);
}
int main(){
 std::cout<<std::fixed<<std::setprecision(5);
 std::cout<<"type,cell_or_freq,hpf_hz,off_or_on,sidechain_gain_db,GR_db\\n";
 bool ok=true;
 for(double hz:{40.,55.,80.,100.,200.,1000.,10000.}){
  const double baseline=response(hz,100.,false);
  for(double cutoff:{80.,100.}){
   const double v=response(hz,cutoff,true);
   const double db=20*std::log10(v/std::max(1e-10,baseline));
   std::cout<<"FR,"<<hz<<","<<cutoff<<",on,"<<db<<",0\\n";
   if(hz>=1000. && std::abs(db)>.15)ok=false;
  }
  if(hz>=1000. && std::abs(20*std::log10(baseline))>.3)ok=false;
 }
 for(int kind=0;kind<3;++kind)for(double hz:{55.,100.,1000.}){
  const double off=compression(kind,hz,100.,false);
  for(double cutoff:{80.,100.}){
   const double on=compression(kind,hz,cutoff,true);
   std::cout<<"GR,"<<kind<<","<<cutoff<<",on,"<<hz<<","<<on<<"\\n";
   if(hz==55. && on>off+.1)ok=false;
   if(hz==1000. && std::abs(on-off)>.7)ok=false;
  }
  std::cout<<"GR,"<<kind<<",0,off,"<<hz<<","<<off<<"\\n";
 }
 std::cerr<<"BUFFERED_HPF_TEST "<<(ok?"PASS":"FAIL")<<"\\n";
 return ok?0:7;
}
