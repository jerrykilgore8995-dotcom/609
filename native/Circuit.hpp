#pragma once

#include "Sparse.hpp"
#include "Controls.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cmath>
#include <vector>

namespace mkii::native {

constexpr double thermal = 1.380649e-23 * 300.15 / 1.602176634e-19;
constexpr double gmin = 1e-12;

// Signed terminal voltage and its two partial derivatives.
struct Jet {
    double value, p = 0.0, q = 0.0;
};
inline Jet operator+(Jet a, Jet b) { return {a.value+b.value,a.p+b.p,a.q+b.q}; }
inline Jet operator-(Jet a, Jet b) { return {a.value-b.value,a.p-b.p,a.q-b.q}; }
inline Jet operator*(Jet a, double b) { return {a.value*b,a.p*b,a.q*b}; }
inline Jet operator*(Jet a, Jet b) {
    return {a.value*b.value,a.p*b.value+a.value*b.p,a.q*b.value+a.value*b.q};
}
inline Jet operator/(Jet a, Jet b) {
    const double inv = 1.0/b.value, y = a.value*inv;
    return {y,(a.p-y*b.p)*inv,(a.q-y*b.q)*inv};
}
inline Jet squareRoot(Jet x) {
    const double value = std::sqrt(x.value), slope = 0.5/value;
    return {value,x.p*slope,x.q*slope};
}

inline double junctionExp(double x) {
    // The junction and detector loads bound x to [-35, 70]. Range reduction
    // keeps r within half a log(2), where this degree-eight series is
    // accurate to better than one part in a billion.
    const int exponent=static_cast<int>(x*1.4426950408889634+64.5)-64;
    const double r=x-exponent*0.6931471805599453;
    const double square=r*r, fourth=square*square;
    const double low=1+r+square*(0.5+r/6);
    const double high=(1.0/24+r/120)+square*(1.0/720+r/5040);
    const auto power=std::bit_cast<double>(static_cast<std::uint64_t>(exponent+1023)<<52);
    return (low+fourth*(high+fourth/40320))*power;
}

inline Jet junction(Jet volts, double emissionVoltage, double saturation) {
    if (saturation == 0) return {0};
    const double x = volts.value/emissionVoltage;
    if (x < -3) {
        const double a = 3.0/(2.718281828459045*x);
        const double cube = a*a*a, slope = saturation*3*cube/volts.value;
        return {-saturation*(1 + cube), slope*volts.p, slope*volts.q};
    }
    // The linear continuation above 1.8 V keeps a rejected Newton
    // iterate finite. It is outside the normal operating region.
    const double limited = std::min(x, 70.0);
    const double exp = junctionExp(limited), slope = saturation*exp/emissionVoltage;
    return {saturation*(exp*(1 + x-limited)-1), slope*volts.p, slope*volts.q};
}

template<double exponent>
const auto junctionPowerTable=[] {
    std::array<double,8193> data{};
    for (std::size_t i=0;i<data.size();++i) data[i]=std::pow(0.5+double(i)/256,-exponent);
    return data;
}();

template<double exponent>
inline Jet capacitance(Jet volts, double zero, double potential, double fc) {
    if (zero == 0) return {0};
    const double knee = potential*fc;
    double value, slope;
    if (volts.value < knee) {
        const double x = 1-volts.value/potential;
        const double index=(x-0.5)*256;
        double m;
        if (index>=0 && index<8192) {
            const int slot=static_cast<int>(index);
            const auto& data=junctionPowerTable<exponent>;
            m=data[slot]+(index-slot)*(data[slot+1]-data[slot]);
        } else m=std::pow(x,-exponent);
        value = potential*zero*(1-x*m)/(1-exponent);
        slope = zero*m;
    } else {
        const double f = std::pow(1-fc,-exponent);
        const double offset = volts.value-knee;
        const double curvature = f*exponent/(potential*(1-fc));
        value = potential*zero*(1-(1-fc)*f)/(1-exponent)
                + zero*offset*(f+0.5*offset*curvature);
        slope = zero*(f+offset*curvature);
    }
    return {value,slope*volts.p,slope*volts.q};
}

struct Constant { int kind; double value; };
struct R { int a,b; double conductance; int pot; };
struct ContactTable { int control,first; std::vector<double> values; };
struct Contact { int a,b; double conductance; bool normallyClosed; std::vector<ContactTable> tables; };
struct C { int a,b; double value, q1=0, q2=0, q=0; };
struct I { int a,b; double value; };
struct V { int a,b,k; double value; };
struct Control { int a,b,p,m; double gain; };
struct DModel { double is,n,cjo,vj,m,tt,fc,bv,ibv; };
struct Inductance {int row,column; double value,q1=0,q2=0;};
struct Cubic {int a,b,control;double gain;};
enum class Behavior {detector,comparator,positiveRegulator,negativeRegulator};
struct Behavioral {Behavior kind;int a,b;std::array<int,4> input;std::array<double,4> param;};
struct QModel {
    double is,bf,nf,vaf,ikf,ise,ne,br,nr,var,ikr,isc,nc,cje,vje,mje,cjc,vjc,mjc,tf,tr,fc;
};
struct JModel { double vto,beta,is,lambda,cgs,cgd,pb,fc; };
struct D { int a,b; double q1=0,q2=0,q=0,slope=0,knee=0; std::array<int,4> slots{}; };
struct Q { int c,b,e; double f1=0,f2=0,f=0,r1=0,r2=0,r=0; Jet fq{},rq{}; int sign=1; std::array<int,9> slots{}; };
struct J { int d,g,s; std::array<int,9> slots{}; };

struct Circuit {
    int size=0,nodes=0;
    std::array<int,4> output{};
    std::array<int,2> meter{};
    std::vector<Constant> constants;
    std::vector<R> resistors;
    std::vector<Contact> contact;
    std::vector<C> capacitors;
    std::vector<I> currents;
    std::vector<V> sources;
    std::vector<Control> controlled;
    std::vector<Inductance> inductance;
    std::vector<Cubic> cubic;
    std::vector<Behavioral> behavioral;
    std::vector<D> diodes;
    std::vector<Q> bjts;
    std::vector<J> jfets;
    std::vector<double> bias;
    std::vector<unsigned char> channel;
};
#include "Deck.hpp"

class Solver {
public:
    explicit Solver(int channel=-1):lane(channel),linkConductance(channel<0?0.0:0.1) {
        loadDeck(deck);
        if (lane>=0) isolateChannel();
    }
    bool prepare(double rate) {
        if (!partitionValid) return false;
        x = deck.bias;
        previous = x;
        older = x;
        for (auto& guess : guesses) guess.nodes=x;
        guessCount=nextGuess=0;
        oldIter=x;
        slope = {};
        fixed.resize(deck.constants.size());
        hostRate=rate;
        lastInput={};
        peerLinkVoltage=0;
        supply({});
        lastControls=defaults;
        for (auto& c : deck.contact) selectContact(c,defaults);
        matrix.resize(deck.size);
        // Include the transient structure in the symbolic factorization.
        // The actual initial solve below uses zero integration conductance.
        inverse = 1.5*rate;
        history1 = -2.0*rate;
        history2 = 0.5*rate;
        diodeStorage.clear();
        bjtStorage.clear();
        stampLinear();
        stampNonlinear<true>();
        PivotOrder order;
#if !defined(MKII_NATIVE_GENERATE_SOLVER) && __has_include("GeneratedSolve.hpp")
        order=findGeneratedOrder(rate,lane);
#endif
        if (!matrix.prepare(order)) return false;
        cacheNonlinearSlots();
        retryMatrix=matrix;
        if (!prepareLinear(false)) return false;
        std::swap(matrix,retryMatrix);
        if (!prepareLinear(true)) return false;
        std::swap(matrix,retryMatrix);
#if !defined(MKII_NATIVE_GENERATE_SOLVER) && __has_include("GeneratedSolve.hpp")
        generatedResidual=findGeneratedResidual(rate,lane);
#endif
        accept(true);
        inverse = 1.5*rate;
        history1 = -2.0*rate;
        history2 = 0.5*rate;
        savedNodes.resize(deck.size);
        savedHistory.resize(deck.capacitors.size()+deck.inductance.size()+deck.diodes.size()+2*deck.bjts.size());
        misses = 0;
        subdivisions=0;
        lastInput={};
        return true;
    }

