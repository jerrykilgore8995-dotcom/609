// Optical component verification in isolation (not compressor timing).
// Datasheet: NSL-32 conductance reaches 63% at 16mA in 3.5ms,
// reaches >=100kohm ~500ms after removing 16mA illumination.
// VTL5C2 3.5ms to 63% at 40mA, decay to 100kohm <=500ms.
// Dark resistance at 10s: NSL >=500k, VTL >=1M.
#include "OpticalCell.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
using namespace optocircuit;
constexpr double fs=44100.;
struct Result{double rise63=-1.,rise90=-1.,off100k=-1.,off500k=-1.;
 double dark10s=0.,Ron=0.,R100=0.,R500=0.,R1000=0.,R2000=0.;};
Result run(int kind,double mA,double onMs,OptoParts parts={}){
 LdrCell c;c.prepare(fs,parts);c.select(static_cast<CellKind>(kind));
 const double darkG=1./c.darkResistance();
 const int steps=int(fs*onMs/1000.);
 const double current=mA/1000.;
 // Reinitialize for each point; compare conductance at the SAME current
 // after the 2-second continuous reference illumination.
 LdrCell reference;reference.prepare(fs,parts);
 reference.select(static_cast<CellKind>(kind));
 for(int n=0;n<int(fs*2);++n)reference.processAtLedCurrent(1.,current);
 const double fullG=1./reference.resistance();
 Result o;
 for(int n=0;n<steps;++n) {
  c.processAtLedCurrent(1.,current);
  const double fraction=(1./c.resistance()-darkG)/(fullG-darkG);
  if(o.rise63<0 && fraction>=.632)o.rise63=n*1000./fs;
  if(o.rise90<0 && fraction>=.9)o.rise90=n*1000./fs;
 }
 o.Ron=c.resistance();
 const int rest=int(fs*10.);
 for(int n=0;n<rest;++n){
   c.processAtLedCurrent(1.,0.);
   const double R=c.resistance(),ms=n*1000./fs;
   if(o.off100k<0 && R>=100000.)o.off100k=ms;
   if(o.off500k<0 && R>=500000.)o.off500k=ms;
   if(n==int(fs*.1))o.R100=R;
   if(n==int(fs*.5))o.R500=R;
   if(n==int(fs*1.))o.R1000=R;
   if(n==int(fs*2.))o.R2000=R;
 }
 o.dark10s=c.resistance();
 return o;
}
int main(){
 std::cout<<std::fixed<<std::setprecision(4);
 std::cout<<"cell,LED_mA,on_ms,rise63_ms,rise90_ms,Ron_ohm,"
 "release_to_100k_ms,release_to_500k_ms,R100_ohm,R500_ohm,"
 "R1000_ohm,R2000_ohm,R10s_ohm\n";
 for(int kind=0;kind<2;++kind)
 for(double current:{1.,10.,16.,20.,40.})
 for(double exposure:{30.,150.,1000.}){
   auto v=run(kind,current,exposure);
   std::cout<<(kind==0?"NSL32":"VTL5C2")<<","<<current<<","
    <<exposure<<","<<v.rise63<<","<<v.rise90<<","<<v.Ron
    <<","<<v.off100k<<","<<v.off500k<<","<<v.R100
    <<","<<v.R500<<","<<v.R1000<<","<<v.R2000
    <<","<<v.dark10s<<"\n";
 }
}
