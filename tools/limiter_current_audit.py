#!/usr/bin/env python3
"""Non-invasive 33609 limiter/compressor DC path current and bias audit.

Read native_seed.py ngspice ASCII .op file, never change circuit components.
Print explicit missing observations, not invented zero voltages.
"""
import json,math,re,sys
from pathlib import Path
source=Path(sys.argv[1]); destination=Path(sys.argv[2])
raw=source.read_text(encoding="latin-1")
header, body=raw.split("Values:",1)
names=re.findall(r"(?m)^\s*\d+\s+([vi]\([^\n]+?\))\s+\w+\s*$",header)
count=int(re.search(r"No. Variables:\s*(\d+)",header).group(1))
numbers=[float(x) for x in body.split()[1:]]
if len(names) not in (count,count-1) or len(numbers)<count:
    raise RuntimeError("Bad ngspice operating point layout")
op=dict(zip((x.lower() for x in names),numbers[-len(names):]))
# Every item is a real node from the existing EX11475 netlist.
nodes=["vcc","scv","ldet_v","ql6b","ql6e","ql10c","ql11b",
       "zref","ql7b","ql7c","ql7e","ql12b","ql12e","attack_tap",
       "limit_env","release_ref","merge_b","link","law_top","vctrl",
       "law_knee","padh","padl","audiov","buffv","preamp","amp_out","linesend",
       "rv4_in","rv4_out","meter_b"]
def find(name):
    candidates=(f"v(xleft.xboard.{name})",f"v(xleft.{name})",f"v({name})")
    for key in candidates:
        if key in op:return op[key]
    return None
volts={n:find(n) for n in nodes}
def resistor(label,a,b,r):
    x,y=volts.get(a),volts.get(b)
    return {"part":label,"from":a,"to":b,"ohms":r,
            "current_mA":None if x is None or y is None else (x-y)/r*1000,
            "voltage_drop_V":None if x is None or y is None else x-y}
currents=[
 resistor("R43","vcc","scv",10),
 resistor("R10","scv","ldet_v",240),
 resistor("R3","ldet_v","ql6e",10000),
 resistor("R12","rv4_out","ql6e",5600),
 resistor("R13","ldet_v","ql6b",33000),
 resistor("R15","ldet_v","ql10c",4700),
 resistor("R22","ql11e","zref",150),
 resistor("R25","scv","zref",12000),
 resistor("R17","scv","ql7b",4700),
 resistor("R24","ql7b","ql11c",16000),
 resistor("R27","scv","ql7e",47),
 resistor("R19","ql7c","0",120000),
 resistor("R20","ql12b","0",120000),
 resistor("R28","ql12e","attack_tap",1600),
 resistor("R30","attack_tap","limit_env",1600),
 resistor("R36","link","law_top",1000),
 resistor("RV2","law_top","vctrl",200.001),
 resistor("R45","vctrl","law_knee",220),
 resistor("R29","law_knee","0",3300),
 resistor("R48","t2h","padh",47000),
 resistor("R49","t2l","padl",47000)]
# Additional probe points are essential for computing currents.
for n in ["ql11e","ql11c","t2h","t2l","0"]:
    if n not in volts:volts[n]=0.0 if n=="0" else find(n)
for c in currents:
    a,b=c["from"],c["to"]
    if a in volts and b in volts and volts[a] is not None and volts[b] is not None:
        c["voltage_drop_V"]=volts[a]-volts[b]
        c["current_mA"]=1000*c["voltage_drop_V"]/c["ohms"]
results=dict(volts=volts,resistor_currents=currents,
             scope="DC operating point only; 1kHz dynamic current/ripple not measured",
             possible_problem="Check both steady diode bridge DC bias and Q12/C14 current and 2kHz ripple under signal; DC alone cannot establish distortion.")
destination.parent.mkdir(parents=True,exist_ok=True)
destination.write_text(json.dumps(results,indent=2))
print(json.dumps(results,indent=2))
