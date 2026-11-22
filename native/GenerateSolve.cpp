#define MKII_NATIVE_GENERATE_SOLVER
#include "Circuit.hpp"

#include <fstream>
#include <iostream>

int main(int argc,char** argv) {
    if (argc!=2) return 2;
    std::ofstream out(argv[1]);
    if (!out) return 3;

    struct Schedule {int rate,lane;const char* name;};
    constexpr Schedule schedules[]={{44100,-1,"exact"},{88200,0,"left88200"},{88200,1,"right88200"},
        {96000,0,"left96000"},{96000,1,"right96000"},{192000,0,"left192000"},{192000,1,"right192000"}};
    out<<"// Generated sparse elimination schedules for the native circuit.\n";
    // Keep the exact MNA circuit when an unrolled schedule cannot be generated.
    // The non-generated numerical Sparse solver remains available.
    std::array<bool,std::size(schedules)> generated{};
    for (std::size_t i=0;i<std::size(schedules);++i) {
        const auto& schedule=schedules[i];
        mkii::native::Solver solver(schedule.lane);
        if (!solver.prepare(schedule.rate)) {
            std::cerr << "Generated schedule unavailable rate=" << schedule.rate
                      << " lane=" << schedule.lane
                      << "; using native numerical solver instead\\n";
            continue;
        }
        solver.emitSolver(out,schedule.name);
        generated[i]=true;
    }
    out<<"using GeneratedFunction = bool(*)(double*,double*,double*,const double*);\n"
         "inline GeneratedFunction findGeneratedSolve(std::uint64_t signature) {\n";
    for (std::size_t i=0;i<std::size(schedules);++i) if (generated[i]) {
        const auto& schedule=schedules[i];
        out<<"  if (signature==signature_"<<schedule.name<<") return solve_"<<schedule.name<<";\n";
    }
    out<<"  return nullptr;\n"
         "}\n"
         "inline PivotOrder findGeneratedOrder(double rate,int lane) {\n";
    for (std::size_t i=0;i<std::size(schedules);++i) if (generated[i]) {
        const auto& schedule=schedules[i];
        out<<"  if (rate=="<<schedule.rate<<" && lane=="<<schedule.lane
           <<") return order_"<<schedule.name<<";\n";
    }
    out<<"  return {};\n"
         "}\n"
         "using GeneratedResidual = void(*)(double*,const double*);\n"
         "inline GeneratedResidual findGeneratedResidual(double rate,int lane) {\n";
    for (std::size_t i=0;i<std::size(schedules);++i) if (generated[i]) {
        const auto& schedule=schedules[i];
        out<<"  if (rate=="<<schedule.rate<<" && lane=="<<schedule.lane
           <<") return rhs_"<<schedule.name<<";\n";
    }
    out<<"  return nullptr;\n"
         "}\n";
    return out ? 0 : 5;
}
