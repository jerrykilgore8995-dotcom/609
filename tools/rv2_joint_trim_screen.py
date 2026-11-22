#!/usr/bin/env python3
"""Factory-style focused RV2=200Ω multi-trim screening using compiled nodal VST3."""
import csv,sys,subprocess,math,json
from pathlib import Path
host,bundle,destination,label=sys.argv[1:5]
out=Path(destination);out.mkdir(parents=True,exist_ok=True)
rows=[]
def tone(tag,ct=0,cr=2,cm=0,cg=0,lt=8,lr=0,ci=0,li=0,fast=0,attack=0,hz=1000,dbu=0):
    arr=[ct,cr,cm,cg,lt,lr,ci,li,fast,attack,hz,0,dbu,0,1.2,44100]
    p=subprocess.run([host,bundle,'--jd-case',*map(str,arr)],capture_output=True,text=True,timeout=90)
    if p.returncode:raise RuntimeError(tag+': '+p.stderr[-900:])
    sp=[complex(*map(float,line.split())) for line in p.stdout.splitlines() if len(line.split())==2]
    if len(sp)!=20:raise RuntimeError(f"{tag} only {len(sp)} harmonic bins. {p.stdout[-300:]}")
    fundamental=abs(sp[0]);m=min(20,int(22049//hz))
    thd=100*math.sqrt(sum(abs(v)**2 for v in sp[1:m]))/fundamental
    dBu=24+20*math.log10(fundamental)
    row={'Variant':label,'Case':tag,'CT':ct,'CR':cr,'CM':cm,'CG':cg,'LT':lt,'LR':lr,'CI':ci,'LI':li,'Hz':hz,'Input_dBu':dbu,
         'Output_dBu':round(dBu,6),'THD_pct':round(thd,7),'K2_dBc':round(20*math.log10(max(1e-15,abs(sp[1])/fundamental)),5),
         'K3_dBc':round(20*math.log10(max(1e-15,abs(sp[2])/fundamental)),5)}
    rows.append(row);return row
def delta(a,b):return b["Output_dBu"]-a["Output_dBu"]
targets=[6.5,5.,3.5,2.5,1.5];tolerances=[1.,1.,1.,.5,.5]
ratio=[]
for i in range(5):
    a=tone(f"ratio{i}-0",cr=i,ci=1,dbu=0)
    b=tone(f"ratio{i}-10",cr=i,ci=1,dbu=10)
    rise=delta(a,b)
    ratio.append({'Ratio':i,'Measured':round(rise,5),'Target':targets[i],'Tolerance':tolerances[i],
                  'Error':round(rise-targets[i],5),'PASS':abs(rise-targets[i])<=tolerances[i]})
# Neve nominal checks; compressor THD across several input levels because factory input not specified for compressor 0.2% test.
for level in (-18,-10,-4,0,4,9,14):
    tone(f"compressor-thd-{level}",ct=1,cr=4,cm=2,cg=10,ci=1,dbu=level)
for lt in (0,8,16,22):
    tone(f"limiter-thd-threshold-{lt}-50",lt=lt,lr=0,li=1,dbu=22)
    tone(f"limiter-thd-threshold-{lt}-800",lt=lt,lr=3,li=1,dbu=22)
    tone(f"limiter-threshold-{lt}-20",lt=lt,li=1,dbu=20)
for level in (10,20):
    tone(f"limiter-ratio-{level}",lt=8,li=1,dbu=level)
for level in (0,9):
    tone(f"through-{level}",ci=0,li=0,dbu=level)
for hz in (20,1000,10000,20000):
    tone(f"through-frequency-{hz}",ci=0,li=0,dbu=0,hz=hz)
# Main operating point and discrete harmonics
for ratio_idx in (0,2,4):
    for level in (0,10,20):
        tone(f"compressor-r{ratio_idx}-in{level}",cr=ratio_idx,ci=1,dbu=level)
for level in (10,20,22,24):
    for lt in (0,8,22):
        tone(f"limiter-lt{lt}-in{level}",lt=lt,li=1,dbu=level)
for level in (10,20,22):
    tone(f"combined-r4-in{level}",ct=1,cr=4,li=1,ci=1,lt=8,dbu=level)
with (out/"screening-tones.csv").open("w",newline="") as f:
    w=csv.DictWriter(f,rows[0].keys());w.writeheader();w.writerows(rows)
ratio_penalty=sum(max(0,abs(x["Error"])-x["Tolerance"]) for x in ratio)
ratio_nominal=sum(abs(x["Error"]) for x in ratio)
def found(k):
    return next(v for v in rows if v["Case"]==k)
limiter_thd=found("limiter-thd-threshold-8-800")["THD_pct"]
comp_thd=found("compressor-thd-9")["THD_pct"]
limiter_out=found("limiter-threshold-8-20")["Output_dBu"]
limiter_delta=delta(found("limiter-ratio-10"),found("limiter-ratio-20"))
res={'Variant':label,'Tests':len(rows),'Ratio':ratio,'RatioAllPass':all(x["PASS"] for x in ratio),
     'RatioMeanAbsoluteError_dB':round(ratio_nominal/5,5),
     'CompressorTHD_9dBu_pct':comp_thd,'LimiterTHD_800ms_22dBu_pct':limiter_thd,
     'LimiterThreshold8Output_dBu':limiter_out,'LimiterRatioOutputStep_dB':round(limiter_delta,5),
     'CompressorTHD_over_0_2':comp_thd>.2,'LimiterTHD_over_0_45':limiter_thd>.45,
     'Warnings':['The Neve compressor THD input level is unspecified by handbook',
                 'Neve limiter THD threshold notation -18dBu does not correspond to available +4..+15dBu setting',
                 'Scoring weighted only for screening; full 776-case sweep required after candidate selection']}
(out/"screening-summary.json").write_text(json.dumps(res,indent=2))
print(json.dumps(res,indent=2),flush=True)
