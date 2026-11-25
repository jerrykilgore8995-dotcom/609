// Physical T4B CdS carrier rise-time exploration with validated two-stage release.
#include "OpticalCell.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
constexpr double fs=44100.,pi=3.14159265358979323846,fsPeak=17.3677;
static double crossing(double x,double dbu,double rise,double& a90,double& burstGR) {
 optocircuit::OptoParts p;
 p.t4CdSTrapConductanceFraction=.15;
 p.t4CdSTrapChargeSeconds=.65;
 p.t4CdSTrapRecoverySeconds=2.8;
 p.t4CdSAttackSeconds=rise;
 optocircuit::OpticalCompressor c;
 c.prepare(fs,p);c.setCellKind(optocircuit::CellKind::T4B);
 c.setLimiter(false);c.setThresholdDb(-18.);
 constexpr int n=88200;
 const double peak=.775*std::sqrt(2.)*std::pow(10.,dbu/20.)/fsPeak;
 double reference=0;
 for(int i=0;i<n;++i) {
  c.process(float(peak*std::sin(2*pi*1000*i/fs)));
  reference=c.reductionDb();
 }
 c.reset();
 c.setCellKind(optocircuit::CellKind::T4B);
 double a63=-1.;a90=-1.;
 for(int i=0;i<n;++i) {
  c.process(float(peak*std::sin(2*pi*1000*i/fs)));
  const double gr=c.reductionDb();
  if(a63<0&&gr>=reference*.632)a63=1000.*i/fs;
  if(a90<0&&gr>=reference*.90)a90=1000.*i/fs;
  burstGR=gr;
 }
 return a63;
}
int main(){
 std::cout<<std::fixed<<std::setprecision(4);
 std::cout<<"cell,rise_tau_seconds,level_dbu,attack63_ms,attack90_ms,GR_2s_db\n";
 for(double rise:{.010,.020,.035,.050,.070})
 for(double dbu:{4.,12.,20.}){
  double a90=0,gr=0;
  double a63=crossing(0,dbu,rise,a90,gr);
  std::cout<<"T4B,"<<rise<<","<<dbu<<","<<a63<<","<<a90<<","<<gr<<"\n";
 }
}
