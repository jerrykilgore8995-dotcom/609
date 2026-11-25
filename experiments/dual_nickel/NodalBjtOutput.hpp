#pragma once
// Five-node modified nodal-analysis (MNA) complementary emitter follower.
// Includes NPN 2N5088, PNP 2N5087, finite base-drive resistance,
// bias feed, nonlinear forward/reverse BJT transport, Early effect, high
// injection, junction/depletion + transit charge, emitter resistors, +/-15V
// rails, output coupling capacitance/ESR and selected 600/10k load.
// Model parameters from reference SPICE cards. This is a Gummel-Poon
// device-equation implementation, not a complete arbitrary ngspice runtime:
// model-card parameters not yet stamped are documented in README.
#include "NickelTransformer.hpp"
#include <array>
#include <algorithm>
#include <cmath>
namespace nickel {
struct GpDevice {
    double is,bf,br,ikf,ikr,vaf,var;
    double nf,nr,ise,ne,isc,nc;
    double cje,mje,vje,cjc,mjc,vjc,fc,tf,tr;
    double rb,re,rc;
    bool pnp;
};
inline constexpr GpDevice device5088{
    5.911e-15,1122.,1.271,.01492,1.e20,62.37,21.5,
    1.,1.,5.911e-15,1.394,0.,2.,
    4.973e-12,.4146,.65,4.017e-12,.3174,.65,.5,821.7e-12,4.673e-9,
    10.,.15,1.61,false
};
inline constexpr GpDevice device5087{
    6.734e-15,254.1,2.683,.1962,1.e20,45.7,1.e20,
    1.,1.,6.734e-15,1.741,0.,2.,
    7.5e-12,.2861,.75,6.2e-12,.301,.75,.5,467.8e-12,10.1e-9,
    10.,0.,1.67,true
};
class NodalOutputDriver {
    using Vec=std::array<double,5>;
    using Matrix=std::array<Vec,5>;
public:
    void prepare(double sr) noexcept {fs=std::max(1000.,sr);reset();}
    void reset() noexcept {
        inputCap=outputCap=daVoltage=previousSignal=0.;
        state={.73,-.73,0.,0.,0.};
        prevBE={0.,0.};prevBC={0.,0.};
        failed=0;
    }
    void setOutputLoadOhms(double load) noexcept {
        loadR=load<1500.?600.:10000.;
    }
    double outputLoadOhms() const noexcept {return loadR;}
    int failureCount() const noexcept {return failed;}
    float process(float sample) noexcept {
        const double dt=1./fs;
        const double vin=double(sample)*fullScalePeakVolts;
        const double inputR=100.+47000.+.4;
        const double current=(vin-inputCap)/(inputR+dt/47.e-6);
        inputCap+=dt*current/47.e-6;
        const double signal=47000.*current;
        const double oldDa=daVoltage;
        daVoltage+=(inputCap-daVoltage)*dt/(.15+dt);
        const double base=signal-.01*(daVoltage-oldDa);
        // Voltage driver resistor and device base resistance enter the base
        // node KCL. This models actual base loading unlike ideal base drive.
        // The ±15 V supply limits the transistor base drive. Emitter and
        // output nodes must remain free to reach their actual nonlinear
        // saturation solution, not an artificial ±13.5 V bound.
        const double targetBN=std::clamp(base+.73,-15.,15.);
        const double targetBP=std::clamp(base-.73,-15.,15.);
        const double rem=22.;
        const double outputRC=loadR+.15+dt/100.e-6;
        auto evaluate=[&](const Vec& x) noexcept {
            const double bn=x[0],bp=x[1],en=x[2],ep=x[3],vo=x[4];
            const auto n=currents(device5088,bn-en,bn-15.);
            const auto p=currents(device5087,ep-bp,-15.-bp);
            const double nBE=bn-en,nBC=bn-15.;
            const double pBE=bp-ep,pBC=bp+15.;
            const double cbeN=n.cbe*(nBE-prevBE[0])/dt;
            const double cbcN=n.cbc*(nBC-prevBC[0])/dt;
            const double cbeP=p.cbe*(pBE-prevBE[1])/dt;
            const double cbcP=p.cbc*(pBC-prevBC[1])/dt;
            Vec f{};
            f[0]=(bn-targetBN)/(100.+device5088.rb)+n.ib+cbeN+cbcN;
            f[1]=(bp-targetBP)/(100.+device5087.rb)-p.ib+cbeP+cbcP;
            f[2]=-(n.ic+n.ib)-cbeN+(en-vo)/(rem+device5088.re);
            f[3]=p.ic+p.ib-cbeP+(ep-vo)/(rem+device5087.re);
            f[4]=(vo-en)/(rem+device5088.re)
                 +(vo-ep)/(rem+device5087.re)
                 +(vo-outputCap)/outputRC;
            return f;
        };
        Vec x=state;
        x[0]=std::clamp(x[0]+base-previousSignal,-15.,15.);
        x[1]=std::clamp(x[1]+base-previousSignal,-15.,15.);
        x[2]=std::clamp(x[2]+base-previousSignal,-18.,18.);
        x[3]=std::clamp(x[3]+base-previousSignal,-18.,18.);
        x[4]=std::clamp(x[4]+base-previousSignal,-18.,18.);
        previousSignal=base;
        bool converged=false;
        for(int step=0;step<15;++step) {
            const Vec f=evaluate(x);
            const double err=maxAbs(f);
            if(!std::isfinite(err))break;
            if(err<2.e-9){converged=true;break;}
            Matrix jac{};
            for(int j=0;j<5;++j) {
                Vec test=x;
                const double h=1.e-5;
                test[j]+=h;
                const Vec g=evaluate(test);
                for(int i=0;i<5;++i)jac[i][j]=(g[i]-f[i])/h;
            }
            Vec delta{};
            for(int i=0;i<5;++i)delta[i]=-f[i];
            if(!linearSolve(jac,delta))break;
            bool accepted=false;
            for(double alpha=1.;alpha>=.015625;alpha*=.5) {
                Vec candidate=x;
                for(int i=0;i<5;++i)
                    candidate[i]=std::clamp(x[i]+alpha*delta[i],
                                i<2?-15.:-18.,i<2?15.:18.);
                if(maxAbs(evaluate(candidate))<err) {
                    x=candidate;accepted=true;break;
                }
            }
            if(!accepted)break;
        }
        if(!converged && maxAbs(evaluate(x))>5.e-7) {
            // The normal warm-start Newton method occasionally leaves the
            // physical solution basin during a large input transition.
            // Retry from a physically plausible complementary follower
            // operating point. Keep capacitor charge history untouched:
            // only the instantaneous five-node algebraic solve is retried.
            // A large input can exceed the +/-15V rail while the actual
            // emitter/output operating point stays near the rail. Seeding
            // with the unbounded input would forward-bias the wrong BJT.
            // Start all three follower nodes at a rail-aware voltage.
            const double physicalEmitterSeed=std::clamp(base,-14.,14.);
            Vec fresh{targetBN,targetBP,
                      physicalEmitterSeed,physicalEmitterSeed,
                      physicalEmitterSeed};
            // Retry unconditionally from the physical seed. A trapped
            // previous solution can have a smaller residual than the seed
            // yet still lie in an invalid Newton basin. Comparing initial
            // residuals alone can perpetuate a bad state for many samples.
            x=fresh;
            double bestErr=maxAbs(evaluate(x));
            for(int iter=0;iter<60 && bestErr>5.e-7;++iter) {
                const Vec f=evaluate(x);
                Matrix jac{};
                for(int j=0;j<5;++j) {
                    Vec plus=x,minus=x;
                    const double h=1.e-5;
                    plus[j]+=h;minus[j]-=h;
                    const Vec fp=evaluate(plus),fm=evaluate(minus);
                    for(int i=0;i<5;++i)
                        jac[i][j]=(fp[i]-fm[i])/(2*h);
                }
                Vec step{};
                for(int i=0;i<5;++i)step[i]=-f[i];
                if(!linearSolve(jac,step))break;
                bool accepted=false;
                for(double alpha=1.;alpha>=1./4096.;alpha*=.5) {
                    Vec trial=x;
                    for(int i=0;i<5;++i)
                        trial[i]=std::clamp(x[i]+alpha*step[i],
                          i<2?-15.:-18.,i<2?15.:18.);
                    const double err=maxAbs(evaluate(trial));
                    if(std::isfinite(err) && err<bestErr) {
                        x=trial;bestErr=err;accepted=true;break;
                    }
                }
                if(!accepted)break;
            }
            if(bestErr>5.e-7)++failed;
        }
        state=x;
        prevBE={x[0]-x[2],x[1]-x[3]};
        prevBC={x[0]-15.,x[1]+15.};
        const double loadCurrent=(x[4]-outputCap)/outputRC;
        outputCap+=dt*loadCurrent/100.e-6;
        const double y=loadCurrent*loadR;
        return std::isfinite(y)?float(y/fullScalePeakVolts):0.f;
    }
private:
    struct Currents {double ic,ib,cbe,cbc;};
    static double ex(double x) noexcept {return std::exp(std::clamp(x,-60.,38.));}
    static double depletion(double voltage,double cap,double vj,double m,double fc) noexcept {
        const double limit=fc*vj;
        if(voltage<limit)return cap*std::pow(std::max(.02,1.-voltage/vj),-m);
        const double at=cap*std::pow(1.-fc,-m);
        return at*(1.+m*(voltage-limit)/(vj*(1.-fc)));
    }
    static Currents currents(const GpDevice& d,double vbe,double vbc) noexcept {
        const double vt=.02585;
        const double f=d.is*(ex(vbe/(d.nf*vt))-1.);
        const double r=d.is*(ex(vbc/(d.nr*vt))-1.);
        const double q1=1./std::clamp(1.-vbc/d.vaf-vbe/d.var,.1,10.);
        const double q2=std::max(0.,f)/d.ikf+std::max(0.,r)/d.ikr;
        const double qb=q1*.5*(1.+std::sqrt(1.+4.*q2));
        const double leakageBE=d.ise*(ex(vbe/(d.ne*vt))-1.);
        const double leakageBC=d.isc*(ex(vbc/(d.nc*vt))-1.);
        const double ic=(f-r)/qb-r/d.br-leakageBC;
        const double ib=f/d.bf+r/d.br+leakageBE+leakageBC;
        const double cbe=depletion(vbe,d.cje,d.vje,d.mje,d.fc)
                  +d.tf*std::max(0.,f)/(vt*std::max(1.,qb));
        const double cbc=depletion(vbc,d.cjc,d.vjc,d.mjc,d.fc)
                  +d.tr*std::max(0.,r)/vt;
        return {ic,ib,cbe,cbc};
    }
    static double maxAbs(const Vec& v) noexcept {
        double peak=0.;
        for(double d:v)peak=std::max(peak,std::abs(d));
        return peak;
    }
    static bool linearSolve(Matrix& a,Vec& b) noexcept {
        for(int k=0;k<5;++k) {
            int pivot=k;
            for(int i=k+1;i<5;++i)
                if(std::abs(a[i][k])>std::abs(a[pivot][k]))pivot=i;
            if(std::abs(a[pivot][k])<1e-15)return false;
            if(pivot!=k) {std::swap(a[pivot],a[k]);std::swap(b[pivot],b[k]);}
            const double diag=a[k][k];
            for(int j=k+1;j<5;++j)a[k][j]/=diag;
            b[k]/=diag;a[k][k]=1.;
            for(int i=k+1;i<5;++i) {
                const double t=a[i][k];a[i][k]=0.;
                for(int j=k+1;j<5;++j)a[i][j]-=t*a[k][j];
                b[i]-=t*b[k];
            }
        }
        for(int k=4;k>=0;--k)
            for(int j=k+1;j<5;++j)b[k]-=a[k][j]*b[j];
        return true;
    }
    Vec state{.73,-.73,0.,0.,0.};
    std::array<double,2> prevBE{},prevBC{};
    double fs=44100.,loadR=10000.,inputCap=0.,outputCap=0.,
           daVoltage=0.,previousSignal=0.;
    int failed=0;
};
} // namespace nickel