    using Frame = std::array<double,2>;
    Frame step(Frame input, const Controls& controls) {
        const bool changed=controls!=lastControls;
        if (changed) {
            for (auto& contact : deck.contact) selectContact(contact,controls);
            const bool currentReady=refactorContacts(matrix);
            const bool retryReady=refactorContacts(retryMatrix);
            if (!currentReady || !retryReady) {
                ++misses;
                return {(v(deck.output[0])-v(deck.output[1]))/fullScalePeakVolts,
                        (v(deck.output[2])-v(deck.output[3]))/fullScalePeakVolts};
            }
            lastControls=controls;
            guessCount=nextGuess=0;
        }
        supply(input);
        prepareHistory();
        const Frame difference{input[0]-lastInput[0],input[1]-lastInput[1]};
        // The two audio signals can have different slopes. Seed each
        // channel from its own history, then solve the coupled network.
        Frame prediction{},best{.013,.013};
        std::array<const Guess*,2> closest{};
        for (int ch=0;ch<2;++ch)
            prediction[ch]=changed || std::abs(slope[ch])<1e-6?0.0:
                std::clamp(difference[ch]/slope[ch],-1.5,1.5);
        for (unsigned i=0;i<guessCount;++i) {
            const auto& g=guesses[i];
            for (int ch=0;ch<2;++ch) {
                const double score=std::abs(g.input[ch]-input[ch])+std::abs(g.delta[ch]-difference[ch]);
                if (score<best[ch]) {best[ch]=score;closest[ch]=&g;}
            }
        }
        for (int i=0;i<deck.size;++i) {
            const int ch=deck.channel[i];
            x[i]=closest[ch]?closest[ch]->nodes[i]:
                previous[i]+prediction[ch]*(previous[i]-older[i]);
            older[i]=previous[i];
        }
        slope=difference;
        fastConvergence=lane<0 ? closest[0]&&closest[1] : closest[lane]!=nullptr;
        const bool fullStep=newton(20) || continueSource(input);
        const bool subdivided=!fullStep && retry(input);
        if (fullStep || subdivided || recoverAtSampleRate(input)) {
            if (subdivided) ++subdivisions;
            else accept(false);
            saveGuess(input,difference);
            lastInput=input;
        } else {
            std::copy(previous.begin(),previous.end(),x.begin());
            ++misses;
        }
        return {(v(deck.output[0])-v(deck.output[1]))/fullScalePeakVolts,
                (v(deck.output[2])-v(deck.output[3]))/fullScalePeakVolts};
    }

