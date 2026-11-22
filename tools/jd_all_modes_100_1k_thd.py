#!/usr/bin/env python3
"""100 Hz and 1 kHz, nine input levels, through/comp/limit/both;
choose highest thresholds to minimize detector action, measure actual GR.
All nine points are retained even where no-compression condition is impossible.
"""
from pathlib import Path
import csv,json,math,subprocess,sys
host,bundle,out=sys.argv[1:];out=Path(out);out.mkdir(parents=True,exist_ok=True)
levels=(-18,-12,-6,-3,0,3,6,12,18)
freqs=(100,1000)
modes=(("through",0,0),("compressor",1,0),("limiter",0,1),("both",1,1))
rows=[]
def capture(mode,ci,li,hz,dbu):
 # Neve JD compressor highest threshold +10 dBu, limiter highest +15 dBu.
 # Neither threshold guarantees zero GR at +18 dBu.
 settings=[15,2,0,0,22,0,ci,li,0,0,hz,0,dbu,0,1.2,44100]
 p=subprocess.run([host,bundle,"--jd-case-80",*map(str,settings)],capture_output=True,text=True,timeout=270)
 if p.returncode:raise RuntimeError(f"{mode} {hz}Hz {dbu} dBu: "+p.stderr[-2000:])
 lines=p.stdout.strip().splitlines()
 if len(lines)!=80:raise RuntimeError(f"{mode} {hz}Hz {dbu}: expected 80 harmonics, got {len(lines)} {p.stdout[-300:]}")
 bins=[complex(*map(float,l.split())) for l in lines]
 fundamental=abs(bins[0])
 upper=min(80,math.floor(22049/hz))
 def thd(n):
  return 100*math.sqrt(sum(abs(x)**2 for x in bins[1:n]))/fundamental
 row=dict(mode=mode,frequency_Hz=hz,input_dBu=dbu,
  compressor_enabled=ci,limiter_enabled=li,
  compressor_threshold_dBu=10,limiter_threshold_dBu=15,
  output_dBu=round(24+20*math.log10(fundamental),7),
  THD_K2_to_K20_pct=round(thd(min(20,upper)),8),
  THD_to_Nyquist_pct=round(thd(upper),8),
  max_harmonic_below_Nyquist=upper,
  K2_dBc=round(20*math.log10(max(1e-15,abs(bins[1])/fundamental)),4),
  K3_dBc=round(20*math.log10(max(1e-15,abs(bins[2])/fundamental)),4),
  K4_dBc=round(20*math.log10(max(1e-15,abs(bins[3])/fundamental)),4),
  K5_dBc=round(20*math.log10(max(1e-15,abs(bins[4])/fundamental)),4))
 return row
for hz in freqs:
 for dbu in levels:
  points=[capture(*m,hz,dbu) for m in modes]
  ref=points[0]["output_dBu"]
  for p in points:
   # This is additional insertion loss from active detector compared to through,
   # not a certified GR reading when the powered signal paths have unequal fixed gains.
   p["through_relative_attenuation_dB"]=round(ref-p["output_dBu"],5)
   p["no_measurable_attenuation_0p1dB"]=abs(ref-p["output_dBu"])<=.1
   rows.append(p)
  print(hz,dbu,[(p["mode"],p["THD_to_Nyquist_pct"],p["through_relative_attenuation_dB"]) for p in points],flush=True)
with (out/"four-modes-100-and-1000hz.csv").open("w",newline="") as f:
 w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
(out/"summary.json").write_text(json.dumps({"results":rows,"conditions":"44.1kHz host, nodal 2x, 600ohm loaded, 1.2s steady sine, stereo link OFF, zero makeup, maximum front-panel thresholds +10 compressor and +15 limiter, both detectors nominally inserted","warning":"At higher input levels, thresholds cannot be raised far enough to guarantee 0 dB compression. Through-relative attenuation is reported for every test. Do not treat nonzero attenuation as no-GR circuit coloration."},indent=2))
