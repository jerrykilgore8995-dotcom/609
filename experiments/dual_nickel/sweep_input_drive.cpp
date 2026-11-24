// Control-position sweep of N49 input transformer, standalone.
// Input tone is fixed at +4 dBu; drive is varied at the actual gain control.
// Uses coherent integer-Hz sine, 1s warmup + 1s capture at 44.1k.
// THD uses only harmonics strictly below Nyquist; above-band K are NA.
// This deliberately measures the INPUT transformer alone, before the limiter,
// makeup or output stage. Absolute core parameters remain experimental.
#include "NickelTransformer.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <limits>
#include <vector>
#include <chrono>

constexpr double pi=3.1415926535897932384626433832795;
constexpr int fs=44100;
constexpr int captureN=fs;
constexpr std::array<int,10> positions{-18,-14,-12,-8,-4,0,4,8,12,18};
constexpr std::array<int,5> frequencies{50,100,1000,5000,10000};
struct Result {
    double fundamental=0, thd=0, phase=0, peak=0, gainDb=0, rms=0;
    std::array<double,21> harmonics{};
};
static double sourcePeak() {
    return std::sqrt(2.0)*0.775*std::pow(10.0,4.0/20.0)
           /nickel::fullScalePeakVolts;
}
static Result measure(int freq,int drive) {
    nickel::DualTransformerGain stage;
    stage.prepare(fs);
    stage.setInputDriveDb(drive);
    stage.setInterstageVolumeDb(0);
    stage.setMakeupDb(0);
    stage.setOutputTrimDb(0);
    stage.setLinked(true);
    std::vector<double> y(captureN);
    const double amplitude=sourcePeak();
    double energy=0;
    Result r;
    for(int n=0;n<2*captureN;++n) {
        const double x=amplitude*std::sin(2*pi*freq*n/fs);
        const double v=stage.processInput(float(x));
        if(!std::isfinite(v)) {std::cerr<<"non-finite output\n"; std::exit(3);}
        if(n>=captureN) {
            y[n-captureN]=v;
            r.peak=std::max(r.peak,std::abs(v));
            energy+=v*v;
        }
    }
    r.rms=std::sqrt(energy/captureN);
    double firstSin=0,firstCos=0;
    for(int k=1;k<=20;++k) {
        if(k*freq>=fs/2.0) {
            r.harmonics[k]=std::numeric_limits<double>::quiet_NaN();
            continue;
        }
        double sine=0,cosine=0;
        const double omega=2*pi*k*freq/fs;
        for(int n=0;n<captureN;++n) {
            sine+=y[n]*std::sin(omega*n);
            cosine+=y[n]*std::cos(omega*n);
        }
        r.harmonics[k]=2.0*std::hypot(sine,cosine)/captureN;
        if(k==1) {firstSin=sine;firstCos=cosine;}
    }
    r.fundamental=r.harmonics[1];
    if(r.fundamental<1e-12 || !std::isfinite(r.fundamental)) std::exit(4);
    double harmonicPower=0;
    for(int k=2;k<=20;++k)
        if(std::isfinite(r.harmonics[k]))
            harmonicPower+=r.harmonics[k]*r.harmonics[k];
    r.thd=std::sqrt(harmonicPower)/r.fundamental;
    r.phase=180.0/pi*std::atan2(firstCos,firstSin);
    const double transformerInputPeak=amplitude*nickel::dbGain(drive);
    r.gainDb=20.0*std::log10(r.fundamental/transformerInputPeak);
    return r;
}
int main() {
    static_assert(positions.front()==-18 && positions.back()==18);
    // Verify the actual input control does not clamp the -18 setting to -12.
    nickel::DualTransformerGain stage;
    stage.prepare(fs);
    stage.setInputDriveDb(-18);
    std::cout<<std::fixed<<std::setprecision(8);
    std::cout<<"drive_db,transformer_in_dbu,frequency_hz,thd_percent,"
             <<"fundamental_gain_db,phase_deg,peak_norm,rms_norm";
    for(int k=2;k<=20;++k) std::cout<<",K"<<k<<"_dBc";
    std::cout<<"\n";
    const auto start=std::chrono::steady_clock::now();
    for(const int drive:positions) {
        for(const int freq:frequencies) {
            const Result r=measure(freq,drive);
            if(!std::isfinite(r.thd)||!std::isfinite(r.gainDb)
               ||r.peak>5.0) return 5;
            std::cout<<drive<<","<<4+drive<<","<<freq<<","
                     <<100*r.thd<<","<<r.gainDb<<","<<r.phase<<","
                     <<r.peak<<","<<r.rms;
            for(int k=2;k<=20;++k) {
                if(std::isfinite(r.harmonics[k]))
                    std::cout<<","<<20.0*std::log10(
                        std::max(1.e-16,r.harmonics[k]/r.fundamental));
                else std::cout<<",NA";
            }
            std::cout<<"\n";
        }
    }
    const double seconds=std::chrono::duration<double>(
        std::chrono::steady_clock::now()-start).count();
    std::cerr<<"Sweep completed: 50 points, 1s warmup + 1s capture, "
             <<seconds<<" seconds CPU walltime on this runner\n";
    return 0;
}
