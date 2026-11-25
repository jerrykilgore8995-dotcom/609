// Stage diagnostic at pathological high-drive operating points.
// Instrumentation only, does not alter the DSP.
#include "AnalogOptoProcessor.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
using analogopto::MonoEngine;
constexpr double pi=3.14159265358979323846,fs=44100.,pk=nickel::fullScalePeakVolts;
int main(){
 std::cout<<std::fixed<<std::setprecision(7);
 std::cout<<"cell,drive_dB,input_dBu,hz,threshold_dBu,magnetic_rms,post_cell_rms,output_rms,post_cell_gain_dB,output_stage_gain_dB,opto_gr_dB,driver_solver_failures\n";
 for(int cell:{0,1,2,3})for(double drive:{0.,12.,18.})
 for(double level:{4.,12.})for(double freq:{1000.,5000.,10000.}){
   MonoEngine m;m.prepare(fs);m.setOptoCell(cell);
   m.setThresholdDb(-18.);m.setInputDb(drive);
   m.setLinkedAutoGain(true);m.setLoadOhms(10000.);
   m.setMakeupDb(0.);m.setOutputTrimDb(0.);m.setPalmMute(false);
   const double amp=std::sqrt(2.)*.775*std::pow(10.,level/20.)/pk;
   double mag2=0.,after2=0.,out2=0.;
   for(int i=0;i<44100;++i){
     const float input=float(amp*std::sin(2*pi*freq*i/fs));
     const float prepared=m.processBeforeOpto(input);
     const float out=m.processFromOpto(prepared,prepared,false);
     if(i<22050)continue;
     const double opto=m.lastOpticalOutput();
     mag2+=double(prepared)*prepared;
     after2+=opto*opto;
     out2+=double(out)*out;
   }
   const double mag=std::sqrt(mag2/22050),after=std::sqrt(after2/22050),
                output=std::sqrt(out2/22050);
   const double cellgain=20.*std::log10(std::max(1e-12,after)/std::max(1e-12,mag));
   const double drivergain=20.*std::log10(std::max(1e-12,output)*std::pow(10.,drive/20.)/std::max(1e-12,after));
   std::cout<<cell<<","<<drive<<","<<level<<","<<freq<<",-18,"
       <<mag<<","<<after<<","<<output<<","<<cellgain<<","<<drivergain
       <<","<<m.opticalGr()<<","<<m.outputSolverFailures()<<"\n";
   if(m.outputSolverFailures()!=0) {
       std::cerr<<"OUTPUT_SOLVER_FAILED cell="<<cell
                <<" drive="<<drive<<" level="<<level
                <<" hz="<<freq<<" count="<<m.outputSolverFailures()<<"\n";
       return 5;
   }
 }
}
