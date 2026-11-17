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
int main(int argc,char**argv){try{ScopedJuceInitialiser_GUI gui;AudioPluginFormatManager fm;auto f=std::make_unique<VST3PluginFormat>();OwnedArray<PluginDescription> d;f->findAllTypesForFile(d,String::fromUTF8(argv[1]));if(d.size()!=1)return 3;fm.addFormat(f.release());String err;auto p=fm.createPluginInstance(*d[0],sr,bs,err);if(!p)return 4;AudioProcessor::BusesLayout layout;layout.inputBuses.add(AudioChannelSet::stereo());layout.outputBuses.add(AudioChannelSet::stereo());if(!p->setBusesLayout(layout))return 5;std::cout<<std::fixed<<std::setprecision(6);
 config(*p);double baseline=level(render(*p,1000,4,4,4));for(int i=0;i<=15;i++){config(*p,true,false,i,2,0,0);auto y=render(*p,1000,4,4,4);double gr=baseline-level(y);if(gr>=4&&gr<=12){std::cout<<"LINELEVEL +4dBu ratio 3:1 threshold "<<-20+2*i<<" actual GR "<<gr;double t=thd(y);std::cout<<" THD "<<t<<" percent\n";}else std::cout<<"LINELEVEL threshold "<<-20+2*i<<" actual GR "<<gr<<"\n";} 
 for(double lev:{4.,10.,14.,18.,20.}){config(*p);double dry=level(render(*p,1000,lev,lev));for(int ct:{5,7,9}){config(*p,true,false,ct,2,0,0);auto c=render(*p,1000,lev,lev);double cl=level(c);for(int lt:{0,8,16}){config(*p,true,true,ct,2,0,0,lt,0,false);auto y=render(*p,1000,lev,lev);double out=level(y);std::cout<<"COMBINED input "<<lev<<" compressor threshold "<<-20+2*ct<<" limiter threshold "<<4+.5*lt<<" comp-only GR "<<dry-cl<<" combined GR "<<dry-out<<" incremental attenuation "<<cl-out;double t=thd(y);std::cout<<" THD "<<t<<" percent\n";}}for(int lt:{0,8,16}){config(*p,false,true,0,0,0,0,lt);auto y=render(*p,1000,lev,lev);std::cout<<"LIMITONLY input "<<lev<<" threshold "<<4+.5*lt<<" GR "<<dry-level(y);double t=thd(y);std::cout<<" THD "<<t<<" percent\n";}}
 // Handbook 527-148 issue 1 p3: +22 dBu, limit only, 800 ms; printed -18 dBu threshold is unavailable.
 config(*p);double factoryDry=level(render(*p,1000,22,22,10));for(int fast=0;fast<2;fast++)for(int lt=0;lt<=22;lt++){config(*p,false,true,0,0,0,0,lt,3,fast,false);auto y=render(*p,1000,22,22,10);double out=level(y);std::cout<<"FACTORY_LIMIT input 22 recovery 800 fast "<<fast<<" threshold "<<4+.5*lt<<" output "<<out<<" GR "<<factoryDry-out;double t=thd(y);std::cout<<" THD "<<t<<" percent relative_to_0.45_limit "<<(t<=.45?"PASS_AT_THIS_THRESHOLD":"ABOVE_LIMIT")<<"\n";}
 config(*p);auto ref=level(render(*p,1000,0,0));for(double hz:{20.,40.,100.,200.,500.,1000.,2000.,5000.,8000.,10000.,12000.,15000.,18000.,20000.}){auto y=render(*p,hz,0,0);std::cout<<"FR Hz "<<hz<<" ";result("relative dB",level(y,hz)-ref,-.5,.5);}
 double targets[]={6.5,5,3.5,2.5,1.5};for(int i=0;i<5;i++){config(*p,true,false,0,i);auto y=render(*p,1000,0,10,6,3*sr);double a=24+db(abs(bin(y,1000,2*sr,3*sr)));double b=level(y);std::cout<<"Ratio detent "<<i<<" measured ratio "<<10/(b-a)<<" ";result("output step dB",b-a,targets[i]-(i<3?1:.5),targets[i]+(i<3?1:.5));}
 config(*p,true,false,15);double base=level(render(*p,1000,-30,-30));for(int i=0;i<=10;i++){config(*p,true,false,15,0,i);std::cout<<"Makeup "<<i*2<<" ";result("gain dB",level(render(*p,1000,-30,-30))-base,i*2-.5,i*2+.5);}
 for(int i=0;i<=22;i++){config(*p,false,true,0,0,0,0,i);std::cout<<"Limit threshold "<<4+.5*i<<" output "<<level(render(*p,1000,20,20))<<"\n";}
 for(int fast=0;fast<2;fast++){config(*p,false,true,0,0,0,0,8,0,fast);auto y=render(*p,1000,10,20,6,3*sr);double a=db(abs(bin(y,1000,2*sr,3*sr))),b=db(abs(bin(y,1000,5*sr,6*sr)));result("Limiter output step dB",b-a,0,.2);int step=3*sr;double old=rms(y,step-441,step);double time=-1;for(int i=step;i<step+sr/10;i++){int end=i+44;if(db(rms(y,i,end)/old)<=1){time=1000.*(end-step)/sr;break;}}std::cout<<"Attack fast "<<fast<<" ";result("attack ms trailing 1ms RMS",time,fast?1:3,fast?3:5);}
 for(int mode=0;mode<2;mode++)for(int i=0;i<6;i++){config(*p,mode==0,mode==1,0,3,0,i,8,i);auto y=render(*p,1000,20,10,18,6*sr);int step=6*sr;double start=db(rms(y,step-441,step))-10, end=db(rms(y,y.size()-441,y.size()));double t63=-1,t90=-1;for(int j=step;j+441<(int)y.size();j+=44){double v=db(rms(y,j,j+441));double fraction=(v-start)/(end-start);if(t63<0&&fraction>=.632)t63=1000.*(j+220-step)/sr;if(t90<0&&fraction>=.9)t90=1000.*(j+220-step)/sr;if(t90>=0)break;}std::cout<<"Audio recovery proxy mode "<<mode<<" detent "<<i<<" 63pct ms "<<t63<<" 90pct ms "<<t90<<"\n";}
 config(*p);auto y=render(*p,1000,9,9);double t=thd(y);std::cout<<" ";result("Through THD percent",t,0,.075);
 config(*p,true,false,1,4,10,2);for(double lev:{0.,9.,10.}){y=render(*p,1000,lev,lev,10);t=thd(y);std::cout<<" Compressor input assumption "<<lev<<" ";result("Compressor THD percent",t,0,.2);}
 for(int lt:{0,8,22}){config(*p,false,true,0,0,0,0,lt,3);y=render(*p,1000,22,22,10);t=thd(y);std::cout<<" Limiter alternative threshold "<<4+.5*lt<<" ";result("Limiter THD percent",t,0,.45);}
 for(int mode=0;mode<2;mode++){config(*p,mode==1,false,15,0,mode?10:0,0,8,0,false,true);y=render(*p,1000,-300,-300);double n=24+db(rms(y,y.size()-sr,y.size())*sqrt(2.));std::cout<<"Noise mode "<<mode<<" ";result("unweighted equivalent dBu",n,-300,mode?-55:-75);}return 0;}catch(std::exception&e){std::cerr<<e.what();return 8;}}