    double linkVoltage() const {return previous[linkNode];}
    void setPeerLinkVoltage(double volts) {peerLinkVoltage=volts;}
    Frame meterVolts() const {return {v(deck.meter[0]),v(deck.meter[1])};}
    int operations() const { return matrix.operationCount(); }
    bool usesGeneratedSolve() const { return matrix.usesGeneratedSolve(); }
    bool usesGeneratedResidual() const {
#if !defined(MKII_NATIVE_GENERATE_SOLVER) && __has_include("GeneratedSolve.hpp")
        return generatedResidual!=nullptr;
#else
        return false;
#endif
    }
    void emitSolver(std::ostream& code,std::string_view name) const {
        matrix.emit(code,name);
        code<<"inline void rhs_"<<name<<"(double* __restrict y,const double* __restrict v) {\n";
        for (const auto& row : linearRows) {
            code<<"    double r"<<row.row<<"=y["<<row.row<<"];\n";
            for (int j=row.begin;j<row.end;++j)
                code<<"    r"<<row.row<<"+="<<std::hexfloat<<terms[j].g<<std::defaultfloat
                    <<"*v["<<terms[j].column<<"];\n";
            code<<"    y["<<row.row<<"]=r"<<row.row<<";\n";
        }
        code<<"}\n";
    }
    unsigned failures() const { return misses; }
    unsigned refinedSamples() const { return subdivisions; }
    double rawOutput() const { return v(deck.output[0])-v(deck.output[1]); }

private:
    static constexpr int unusedNode=-10000;

    void isolateChannel() {
        std::vector<int> map(deck.size,unusedNode);
        const int originalNodes=deck.nodes;
        deck.nodes=0;
        std::vector<double> bias;
        for (int n=0;n<deck.size;++n) if (deck.channel[n]==lane) {
            map[n]=static_cast<int>(bias.size());
            bias.push_back(deck.bias[n]);
            if (n<originalNodes) ++deck.nodes;
        }
        const auto node=[&](int n) {return n<0 ? n : map[n];};
        const auto remap=[&](auto& part,auto... member) {
            const bool here=(((part.*member)>=0 && node(part.*member)!=unusedNode)||...);
            const bool elsewhere=((node(part.*member)==unusedNode)||...);
            if (here && elsewhere) partitionValid=false;
            if (!here || elsewhere) return false;
            ((part.*member=node(part.*member)),...);
            return true;
        };
        const auto retain=[&](auto& parts,auto keep) {
            std::size_t count=0;
            for (std::size_t i=0;i<parts.size();++i) if (keep(parts[i],i)) {
                if (count!=i) parts[count]=std::move(parts[i]);
                ++count;
            }
            parts.resize(count);
        };
        const auto trim=[&](auto& parts,auto... member) {
            retain(parts,[&](auto& part,std::size_t) {return remap(part,member...);});
        };
        trim(deck.resistors,&R::a,&R::b);
        int linkContacts=0;
        retain(deck.contact,[&](auto& part,std::size_t) {
            if (part.a>=0 && part.b>=0 && deck.channel[part.a]!=deck.channel[part.b]) {
                if (part.tables.empty() || part.tables.front().control!=19) partitionValid=false;
                linkNode=node(deck.channel[part.a]==lane?part.a:part.b);
                ++linkContacts;
                return false;
            }
            return remap(part,&Contact::a,&Contact::b);
        });
        if (linkContacts!=1) partitionValid=false;
        if (partitionValid) {
            deck.resistors.push_back({linkNode,-1,linkConductance,0});
            // The linked path uses the benchmarked, fixed 10 ohm shunt.
            // Cancel it when the relay opens, without resetting circuit state.
            deck.contact.push_back({linkNode,-1,0,false,
                {{19,0,{-linkConductance,0}},{20,0,{-linkConductance,0}}}});
        }
        trim(deck.capacitors,&C::a,&C::b);
        trim(deck.inductance,&Inductance::row,&Inductance::column);
        trim(deck.currents,&I::a,&I::b);
        trim(deck.sources,&V::a,&V::b,&V::k);
        trim(deck.controlled,&Control::a,&Control::b,&Control::p,&Control::m);
        trim(deck.cubic,&Cubic::a,&Cubic::b,&Cubic::control);
        if (!deck.behavioral.empty()) partitionValid=false;
        // Keep indices for the generated model groups.
        for (auto& d:deck.diodes)
            if (!remap(d,&D::a,&D::b)) d.a=unusedNode;
        for (auto& q:deck.bjts)
            if (!remap(q,&Q::c,&Q::b,&Q::e)) q.c=unusedNode;
        for (auto& j:deck.jfets)
            if (!remap(j,&J::s,&J::d,&J::g)) j.d=unusedNode;
        for (auto& n:deck.output) n=std::max(-1,node(n));
        for (auto& n:deck.meter) n=std::max(-1,node(n));
        deck.size=static_cast<int>(bias.size());
        deck.channel.assign(deck.size,lane);
        deck.bias=std::move(bias);
    }

    struct Guess {Frame input,delta;std::vector<double> nodes;};
    void saveGuess(Frame input,Frame delta) {
        auto& g=guesses[nextGuess];
        nextGuess=(nextGuess+1)%guesses.size();
        if (guessCount<guesses.size()) ++guessCount;
        g.input=input;
        g.delta=delta;
        std::copy(x.begin(),x.end(),g.nodes.begin());
    }
    struct History { double previous, older; };

