#include "AnalogOptoProcessor.hpp"
#include <iostream>
#include <cmath>
#include <array>
#include <chrono>
#include <algorithm>
constexpr double pi=3.14159265358979323846;
static constexpr double fs=44100.;
static float lineLevel(double dbu,double hz,int n) {
    return float(std::sqrt(2.)*.775*std::pow(10.,dbu/20.)/
           nickel::fullScalePeakVolts*std::sin(2*pi*hz*n/fs));
}
struct State {double optical=0.,lowMid=0.,r=0.,energy=0.;};
State run(double dbu,double hz,int count,bool palm,bool hpf,
          double threshold=-18.) {
    analogopto::MonoEngine m;
    m.prepare(fs);
    m.setThresholdDb(threshold);
    m.setAttackMs(15);m.setReleaseMs(250);
    m.setPalmMute(palm);m.setPalmCenterHz(170);
    m.setPalmBandwidthOct(1.3);m.setPalmThresholdDb(-25);
    m.setScHpf(hpf);m.setScHpfHz(120);
    m.setLoadOhms(10000);
    State s{};
    for(int n=0;n<count;++n) {
        const auto input=lineLevel(dbu,hz,n);
        const auto y=m.process(input);
        if(!std::isfinite(y)) std::exit(7);
        if(n>count/2)s.energy+=double(y)*double(y);
    }
    s.optical=m.opticalGr();
    s.lowMid=m.lowMidGr();
    s.r=m.photocellResistance();
    return s;
}
int main(){
    std::cout.setf(std::ios::fixed);
    std::cout.precision(6);
    optocircuit::LdrCell cell;
    cell.prepare(fs);
    for(int i=0;i<44100;++i)cell.process(1.,0.);
    const auto dark=cell.resistance();
    for(int i=0;i<22050;++i)cell.process(1.,3.);
    const auto lit=cell.resistance();
    const auto gr=cell.gainReduction();
    if(cell.actualLedAmps()<1e-6)return 11; // current-sense network must actually receive current
    for(int i=0;i<44100;++i)cell.process(1.,0.);
    const auto decay=cell.resistance();
    std::cout<<"OPTO_PHYSICS dark="<<dark<<" lit="<<lit
             <<" recovered="<<decay<<" gr="<<gr<<"\n";
    if(!(lit<dark && decay>lit && gr>0.))return 2;

    lowmid::DynamicLowMid lm;
    lm.prepare(fs);
    lm.setEnabled(true);lm.setCenterHz(170);
    lm.setWidthOctaves(1.3);lm.setThresholdDb(-40);
    lm.setMaxCutDb(6);
    double before=0.,after=0.;
    for(int n=0;n<44100;++n){
        double v=.2*std::sin(2*pi*170*n/fs);
        double y=lm.process(float(v));
        if(n>=22050){before+=v*v;after+=y*y;}
    }
    std::cout<<"LOWMID active_cut_db="
             <<10*std::log10(after/before)
             <<" gr="<<lm.reductionDb()<<"\n";
    if(!(after<before))return 3;

    // A linked L/R detector must keep both channels on the same GR curve
    // even if their actual audio signals are different.
    analogopto::StereoEngine st;
    st.prepare(fs);
    st.setThresholdDb(-18);
    for(int n=0;n<44100;++n) {
        float l=lineLevel(8.,100.,n);
        float r=lineLevel(0.,100.,n);
        st.process(l,r);
        if(!std::isfinite(l)||!std::isfinite(r))return 8;
    }
    std::cout<<"STEREO linked_left="<<st.grLeft()
             <<" linked_right="<<st.grRight()<<"\\n";
    if(std::abs(st.grLeft()-st.grRight())>.01)return 9;

    // Dynamic band in transparent mode must reconstruct its frequency
    // magnitude rather than producing static crossover notches.
    for(double freq:{50.,100.,170.,400.,1000.,5000.}) {
        lowmid::DynamicLowMid transparent;
        transparent.prepare(fs);
        transparent.setEnabled(true);
        transparent.setMaxCutDb(0.);
        double inPower=0.,outPower=0.;
        for(int n=0;n<22050;++n) {
            const double input=.1*std::sin(2*pi*freq*n/fs);
            const double output=transparent.process(float(input));
            if(n>11025){inPower+=input*input;outPower+=output*output;}
        }
        const double err=10*std::log10(outPower/inPower);
        std::cout<<"CROSSOVER freq="<<freq<<" magnitude_error_db="<<err<<"\\n";
        if(!std::isfinite(err)||std::abs(err)>.5)return 10;
    }
    const auto quiet=run(-18.,100.,44100,false,false);
    const auto hot=run(4.,100.,44100,false,false);
    const auto hotWithHpf=run(4.,100.,44100,false,true);
    const auto palm=run(4.,170.,44100,true,false);
    const auto maxLevel=run(16.,100.,44100,false,false);
    std::cout<<"CHAIN quiet optical="<<quiet.optical<<" cell="<<quiet.r<<"\n";
    std::cout<<"CHAIN hot optical="<<hot.optical<<" cell="<<hot.r<<"\n";
    std::cout<<"CHAIN sidechain_120Hz optical="<<hotWithHpf.optical
             <<" cell="<<hotWithHpf.r<<"\n";
    std::cout<<"CHAIN palm_170Hz optical="<<palm.optical
             <<" lowmid="<<palm.lowMid<<"\n";
    if(!(quiet.r>hot.r && hot.optical>quiet.optical))return 4;
    if(!(hotWithHpf.optical<hot.optical))return 5;
    if(!(palm.lowMid>0.2))return 6;
    analogopto::MonoEngine bm;bm.prepare(fs);
    bm.setThresholdDb(-18);bm.setPalmMute(true);
    const auto m0=std::chrono::steady_clock::now();
    float sink=0.;
    for(int n=0;n<44100;++n)
        sink=bm.process(lineLevel(4.,170.,n));
    const auto m1=std::chrono::steady_clock::now();
    analogopto::StereoEngine bs;bs.prepare(fs);
    bs.setThresholdDb(-18);bs.setPalmMute(true);
    for(int n=0;n<44100;++n) {
        float l=lineLevel(4.,100.,n),r=lineLevel(4.,170.,n);
        bs.process(l,r);sink+=0.00001f*(l+r);
    }
    const auto m2=std::chrono::steady_clock::now();
    const double monoMs=std::chrono::duration<double>(m1-m0).count()*1000.*512./44100.;
    const double stereoMs=std::chrono::duration<double>(m2-m1).count()*1000.*512./44100.;
    std::cout<<"MONO_FULL_CHAIN_MS_512="<<monoMs
             <<" STEREO_FULL_CHAIN_MS_512="<<stereoMs
             <<" sink="<<sink<<std::endl;
    std::cout<<"ANALOG_OPTO_PIPELINE=PASS\n";
    return 0;
}
