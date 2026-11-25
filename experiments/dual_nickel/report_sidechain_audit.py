#!/usr/bin/env python3
"""Analyze measured physical-circuit knee/timing matrix without failing CI.

No DSP changes. Compare HPF-on vs bypass for passband signals, show >10%
differences as calibration candidates, and check whether threshold increases
move the measured onset upward. Emit concise JSON and readable report.
"""
import csv,json,sys,collections,math
from pathlib import Path
p=Path(sys.argv[1] if len(sys.argv)>1 else "sidechain-knee-timing-matrix.csv")
rows=list(csv.DictReader(p.open()))
def num(x,key):return float(x[key])
by={ (r["measurement"],r["cell"],float(r["freq_hz"]),float(r["hpf_hz"]),float(r["threshold_dbu"]),float(r["input_dbu"]),float(r["burst_ms"])):r for r in rows }
stat=[r for r in rows if r["measurement"]=="static"]
tim=[r for r in rows if r["measurement"]=="timing"]
flags=[];checks=0
for r in stat:
 if float(r["freq_hz"])!=1000 or float(r["hpf_hz"])==0:continue
 key=("static",r["cell"],num(r,"freq_hz"),0.,num(r,"threshold_dbu"),num(r,"input_dbu"),0.)
 base=by.get(key)
 if not base:continue
 a=num(r,"GR_average_db");b=num(base,"GR_average_db")
 if b>=1:
  checks+=1
  error=100*abs(a-b)/b
  if error>10:
   flags.append({"mode":"passband_GR","cell":r["cell"],"hpf_hz":r["hpf_hz"],
                 "threshold_dbu":r["threshold_dbu"],"input_dbu":r["input_dbu"],
                 "baseline_gr":round(b,3),"actual_gr":round(a,3),"error_pct":round(error,1)})
# Different filter cutoff influences onset via active signal; for each cell
# threshold scan report the FIRST level where GR reaches 1dB.
knee=[]
for (cell,freq,hpf,th),group in __import__("itertools").groupby(
 sorted(stat,key=lambda r:(r["cell"],r["freq_hz"],r["hpf_hz"],r["threshold_dbu"])),
 key=lambda r:(r["cell"],r["freq_hz"],r["hpf_hz"],r["threshold_dbu"])):
 points=sorted(group,key=lambda r:float(r["input_dbu"]))
 onset=None
 for previous,current in zip(points,points[1:]):
  x0,x1=num(previous,"input_dbu"),num(current,"input_dbu")
  y0,y1=num(previous,"GR_average_db"),num(current,"GR_average_db")
  if y0<=1<=y1 and y1>y0:
   onset=x0+(1-y0)*(x1-x0)/(y1-y0)
   break
 knee.append({"cell":cell,"freq_hz":float(freq),"hpf_hz":float(hpf),
              "threshold_dbu":float(th),"onset_1db_input_dbu":round(onset,2) if onset is not None else None})
# 1kHz release and attack at 1s burst when HPF enabled compared to off.
for r in tim:
 if num(r,"freq_hz")!=1000 or num(r,"hpf_hz")==0 or num(r,"burst_ms")!=1000:continue
 key=("timing",r["cell"],num(r,"freq_hz"),0.,num(r,"threshold_dbu"),num(r,"input_dbu"),num(r,"burst_ms"))
 base=by.get(key)
 if base is None:continue
 for field in ["attack90_ms","release50_ms","release10_ms"]:
  a=num(r,field);b=num(base,field)
  if a>0 and b>0:
   checks+=1
   error=100*abs(a-b)/b
   if error>10:flags.append({"mode":"passband_timing","cell":r["cell"],
          "hpf_hz":r["hpf_hz"],"threshold_dbu":r["threshold_dbu"],
          "input_dbu":r["input_dbu"],"metric":field,
          "baseline_ms":round(b,2),"actual_ms":round(a,2),"error_pct":round(error,1)})
report={"measured_points":len(rows),"static":len(stat),"timing":len(tim),
 "passband_10pct_checks":checks,"flags_over_10pct":len(flags),
 "deviations":flags,"knee_onset_1db":knee}
Path("sidechain-audit-report.json").write_text(json.dumps(report,indent=2))
print("MEASURED",len(rows),"static",len(stat),"timing",len(tim))
print("PASSBAND_COMPARISONS",checks,"OVER_10PCT",len(flags))
for item in flags[:45]:print("ADVISORY",json.dumps(item,separators=(",",":")))
for item in knee:
 if item["freq_hz"] in (55,220,1000) and item["hpf_hz"] in (0,100):
  print("KNEE_ONSET",json.dumps(item,separators=(",",":")))
print("Measurement-only audit complete: deviations DO NOT fail the workflow.")
