#!/usr/bin/env python3
"""RV1 full travel, test checkout only. All other component values untouched."""
from pathlib import Path
import re,sys
if len(sys.argv)!=2: raise SystemExit("Usage: fixture.py RV1_PERCENT")
pct=int(sys.argv[1])
if pct not in range(0,101,10):raise SystemExit("Only 0,10,...,100 percent allowed")
p=Path("circuit/609.cir")
t=p.read_text()
# Change the one top-level PRESET only: CH609/EX11475 inherit via params.
pattern=r"(?m)^(\.param )PRESET=0\.345( LAW=0\.04 MTR=0\.5 LIMCAL=0\.166966 COMPCAL=0\.3 BIAS=0\.445245)$"
t,n=re.subn(pattern,lambda m:m.group(1)+f"PRESET={pct/100:.2f}"+m.group(2),t)
if n!=1: raise RuntimeError("Existing factory default changed; no RV1 patch applied")
p.write_text(t)
# Validate unchanged manufacturer bridge & stock resistors.
b=Path("circuit/ex11475.inc").read_text()
assert "R28 QL12E ATTACK_TAP 1600" in b and "R30 ATTACK_TAP LIMIT_ENV 1600" in b
assert all(re.search(rf"(?m)^D{k} \S+ \S+ DI_TEST_4148$",b) for k in (14,15,16,17))
print(f"RV1 at {pct}%; RV2=200 ohm RV4=1669.66 ohm, genuine 1N4148W and 1k6 attack preserved")
