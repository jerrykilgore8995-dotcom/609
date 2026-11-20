#define MKII_NATIVE_GENERATE_SOLVER
#include "Circuit.hpp"

#include <fstream>

int main(int argc,char** argv) {
    if (argc!=2) return 2;
    std::ofstream out(argv[1]);
    if (!out) return 3;
    mkii::native::Solver solver;
    out<<"// Generated sparse elimination schedules for the native circuit.\n";
    if (!solver.prepare(44100)) return 4;
    solver.emitSolver(out,"hz44100");
    if (!solver.prepare(48000)) return 4;
    solver.emitSolver(out,"hz48000");
    if (!solver.prepare(96000)) return 4;
    solver.emitSolver(out,"hz96000");
    out<<"using GeneratedFunction = bool(*)(double*,double*,double*,const double*);\n"
         "inline GeneratedFunction findGeneratedSolve(std::uint64_t signature) {\n"
         "  if (signature==signature_hz44100) return solve_hz44100;\n"
         "  if (signature==signature_hz48000) return solve_hz48000;\n"
         "  if (signature==signature_hz96000) return solve_hz96000;\n"
         "  return nullptr;\n"
         "}\n"
         "inline PivotOrder findGeneratedOrder(double rate) {\n"
         "  if (rate==44100) return order_hz44100;\n"
         "  if (rate==48000) return order_hz48000;\n"
         "  if (rate==96000) return order_hz96000;\n"
         "  return {};\n"
         "}\n"
         "using GeneratedResidual = void(*)(double*,const double*);\n"
         "inline GeneratedResidual findGeneratedResidual(double rate) {\n"
         "  if (rate==44100) return rhs_hz44100;\n"
         "  if (rate==48000) return rhs_hz48000;\n"
         "  if (rate==96000) return rhs_hz96000;\n"
         "  return nullptr;\n"
         "}\n";
    return out ? 0 : 5;
}