    template<class Operation>
    void visitHistory(Operation operation) {
        std::size_t at=0;
        for (auto& d : deck.capacitors) operation(savedHistory[at++],d.q1,d.q2);
        for (auto& d : deck.inductance) operation(savedHistory[at++],d.q1,d.q2);
        for (auto& d : deck.diodes) operation(savedHistory[at++],d.q1,d.q2);
        for (auto& d : deck.bjts) {
            operation(savedHistory[at++],d.f1,d.f2);
            operation(savedHistory[at++],d.r1,d.r2);
        }
    }

    void prepareHistory() {
        std::copy(constantRhs.begin(),constantRhs.end(),sampleRhs.begin());
        for (const auto& term : knownTerms)
            sampleRhs[term.row]+=term.g*fixed[term.column];
        for (const auto& d : deck.capacitors) {
            const double charge=history1*d.q1+history2*d.q2;
            if (d.a>=0) sampleRhs[d.a]-=charge;
            if (d.b>=0) sampleRhs[d.b]+=charge;
        }
        for (const auto& d : deck.inductance) sampleRhs[d.row]+=history1*d.q1+history2*d.q2;
        if (linkNode>=0 && lastControls[19] && lastControls[20])
            sampleRhs[linkNode]+=linkConductance*peerLinkVoltage;
    }

    bool recoverAtSampleRate(Frame input) {
        // Retry on the original audio-sample grid after subdivision stalls.
        // Near rectifier transitions the residual can briefly rise while the
        // iterate crosses a conduction transition.
        x=previous;
        supply(input);
        prepareHistory();
        fastConvergence=false;
        return newton<true>(96);
    }

    bool continueSource(Frame input) {
        // Try a numerical continuation before changing the time integration.
        for (int count : {2,4,8}) {
            std::copy(previous.begin(),previous.end(),x.begin());
            int i=1;
            for (;i<=count;++i) {
                supply({std::lerp(lastInput[0],input[0],double(i)/count),
                        std::lerp(lastInput[1],input[1],double(i)/count)});
                // Moving the source also changes the known-voltage
                // contributions to the linear residual.
                prepareHistory();
                if (!newton(24)) break;
            }
            if (i>count) return true;
        }
        return false;
    }

    bool retry(Frame input) {
        std::copy(previous.begin(),previous.end(),savedNodes.begin());
        visitHistory([](History& save,double& q1,double& q2){save={q1,q2};});
        const auto restore=[&] {
            std::copy(savedNodes.begin(),savedNodes.end(),previous.begin());
            std::copy(savedNodes.begin(),savedNodes.end(),x.begin());
            visitHistory([](const History& save,double& q1,double& q2){q1=save.previous;q2=save.older;});
        };
        bool solved=false;
        // Advance physical time with backward Euler on the shorter steps.
        // The next host sample resumes BDF2 with history on the host grid.
        std::swap(matrix,retryMatrix);
        for (int count : {2,4,8,16,32,64,128}) {
            restore();
            inverse=count*hostRate;
            history1=-inverse;
            history2=0;
            solved=true;
            for (int i=1;i<=count;++i) {
                supply({std::lerp(lastInput[0],input[0],double(i)/count),
                        std::lerp(lastInput[1],input[1],double(i)/count)});
                prepareHistory();
                if (!newton(30)) {solved=false;break;}
                accept(false);
            }
            if (solved) break;
        }
        std::swap(matrix,retryMatrix);
        if (solved) visitHistory([](const History& save,double&,double& q2){q2=save.previous;});
        else restore();
        inverse=1.5*hostRate;
        history1=-2.0*hostRate;
        history2=0.5*hostRate;
        return solved;
    }

    double v(int k) const { return k<0 ? fixed[-k-1] : x[k]; }
    void current(int a,int b,double i) {
        // The linear system solves for the negative residual.
        if (a>=0) matrix.rhs[a]-=i;
        if (b>=0) matrix.rhs[b]+=i;
    }
    void derivatives(int a,int b,int p,int n,double g) {
        matrix.add(a,p,g);
        matrix.add(a,n,-g);
        matrix.add(b,p,-g);
        matrix.add(b,n,g);
    }
    void conduct(int a,int b,double g) {
        current(a,b,(v(a)-v(b))*g);
        derivatives(a,b,a,b,g);
    }
    template<bool sizing>
    void terminal(int node,int c,int b,int e,int sign,Jet j,const int* slots) {
        if (node>=0) matrix.rhs[node]-=sign*j.value;
        if constexpr (sizing) {
            matrix.add(node,c,-j.q);
            matrix.add(node,b,j.p+j.q);
            matrix.add(node,e,-j.p);
        } else {
            matrix.addSlot(slots[0],-j.q);
            matrix.addSlot(slots[1],j.p+j.q);
            matrix.addSlot(slots[2],-j.p);
        }
    }

    void cacheNonlinearSlots() {
        const auto assign=[&](auto& slots,const auto& nodes) {
            int at=0;
            for (int row : nodes) for (int col : nodes) slots[at++]=matrix.slot(row,col);
        };
        for (auto& d : deck.diodes) if (d.a!=unusedNode) assign(d.slots,std::array{d.a,d.b});
        for (auto& d : deck.bjts) if (d.c!=unusedNode) assign(d.slots,std::array{d.c,d.b,d.e});
        for (auto& d : deck.jfets) if (d.d!=unusedNode) assign(d.slots,std::array{d.d,d.g,d.s});
    }

