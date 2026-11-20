#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <bit>
#include <cstring>
#include <new>
#if defined(MKII_CODE_SPEED)
#include <immintrin.h>
#endif
#include <cstdint>
#include <ostream>
#include <span>
#include <string_view>
#include <vector>

namespace mkii::native {
#if defined(MKII_CODE_SPEED)
template<class T> struct CacheAllocator {
    using value_type=T;
    CacheAllocator()=default;
    template<class U> CacheAllocator(const CacheAllocator<U>&) noexcept {}
    T* allocate(std::size_t count) {
        return static_cast<T*>(::operator new(count*sizeof(T),std::align_val_t{64}));
    }
    void deallocate(T* values,std::size_t) noexcept {::operator delete(values,std::align_val_t{64});}
    template<class U> bool operator==(const CacheAllocator<U>&) const noexcept {return true;}
};
using SolveBuffer=std::vector<double,CacheAllocator<double>>;
#else
using SolveBuffer=std::vector<double>;
#endif

inline bool finiteSolution(const double* values,int count) {
#if defined(MKII_CODE_SPEED)
    constexpr std::uint64_t exponent=0x7ff0000000000000ULL;
    const auto mask=_mm256_set1_epi64x(exponent);
    auto invalid=_mm256_setzero_si256();
    int i=0;
    for (;i+4<=count;i+=4) {
        const auto bits=_mm256_castpd_si256(_mm256_load_pd(values+i));
        invalid=_mm256_or_si256(invalid,_mm256_cmpeq_epi64(_mm256_and_si256(bits,mask),mask));
    }
    if (!_mm256_testz_si256(invalid,invalid)) return false;
    for (;i<count;++i)
        if ((std::bit_cast<std::uint64_t>(values[i])&exponent)==exponent) return false;
    return true;
#else
    bool finite=true;
    for (int i=0;i<count;++i) finite &= std::isfinite(values[i]);
    return finite;
#endif
}

using PivotOrder=std::span<const std::array<int,2>>;
#if !defined(MKII_NATIVE_GENERATE_SOLVER) && __has_include("GeneratedSolve.hpp")
#include "GeneratedSolve.hpp"
#endif

// Choose and factor the symbolic pattern at prepareToPlay. The audio thread
// reuses a fixed elimination schedule with no searches, allocation or fill-in.
class Sparse {
public:
    void resize(int n) {
        size = n;
        lookup.assign(n * n, -1);
        dense.assign(n * n, 0);
        rhs.assign(n, 0);
        x.assign(n, 0);
        ready = false;
    }

    void add(int row, int column, double value) {
        if (row < 0 || column < 0) return;
        const int offset = row * size + column;
        if (!ready) {
            lookup[offset] = 0;
            dense[offset] += value;
        } else {
            const int slot = lookup[offset];
            if (slot >= 0) entries[slot] += value;
        }
    }

