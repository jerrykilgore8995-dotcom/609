#!/usr/bin/env python3
"""100 Hz VST3 level sweep and physical nodal THD stage localization.
Test: through, both dynamics OFF, link OFF, 44.1k host / native 2x; K2..K80 = up to 8 kHz.
All levels are dBu referenced to 0dBFS=+24dBu.
"""
import subprocess,csv,json,math,sys
from pathlib import Path
host,bundle,probe,root=sys.argv[1:]
root=Path(root);root.mkdir(parents=True,exist_ok=True)
levels=(-18,-12,-6,-3,0,3,6,12,18)
spectrum=[]
stages=[]
for level in levels:
 params=[0,2,0,0,8,0,0,0,0,0,100,0,level,0,1.2,44100]
 args=[host,bundle,"--jd-case-80",*map(str,params)]
 p=subprocess.run(args,capture_output=True,text=True,timeout=360)
 if p.returncode:raise RuntimeError(f"VST3 100 Hz level {level}: "+p.stderr[-1800:])
 raw=p.stdout.strip().splitlines()
 if len(raw)!=80:raise RuntimeError(f"Expected 80 harmonic bins at level {level}, got {len(raw)}: "+p.stdout[-500:])
 h=[complex(*map(float,line.split())) for line in raw]
 fundamental=abs(h[0])
 thd=lambda count:100*math.sqrt(sum(abs(x)**2 for x in h[1:count]))/fundamental
 dBc=lambda k:20*math.log10(max(1e-15,abs(h[k-1])/fundamental))
 row={"input_dBu":level,"output_dBu":round(24+20*math.log10(fundamental),6),
   "transfer_gain_dB":round(24+20*math.log10(fundamental)-level,6),
   "thd_K2_to_K20_pct":round(thd(20),7),"thd_K2_to_K80_pct":round(thd(80),7),
   **{f"K{k}_dBc":round(dBc(k),4) for k in range(2,21)}}
 spectrum.append(row)
 print("VST3",level,"output",row["output_dBu"],"THD k2-k20",row["thd_K2_to_K20_pct"],"THD k2-k80",row["thd_K2_to_K80_pct"],flush=True)
 p=subprocess.run([probe,str(level)],capture_output=True,text=True,timeout=480)
 if p.returncode:raise RuntimeError(f"STAGE {level}: "+p.stderr[-1600:])
 for line in p.stdout.strip().splitlines():
  z=line.split(",")
  if len(z)!=11:raise RuntimeError(f"Stage {level} bad row: {line}")
  keys=["input_dBu","stage","fundamental_peak_volts","thd_K2_to_K20_pct","thd_K2_to_K50_pct","K2_dBc","K3_dBc","K4_dBc","min_voltage","max_voltage","solve_failures"]
  d=dict(zip(keys,z))
  for k in keys:
   if k not in ("stage",):d[k]=float(d[k])
  stages.append(d)
 print("STAGES",level,[(x["stage"],round(x["thd_K2_to_K20_pct"],3)) for x in stages if x["input_dBu"]==level],flush=True)
for fname,rows in [("vst3-thd-100hz.csv",spectrum),("physical-stage-thd-100hz.csv",stages)]:
 with (root/fname).open("w",newline="") as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
(root/"summary.json").write_text(json.dumps({"through_vst3":spectrum,"stage_nodal":stages,
  "limitations":["Transformer models TF10016,TF10015,TF12009 are linear coupled inductors with resistive and capacitive loading; no physical core saturation mechanism is present.","Stage probe runs at direct 88.2kHz nodal solver; packaged VST3 runs 44.1kHz host with 2x nodal solver and an interpolation filter.","THD K2-K80 at 100Hz spans 200Hz through 8kHz, not full 80kHz bandwidth.","Stage internal voltages are not normalized to output reference; relative harmonic ratios are comparable.","THD by stage does not independently identify a particular transistor; transistors and diode models are nonlinear.","Both compressor and limiter disabled; stereo link off; loaded channel with 600 ohm external load."]},indent=2))
