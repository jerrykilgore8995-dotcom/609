#!/usr/bin/env python3
"""Independent ngspice circuit-vs-nodal check; not a plugin change."""
import pathlib,re,sys
source=pathlib.Path("circuit/609.cir").read_text()
out=pathlib.Path("results/spice");out.mkdir(parents=True,exist_ok=True)
for rec in (1,4):
 for version in ("fitted","schematic"):
  circuit=source
  circuit=circuit.replace(".param PRESET=0.226248 LAW=0.35 MTR=0.5 LIMCAL=0.166966 COMPCAL=0.4073 BIAS=0.445245",
    ".param PRESET=0.345 LAW=0.04 MTR=0.5 LIMCAL=0.166966 COMPCAL=0.30 BIAS=0.445245")
  circuit=circuit.replace("LEL=0 FASTL=0","LEL=1 FASTL=0").replace("LRL=1 CEL=0",f"LRL={rec} CEL=0")
  circuit=circuit.replace("VINL SL 0 SIN(0 {AMP_L} {FREQ}) AC 1",
   "BINPUT SL 0 V=sin(2*3.141592653589793*1000*time)*(0.1995262315+0.4314311135*u(4-time))")
  circuit=circuit.replace("\n.end\n",""".control
set noaskquit
set wr_singlescale
set wr_vecnames
tran 100u 6
wrdata ../results/spice/spice_"""+str(rec)+"_"+version+""".csv v(xleft.xboard.limit_env) v(xleft.xboard.ql7c) v(xleft.xboard.ql12b)
quit
.endc
.end\n""",1)
  outname=f"circuit/609-spice-rec{rec}-{version}.cir"
  pathlib.Path(outname).write_text(circuit)
  if version=="schematic":
   p=pathlib.Path("circuit/ex11475.inc")
   # This version is changed only in a fresh per-case working directory by the workflow.
  print(outname)
