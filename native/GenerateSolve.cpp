#define MKII_NATIVE_GENERATE_SOLVER
#include "Circuit.hpp"

#include <fstream>

int main(int argc,char** argv) {
    if (argc!=2) return 2;
    std::ofstream out(argv[1]);
    if (!out) return 3;

    struct Schedule {int rate,lane;double conductance;const char* name;};
    constexpr Schedule schedules[]={{44100,0,0.0,"leftDualMono"},{44100,1,0.0,"rightDualMono"},
        {44100,0,0.045,"left22"},{44100,1,0.045,"right22"},
        {44100,0,0.1,"left10"},{44100,1,0.1,"right10"}};
    out<<"// Generated sparse elimination schedules for the native circuit.\n";
    for (const auto& schedule:schedules) {
        mkii::native::Solver solver(schedule.lane,schedule.conductance);
        if (!solver.prepare(schedule.rate)) return 4;
        solver.emitSolver(out,schedule.name);
    }
    out<<"using GeneratedFunction = bool(*)(double*,double*,double*,const double*);\n"
         "inline GeneratedFunction findGeneratedSolve(std::uint64_t signature) {\n";
    for (const auto& schedule:schedules)
        out<<"  if (signature==signature_"<<schedule.name<<") return solve_"<<schedule.name<<";\n";
    out<<"  return nullptr;\n"
         "}\n"
         "inline PivotOrder findGeneratedOrder(double rate,int lane,double conductance) {\n";
    for (const auto& schedule:schedules)
        out<<"  if (rate=="<<schedule.rate<<" && lane=="<<schedule.lane<<" && conductance=="<<schedule.conductance
           <<") return order_"<<schedule.name<<";\n";
    out<<"  return {};\n"
         "}\n"
         "using GeneratedResidual = void(*)(double*,const double*);\n"
         "inline GeneratedResidual findGeneratedResidual(double rate,int lane,double conductance) {\n";
    for (const auto& schedule:schedules)
        out<<"  if (rate=="<<schedule.rate<<" && lane=="<<schedule.lane<<" && conductance=="<<schedule.conductance
           <<") return rhs_"<<schedule.name<<";\n";
    out<<"  return nullptr;\n"
         "}\n";
    return out ? 0 : 5;
}
