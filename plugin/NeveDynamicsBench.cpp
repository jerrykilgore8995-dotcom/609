#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <complex>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <iomanip>
#include <algorithm>
using namespace juce;
constexpr int sr=44100, bs=512;
constexpr double pi=3.14159265358979323846;
void set(AudioProcessor& p,String name,float v){for(auto* a:p.getParameters())if(a->getName(100)==name){a->setValueNotifyingHost(v);return;}throw std::runtime_error(name.toStdString());}
void config(AudioProcessor& p,bool c=false,bool l=false,int ct=0,int ratio=0,int gain=0,int cr=0,int lt=8,int lr=0,bool fast=false,bool noise=false){
 set(p,"Power",1);set(p,"Bypass In",1);set(p,"External Control",0);set(p,"Stereo Link",0);set(p,"PSU Noise",noise);
 for(int ch=1;ch<=2;ch++){String s="Channel "+String(ch)+" ";set(p,s+"Compress In",c);set(p,s+"Limit In",l);set(p,s+"Compress Threshold",ct/15.f);set(p,s+"Compress Ratio",ratio/4.f);set(p,s+"Compress Gain",gain/10.f);set(p,s+"Compress Recovery",cr/5.f);set(p,s+"Limit Threshold",lt/22.f);set(p,s+"Limit Recovery",lr/5.f);set(p,s+"Fast Attack",fast);}
}
double db(double x){return 20*log10(std::max(x,1e-30));}
std::vector<double> render(AudioProcessor& p,double f,double before,double after,int seconds=4,int step=-1){
 int n=sr*seconds;std::vector<double> y(n);p.setNonRealtime(true);p.prepareToPlay(sr,bs);AudioBuffer<float>b(2,bs);MidiBuffer m;
 for(int j=0;j<n;j+=bs){for(int i=0;i<bs;i++){int t=j+i;double lev=step>=0&&t>=step?after:before;float x=lev<-200?0:std::pow(10.,(lev-24)/20)*std::sin(2*pi*f*t/sr);for(int k=0;k<2;k++)b.setSample(k,i,x);}p.processBlock(b,m);for(int i=0;i<bs&&j+i<n;i++)y[j+i]=b.getSample(0,i);}
 p.releaseResources();return y;
}
double rms(const std::vector<double>&y,int a,int b){double s=0;for(int i=a;i<b;i++)s+=y[i]*y[i];return sqrt(s/(b-a));}
std::complex<double> bin(const std::vector<double>& y,double f,int a,int b){std::complex<double>s{};for(int i=a;i<b;i++)s+=y[i]*std::polar(1.,-2*pi*f*i/sr);return s*(2./(b-a));}
double level(const std::vector<double>& y,double f=1000){return 24+db(abs(bin(y,f,y.size()-sr,y.size())));}
double thd(const std::vector<double>&y,double f=1000){double sum=0;for(int k=2;k<=20&&k*f<sr/2;k++){auto x=bin(y,k*f,y.size()-sr,y.size());sum+=norm(x);std::cout<<" K"<<k<<"="<<db(abs(x)/abs(bin(y,f,y.size()-sr,y.size())));}return 100*sqrt(sum)/abs(bin(y,f,y.size()-sr,y.size()));}
void result(const char* n,double v,double low,double high){std::cout<<n<<","<<v<<","<<low<<","<<high<<","<<(v>=low&&v<=high?"PASS":"FAIL")<<"\n";}