    void supply(Frame input) {
        for (std::size_t i=0;i<deck.constants.size();++i) {
            const auto& c=deck.constants[i];
            fixed[i]=c.kind==0?c.value:input[c.kind-1]*fullScalePeakVolts;
        }
    }

    static void selectContact(Contact& contact,const Controls& controls) {
        double value=contact.normallyClosed?0:100;
        for (const auto& table : contact.tables) {
            const auto selected=std::clamp(controls[table.control]-table.first,0,
                                           static_cast<int>(table.values.size())-1);
            value=contact.normallyClosed?std::max(value,table.values[selected]):
                                          std::min(value,table.values[selected]);
        }
        contact.conductance=value;
    }

    void stampBehavior() {
        for (const auto& d : deck.cubic) {
            const double x0=v(d.control), square=x0*x0;
            current(d.a,d.b,d.gain*square*x0);
            derivatives(d.a,d.b,d.control,-1,3*d.gain*square);
        }
        for (const auto& d : deck.behavioral) {
            const auto n=d.input;
            const auto p=d.param;
            const double a=v(n[0]), b=v(n[1]), c=v(n[2]), e=v(n[3]);
            double value=0;
            std::array<double,4> slope{};
            switch (d.kind) {
                case Behavior::detector: {
                    const double input=a+(b-c)/10000;
                    const double argument=(p[2]-e+c)/p[3];
                    const double factor=p[0]/(p[1]*p[1])*junctionExp(std::clamp(argument,-35.,35.));
                    value=factor*(input*input+1e-18);
                    if (value<0.002) {
                        const double exponentSlope=std::abs(argument)<35?value/p[3]:0;
                        slope={2*factor*input,2*factor*input/10000,
                               exponentSlope-2*factor*input/10000,-exponentSlope};
                    } else value=0.002;
                    break;
                }
                case Behavior::comparator: {
                    const double argument=(c-e)/0.003;
                    const double bounded=std::clamp(argument,-3.,3.);
                    const double square=bounded*bounded;
                    const double reciprocal=1/(27+9*square);
                    const double t=bounded*(27+square)*reciprocal;
                    const double g=1e-9+0.025*(1+t);
                    const double derivative=(a-b)*(0.025/0.003)*9
                        *(square-9)*(square-9)*reciprocal*reciprocal;
                    value=(a-b)*g;
                    slope={g,-g,derivative,-derivative};
                    break;
                }
                default: {
                    const double sign=d.kind==Behavior::positiveRegulator?1:-1;
                    const double vin=sign*(a-b);
                    const double nominal=15+0.0005*(vin-24);
                    const double available=std::max(0.,vin-2);
                    value=-sign*std::min(nominal,available);
                    slope[0]=nominal<available?-0.0005:(available>0?-1:0);
                    slope[1]=-slope[0];
                    break;
                }
            }
            current(d.a,d.b,value);
            for (std::size_t i=0;i<n.size();++i)
                derivatives(d.a,d.b,n[i],-1,slope[i]);
        }
    }

    void stampLinear() {
        for (const auto& d : deck.resistors) conduct(d.a,d.b,d.conductance);
        for (const auto& d : deck.contact) conduct(d.a,d.b,d.conductance);
        for (auto& d : deck.capacitors) {
            d.q=(v(d.a)-v(d.b))*d.value;
            current(d.a,d.b,inverse*d.q+history1*d.q1+history2*d.q2);
            derivatives(d.a,d.b,d.a,d.b,inverse*d.value);
        }
        for (const auto& d : deck.inductance) {
            current(d.row,-1,-d.value*inverse*v(d.column)-history1*d.q1-history2*d.q2);
            derivatives(d.row,-1,d.column,-1,-d.value*inverse);
        }
        stampBehavior();
        for (const auto& d : deck.currents) current(d.a,d.b,d.value);
        for (const auto& d : deck.sources) {
            current(d.a,d.b,x[d.k]);
            derivatives(d.a,d.b,d.k,-1,1);
            current(d.k,-1,v(d.a)-v(d.b)-d.value);
            derivatives(d.k,-1,d.a,d.b,1);
        }
        for (const auto& d : deck.controlled) {
            current(d.a,d.b,(v(d.p)-v(d.m))*d.gain);
            derivatives(d.a,d.b,d.p,d.m,d.gain);
        }
    }

    bool refactorContacts(Sparse& target) {
        target.resetLinearEntries();
        for (const auto& d : deck.contact) {
            target.add(d.a,d.a,d.conductance);
            target.add(d.a,d.b,-d.conductance);
            target.add(d.b,d.a,-d.conductance);
            target.add(d.b,d.b,d.conductance);
        }
        if (target.cacheStaticContacts()) return true;
        target.resetLinearEntries();
        return target.cacheDynamicContacts();
    }

