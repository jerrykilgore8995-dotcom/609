// Intermodulation fingerprint: 60Hz bass plus 1kHz guitar tone.
// Coherent 0.5-second DFT at 44.1kHz. Each source tone carries
// total target RMS minus 3.0103dB, so the combined RMS is target dBu.
// It measures genuine 940/1060Hz (fH +/- 60Hz) sidebands caused by
// nonlinear circuit interactions rather than synthetic harmonics.
#include "AnalogOptoProcessor.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <array>
constexpr double sr=44100.,pi=3.14159265358979323846;
constexpr int N=22050;
struct Dft {
  double sine=0,cosine=0;
  double abs() const{return 2.*std::hypot(sine,cosine)/N;}
};
int main(){
 std::cout<<std::fixed<<std::setprecision(6);
 std::cout<<"load_ohm,combined_level_dbu,optical,GR_dB,"
     "L60_dBFS,H1000_dBFS,SB940_dBc,SB1060_dBc,SB880_dBc,SB1120_dBc\n";
 for(double load:{10000.,600.})
 for(double level:{4.,12.})
 for(bool optical:{false,true}){
   analogopto::MonoEngine engine;engine.prepare(sr);
   engine.setLoadOhms(load);engine.setPalmMute(false);
   engine.setThresholdDb(optical?-18.:18.);
   engine.setAttackMs(15.);engine.setReleaseMs(250.);
   const double eachDbu=level-3.01029995664;
   const double amplitude=std::sqrt(2.)*.775*
                 std::pow(10.,eachDbu/20.)/nickel::fullScalePeakVolts;
   constexpr std::array<int,6> bands{60,1000,940,1060,880,1120};
   std::array<Dft,6> dft{};
   for(int i=0;i<2*N;++i){
      const double t=double(i)/sr;
      const float input=float(amplitude*(std::sin(2*pi*60*t)+
                                     std::sin(2*pi*1000*t)));
      const double out=engine.process(input);
      if(!std::isfinite(out))return 2;
      if(i<N)continue;
      for(size_t b=0;b<bands.size();++b){
        const double a=2*pi*bands[b]*(i-N)/sr;
        dft[b].sine+=out*std::sin(a);
        dft[b].cosine+=out*std::cos(a);
      }
   }
   const double ref=dft[1].abs();
   auto dB=[](double x){return 20*std::log10(std::max(1e-15,x));};
   std::cout<<load<<","<<level<<","<<int(optical)<<","
        <<engine.opticalGr()<<","<<dB(dft[0].abs())<<","
        <<dB(ref);
   for(int j=2;j<6;++j) std::cout<<","<<dB(dft[j].abs()/
                                                   std::max(1e-15,ref));
   std::cout<<"\n";
   if(ref<=1e-9)return 3;
 }
 return 0;
}
