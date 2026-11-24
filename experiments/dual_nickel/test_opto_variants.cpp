#include "OpticalCell.hpp"
#include <cmath>
#include <iostream>
#include <iomanip>
#include <algorithm>
constexpr double fs=44100.;
static double resistance(double mA){
  optocircuit::LdrCell c;c.prepare(fs);c.select(optocircuit::CellKind::VTL5C2);
  for(int i=0;i<int(3*fs);++i)c.processAtLedCurrent(1.,mA*.001);
  return c.resistance();
}
static void check(bool valid,int code) {if(!valid)std::exit(code);}
int main(){
  std::cout<<std::fixed<<std::setprecision(5);
  for(double mA:{0.,1.,10.,20.,40.}){
    const double r=resistance(mA);
    std::cout<<"VTL5C2 LED_mA="<<mA<<" LDR_ohm="<<r<<"\n";
    if(mA==0.)check(r>980000.,11);
    if(mA==1.)check(r>5000 && r<6100,2);
    if(mA==10.)check(r>730 && r<900,3);
    if(mA==40.)check(r>185 && r<220,4);
  }
  // Test VTL5C2 maximum turn-off specification following 40mA
  // illumination. Datasheet calls out 100k ohms in <=500ms.
  optocircuit::LdrCell vtl;vtl.prepare(fs);
  vtl.select(optocircuit::CellKind::VTL5C2);
  for(int i=0;i<int(fs);++i)vtl.processAtLedCurrent(1.,.040);
  const double lit40=vtl.resistance();
  for(int i=0;i<int(.5*fs);++i)vtl.processAtLedCurrent(1.,0.);
  const double after500=vtl.resistance();
  std::cout<<"VTL5C2_OFF lit_40mA="<<lit40
           <<" after_500ms_R="<<after500<<"\\n";
  check(after500>=100000.,10);
  optocircuit::LdrCell electrical;electrical.prepare(fs);
  electrical.select(optocircuit::CellKind::VTL5C2);
  const double vf=electrical.ledForwardVoltage(.020);
  std::cout<<"VTL5C2 typical_forward_20mA_V="<<vf<<"\\n";
  check(vf>1.60&&vf<1.70,13);
  optocircuit::LdrCell c;c.prepare(fs);
  c.select(optocircuit::CellKind::T4B);
  check(c.selected()==optocircuit::CellKind::T4B,5);
  const double dark=c.resistance();
  double peak=0,initial=0,after50=0,after1000=0,halfRecoveryMs=-1.;
  for(int i=0;i<int(1.*fs);++i) {
    c.processAtPanelVac(1.,120.*std::sqrt(2.)*
                      std::sin(2.*optocircuit::pi*1000.*i/fs));
    peak=std::max(peak,c.gainReduction());
  }
  initial=c.gainReduction();
  const double meterOn=c.meterPhotocellOhms();
  for(int i=0;i<int(4*fs);++i){
    c.processAtPanelVac(1.,0.);
    if(i==int(.05*fs))after50=c.gainReduction();
    if(halfRecoveryMs<0 && c.gainReduction()<=initial*.5)
       halfRecoveryMs=1000.*i/fs;
    if(i==int(1.*fs))after1000=c.gainReduction();
  }
  std::cout<<"T4B dark_R="<<dark<<" drive_120VAC_peak_GR="<<peak
    <<" onset_GR="<<initial<<" GR_off_50ms="<<after50
    <<" GR_off_1000ms="<<after1000
    <<" recovery_50percent_ms="<<halfRecoveryMs
    <<" meter_R_lit="<<meterOn
    <<" meter_R_dark="<<c.meterPhotocellOhms()<<"\n";
  check(initial>1 && after50<initial && after1000<after50,6);
  check(halfRecoveryMs>20. && halfRecoveryMs<150.,12);
  check(c.meterPhotocellOhms()>meterOn,7);
  optocircuit::OpticalCompressor comp;
  comp.prepare(fs);
  for(int selection=0;selection<3;++selection){
    comp.setCellKind(static_cast<optocircuit::CellKind>(selection));
    check(int(comp.selectedCellKind())==selection,8);
    comp.setThresholdDb(-18);
    const double amplitude=std::sqrt(2.)*.775*std::pow(10.,4./20.)/17.3677;
    for(int n=0;n<int(fs);++n){
       const auto input=float(amplitude*std::sin(2*optocircuit::pi*1000*n/fs));
       const float out=comp.process(input);
       check(std::isfinite(out),9);
    }
    std::cout<<"CELL_SELECTION kind="<<selection
      <<" gain_reduction_db="<<comp.reductionDb()
      <<" resistance_ohms="<<comp.ldrOhms()<<"\n";
  }
  std::cout<<"OPTO_MODEL_SWITCH=PASS\n";
}
