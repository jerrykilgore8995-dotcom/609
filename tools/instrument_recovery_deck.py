#!/usr/bin/env python3
"""Instrument already-calibrated generated Deck.hpp; no circuit parameter is changed."""
import re,sys,pathlib
p=pathlib.Path(sys.argv[1]);s=p.read_text()
nodes={}
for name in ("c13","c14"):
    matches=re.findall(r'c\.capacitors\.push_back\(\{(\d+),\s*(-?\d+),[^\n]*?\}\);\s*//\s*([^\n]*)',s)
    found=[(int(a),comment) for a,b,comment in matches if comment.strip().lower().startswith("xleft.") and comment.strip().lower().endswith("."+name)]
    if len(found)!=1:raise RuntimeError(f"expected one left {name}; found {found!r}")
    nodes[name]=found[0][0]
assert 'c.meter = ' in s
s=s.replace('    c.meter = ',f'    c.controlProbe = {{{nodes["c13"]}, {nodes["c14"]}}};\n    c.meter = ',1)
p.write_text(s)
print("Probe C13 COMP_ENV and C14 LIMIT_ENV nodes:",nodes)