    bool prepareLinear(bool variableStep) {
        const int n = deck.size;
        const int columns = n+static_cast<int>(fixed.size());
        std::vector<double> dense(n*columns,0.0);
        constantRhs.assign(n,0);
        sampleRhs.resize(n);
        terms.clear();
        knownTerms.clear();
        linearRows.clear();
        matrix.clear();
        const auto bind = [&](int row,int col,double g) {
            if (row<0) return;
            if (col>=0) matrix.add(row,col,g);
            else if (deck.constants[-col-1].kind == 0) {
                constantRhs[row] -= g*fixed[-col-1];
                return;
            }
            dense[row*columns+(col<0?n-col-1:col)] -= g;
        };
        const auto branch = [&](int a,int b,int p,int m,double g) {
            bind(a,p,g); bind(a,m,-g); bind(b,p,-g); bind(b,m,g);
        };
        const auto dc = [&](int a,int b,double i) {
            if (a>=0) constantRhs[a]-=i;
            if (b>=0) constantRhs[b]+=i;
        };
        for (const auto& d : deck.resistors) branch(d.a,d.b,d.a,d.b,d.conductance);
        for (const auto& d : deck.capacitors) branch(d.a,d.b,d.a,d.b,inverse*d.value);
        for (const auto& d : deck.inductance) bind(d.row,d.column,-inverse*d.value);
        for (const auto& d : deck.currents) dc(d.a,d.b,d.value);
        for (const auto& d : deck.sources) {
            branch(d.a,d.b,d.k,-1,1);
            branch(d.k,-1,d.a,d.b,1);
            dc(d.k,-1,-d.value);
        }
        for (const auto& d : deck.controlled) branch(d.a,d.b,d.p,d.m,d.gain);
        std::vector<int> mutableSlots;
        for (const auto& d : deck.diodes) if (d.a!=unusedNode) mutableSlots.insert(mutableSlots.end(),d.slots.begin(),d.slots.end());
        for (const auto& d : deck.bjts) if (d.c!=unusedNode) mutableSlots.insert(mutableSlots.end(),d.slots.begin(),d.slots.end());
        for (const auto& d : deck.jfets) if (d.d!=unusedNode) mutableSlots.insert(mutableSlots.end(),d.slots.begin(),d.slots.end());
        std::vector<int> contactSlots;
        const auto twoTerminal=[&](std::vector<int>& slots,int a,int b) {
            for (int r : {a,b}) for (int c : {a,b}) slots.push_back(matrix.slot(r,c));
        };
        for (const auto& d : deck.contact) twoTerminal(contactSlots,d.a,d.b);
        if (variableStep) {
            for (const auto& d : deck.capacitors) twoTerminal(mutableSlots,d.a,d.b);
            for (const auto& d : deck.inductance) mutableSlots.push_back(matrix.slot(d.row,d.column));
        }
        for (const auto& d : deck.cubic)
            for (int row : {d.a,d.b}) mutableSlots.push_back(matrix.slot(row,d.control));
        for (const auto& d : deck.behavioral)
            for (int row : {d.a,d.b}) for (int n : d.input) mutableSlots.push_back(matrix.slot(row,n));
        matrix.configureLinear(mutableSlots,contactSlots);
        for (int row=0;row<n;++row) {
            const auto begin=static_cast<int>(terms.size());
            for (int col=0;col<columns;++col) {
                const double g=dense[row*columns+col];
                if (g!=0) {
                    if (col<n) terms.push_back({col,g});
                    else knownTerms.push_back({row,col-n,g});
                }
            }
            const auto end=static_cast<int>(terms.size());
            if (begin!=end) linearRows.push_back({row,begin,end});
        }
        return refactorContacts(matrix);
    }

    void stampStep() {
        matrix.startIteration(sampleRhs);
#if !defined(MKII_NATIVE_GENERATE_SOLVER) && __has_include("GeneratedSolve.hpp")
        if (generatedResidual) generatedResidual(matrix.rhs.data(),x.data());
        else
#endif
        for (const auto& row : linearRows) {
            double sum=matrix.rhs[row.row];
            for (int j=row.begin;j<row.end;++j)
                sum+=terms[j].g*x[terms[j].column];
            matrix.rhs[row.row]=sum;
        }
        const double delta=inverse-1.5*hostRate;
        if (delta!=0)
            for (const auto& d : deck.capacitors) conduct(d.a,d.b,delta*d.value);
        if (delta!=0) for (const auto& d : deck.inductance) {
            current(d.row,-1,-d.value*delta*v(d.column));
            derivatives(d.row,-1,d.column,-1,-d.value*delta);
        }
        if (matrix.contactsAreDynamic()) {
            for (const auto& d : deck.contact) conduct(d.a,d.b,d.conductance);
        } else {
            for (const auto& d : deck.contact) current(d.a,d.b,(v(d.a)-v(d.b))*d.conductance);
        }
        stampBehavior();
        stampNonlinear<false>();
    }

