// Full dual-transformer sweep: +4 dBu source, variable N49 input drive and
// variable interstage level into high-nickel output transformer.
// The zener limiter is NOT connected in this experiment.
// Linked -inputDrive trim is applied only after the output transformer.
// Prints input-alone versus complete-chain K2..K20, THD, gain, phase, peaks.
#include "NickelTransformer.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>
#include <chrono>
constexpr double pi=3.1415926535897932384626433832795;
constexpr int sampleRate=44100;
constexpr int points=sampleRate;
constexpr std::array<int,10> inputDrives{-18,-14,-12,-8,-4,0,4,8,12,18};
// Seven representative output drives, including the full +24 dB end-stop.
// This is the interstage volume, not the separate makeup control.
constexpr std::array<int,7> outputDrives{-12,0,4,8,12,18,24};
constexpr std::array<int,5> frequencies{50,100,1000,5000,10000};
struct Metrics {
    double thd=0,peak=0,level=0,gain=0,phase=0,dc=0;
    std::array<double,21> harmonic{};
};
Metrics analyze(const std::vector<double>& y,int frequency,double expectedPeak) {
    Metrics r;
    double energy=0;
    for(double x:y) {r.peak=std::max(r.peak,std::abs(x));energy+=x*x;r.dc+=x;}
    r.level=std::sqrt(energy/y.size());
    r.dc/=y.size();
    double sine=0,cosine=0;
    for(int h=1;h<=20;++h) {
        if(h*frequency>=sampleRate/2.0) {
            r.harmonic[h]=std::numeric_limits<double>::quiet_NaN();
            continue;
        }
        double sn=0,cs=0;
        const double omega=2*pi*h*frequency/sampleRate;
        for(int n=0;n<points;++n) {
            sn+=y[n]*std::sin(omega*n);
            cs+=y[n]*std::cos(omega*n);
        }
        r.harmonic[h]=2*std::hypot(sn,cs)/points;
        if(h==1) {sine=sn;cosine=cs;}
    }
    const double fundamental=r.harmonic[1];
    double power=0;
    for(int h=2;h<=20;++h)
        if(std::isfinite(r.harmonic[h])) power+=r.harmonic[h]*r.harmonic[h];
    r.thd=fundamental>1e-14?100*std::sqrt(power)/fundamental:0;
    r.gain=20*std::log10(std::max(1e-15,fundamental)/expectedPeak);
    r.phase=std::atan2(cosine,sine)*180/pi;
    return r;
}
static void outputHarmonics(const Metrics& r) {
    for(int h=2;h<=20;++h) {
        if(std::isfinite(r.harmonic[h]))
            std::cout<<","<<20*std::log10(std::max(1e-16,r.harmonic[h]/r.harmonic[1]));
        else std::cout<<",NA";
    }
}
int main() {
    constexpr double sourceDbu=4;
    const double sourcePeak=std::sqrt(2.0)*0.775*std::pow(10.,sourceDbu/20.)
                             /nickel::fullScalePeakVolts;
    std::cout<<std::fixed<<std::setprecision(7);
    std::cout<<"input_drive_db,interstage_db,source_dbu,input_transformer_dbu,"
                "nominal_output_transformer_drive_dbu,hz,input_thd_percent,"
                "chain_thd_percent,chain_minus_input_thd_pct_point,"
                "input_gain_db,chain_gain_db,input_peak,chain_peak,"
                "input_rms,chain_rms,input_phase_deg,chain_phase_deg,chain_dc";
    for(int k=2;k<=20;++k)std::cout<<",input_K"<<k<<"_dBc";
    for(int k=2;k<=20;++k)std::cout<<",chain_K"<<k<<"_dBc";
    std::cout<<"\n";
    const auto started=std::chrono::steady_clock::now();
    int count=0;
    for(int inDrive:inputDrives) for(int outDrive:outputDrives) for(int hz:frequencies) {
        nickel::DualTransformerGain processor;
        processor.prepare(sampleRate);
        processor.setInputDriveDb(inDrive);
        processor.setInterstageVolumeDb(outDrive);
        processor.setMakeupDb(0);
        processor.setOutputTrimDb(0);
        processor.setLinked(true);
        std::vector<double> input(points),chain(points);
        for(int n=0;n<2*points;++n) {
            const float x=float(sourcePeak*std::sin(2*pi*hz*n/sampleRate));
            const float afterInput=processor.processInput(x);
            const float afterBoth=processor.processOutput(afterInput);
            if(!std::isfinite(afterInput)||!std::isfinite(afterBoth)) {
                std::cerr<<"FAIL non-finite inDrive="<<inDrive<<" outDrive="
                         <<outDrive<<" hz="<<hz<<" n="<<n<<"\n";
                return 2;
            }
            if(n>=points) {
                input[n-points]=afterInput;
                chain[n-points]=afterBoth;
            }
        }
        const double inputExpected=sourcePeak*nickel::dbGain(inDrive);
        // The *post-transformer* gain trim is linked to -inputDrive.
        const double chainExpected=sourcePeak*nickel::dbGain(outDrive);
        const Metrics a=analyze(input,hz,inputExpected);
        const Metrics b=analyze(chain,hz,chainExpected);
        if(!std::isfinite(a.thd)||!std::isfinite(b.thd)
           ||!std::isfinite(a.gain)||!std::isfinite(b.gain))return 3;
        std::cout<<inDrive<<","<<outDrive<<","<<sourceDbu<<","
                 <<sourceDbu+inDrive<<","<<sourceDbu+inDrive+outDrive
                 <<","<<hz<<","<<a.thd<<","<<b.thd<<","
                 <<b.thd-a.thd<<","<<a.gain<<","<<b.gain<<","
                 <<a.peak<<","<<b.peak<<","<<a.level<<","<<b.level
                 <<","<<a.phase<<","<<b.phase<<","<<b.dc;
        outputHarmonics(a);
        outputHarmonics(b);
        std::cout<<"\n";
        ++count;
    }
    const auto elapsed=std::chrono::duration<double>(
        std::chrono::steady_clock::now()-started).count();
    std::cerr<<"dual_transformer_sweep points="<<count
             <<" elapsed_seconds="<<elapsed<<"\n";
    return count==350?0:4;
}
