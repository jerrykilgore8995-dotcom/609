// Physical resistor and silicon-junction bias search for a progressive
// optical soft knee. Only passive component values vary across measurements.
#include "OpticalCell.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <iomanip>
constexpr double pi=3.14159265358979323846,fs=44100.,fsPeak=17.3677;
static double eval(double input,double threshold,double feedbackR,double kneeR) {
    optocircuit::OptoParts p;
    p.ledCompFeedbackR=feedbackR;
    p.compKneeSeriesR=kneeR;
    p.ledCompFeedbackTapR=0.;
    optocircuit::OpticalCompressor c;
    c.prepare(fs,p);c.setCellKind(optocircuit::CellKind::NSL32);
    c.setLimiter(false);c.setThresholdDb(threshold);
    c.setAttackMs(15.);c.setReleaseMs(250.);
    double sum=0.;
    constexpr int total=66150,start=55125;
    const double amplitude=.775*std::sqrt(2.)*std::pow(10.,input/20.)/fsPeak;
    for(int n=0;n<total;++n) {
        const auto y=c.process(float(amplitude*std::sin(2*pi*1000.*n/fs)));
        if(n>=start)sum+=y*y;
    }
    return 20.*std::log10(std::sqrt(sum/(total-start))*fsPeak/.775);
}
int main(){
    constexpr std::array<double,8> inputs{4.,6.,8.,10.,12.,14.,16.,20.};
    std::cout<<std::fixed<<std::setprecision(4);
    std::cout<<"feedback_ohm,knee_series_ohm,threshold_dbu,input_dbu,gr_db,interval_ratio\n";
    for(double feedback:{4300.,4500.,4700.,5000.,5200.})
    for(double knee:{22000.,33000.,47000.,68000.}) {
        double oldIn=0.,oldOut=0.;
        bool first=true;
        for(double in:inputs) {
            const double out=eval(in,-18.,feedback,knee);
            const double local=first?0.:(in-oldIn)/(out-oldOut);
            std::cout<<feedback<<","<<knee<<",-18,"
                     <<in<<","<<in-out<<","<<local<<"\n";
            oldIn=in;oldOut=out;first=false;
        }
    }
}
