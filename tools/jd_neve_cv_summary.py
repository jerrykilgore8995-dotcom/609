#!/usr/bin/env python3
"""Direct time-constant measurements, no audio surrogate for limiter C14.
Neve limits nominal recovery ±50%, manual does not specify numeric end-voltage tolerance.
Report first 63.2/90/95% and sustained 5%-band; do not claim 90% is factory-defined.
"""
import csv,sys,json,statistics,math
from pathlib import Path
folder=Path(sys.argv[1]);dest=Path(sys.argv[2]);dest.mkdir(parents=True,exist_ok=True)
results=[]
for p in sorted(folder.glob("*.csv")):
 rows=[]
 with p.open() as f:
  for x in csv.DictReader(f):
   try:rows.append((float(x["time_s"]),float(x["control_voltage_V"])))
   except (ValueError,KeyError):continue
 if len(rows)<100:raise RuntimeError("incomplete "+str(p))
 end=rows[-1][0]
 initial=statistics.median(v for t,v in rows if 7.70<t<7.98)
 final=statistics.median(v for t,v in rows if end-.25<t<end-.05)
 drift0=statistics.median(v for t,v in rows if 7.4<t<7.65)
 drift1=statistics.median(v for t,v in rows if end-.55<t<end-.3)
 delta=final-initial
 if abs(delta)<.001:raise RuntimeError("step without C13/C14 response "+str(p))
 post=[(t-8,(v-initial)/delta) for t,v in rows if t>=8]
 result=dict(case=p.stem,pre_V=round(initial,7),post_V=round(final,7),
  changed_V=round(delta,7),pre_drift_pct=round(100*abs(drift0-initial)/abs(delta),3),
  final_drift_pct=round(100*abs(drift1-final)/abs(delta),3),
  total_observation_s=round(end-8,3))
 for pc,label in ((.63212,"p63_ms"),(.9,"p90_ms"),(.95,"p95_ms")):
  crossing=next((t*1000 for t,y in post if y>=pc),None)
  result[label]=round(crossing,2) if crossing is not None else None
 # Hold inside +-5% of new final voltage for 50ms, not 5% of full scale
 dwell=50
 result["settled_5pct_50ms_ms"]=None
 for i,(t,_) in enumerate(post):
  if t<0:continue
  if all(abs(y-1)<=.05 for _,y in post[i:i+dwell]) and len(post[i:i+dwell])==dwell:
   result["settled_5pct_50ms_ms"]=round(t*1000,2);break
 result["endpoint_valid"]=result["pre_drift_pct"]<1 and result["final_drift_pct"]<1
 results.append(result)
 print(result,flush=True)
with (dest/"direct-cv.csv").open("w",newline="") as f:
 w=csv.DictWriter(f,fieldnames=list(results[0]));w.writeheader();w.writerows(results)
(dest/"report.json").write_text(json.dumps({"results":results,"notes":["Limiter Neve procedure: +20 dBu to +10 dBu, measure actual C14 voltage","Compressor handbook lists detent labels; no numeric fraction is specified for attack/recovery; readings display multiple explicit fractions","90% CV crossings are not claimed as manufacturer-defined timing endpoint","Auto recoveries depend on initial history, longer conditioning and shorter bursts needed for full auto law"]},indent=2))
