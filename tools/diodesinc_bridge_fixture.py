#!/usr/bin/env python3
"""Test-fixture-only bridge model selection; leave all other circuit devices unchanged."""
from pathlib import Path
import sys,re
variant=sys.argv[1].lower()
models={
 "generic":(".model DI_TEST_4148 D(IS=2.52n N=1.752 RS=.568 CJO=4p VJ=.75 M=.4 TT=4n BV=100 IBV=.1u)","D1N4148"),
 "w":(".model DI_TEST_4148 D(IS=10.4n RS=51.5m BV=75.0 IBV=1.00u CJO=2.00p M=0.333 N=2.07 TT=5.76n)","1N4148W"),
 "ws":(".model DI_TEST_4148 D(IS=10.4n RS=51.5m BV=75.0 IBV=1.00u CJO=2.00p M=0.333 N=2.07 TT=5.76n)","1N4148WS"),
 "wt":(".model DI_TEST_4148 D(IS=10.4n RS=51.5m BV=80.0 IBV=1.00u CJO=2.00p M=0.333 N=2.07 TT=5.76n)","DI_1N4148WT")
}
if variant not in models:raise SystemExit("Expected generic, w, ws or wt")
model,manufacturer_label=models[variant]
# Model constants are taken literally from Diodes Incorporated official SPICE
# files and the repo's pre-existing D1N4148 baseline. Do not tune these.
board=Path("circuit/ex11475.inc")
s=board.read_text()
for diode in (14,15,16,17):
    pattern=rf"(?m)^XV{diode} (\S+) (\S+) VISHAY4148$"
    s,count=re.subn(pattern,rf"D{diode} \1 \2 DI_TEST_4148",s)
    if count!=1:raise SystemExit(f"Expected exactly one Vishay diode XV{diode}; found {count}")
board.write_text(s)
devices=Path("circuit/devices.inc")
devices.write_text(devices.read_text()+"\n* Selected diode model for isolated 1kHz benchmark: "+manufacturer_label+"\n"+model+"\n")
top=Path("circuit/609.cir")
# Original Vishay include is still present but unused; the four actual bridge
# devices all reference DI_TEST_4148. The include is not modified.
assert all(re.search(rf"(?m)^D{n} \S+ \S+ DI_TEST_4148$",s) for n in (14,15,16,17))
print("TEST BRIDGE D14-D17:",manufacturer_label,model)
