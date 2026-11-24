#include "OpticalCell.hpp"
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
constexpr double fs=44100., pi=3.14159265358979323846, voltsPerPeak=17.3677;
static double measure(double inputDbu,double threshold) {
    optocircuit::OpticalCompressor compressor;
    compressor.prepare(fs);
    compressor.setCellKind(optocircuit::CellKind::NSL32);
    compressor.setThresholdDb(threshold);
    compressor.setAttackMs(15.);
    compressor.setReleaseMs(250.);
    compressor.setFeedforward(false);
    compressor.setSidechainHpF(false);
    const double peak=std::sqrt(2.)*.775*std::pow(10.,inputDbu/20.)/voltsPerPeak;
    double sum=0.;
    constexpr int count=66150, start=55125;
    for(int i=0;i<count;++i) {
        const float in=float(peak*std::sin(2.*pi*1000.*i/fs));
        const double out=compressor.process(in);
        if(i>=start)sum+=out*out;
    }
    return 20.*std::log10(std::sqrt(sum/(count-start))*voltsPerPeak/.775);
}
int main() {
    const std::array<double,10> levels{-24.,-18.,-12.,-6.,0.,4.,8.,12.,16.,20.};
    const std::array<double,7> thresholds{-36.,-30.,-24.,-18.,-12.,-6.,0.};
    std::cout<<std::fixed<<std::setprecision(5);
    std::cout<<"threshold_dbu,input_dbu,output_dbu,gain_reduction_db,interval_ratio\n";
    for(double threshold:thresholds) {
        double prevIn=0.,prevOut=0.;
        bool first=true;
        for(double input:levels) {
            const double out=measure(input,threshold);
            const double interval=first?0.:(input-prevIn)/(out-prevOut);
            std::cout<<threshold<<","<<input<<","<<out<<","
                     <<input-out<<","<<(first?0.:interval)<<"\n";
            prevIn=input;prevOut=out;first=false;
        }
    }
}
