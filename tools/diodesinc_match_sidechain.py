#!/usr/bin/env python3
"""Test-only: after bridge fixture, match the remaining 1N4148s to bridge model.

No generic non-1N4148 diodes, Zeners or Schottkys are changed.
No values, topology or production circuit file is permanently edited.
"""
from pathlib import Path
import re
p=Path("circuit/ex11475.inc")
text=p.read_text()
names=("D1","D2","D3","D4","D5","D6","D7","D8","D9","D10","D11")
for name in names:
    pattern=rf"(?m)^({name}\s+\S+\s+\S+)\s+D1N4148$"
    text,count=re.subn(pattern,r"\1 DI_TEST_4148",text)
    if count!=1: raise SystemExit(f"Missing or repeated generic 1N4148 at {name}: {count}")
p.write_text(text)
print("Replaced only sidechain/control 1N4148 diode model references:",names)