    bool prepare(PivotOrder generatedOrder={}) {
        if (!generatedOrder.empty() && generatedOrder.size()!=static_cast<std::size_t>(size)) return false;
        rows.clear();
        updates.clear();
        activePivots.clear();
        std::vector<bool> liveRows(size, true), liveColumns(size, true);
        const auto order=generatedOrder.empty()?reverseCuthillMcKee():std::vector<int>{};
        for (int step = 0; step < size; ++step) {
            int pr=-1, pc=-1;
            if (!generatedOrder.empty()) {
                pr=generatedOrder[step][0];
                pc=generatedOrder[step][1];
                if (pr<0 || pr>=size || pc<0 || pc>=size || !liveRows[pr] ||
                    !liveColumns[pc] || !has(pr,pc))
                    return false;
                const double pivot=v(pr,pc);
                if (std::abs(pivot)<1e-17 || !std::isfinite(pivot)) return false;
            } else {
                std::vector<int> rowCount(size, 0), colCount(size, 0);
                std::vector<double> colMax(size, 0), rowMax(size, 0);
                for (int row = 0; row < size; ++row) if (liveRows[row]) {
                    for (int col = 0; col < size; ++col) if (liveColumns[col] && has(row,col)) {
                        ++rowCount[row];
                        ++colCount[col];
                        colMax[col] = std::max(colMax[col], std::abs(v(row,col)));
                        rowMax[row] = std::max(rowMax[row], std::abs(v(row,col)));
                    }
                }
                int best = 2*size*size;
                double tie = -1.0;
                for (int row : order) if (liveRows[row]) {
                    for (int col : order) if (liveColumns[col] && has(row,col)) {
                        const double pivot = std::abs(v(row,col));
                        if (pivot < 1e-17 || pivot < 0.01 * colMax[col]) continue;
                        // Prefer self-admittances to off-diagonal transistor
                        // gains which can disappear as the device cuts off.
                        const int cost = (rowCount[row]-1) * (colCount[col]-1) + (row!=col?size*size:0);
                        const double scaled = pivot / (rowMax[row] + 1e-30);
                        if (cost < best || (cost == best && scaled > tie)) {
                            best = cost;
                            tie = scaled;
                            pr = row;
                            pc = col;
                        }
                    }
                }
            }
            if (pr < 0) return false;
            liveRows[pr] = liveColumns[pc] = false;
            Row s{};
            s.row = pr;
            s.column = pc;
            for (int col = 0; col < size; ++col)
                if (liveColumns[col] && has(pr,col)) s.upper.push_back({col, 0});
            for (int row = 0; row < size; ++row) {
                if (!liveRows[row] || !has(row,pc)) continue;
                s.lower.push_back({row, 0});
                const double factor = v(row,pc)/v(pr,pc);
                for (const auto& upper : s.upper) {
                    lookup[row*size + upper.index] = 0;
                    v(row,upper.index) -= factor * v(pr,upper.index);
                }
            }
            rows.push_back(std::move(s));
        }
        int count = 0;
        for (auto& slot : lookup) if (slot >= 0) slot = count++;
        entries.assign(count+1, 0);
        plan.clear();
        uppers.clear();
        lowers.clear();
        for (auto& row : rows) {
            const int upperBegin=static_cast<int>(uppers.size());
            const int lowerBegin=static_cast<int>(lowers.size());
            for (auto& upper : row.upper) {
                upper.slot = at(row.row,upper.index);
                uppers.push_back(upper);
            }
            for (auto& lower : row.lower) {
                lower.slot = at(lower.index,row.column);
                lowers.push_back(lower);
                for (const auto& upper : row.upper)
                    updates.push_back(at(lower.index,upper.index));
            }
            plan.push_back({row.row,row.column,at(row.row,row.column),upperBegin,
                            static_cast<int>(uppers.size()),lowerBegin,static_cast<int>(lowers.size())});
        }
        rows.clear();
        dense.clear();
        ready = true;
        return true;
    }

    void configureLinear(const std::vector<int>& mutableSlots,const std::vector<int>& contacts) {
        linearEntries=entries;
        circuitSlots=mutableSlots;
        allMutableSlots=mutableSlots;
        allMutableSlots.insert(allMutableSlots.end(),contacts.begin(),contacts.end());
    }

    void resetLinearEntries() { entries=linearEntries; }
    bool cacheStaticContacts() {
        dynamicContacts=!cacheLinearPart(circuitSlots);
        return !dynamicContacts;
    }
    bool cacheDynamicContacts() {
        dynamicContacts=true;
        return cacheLinearPart(allMutableSlots);
    }
    bool contactsAreDynamic() const { return dynamicContacts; }

    bool cacheLinearPart(const std::vector<int>& mutableSlots) {
        std::vector<bool> dynamic(entries.size(),false);
        for (const int slot : mutableSlots) dynamic[slot]=true;
        runtime.clear();
        activePivots.clear();
        activeUpdates.clear();
        factors.assign(plan.size()+lowers.size(),0);
        int nextUpdate=0;
        for (const auto& s : plan) {
            const bool pivotChanges=dynamic[s.pivot];
            const double pivot=entries[s.pivot];
            if (!pivotChanges && (std::abs(pivot)<1e-17 || !std::isfinite(pivot))) return false;
            const double reciprocal=pivotChanges?0:1/entries[s.pivot];
            factors[activePivots.size()]=reciprocal;
            const int begin=static_cast<int>(runtime.size());
            for (int j=s.lowerBegin;j<s.lowerEnd;++j) {
                const auto& row=lowers[j];
                const bool factorChanges=pivotChanges||dynamic[row.slot];
                const double factor=factorChanges?0:entries[row.slot]*reciprocal;
                const int first=static_cast<int>(activeUpdates.size());
                for (int k=s.upperBegin;k<s.upperEnd;++k) {
                    const int target=updates[nextUpdate++];
                    const int source=uppers[k].slot;
                    if (!factorChanges && !dynamic[source]) entries[target]-=factor*entries[source];
                    else {
                        dynamic[target]=true;
                        activeUpdates.push_back({target,source});
                    }
                }
                factors[plan.size()+runtime.size()]=factor;
                runtime.push_back({row.index,row.slot,first,static_cast<int>(activeUpdates.size()),
                                   factor,factorChanges});
            }
            activePivots.push_back({s.row,s.pivot,begin,static_cast<int>(runtime.size()),
                                    reciprocal,pivotChanges});
        }
        fixed=entries;
        // The generated code reads numeric factors from this instance. Only
        // the shape of the elimination schedule belongs in its signature.
        signature=0xcbf29ce484222325ULL;
        const auto add=[&](std::uint64_t value) {
            signature^=value;
            signature*=0x100000001b3ULL;
        };
        add(size);
        for (const auto& row : activePivots) {
            for (int v : {row.row,row.pivot,row.begin,row.end}) add(v);
            add(row.changes);
        }
        for (const auto& row : runtime) {
            for (int v : {row.index,row.slot,row.first,row.last}) add(v);
            add(row.changes);
        }
        for (const auto& e : activeUpdates) {add(e.index);add(e.slot);}
        for (const auto& s : plan) {
            for (int v : {s.row,s.column,s.pivot,s.upperBegin,s.upperEnd}) add(v);
        }
        for (const auto& e : uppers) {add(e.index);add(e.slot);}
#if !defined(MKII_NATIVE_GENERATE_SOLVER) && __has_include("GeneratedSolve.hpp")
        generated=findGeneratedSolve(signature);
#endif
        return true;
    }