// Measure per-cycle fundamental using a least-squares sine/cosine fit,
// with the same 1ms analysis bins and step times as measure_dynamics.py.
double timingTest(AudioProcessor& p,bool lim,int ratio,int recovery,bool fast,int burst=800,int ct=0,double high=4,bool report=false){
 int start=450, stop=start+burst, end=stop+8000;
 config(p,!lim,lim,ct,ratio,0,recovery,8,recovery,fast,false);
 std::vector<double> y((end*sr)/1000);
 double lo=lim?10:-30,hi=high;
 p.setNonRealtime(true);p.prepareToPlay(sr,bs);AudioBuffer<float>b(2,bs);MidiBuffer midi;
 for(int j=0;j<(int)y.size();j+=bs){for(int i=0;i<bs;i++){int n=j+i;double ms=1000.*n/sr;double dbu=ms>=start&&ms<stop?hi:lo;float x=std::pow(10.,(dbu-24)/20)*std::sin(2*pi*1000*n/sr);b.setSample(0,i,x);b.setSample(1,i,0);}p.processBlock(b,midi);for(int i=0;i<bs&&j+i<(int)y.size();i++)y[j+i]=b.getSample(0,i);}p.releaseResources();
 std::vector<double> gain(end-1),output(end-1);
 for(int ms=0;ms<end-1;ms++){int a=(ms*sr+999)/1000,z=((ms+1)*sr+999)/1000;double ss=0,cc=0,sc=0,sy=0,cy=0;for(int n=a;n<z;n++){double s=sin(2*pi*1000*n/sr),c=cos(2*pi*1000*n/sr);ss+=s*s;cc+=c*c;sc+=s*c;sy+=s*y[n];cy+=c*y[n];}double d=ss*cc-sc*sc;double u=(sy*cc-cy*sc)/d,v=(cy*ss-sy*sc)/d;output[ms]=24+db(hypot(u,v));gain[ms]=output[ms]-(ms>=start&&ms<stop?hi:lo);}
 auto median=[&](const std::vector<double>& x,int a,int z){std::vector<double> v(x.begin()+a,x.begin()+z);std::sort(v.begin(),v.end());return (v[(v.size()-1)/2]+v[v.size()/2])*.5;};
 double idle=median(gain,380,430),driven=median(gain,stop-15,stop-3),pre=median(output,380,430);
 auto cross=[&](int a,int z,double from,double to,double f){if(abs(to-from)<1e-6)return -1.;for(int i=a;i<z-10;i++){bool ok=true;for(int k=0;k<10;k++)if((gain[i+k]-from)/(to-from)<f){ok=false;break;}if(ok)return i-a+.5;}return -1.;};
 double within=-1;for(int i=start;i<stop;i++)if(output[i]<=pre+1){within=i-start+.5;break;}
 if(report) std::cout<<"LIGHT_DYNAMICS threshold_index "<<ct<<" input_dbu "<<hi<<" actual_gr "<<idle-driven<<" release25 "<<cross(stop,end-1,driven,idle,.25)<<" release50 "<<cross(stop,end-1,driven,idle,.5)<<" DYNAMICS mode "<<(lim?"limit":"comp")<<" ratio_index "<<ratio<<" fast "<<fast<<" recovery "<<recovery+1<<" burst_ms "<<burst<<" idle_gain "<<idle<<" driven_gain "<<driven<<" attack63 "<<cross(start,stop,idle,driven,.632)<<" release63 "<<cross(stop,end-1,driven,idle,.632)<<" release90 "<<cross(stop,end-1,driven,idle,.9)<<" settle1dB "<<within<<"\n"; return idle-driven;
}
int main(int argc,char**argv){try{ScopedJuceInitialiser_GUI gui;AudioPluginFormatManager fm;auto f=std::make_unique<VST3PluginFormat>();OwnedArray<PluginDescription> d;f->findAllTypesForFile(d,String::fromUTF8(argv[1]));if(d.size()!=1)return 3;fm.addFormat(f.release());String err;auto p=fm.createPluginInstance(*d[0],sr,bs,err);if(!p)return 4;AudioProcessor::BusesLayout layout;layout.inputBuses.add(AudioChannelSet::stereo());layout.outputBuses.add(AudioChannelSet::stereo());if(!p->setBusesLayout(layout))return 5;std::cout<<std::fixed<<std::setprecision(6);

 for(int r:{4,5}) { for(double target:{1.,4.}) {int best=0; double err=1e9; for(int ct=0;ct<16;ct++){double gr=timingTest(*p,false,3,r,false,100,ct,4); if(std::abs(gr-target)<err){err=std::abs(gr-target);best=ct;}} double lo=-24,hi=24; for(int i=0;i<16;i++){double mid=(lo+hi)/2; double gr=timingTest(*p,false,3,r,false,100,best,mid); if(gr>target)hi=mid;else lo=mid;} std::cout<<"TARGET "<<target<<" "; timingTest(*p,false,3,r,false,100,best,(lo+hi)/2,true); } for(int ratio:{3,4}) {std::cout<<"MAX_LINE_INPUT ";timingTest(*p,false,ratio,r,false,100,0,4,true);std::cout<<"MAX_DIGITAL_INPUT ";timingTest(*p,false,ratio,r,false,100,0,24,true);} }
 return 0;}catch(std::exception&e){std::cerr<<e.what();return 8;}}
