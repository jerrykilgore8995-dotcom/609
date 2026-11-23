#!/usr/bin/env python3
"""Isolated two-dimensional RV2/RV4 factory control sweep.

Use after diodesinc_bridge_fixture.py w. Modify only the top-level control
LAW (RV2=5000*LAW) and LIMCAL (RV4=10000*LIMCAL) in circuit/609.cir.
Keep the original compressor, Q12, output amplifier and all hardware
diode parameters untouched.
"""
from pathlib import Path
import sys,re
if len(sys.argv)!=3:raise SystemExit("Usage: sweep_fixture.py RV2_OHMS RV4_OHMS")
rv2=int(sys.argv[1]);rv4=int(sys.argv[2])
if rv2 not in (100,150,200,250,300):raise SystemExit("RV2 invalid")
if rv4 not in (1200,1670,2200):raise SystemExit("RV4 invalid")
p=Path("circuit/609.cir")
text=p.read_text()
pattern=r"(?m)^(\.param PRESET=0\.345 )LAW=0\.04( MTR=0\.5 )LIMCAL=0\.166966( COMPCAL=0\.3 BIAS=0\.445245)$"
replace=rf"\g<1>LAW={rv2/5000:.7f}\g<2>LIMCAL={rv4/10000:.7f}\g<3>"
text,n=re.subn(pattern,replace,text)
if n!=1:raise SystemExit("Factory defaults differ; refusing circuit modification")
p.write_text(text)
print(f"DIAG_ONLY RV2={rv2} ohm RV4={rv4} ohm, original compressor untouched")
