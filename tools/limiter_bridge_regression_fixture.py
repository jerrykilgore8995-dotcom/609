#!/usr/bin/env python3
"""Test ONLY bridge diode substitution: original fitted D1N4153 versus manufacturer W.

Both tests use identical current 609 circuit, limiter settings and DSP solver.
No changes to RV2, RV4, Q12, C14 or compressor values. Runner-local only.
"""
from pathlib import Path
import re,sys
mode=sys.argv[1]
assert mode in {"oldfit","diw"}
if mode=="diw":
 import subprocess
 subprocess.run([sys.executable,"tools/diodesinc_bridge_fixture.py","w"],check=True)
else:
 board=Path("circuit/ex11475.inc")
 s=board.read_text()
 for i in (14,15,16,17):
  s,n=re.subn(rf"(?m)^XV{i} (\\S+) (\\S+) VISHAY4148$",rf"D{i} \\1 \\2 D1N4153",s)
  assert n==1,(i,n)
 board.write_text(s)
 d=Path("circuit/devices.inc")
 text=d.read_text()
 if ".model D1N4153 " in text:raise RuntimeError("Unexpected pre-existing fit")
 text+="\\n* Previous 609 calibrated bridge fit, test-only\\n.model D1N4153 D(IS=62.368532n N=2.6 RS=.85 CJO=2p VJ=.72 M=.35 TT=3n BV=75 IBV=.1u)\\n"
 d.write_text(text)
print("TEST_ONLY_BRIDGE",mode)
