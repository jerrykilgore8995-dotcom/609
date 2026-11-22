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
# Trace charge drive and voltage references independently from the recovery capacitor.
# Node indices are obtained from the actual calibrated Deck.hpp component comments.
res={}
for label in ("r28","r30","r8","r19","r20","r24","r25"):
    matched=[(int(a),int(b)) for a,b,c in re.findall(r'c\.resistors\.push_back\(\{(\d+),\s*(-?\d+),[^\n]*?\}\);\s*//\s*([^\n]*)',s)
             if c.strip().lower()=="xleft.xboard."+label]
    if len(matched)!=1:raise RuntimeError(f"missing diagnostic resistor {label}: {matched!r}")
    res[label]=matched[0]
# R28: QL12E; R30: ATTACK_TAP; R8: RELEASE_REF; R19: QL7C;
# R20: QL12B; R24: QL7B; R25: ZREF
idx=[res["r28"][0],res["r30"][0],res["r8"][1],res["r19"][0],res["r20"][0],res["r24"][0],res["r25"][1]]
s=s.replace('    c.meter = ', '    c.diagnosticNodes = {'+', '.join(map(str,idx))+'};\n    c.meter = ',1)
print("Read-only diagnostic nodes",list(zip(("QL12E","ATTACK_TAP","RELEASE_REF","QL7C","QL12B","QL7B","ZREF"),idx)))
p.write_text(s)
print("Probe C13 COMP_ENV and C14 LIMIT_ENV nodes:",nodes)