    void startIteration(const std::vector<double>& initial) {
#if defined(MKII_CODE_SPEED)
        std::size_t i=0;
        for (;i+4<=fixed.size();i+=4)
            _mm256_store_pd(entries.data()+i,_mm256_load_pd(fixed.data()+i));
        std::copy(fixed.begin()+i,fixed.end(),entries.begin()+i);
        std::memcpy(rhs.data(),initial.data(),initial.size()*sizeof(double));
#else
        std::copy(fixed.begin(),fixed.end(),entries.begin());
        std::copy(initial.begin(),initial.end(),rhs.begin());
#endif
    }

    void clear() {
        std::fill(entries.begin(), entries.end(), 0.0);
        std::fill(rhs.begin(), rhs.end(), 0.0);
    }

    void emit(std::ostream& code,std::string_view name) const {
        code<<"inline constexpr std::array<int,2> order_"<<name<<"[]={\n";
        for (const auto& pivot : plan) code<<"    {"<<pivot.row<<","<<pivot.column<<"},\n";
        code<<"};\n";
        code<<"inline constexpr std::uint64_t signature_"<<name<<'='<<signature<<"ULL;\n";
        code<<"inline bool solve_"<<name<<"(double* __restrict a,double* __restrict b,double* __restrict x,const double* __restrict factors) {\n";
        code<<"    double u=0,v=0;\n";
        for (const auto& s : activePivots) {
            if (s.changes) {
                code<<"    u=a["<<s.pivot<<"]; if (std::abs(u)<1e-17 || !std::isfinite(u)) return false; "
                    <<"u=1/u; a["<<s.pivot<<"]=u;\n";
            }
            const auto pivotIndex=&s-activePivots.data();
            for (int j=s.begin;j<s.end;++j) {
                const auto& row=runtime[j];
                if (row.changes) {
                    code<<"    v=a["<<row.slot<<"]*";
                    if (s.changes) code<<"u";
                    else code<<"factors["<<pivotIndex<<"]";
                    code<<";\n";
                }
                else code<<"    v=factors["<<plan.size()+j<<"];\n";
                code<<"    b["<<row.index<<"]-=v*b["<<s.row<<"];\n";
                for (int k=row.first;k<row.last;++k)
                    code<<"    a["<<activeUpdates[k].index<<"]-=v*a["<<activeUpdates[k].slot<<"];\n";
            }
        }
        for (auto s=plan.rbegin();s!=plan.rend();++s) {
            code<<"    v=b["<<s->row<<"];\n";
            for (int k=s->upperBegin;k<s->upperEnd;++k)
                code<<"    v-=a["<<uppers[k].slot<<"]*x["<<uppers[k].index<<"];\n";
            const auto pivotIndex=(s+1).base()-plan.begin();
            const auto& forward=activePivots[pivotIndex];
            if (forward.changes) code<<"    x["<<s->column<<"]=v*a["<<s->pivot<<"];\n";
            else code<<"    x["<<s->column<<"]=v*factors["<<pivotIndex<<"];\n";

        }
        code<<"    return finiteSolution(x,"<<size<<");\n}\n";
    }

