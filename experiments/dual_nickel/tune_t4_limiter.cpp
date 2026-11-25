// Physical pre/post detector summing resistor calibration for the EL T4B limiter.
#include "OpticalCell.hpp"
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
constexpr double pi=3.14159265358979323846,fs=44100.,fullScalePeak=17.3677;
double measure(double dbu,double threshold,double fraction) {
    optocircuit::OptoParts p;
    p.elLimitFeedbackTapR=100000.*fraction;
    p.elLimitInputTapR=100000.*(1.-fraction);
    optocircuit::OpticalCompressor c;
    c.prepare(fs,p);c.setCellKind(optocircuit::CellKind::T4B);
    c.setLimiter(true);c.setThresholdDb(threshold);c.setFeedforward(false);
    const double amplitude=std::sqrt(2.)*.775*std::pow(10.,dbu/20.)/fullScalePeak;
    double energy=0.;
    constexpr int samples=66150,start=55125;
    for(int n=0;n<samples;++n) {
        const float y=c.process(float(amplitude*std::sin(2.*pi*1000.*n/fs)));
        if(n>=start)energy+=double(y)*y;
    }
    return 20*std::log10(std::sqrt(energy/(samples-start))*fullScalePeak/.775);
}
int main(){
    std::cout<<std::fixed<<std::setprecision(5);
    std::cout<<"pre_fraction,threshold_dbu,input_dbu,gr_db,interval_ratio\n";
    for(double f:{0.,.025,.05,.075,.10,.125,.15,.20,.25,.3125})
    for(double threshold:{-18.,0.}) {
        double lastInput=0,lastOutput=0;bool first=true;
        for(double input:{0.,2.,4.,6.,8.,10.,12.,14.,16.,18.,20.}){
            const double out=measure(input,threshold,f);
            const double ratio=first?0.:(input-lastInput)/(out-lastOutput);
            std::cout<<f<<","<<threshold<<","<<input<<","
                <<input-out<<","<<ratio<<"\n";
            first=false;lastInput=input;lastOutput=out;
        }
    }
}