    template<bool sizing>
    void stampNonlinear() {
        stamp_diodes([&]<DModel m>(int begin,int count) {
        for (int at=begin;at<begin+count;++at) {
            auto& d=deck.diodes[at];
            if (d.a==unusedNode) continue;
            const Jet voltage{v(d.a)-v(d.b),1,0};
            Jet i=junction(voltage,m.n*thermal,m.is);
            if constexpr (m.bv>0) {
                const double knee=m.bv-thermal*std::log1p(m.ibv/m.is);
                if constexpr (sizing) d.knee=knee;
                if (voltage.value < -knee) {
                    const double x=-(voltage.value+knee)/thermal;
                    const double bounded=std::min(x,70.0);
                    const double current=m.is*junctionExp(bounded);
                    i={-current*(1+x-bounded),current/thermal,0};
                }
            }
            double j=i.value+voltage.value*gmin;
            double g=i.p+gmin;
            if constexpr (m.cjo!=0 || m.tt!=0) {
                if constexpr (sizing) diodeStorage.push_back(at);
                const Jet q=capacitance<m.m>(voltage,m.cjo,m.vj,m.fc)+i*m.tt;
                d.q=q.value;
                d.slope=q.p;
                j+=inverse*d.q+history1*d.q1+history2*d.q2;
                g+=inverse*q.p;
            }
            current(d.a,d.b,j);
            if constexpr (sizing) derivatives(d.a,d.b,d.a,d.b,g);
            else {
                matrix.addSlot(d.slots[0],g);
                matrix.addSlot(d.slots[1],-g);
                matrix.addSlot(d.slots[2],-g);
                matrix.addSlot(d.slots[3],g);
            }
        }
        });
        stamp_bjts([&]<QModel m,int sign>(int begin,int count) {
        for (int at=begin;at<begin+count;++at) {
            auto& d=deck.bjts[at];
            if (d.c==unusedNode) continue;
            const Jet be{sign*(v(d.b)-v(d.e)),1,0},bc{sign*(v(d.b)-v(d.c)),0,1};
            const Jet f=junction(be,m.nf*thermal,m.is),r=junction(bc,m.nr*thermal,m.is);
            const Jet inf=be*(m.var ? 1.0/m.var : 0.0)+bc*(m.vaf ? 1.0/m.vaf : 0.0);
            Jet baseCharge=Jet{1}-inf;
            if (m.ikf || m.ikr) {
                const Jet knee=Jet{1}+(f*(m.ikf ? 1.0/m.ikf : 0.0)+r*(m.ikr ? 1.0/m.ikr : 0.0))*4;
                baseCharge=baseCharge/(Jet{0.5}+squareRoot(knee)*0.5);
            }
            const Jet transport=(f-r)*baseCharge;
            Jet ibf=f*(1/m.bf)+junction(be,m.ne*thermal,m.ise)+be*gmin;
            Jet ibr=r*(1/m.br)+junction(bc,m.nc*thermal,m.isc)+bc*gmin;
            if constexpr (m.cje!=0 || m.tf!=0 || m.cjc!=0 || m.tr!=0) {
                if constexpr (sizing) {
                    bjtStorage.push_back(at);
                    d.sign=sign;
                }
                if constexpr (m.cje!=0 || m.tf!=0) {
                    Jet qf=capacitance<m.mje>(be,m.cje,m.vje,m.fc);
                    if (m.tf && be.value>0) qf=qf+f*baseCharge*m.tf;
                    d.f=qf.value;
                    d.fq=qf;
                    ibf=ibf+qf*inverse;
                    ibf.value+=history1*d.f1+history2*d.f2;
                }
                if constexpr (m.cjc!=0 || m.tr!=0) {
                    const Jet qr=capacitance<m.mjc>(bc,m.cjc,m.vjc,m.fc)+r*m.tr;
                    d.r=qr.value;
                    d.rq=qr;
                    ibr=ibr+qr*inverse;
                    ibr.value+=history1*d.r1+history2*d.r2;
                }
            }
            terminal<sizing>(d.c,d.c,d.b,d.e,sign,transport-ibr,d.slots.data());
            terminal<sizing>(d.b,d.c,d.b,d.e,sign,ibf+ibr,d.slots.data()+3);
            terminal<sizing>(d.e,d.c,d.b,d.e,sign,(transport+ibf)*-1,d.slots.data()+6);
        }
        });
        stamp_jfets([&]<JModel m,int sign>(int begin,int count) {
        for (int at=begin;at<begin+count;++at) {
            const auto& d=deck.jfets[at];
            if (d.d==unusedNode) continue;
            const Jet gs{sign*(v(d.g)-v(d.s)),1,0},gd{sign*(v(d.g)-v(d.d)),0,1};
            const Jet ds=gs-gd;
            const bool reverse=ds.value<0;
            const Jet u=(reverse ? gd : gs)-Jet{m.vto};
            const Jet vd=ds*(reverse ? -1 : 1);
            const Jet beta=Jet{m.beta}+vd*(m.beta*m.lambda);
            const Jet ids=u.value<=0 ? Jet{0} : beta*(vd.value>=u.value ? u*u : vd*(u*2-vd));
            const Jet id=ids*(reverse ? -1 : 1);
            const Jet igs=junction(gs,thermal,m.is)+gs*gmin;
            const Jet igd=junction(gd,thermal,m.is)+gd*gmin;
            terminal<sizing>(d.d,d.d,d.g,d.s,sign,id-igd,d.slots.data());
            terminal<sizing>(d.g,d.d,d.g,d.s,sign,igs+igd,d.slots.data()+3);
            terminal<sizing>(d.s,d.d,d.g,d.s,sign,(id+igs)*-1,d.slots.data()+6);
        }
        });
    }

    // Scale the inductor voltage equations alongside the node currents.
    double merit() const {
        double total=0;
#if defined(MKII_CODE_SPEED)
        // Vectorize the products and sum them in the original row order.
        const auto* rhs=matrix.rhs.data();
        alignas(32) double squares[4];
        int i=0;
        for (;i+4<=deck.nodes;i+=4) {
            const auto y=_mm256_load_pd(rhs+i);
            _mm256_store_pd(squares,_mm256_mul_pd(y,y));
            total+=squares[0];
            total+=squares[1];
            total+=squares[2];
            total+=squares[3];
        }
        for (;i<deck.nodes;++i) total+=rhs[i]*rhs[i];
        for (;i<deck.size;++i) {
            const auto y=rhs[i]*0.005;
            total+=y*y;
        }
#else
        for (int i=0;i<deck.size;++i) {
            const auto y=matrix.rhs[i]*(i<deck.nodes?1:0.005);
            total+=y*y;
        }
#endif
        return total;
    }

