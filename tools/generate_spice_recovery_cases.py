#!/usr/bin/env python3
"""Independent SPICE check of original and factory charging resistors."""
from pathlib import Path
from shutil import copyfile
source=Path("circuit/609.cir").read_text()
for recovery in (1,4):
 for variant in ("fitted","schematic"):
  folder=Path("spice-cases")/f"rec{recovery}-{variant}"
  folder.mkdir(parents=True,exist_ok=True)
  for inc in Path("circuit").glob("*.inc"):
   c=inc.read_text()
   if inc.name=="ex11475.inc" and variant=="schematic":
    c=c.replace("R28 QL12E ATTACK_TAP 680","R28 QL12E ATTACK_TAP 1600")
    c=c.replace("R30 ATTACK_TAP LIMIT_ENV 820","R30 ATTACK_TAP LIMIT_ENV 1600")
   (folder/inc.name).write_text(c)
  c=source
  c=c.replace(".param PRESET=0.226248 LAW=0.35 MTR=0.5 LIMCAL=0.166966 COMPCAL=0.4073 BIAS=0.445245",
     ".param PRESET=0.345 LAW=0.04 MTR=0.5 LIMCAL=0.166966 COMPCAL=0.30 BIAS=0.445245")
  c=c.replace("LEL=0 FASTL=0","LEL=1 FASTL=0").replace("LRL=1 CEL=0",f"LRL={recovery} CEL=0")
  c=c.replace("VINL SL 0 SIN(0 {AMP_L} {FREQ}) AC 1",
     "BINPUT SL 0 V=sin(2*3.141592653589793*1000*time)*(0.1995262315+0.4314311135*u(4-time))")
  control=f"""
.control
set noaskquit
set wr_singlescale
set wr_vecnames
tran 100u 6
wrdata ../../results/spice/spice_{recovery}_{variant}.txt v(xleft.xboard.limit_env) v(xleft.xboard.ql7c) v(xleft.xboard.ql12b)
quit
.endc
.end
"""
  if "\n.end\n" not in c:raise RuntimeError("missing deck end")
  c=c.replace("\n.end\n",control,1)
  (folder/"run.cir").write_text(c)
  print(folder/"run.cir")
