// Circuit-part-value study, no digital ratio correction and no change
// to photocell models.  Candidate parts go to OpticalCompressor::prepare().
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
    const std::array<std::array<double,2>,10> combos{{{17000,6800},{12000,10000},
      {10000,15000},{8200,22000},{12000,33000},{10000,33000},
      {15000,33000},{17000,22000},{8200,6800},{6800,6800}}};
    const std::array<double,9> inputs{0,2,4,6,8,10,12,16,20};
    std::cout<<std::fixed<<std::setprecision(4);
    std::cout<<"feedback_ohm,knee_series_ohm,threshold_dbu,input_dbu,gr_db,interval_ratio\n";
    for(auto p:combos)for(double t:{-18.,-6.,0.}){
        double oldIn=0.,oldOut=0.;
        bool first=true;
        for(double in:inputs){
            const double out=eval(in,t,p[0],p[1]);
            const double local=first?0.:(in-oldIn)/(out-oldOut);
            std::cout<<p[0]<<","<<p[1]<<","<<t<<","<<in<<","
                      <<in-out<<","<<local<<"\n";
            oldIn=in;oldOut=out;first=false;
        }
    }
}
