#include "NickelTransformer.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
int main() {
    nickel::DualTransformerGain d;
    d.prepare(44100);
    d.setInputDriveDb(12.0);
    d.setInterstageVolumeDb(0.0);
    d.setMakeupDb(3.0);
    d.setLinked(true);
    double peak=0;
    for(int i=0;i<44100;++i) {
        float x=0.1f*std::sin(6.283185307179586*100.0*i/44100.0);
        float y=d.processOutput(d.processInput(x));
        assert(std::isfinite(y));
        peak=std::max(peak,std::abs(double(y)));
    }
    std::cout<<"100 Hz finite smoke output peak: "<<peak<<"\n";
}