    bool solve() {
#if !defined(MKII_NATIVE_GENERATE_SOLVER) && __has_include("GeneratedSolve.hpp")
        if (generated) return generated(entries.data(),rhs.data(),x.data(),factors.data());
#endif
        const auto* upper=uppers.data();
        auto* values=entries.data();
        auto* vector=rhs.data();
        for (const auto& s : activePivots) {
            double reciprocal=s.reciprocal;
            if (s.changes) {
                const double pivot=values[s.pivot];
                if (std::abs(pivot)<1e-17 || !std::isfinite(pivot)) return false;
                reciprocal=1.0/pivot;
            }
            values[s.pivot]=reciprocal;
            const double y=vector[s.row];
            for (int j=s.begin;j<s.end;++j) {
                const auto& row=runtime[j];
                const double factor=row.changes?values[row.slot]*reciprocal:row.factor;
                vector[row.index]-=factor*y;
                for (int k=row.first;k<row.last;++k)
                    values[activeUpdates[k].index]-=factor*values[activeUpdates[k].slot];
            }
        }
        for (auto s=plan.rbegin();s!=plan.rend();++s) {
            double y=vector[s->row];
            for (int k=s->upperBegin;k<s->upperEnd;++k)
                y-=values[upper[k].slot]*x[upper[k].index];
            x[s->column]=y*values[s->pivot];
            if (!std::isfinite(x[s->column])) return false;
        }
        return true;
    }

    int slot(int row,int col) const {
        if (row<0 || col<0) return static_cast<int>(entries.size()-1);
        const int i=at(row,col);
        return i<0?static_cast<int>(entries.size()-1):i;
    }
    void addSlot(int slot,double value) { entries[slot]+=value; }

    bool usesGeneratedSolve() const {
#if !defined(MKII_NATIVE_GENERATE_SOLVER) && __has_include("GeneratedSolve.hpp")
        return generated!=nullptr;
#else
        return false;
#endif
    }

    int operationCount() const { return static_cast<int>(activeUpdates.size()); }
    SolveBuffer rhs, x;

private:
    std::vector<int> reverseCuthillMcKee() const {
        std::vector<int> order;
        // Reverse Cuthill–McKee breaks equal-cost, equally stable pivot
        // ties. Markowitz and the pivot guards still control fill and stability.
        std::vector<std::vector<int>> neighbors(size);
        for (int row=0;row<size;++row) for (int col=0;col<size;++col)
            if (row!=col && (has(row,col) || has(col,row))) neighbors[row].push_back(col);
        const auto degree=[&](int a,int b){return neighbors[a].size()<neighbors[b].size();};
        for (auto& node : neighbors) std::stable_sort(node.begin(),node.end(),degree);
        std::vector<bool> visited(size);
        while (order.size()<static_cast<std::size_t>(size)) {
            int root=-1;
            for (int i=0;i<size;++i)
                if (!visited[i] && (root<0 || degree(i,root))) root=i;
            auto next=order.size();
            order.push_back(root);
            visited[root]=true;
            for (;next<order.size();++next)
                for (int n : neighbors[order[next]]) if (!visited[n]) {
                    order.push_back(n);
                    visited[n]=true;
                }
        }
        std::reverse(order.begin(),order.end());
        return order;
    }

    struct Terminal { int index, slot; };
    struct Row {
        int row, column;
        std::vector<Terminal> upper, lower;
    };
    int at(int row, int col) const { return lookup[row*size+col]; }
    bool has(int row, int col) const { return at(row,col) >= 0; }
    double& v(int row, int col) { return dense[row*size+col]; }

    std::uint64_t signature=0;
#if !defined(MKII_NATIVE_GENERATE_SOLVER) && __has_include("GeneratedSolve.hpp")
    GeneratedFunction generated=nullptr;
#endif
    int size = 0;
    bool ready = false;
    bool dynamicContacts=false;
    std::vector<int> lookup, updates;
    std::vector<Row> rows;
    struct Pivot { int row,column,pivot,upperBegin,upperEnd,lowerBegin,lowerEnd; };
    std::vector<Pivot> plan;
    struct ActiveLower { int index,slot,first,last;double factor;bool changes; };
    struct ActivePivot { int row,pivot,begin,end;double reciprocal;bool changes; };
    std::vector<ActiveLower> runtime;
    std::vector<ActivePivot> activePivots;
    std::vector<Terminal> uppers,lowers,activeUpdates;
    std::vector<int> circuitSlots,allMutableSlots;
    std::vector<double> dense;
    SolveBuffer entries,fixed,factors,linearEntries;
};
}