    template<bool recovery=false>
    bool newton(int iterations) {
        stampStep();
        double residual=merit();
        double previousMerit=0,olderMerit=0;
        for (int iter=0;iter<iterations;++iter) {
            const auto initial=residual;
            if (initial<1e-9) return true;
            if (!matrix.solve()) return false;
            double maxNode=0, maxBranch=0;
            for (int i=0;i<deck.nodes;++i) maxNode=std::max(maxNode,std::abs(matrix.x[i]));
            for (int i=deck.nodes;i<deck.size;++i) maxBranch=std::max(maxBranch,std::abs(matrix.x[i]));
            const auto dx=[&](int n){return n<0?0:matrix.x[n];};
            const auto peakJunction=[&](double forward,double reverse) {
                double maximum=0;
                const auto compare=[&](int a,int b,int sign,double knee) {
                    const auto dv=sign*(dx(a)-dx(b));
                    const auto voltage=sign*(v(a)-v(b));
                    if (std::max(voltage,voltage+dv)>forward ||
                       (knee>0 && std::min(voltage,voltage+dv)<-knee+reverse))
                        maximum=std::max(maximum,std::abs(dv));
                };
                for (const auto& d:deck.diodes) if (d.a!=unusedNode) compare(d.a,d.b,1,d.knee);
                for (const auto& q:deck.bjts) if (q.c!=unusedNode) {compare(q.b,q.e,q.sign,0);compare(q.b,q.c,q.sign,0);}
                for (const auto& j:deck.jfets) if (j.d!=unusedNode) {compare(j.g,j.s,1,0);compare(j.g,j.d,1,0);}
                return maximum;
            };
            const bool close=fastConvergence
                ? (maxNode<0.055 && maxBranch<0.00011) || (maxNode<1.5 && peakJunction(0.22,0.07)<0.021)
                : maxNode<0.22 && peakJunction(0.22,0.07)<0.025;
            if (!recovery && close) {
                const auto delta=[&](int n){return n<0?0:matrix.x[n];};
                for (int i=0;i<deck.size;++i) x[i]+=matrix.x[i];
                for (const int at : diodeStorage) {
                    auto& d=deck.diodes[at];
                    d.q+=d.slope*(delta(d.a)-delta(d.b));
                }
                for (const int at : bjtStorage) {
                    auto& d=deck.bjts[at];
                    const double f=d.sign*(delta(d.b)-delta(d.e));
                    const double r=d.sign*(delta(d.b)-delta(d.c));
                    d.f+=d.fq.p*f+d.fq.q*r;
                    d.r+=d.rq.p*f+d.rq.q*r;
                }
                return true;
            }
            // Backtrack at cutoff and retain the accepted trial stamp
            // for the next factorization.
            oldIter=x;
            double meritLimit=initial;
            if constexpr (recovery) {
                meritLimit=std::max({initial,previousMerit,olderMerit});
                olderMerit=previousMerit;
                previousMerit=initial;
            }
            double scale=1;
            bool acceptable=false;
            for (;scale>=0.0001220703125;scale*=.5) {
                for (int i=0;i<deck.size;++i) x[i]=oldIter[i]+scale*matrix.x[i];
                stampStep();
                residual=merit();
                if (residual<(1-1e-5*scale)*meritLimit) {acceptable=true;break;}
            }
            if (!acceptable) return false;
        }
        return residual<(fastConvergence?1e-10:1e-12);
    }

    void accept(bool dc) {
        for (auto& d : deck.inductance) {
            const double q=d.value*v(d.column);
            d.q2=dc?q:d.q1;
            d.q1=q;
        }
        for (auto& d : deck.capacitors) {
            d.q=(v(d.a)-v(d.b))*d.value;
            d.q2=dc?d.q:d.q1;
            d.q1=d.q;
        }
        for (const int i : diodeStorage) {
            auto& d=deck.diodes[i];
            d.q2=dc?d.q:d.q1;
            d.q1=d.q;
        }
        for (const int i : bjtStorage) {
            auto& d=deck.bjts[i];
            d.f2=dc?d.f:d.f1;
            d.f1=d.f;
            d.r2=dc?d.r:d.r1;
            d.r1=d.r;
        }
        std::copy(x.begin(), x.end(), previous.begin());
    }

    struct Term { int column; double g; };
    int lane;
    int linkNode=-1;
    const double linkConductance;
    double peerLinkVoltage=0;
    bool partitionValid=true;
    Circuit deck;
    std::vector<Term> terms;
    struct KnownTerm {int row,column;double g;};
    struct LinearRow {int row,begin,end;};
    std::vector<KnownTerm> knownTerms;
    std::vector<LinearRow> linearRows;
#if !defined(MKII_NATIVE_GENERATE_SOLVER) && __has_include("GeneratedSolve.hpp")
    GeneratedResidual generatedResidual=nullptr;
#endif
    std::vector<int> diodeStorage,bjtStorage;
    std::vector<double> constantRhs,sampleRhs;
    Sparse matrix,retryMatrix;
    std::vector<double> x,previous,fixed,savedNodes;
    std::vector<History> savedHistory;
    double inverse=0,history1=0,history2=0;
    double hostRate=0;
    std::array<Guess,64> guesses;
    unsigned guessCount=0,nextGuess=0;
    bool fastConvergence=false;
    std::vector<double> oldIter;
    Frame lastInput{},slope{};
    std::vector<double> older;
    Controls lastControls=defaults;
    unsigned misses=0,subdivisions=0;

};
}
