#!/usr/bin/env python3
"""Neve 33609JD real circuit CV ripple causality test.
Measure 100Hz +12,+18 and 1kHz +18; through, compressor, limiter, both.
For each, clamp final VCTRL to baseline DC mean with ideal source; compare bridge and output
THD, output level & sidechain ripple before/after. DO NOT ALTER working plugin.
"""
from pathlib import Path
import subprocess,csv,json,sys
exe,out=sys.argv[1:];out=Path(out);out.mkdir(parents=True,exist_ok=True)
rows=[]
scenarios=[(100,12),(100,18),(1000,18)]
for hz,dbu in scenarios:
 for mode in ("through","comp","limit","both"):
  def run(clamp=None):
   args=[exe,mode,str(hz),str(dbu)]+([] if clamp is None else [str(clamp)])
   p=subprocess.run(args,capture_output=True,text=True,timeout=480)
   if p.returncode:raise RuntimeError(f"{mode} {hz} {dbu} clamp {clamp}: {p.stderr[-900:]} {p.stdout[-900:]}")
   z=list(csv.DictReader(p.stdout.strip().splitlines()))
   if len(z)!=8:raise RuntimeError(f"Expected 8 internal voltages got {len(z)}: {p.stdout[-200:]}")
   for x in z:
    for k,v in list(x.items()):
     if k not in ("mode","stage"):x[k]=float(v)
   return {x["stage"]:x for x in z}
  unpinned=run()
  cv=unpinned["VCTRL"]["mean_V"]
  pinned=run(cv)
  for kind,d in (("normal",unpinned),("CV_frozen_at_mean",pinned)):
   for key,v in d.items():
    rows.append({"frequency_Hz":hz,"input_dBu":dbu,"mode":mode,"test":kind,"vctrl_mean_original_V":cv,**v})
  keys=("bridge_PADH_minus_PADL","VCTRL","C13_comp_env","C14_limit_env","output")
  def compact(d):return {k:dict(mean=round(d[k]["mean_V"],6),pp=round(d[k]["peak_to_peak_V"],6),thd=round(d[k]["THD_pct"],6),second=round(d[k]["second_harmonic_peak_V"],8)) for k in keys}
  print(json.dumps({"hz":hz,"input_dBu":dbu,"mode":mode,"normal":compact(unpinned),"clamped":compact(pinned),"output_thd_reduction_factor":unpinned["output"]["THD_pct"]/max(pinned["output"]["THD_pct"],1e-15)},indent=2),flush=True)
with (out/"cv-causality-stages.csv").open("w",newline="") as f:
 w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
(out/"cv-causality-stages.json").write_text(json.dumps(rows,indent=2))
