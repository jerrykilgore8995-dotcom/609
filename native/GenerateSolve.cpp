#define MKII_NATIVE_GENERATE_SOLVER
#include "Circuit.hpp"

#include <fstream>

int main(int argc,char** argv) {
    if (argc!=2) return 2;
    std::ofstream out(argv[1]);
    if (!out) return 3;

    struct Schedule {int rate,lane;const char* name;};
#ifdef CLASSIC_MODEL
    constexpr Schedule schedules[]={
        {132300,0,"left132300"},{132300,1,"right132300"},
        {144000,0,"left144000"},{144000,1,"right144000"},
        {176400,0,"left176400"},{176400,1,"right176400"},
        {192000,0,"left192000"},{192000,1,"right192000"}};
#else
    constexpr Schedule schedules[]={{44100,-1,"exact"},{44100,0,"left44100"},{44100,1,"right44100"}};
#endif
    out<<"// Generated sparse elimination schedules for the native circuit.\n";
    for (const auto& schedule:schedules) {
        mkii::native::Solver solver(schedule.lane);
        if (!solver.prepare(schedule.rate)) return 4;
        solver.emitSolver(out,schedule.name);
    }
    out<<"using GeneratedFunction = bool(*)(double*,double*,double*,const double*);\n"
         "inline GeneratedFunction findGeneratedSolve(std::uint64_t signature) {\n";
    for (const auto& schedule:schedules)
        out<<"  if (signature==signature_"<<schedule.name<<") return solve_"<<schedule.name<<";\n";
    out<<"  return nullptr;\n"
         "}\n"
         "inline PivotOrder findGeneratedOrder(double rate,int lane) {\n";
    for (const auto& schedule:schedules)
        out<<"  if (rate=="<<schedule.rate<<" && lane=="<<schedule.lane
           <<") return order_"<<schedule.name<<";\n";
    out<<"  return {};\n"
         "}\n"
         "using GeneratedResidual = void(*)(double*,const double*);\n"
         "inline GeneratedResidual findGeneratedResidual(double rate,int lane) {\n";
    for (const auto& schedule:schedules)
        out<<"  if (rate=="<<schedule.rate<<" && lane=="<<schedule.lane
           <<") return rhs_"<<schedule.name<<";\n";
    out<<"  return nullptr;\n"
         "}\n";
    return out ? 0 : 5;
}
