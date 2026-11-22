#!/usr/bin/env python3
"""JD service-bench procedures; current VST3, independently recorded exact stimulus.
THD: 192kHz sample rate, harmonics K2..K80 (80 kHz).
Labels disclose exact knob detents rather than invent a physically unavailable limiter position.
"""
from pathlib import Path
import sys,subprocess,math,csv,json
host,bundle,dest=sys.argv[1:]
dest=Path(dest);dest.mkdir(parents=True,exist_ok=True)
cases=[]
def spectrum(label,*,ct=6,cr=4,cm=3,cg=10,lt=8,lr=3,ci=0,li=0,fast=0,atk=0,hz=1000,level=9,rate=192000,nbin=80,stop=1.2):
    params=[ct,cr,cm,cg,lt,lr,ci,li,fast,atk,hz,0,level,0,stop,rate]
    cmd=[host,bundle,'--jd-case-80' if nbin==80 else '--jd-case',*map(str,params)]
    p=subprocess.run(cmd,text=True,capture_output=True,timeout=270)
    if p.returncode:raise RuntimeError(label+" "+p.stderr[-1600:])
    lines=p.stdout.strip().splitlines()
    if len(lines)!=nbin:raise RuntimeError(f'{label} expected {nbin} spectra, got {len(lines)}: {p.stdout[-400:]}')
    h=[complex(*map(float,line.split())) for line in lines]
    f=abs(h[0])
    usable=[abs(x) for k,x in enumerate(h,1) if k>=2 and k*hz<=80000 and k*hz<rate/2]
    thd=100*math.sqrt(sum(x*x for x in usable))/f
    output=24+20*math.log10(f)
    d=dict(case=label,frequency_hz=hz,input_dBu=level,sample_rate=rate,
        input_reference_dBu_0dBFS=24,compress_threshold_dBu=-20+2*ct,compress_ratio=["1.5","2","3","4","6"][cr],
        compress_recovery_ms=["100","400","800","1500","auto1","auto2"][cm],
        compress_makeup_dB=2*cg,limiter_threshold_detent_dBu=4+0.5*lt,
        limiter_recovery_ms=["50","100","200","800","auto1","auto2"][lr],
        compressor_enabled=ci,limiter_enabled=li,output_dBu=round(output,7),
        thd_80k_percent=round(thd,7),k2_dBc=round(20*math.log10(abs(h[1])/f),5),
        k3_dBc=round(20*math.log10(abs(h[2])/f),5),
        phase_deg=round(math.degrees(math.atan2(h[0].imag,h[0].real)),5),
        harmonics_measured=len(usable),bandwidth_hz=80000)
    cases.append(d)
    print(label,output,thd,flush=True)
    return d
# Three official THD configurations (only the printed manual conditions are certified
# as such; the compressor THD entry does not give a stimulus level in the handbook).
spectrum("through +9dBu, limiter/compressor OUT; factory <=0.075%",ci=0,li=0,cg=0,level=9)
spectrum("compressor 6:1, makeup +20, recovery 800, threshold -8; level +9dBu characterization",ci=1,li=0,ct=6,cr=4,cm=2,cg=10,level=9)
spectrum("limiter +22 input, recovery 800, calibrated +8 front-panel detent; factory <=0.45%",ci=0,li=1,lt=8,lr=3,cg=0,level=22)
# Other Neve electrical checks at 44.1 kHz, 0 dBu and 600ohm per compiled netlist.
ref=spectrum("frequency reference through 1000Hz 0dBu",ci=0,li=0,cg=0,level=0,rate=44100,nbin=20)
fr=[]
for hz in (20,40,100,500,5000,10000,15000,18000,20000):
 p=spectrum(f"frequency response {hz}Hz +0dBu",hz=hz,ci=0,li=0,cg=0,level=0,rate=44100,nbin=20)
 fr.append(dict(hz=hz,delta_dB=round(p["output_dBu"]-ref["output_dBu"],6),
   within_0_5dB=abs(p["output_dBu"]-ref["output_dBu"])<=.5,
   phase_degrees_generator_relative=p["phase_deg"]))
# Manual ratio: +0->+10, -20 threshold, 100 ms
ratios=[]
for cr,(nom,tol) in enumerate(zip((6.5,5,3.5,2.5,1.5),(1,1,1,.5,.5))):
 a=spectrum(f"compress ratio {cr} input +0",ct=0,cr=cr,cm=0,cg=0,ci=1,li=0,level=0,rate=44100,nbin=20)
 b=spectrum(f"compress ratio {cr} input +10",ct=0,cr=cr,cm=0,cg=0,ci=1,li=0,level=10,rate=44100,nbin=20)
 change=b["output_dBu"]-a["output_dBu"]
 ratios.append(dict(ratio=["1.5","2","3","4","6"][cr],step_dB=round(change,6),nominal_dB=nom,tolerance_dB=tol,pass_factory=abs(change-nom)<=tol))
# limiter factory +10->+20, threshold detent +8, 50 ms
a=spectrum("limiter output 10dBu",li=1,ci=0,lt=8,lr=0,cg=0,level=10,rate=44100,nbin=20)
b=spectrum("limiter output 20dBu",li=1,ci=0,lt=8,lr=0,cg=0,level=20,rate=44100,nbin=20)
limiter=dict(upper_level_output_dBu=b["output_dBu"],output_step_dB=round(b["output_dBu"]-a["output_dBu"],6),
  factory_limit_ratio_pass=abs((b["output_dBu"]-a["output_dBu"])-.1)<=.1)
for fname,rows in (("thd-and-spectra.csv",cases),("fr.csv",fr),("ratio.csv",ratios)):
 with (dest/fname).open("w",newline="") as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
report=dict(thd=[x for x in cases if ("factory" in x["case"] or "characterization" in x["case"]) and x["frequency_hz"]==1000 and x["sample_rate"]==192000],
 frequency_response=fr,ratios=ratios,limiter=limiter,
 source="Neve PN33609/JD technical handbook pages 2-3",
 disclaimer="Manual prints THD threshold -18. User confirms -8 typo correction. Plugin LIMIT knob only has +4 through +15 detents; this test explicitly uses calibrated +8 detent, not fictitious -8 knob. Compressor THD input level not supplied in manual, +9 is characterization. Noise/80 kHz-band results cannot be inferred from 44.1kHz sweeps.")
(dest/"report.json").write_text(json.dumps(report,indent=2))
print("REPORT",json.dumps(report,indent=2),flush=True)
