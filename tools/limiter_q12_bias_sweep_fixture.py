#!/usr/bin/env python3
"""Isolated EX11475 limiter diagnosis, Diodes Incorporated 1N4148W bridge.

Use after diodesinc_bridge_fixture.py w. Each case edits temporary runner
checkout only. Actual C, R, Q, diode parameters in the production branch
remain unchanged. Q12 BF is changed ONLY for Q12, not other BC184Cs.
"""
from pathlib import Path
import re,sys
case=sys.argv[1]
valid={"baseline","q12-bf250","q12-bf650","q12-bf800",
       "law-002","law-006","rv4-012","rv4-022",
       "c14-8u","c14-12u"}
if case not in valid:raise SystemExit("Unknown diagnostic variant: "+case)
board=Path("circuit/ex11475.inc")
devices=Path("circuit/devices.inc")
top=Path("circuit/609.cir")
b=board.read_text()
d=devices.read_text()
t=top.read_text()
if case.startswith("q12-bf"):
    bf=int(case.split("bf")[-1])
    match=re.search(r"(?m)^\.model QBC184C NPN\(([^\n]+)\)",d)
    if not match:raise RuntimeError("Existing BC184C SPICE model not found")
    params=match.group(1)
    if not re.search(r"\bBF=455\b",params):raise RuntimeError("BC184C baseline BF changed")
    custom=re.sub(r"\bBF=455\b",f"BF={bf}",params)
    d+="\n* Test-only isolated Q12 variant; no other BC184C changed\n.model QBC184C_Q12 NPN("+custom+")\n"
    b,n=re.subn(r"(?m)^Q12 SCV QL12B QL12E QBC184C$",
                 "Q12 SCV QL12B QL12E QBC184C_Q12",b)
    if n!=1:raise RuntimeError("Q12 instance topology differs from expected")
elif case.startswith("law-"):
    value={"law-002":"0.02","law-006":"0.06"}[case]
    lines=t.splitlines(keepends=True)
    pos=next(i for i,line in enumerate(lines) if line.startswith(".param PRESET="))
    if "LAW=0.04" not in lines[pos]:raise RuntimeError("Factory LAW baseline differs")
    lines[pos]=lines[pos].replace("LAW=0.04","LAW="+value,1)
    t="".join(lines)
elif case.startswith("rv4-"):
    value={"rv4-012":"0.12","rv4-022":"0.22"}[case]
    lines=t.splitlines(keepends=True)
    pos=next(i for i,line in enumerate(lines) if line.startswith(".param PRESET="))
    if "LIMCAL=0.166966" not in lines[pos]:raise RuntimeError("Factory RV4 baseline differs")
    lines[pos]=lines[pos].replace("LIMCAL=0.166966","LIMCAL="+value,1)
    t="".join(lines)
elif case.startswith("c14-"):
    value={"c14-8u":"8u","c14-12u":"12u"}[case]
    b,n=re.subn(r"(?m)^C14 LIMIT_ENV GND 10u$",
                 "C14 LIMIT_ENV GND "+value,b)
    if n!=1:raise RuntimeError("Unexpected C14 match count")
board.write_text(b)
devices.write_text(d)
top.write_text(t)
assert all(re.search(rf"(?m)^D{k} \S+ \S+ DI_TEST_4148$",b) for k in (14,15,16,17))
print("DIAG_VARIANT",case)
print("Diodes Inc 1N4148W D14-D17 retained; all other changes isolated to requested single parameter")
