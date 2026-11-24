// Full analog path: 1kHz with INPUT at 0 dB, no low-mid processing,
// no makeup. Unlike the isolated optical bench, this includes the nickel
// transformer, optical stage, and the discrete transistor output.
#include "AnalogOptoProcessor.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
constexpr double fs=44100.,pi=3.14159265358979323846;
static void measure(int cell,double input,double& output,double& gr) {
  analogopto::MonoEngine m;m.prepare(fs);m.setOptoCell(cell);
  m.setInputDb(0.);m.setThresholdDb(-18.);
  m.setLimiter(false);m.setAttackMs(15.);m.setReleaseMs(250.);
  m.setPalmMute(false);m.setMakeupDb(0.);m.setOutputTrimDb(0.);
  m.setLoadOhms(10000.);
  constexpr int count=66150,start=55125;
  const double peak=std::sqrt(2.)*.775*std::pow(10.,input/20.)/
                    nickel::fullScalePeakVolts;
  double sum=0.;
  for(int i=0;i<count;++i){
    const double in=peak*std::sin(2.*pi*1000.*i/fs);
    const double out=m.process(float(in));
    if(i>=start)sum+=out*out;
  }
  output=20.*std::log10(std::sqrt(sum/(count-start))*
                        nickel::fullScalePeakVolts/.775);
  gr=m.opticalGr();
}
int main() {
  std::cout<<std::fixed<<std::setprecision(5);
  std::cout<<"cell,drive_db,threshold_dbu,input_dbu,output_dbu,opto_gr_db,local_ratio\n";
  for(int cell=0;cell<3;++cell) {
    bool first=true;double previousIn=0.,previousOut=0.;
    for(double input:{-4.,0.,2.,4.,6.,8.,10.,12.,16.,20.}){
      double out=0.,gr=0.;measure(cell,input,out,gr);
      const double ratio=first?0.:(input-previousIn)/(out-previousOut);
      std::cout<<(cell==0?"NSL32":cell==1?"VTL5C2":"T4B")
        <<",0,-18,"<<input<<","<<out<<","<<gr<<","<<ratio<<"\n";
      previousIn=input;previousOut=out;first=false;
    }
  }
}
