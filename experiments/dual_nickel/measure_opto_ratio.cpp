#include "OpticalCell.hpp"
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
constexpr double fs=44100.,pi=3.14159265358979323846,fullScalePeak=17.3677;
static double measure(double inputDbu,double threshold,bool limiter,optocircuit::CellKind kind) {
    optocircuit::OpticalCompressor c;
    c.prepare(fs);c.setCellKind(kind);c.setLimiter(limiter);
    c.setThresholdDb(threshold);c.setAttackMs(15.);c.setReleaseMs(250.);
    c.setFeedforward(false);c.setSidechainHpF(false);
    const double peak=std::sqrt(2.)*.775*std::pow(10.,inputDbu/20.)/fullScalePeak;
    constexpr int count=66150,start=55125;
    double rms=0.;
    for(int n=0;n<count;++n) {
        const auto y=c.process(float(peak*std::sin(2.*pi*1000.*n/fs)));
        if(n>=start)rms+=double(y)*y;
    }
    const double v=std::sqrt(rms/(count-start))*fullScalePeak;
    return 20.*std::log10(std::max(v,1e-12)/.775);
}
int main() {
    const std::array<double,16> levels{-18.,-12.,-6.,-2.,0.,2.,4.,6.,8.,10.,12.,14.,16.,18.,20.,24.};
    const std::array<double,5> thresholds{-30.,-18.,-12.,-6.,0.};
    std::cout<<std::fixed<<std::setprecision(5);
    std::cout<<"cell,mode,threshold_dbu,input_dbu,output_dbu,gain_reduction_db,interval_ratio\n";
    // Both modes, independently initialized from silence for every point.
    for(int kind=0;kind<3;++kind)
      for(int mode=0;mode<2;++mode)
        for(const double threshold:thresholds) {
            double previousIn=0.,previousOut=0.;
            bool first=true;
            for(const double input:levels) {
                const double out=measure(input,threshold,mode!=0,
                                static_cast<optocircuit::CellKind>(kind));
                const double delta=out-previousOut;
                const double ratio=first?0.:
                    (std::abs(delta)<1e-6?std::numeric_limits<double>::infinity():
                                       (input-previousIn)/delta);
                std::cout<<(kind==0?"NSL32":kind==1?"VTL5C2":"T4B")<<","
                         <<(mode==0?"Compressor":"Limiter")<<","
                         <<threshold<<","<<input<<","<<out<<","<<input-out<<","
                         <<ratio<<"\n";
                previousIn=input;previousOut=out;first=false;
            }
        }
}
