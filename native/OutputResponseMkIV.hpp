#pragma once
#include "Controls.hpp"
#include <array>
#include <algorithm>
#include <cmath>
namespace mkii::native {
// MK IV: wet output only. MK II circuit, solver, detector and GR are unchanged.
// Two stable high-shelf sections fitted to the actual packaged MK II DLL's
// 44.1 kHz response, separately for the measured operating-point envelopes.
class OutputResponseMkIV {
  struct Section {
    double b0,b1,b2,a1,a2,z1=0,z2=0;
    double process(double x) {
      const double y=b0*x+z1;
      z1=b1*x-a1*y+z2;
      z2=b2*x-a2*y;
      return y;
    }
  };
  static constexpr double coefficients[5][2][5]={
    {{1.33656827871,-0.20605526141,0.23493870583,0.18764740244,0.17780432068},{1.19292038491,0.78527398193,0.29732593045,0.94234942013,0.33317087717}},
    {{1.19226985677,0.42989061848,0.23207440186,0.61499273293,0.23924214418},{1.07186627926,1.13240883805,0.40333484085,1.17872164199,0.42888831618}},
    {{1.17415838654,0.01650566566,0.20149474768,0.21258560459,0.17957319529},{1.02801992568,1.11858954698,0.40015270597,1.13671813844,0.41004404019}},
    {{1.1859263923,0.16467988565,0.20751772458,0.36314230329,0.19498169925},{1.04721374946,1.0778312929,0.38274059767,1.10951241357,0.39827322646}},
    {{1.01627640464,1.08250682657,0.38598847226,1.09333717533,0.39143452813},{0.99329900848,1.49966717391,0.6041567767,1.49694626245,0.60017669664}}
  };
  std::array<std::array<Section,2>,5> paths{};
  double squared=0,follow=0;
public:
  void prepare(double rate) {
    squared=0;
    follow=1-std::exp(-1/(std::max(1.,rate)*.035));
    for(int i=0;i<5;++i) for(int j=0;j<2;++j) {
      const auto& p=coefficients[i][j];
      paths[i][j]={p[0],p[1],p[2],p[3],p[4]};
    }
  }
  double process(double input,double wet,const Controls& c,int lane) {
    squared+=follow*(input*input-squared);
    const double dbu=24+20*std::log10(std::max(1.e-8,std::sqrt(2*squared)));
    double y[5];
    for(int i=0;i<5;++i) {
      double v=wet;
      for(auto& section:paths[i]) v=section.process(v);
      y[i]=v;
    }
    const int base=lane*9;
    const bool comp=c[18] && c[20] && c[base+6];
    const bool limit=c[18] && c[20] && c[base+7];
    if(comp) {
      if(dbu<10.)return std::lerp(y[1],y[2],std::clamp((dbu-4.)/6.,0.,1.));
      return std::lerp(y[2],y[3],std::clamp((dbu-10.)/10.,0.,1.));
    }
    if(limit) {
      const double threshold=4.+.5*std::clamp(c[base+4]-1,0,22);
      const double blend=std::clamp((dbu-threshold)/std::max(2.,20.-threshold),0.,1.);
      return std::lerp(y[0],y[4],blend);
    }
    return y[0];
  }
};
}