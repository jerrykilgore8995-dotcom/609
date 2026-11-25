#include "OpticalCell.hpp"
#include <cmath>
#include <iostream>
#include <iomanip>
using namespace optocircuit;
constexpr double pk=17.3677;
int main(){
 std::cout<<std::fixed<<std::setprecision(6);
 std::cout<<"cell,fs,mode,level_dbu,hz,gr_db,detector_v,led_v,led_mA\n";
 for(int cell=0;cell<4;++cell)
 for(double sr:{44100.,96000.})
 for(int mode=0;mode<2;++mode)
 for(double level:{4.,12.})
 for(double hz:{1000.,5000.,10000.,15000.,20000.}){
   OpticalCompressor comp;comp.prepare(sr);
   comp.setCellKind(static_cast<CellKind>(cell));
   comp.setLimiter(false);comp.setFeedforward(mode==1);
   comp.setThresholdDb(-18.);comp.setAttackMs(15.);comp.setReleaseMs(250.);
   const int count=int(sr*.8);
   for(int i=0;i<count;++i) {
     const double x=std::sqrt(2.)*.775*dB(level)/pk*
              std::sin(2*pi*hz*i/sr);
     comp.process(float(x));
   }
   std::cout<<cell<<","<<sr<<","<<(mode?"FF":"FB")<<","
            <<level<<","<<hz<<","<<comp.reductionDb()<<","
            <<comp.detectorVolts()<<","<<comp.ledDriveVolts()<<","
            <<1000.*comp.ledAmps()<<"\n";
 }
}