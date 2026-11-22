#!/usr/bin/env python3
"""VST3-only RV2=200 ohm vs original broad measured conformance: raw spectra + summaries.
All scores tied to actual requested settings; different from internal Neve trim procedures.
"""
import argparse, csv, json, math, re, statistics, subprocess, sys
from pathlib import Path

FACTORY=[6.5,5.,3.5,2.5,1.5]
TOL=[1.,1.,1.,.5,.5]
RATIOS=["1.5:1","2:1","3:1","4:1","6:1"]
FREQS=[20,40,100,500,1000,5000,10000,15000,18000,20000]
COUNT=0

def tone(tester,bundle,params):
    args=[tester,bundle,"--jd-case",*[str(x) for x in params]]
    p=subprocess.run(args,capture_output=True,text=True,timeout=90)
    if p.returncode:
        raise RuntimeError(f"VST3 exit {p.returncode}: {p.stderr[-1000:]} | {' '.join(args)}")
    lines=p.stdout.strip().splitlines()
    if len(lines)!=20:
        raise RuntimeError(f"VST3 returned {len(lines)} spectrum lines: {p.stdout[-500:]}, {p.stderr[-500:]}")
    complex_bins=[complex(*map(float,line.split())) for line in lines]
    mag=abs(complex_bins[0])
    if not math.isfinite(mag) or mag<1e-12: raise RuntimeError("Invalid fundamental")
    hz=float(params[10])
    # Nyquist; exclude the fundamental for high-frequency tones.
    usable=min(20,int(22049//hz))
    thd=100*math.sqrt(sum(abs(x)**2 for x in complex_bins[1:usable]))/mag
    dbu=24+20*math.log10(mag)
    def harmonic(k):
        if k>usable:return None
        return 20*math.log10(max(1e-15,abs(complex_bins[k-1])/mag))
    return dict(Output_dBu=round(dbu,7),THD_pct=round(thd,7),K2_dBc=round(harmonic(2),4) if usable>=2 else None,
                K3_dBc=round(harmonic(3),4) if usable>=3 else None,
                K4_dBc=round(harmonic(4),4) if usable>=4 else None,
                K5_dBc=round(harmonic(5),4) if usable>=5 else None,
                K6_dBc=round(harmonic(6),4) if usable>=6 else None,
                Phase_deg=round(math.degrees(math.atan2(complex_bins[0].imag,complex_bins[0].real)),4),
                Harmonics_dBc=json.dumps([round(harmonic(k),4) for k in range(2,usable+1)]))

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("tester")
    parser.add_argument("bundle")
    parser.add_argument("destination")
    parser.add_argument("--law",required=True)
    args=parser.parse_args()
    target=Path(args.destination);target.mkdir(parents=True,exist_ok=True)
    rows=[]
    warnings=[]
    def add(group,label,*,ct=0,cr=2,rec=0,makeup=0,lt=8,lr=0,ci=0,li=0,fast=0,attack=0,hz=1000,level=0,duration=1.2):
        p=(ct,cr,rec,makeup,lt,lr,ci,li,fast,attack,hz,0,level,0,duration,44100)
        point=tone(args.tester,args.bundle,p)
        row=dict(Group=group,Case=label,LAW=args.law,RV2_ohm=round(float(args.law)*5000,3),
                 ThresholdC_dBu=-20+2*ct,Ratio=RATIOS[cr],
                 RecoveryC= ["100","400","800","1500","Auto1","Auto2"][rec],
                 Makeup_dB=2*makeup,
                 ThresholdL_dBu=4+.5*lt,
                 RecoveryL=["50","100","200","800","Auto1","Auto2"][lr],
                 Compressor=ci,Limiter=li,FastLimiter=fast,AttackC=attack,Frequency_Hz=hz,
                 Input_dBu=level,**point)
        rows.append(row)
        n=len(rows)
        if n%40==0:print(f"MEASURED {n} tones RV2 {args.law} (last {group} {label})",flush=True)
        return row
    def diff(a,b):return b["Output_dBu"]-a["Output_dBu"]
    checks=[]
    def check(category,name,observed,target=None,tolerance=None,method="factory",note=""):
        passed=None if target is None or tolerance is None else abs(observed-target)<=tolerance
        checks.append(dict(Category=category,Case=name,Measured=round(observed,5),FactoryNominal=target,
                           Tolerance=tolerance,Pass=passed,Method=method,Note=note))
    # Neve 0→10 dBu with -20 threshold, 100ms recovery, no makeup, 1kHz; five ratios.
    for i in range(5):
        lo=add("factory-comp-ratio",f"{RATIOS[i]}-0",cr=i,ci=1,level=0)
        hi=add("factory-comp-ratio",f"{RATIOS[i]}-10",cr=i,ci=1,level=10)
        check("comp-ratio",RATIOS[i],diff(lo,hi),FACTORY[i],TOL[i])
    # factory limiter +20 into 8 dBu threshold, 50ms; plus 10->20 limiter ratios at +4 and +8 threshold.
    for lt in (0,8,12,22):
        lo=add("factory-limiter",f"limit{4+.5*lt}-in10",lt=lt,li=1,level=10)
        hi=add("factory-limiter",f"limit{4+.5*lt}-in20",lt=lt,li=1,level=20)
        if lt<=8:
            check("limiter-ratio",f"threshold {4+.5*lt} +10->+20",diff(lo,hi),.1,.1)
        else:
            check("limiter-ratio-exploratory",f"threshold {4+.5*lt} +10->+20",diff(lo,hi),None,None,"characterization","Not fully in limiting region at +10 dBu input")
        if lt==8:
            check("limiter-alignment","+20 input, +8 threshold",hi["Output_dBu"],8,None,"calibration-reference")
    # limiter THD factory proxy and bypass compressor THD.
    for mode,ci,li,level,ct,cr,lt,makeup in [
        ("through+9",0,0,9,0,2,8,0),
        ("comp6+9",1,0,9,1,4,8,10),
        ("limit+22",0,1,22,0,2,8,0),
        ("both+22",1,1,22,4,4,8,0)]:
        pt=add("factory-thd",mode,ci=ci,li=li,level=level,ct=ct,cr=cr,lt=lt,makeup=makeup)
        nominal={"through+9":.075,"comp6+9":.2,"limit+22":.45}.get(mode)
        check("thd",mode,pt["THD_pct"],nominal,None,
              "factory-proxy" if nominal is not None else "characterization",
              "THD limit is one sided; limiter +8 threshold differs from handbook -18 dBu procedure")
    # Through-mode FR: Neve +/-0.5 dB, relative 1kHz; also phase.
    ref=add("factory-fr","through-1k",hz=1000,ci=0,li=0,level=0)
    for hz in FREQS:
        if hz==1000:continue
        pt=add("factory-fr",f"through-{hz}",hz=hz,ci=0,li=0,level=0)
        check("through-frequency",f"{hz} Hz",pt["Output_dBu"]-ref["Output_dBu"],0,.5)
    # actual threshold & ratio transfer curves covering every compressor position at 0/10/+20 dBu.
    for ratio in range(5):
        for threshold in range(16):
            for level in (0,10,20):
                add("comp-all-ratio-threshold-thd",
                    f"r{ratio}_ct{threshold}_in{level}",ct=threshold,cr=ratio,ci=1,level=level)
    # Additional dynamics: every compressor ratio and all compressor recovery positions with three attack positions at +10dBu.
    for ratio in range(5):
        for rec in range(6):
            for attack in range(3):
                add("comp-recovery-attack-thd",f"r{ratio}_rec{rec}_attack{attack}",ct=8,cr=ratio,rec=rec,attack=attack,ci=1,level=10)
    # All limiter threshold detents at 3 input levels, and both attack positions; THD and harmonics each.
    for lt in range(23):
        for level in (10,20,24):
            for fast in (0,1):
                add("limiter-all-threshold-attack-thd",f"lt{lt}_in{level}_fast{fast}",lt=lt,li=1,fast=fast,level=level)
    # All limiter fixed and auto recoveries across five representative thresholds and two input levels.
    for lt in (0,4,8,16,22):
        for recovery in range(6):
            for level in (10,22):
                add("limiter-recovery-thd",f"lt{lt}_rec{recovery}_in{level}",lt=lt,lr=recovery,li=1,level=level)
    # Both sections simultaneously: five compressor ratios × 5 thresholds × three input levels.
    for cr in range(5):
        for ct in (0,4,8,12,15):
            for level in (0,10,20):
                add("comp-and-limiter-thd",f"r{cr}_ct{ct}_in{level}",ct=ct,cr=cr,lt=8,ci=1,li=1,level=level)
    # Limiter range and true compression GR vs same dry setting.
    for level in (-20,-12,-4,0,4,8,12,16,20,24):
        dry=add("transfer-dry",f"in{level}",ci=0,li=0,level=level)
        for cr in range(5):
            wet=add("comp-range",f"r{cr}_in{level}",ci=1,cr=cr,level=level)
            check("comp-gr",f"r{cr} input {level} dBu",dry["Output_dBu"]-wet["Output_dBu"],method="characterization")
        for lt in (0,8,16,22):
            wet=add("limiter-range",f"lt{lt}_in{level}",li=1,lt=lt,level=level)
            check("limit-gr",f"lt{lt} input {level} dBu",dry["Output_dBu"]-wet["Output_dBu"],method="characterization")
    # compressor makeup gain across 0..+20 in 2dB; compared to no gain case.
    base=add("makeup","0",ct=8,cr=2,ci=1,level=0,makeup=0)
    for gain in range(1,11):
        wet=add("makeup",str(gain*2),ct=8,cr=2,ci=1,level=0,makeup=gain)
        check("makeup",f"+{gain*2}",diff(base,wet),2*gain,.5)
    # FR active at reference settings: watch phase, THD and HF rolloff.
    for mode, ci,li,ct,cr,lt,level in [("compress",1,0,8,2,8,4),("limit",0,1,0,2,8,20),("both",1,1,8,2,8,20)]:
        for hz in FREQS:
            add("active-frequency",f"{mode}-{hz}",ci=ci,li=li,ct=ct,cr=cr,lt=lt,level=level,hz=hz)
    def savecsv(path,data):
        if not data:return
        with (target/path).open("w",newline="",encoding="utf-8") as out:
            writer=csv.DictWriter(out,fieldnames=data[0].keys())
            writer.writeheader();writer.writerows(data)
    savecsv("all-vst3-tones.csv",rows)
    savecsv("factory-and-range-checks.csv",checks)
    report={"law":args.law,"rv2_ohms":5000*float(args.law),
            "number_of_tones":len(rows),"number_of_checks":len(checks),
            "ratio":[x for x in checks if x["Category"]=="comp-ratio"],
            "limiter_ratio":[x for x in checks if x["Category"]=="limiter-ratio"],
            "limiter_alignment":[x for x in checks if x["Category"]=="limiter-alignment"],
            "thd_factory_proxies":[x for x in checks if x["Category"]=="thd"],
            "makeup":[x for x in checks if x["Category"]=="makeup"],
            "fr":[x for x in checks if x["Category"]=="through-frequency"],
            "notes":["No factory control-voltage timing verification with steady tones",
                     "THD at high-frequency tones is restricted to harmonics under Nyquist",
                     "Limiter THD +22 with +8 threshold not exact Neve -18 dBu manual bench",
                     "Measurement host 24 dBu = 0 dBFS signal reference; phase is generator-relative only"]}
    (target/"summary.json").write_text(json.dumps(report,indent=2))
    for k in ("ratio","limiter_ratio","limiter_alignment","thd_factory_proxies"):
        print(k,json.dumps(report[k]),flush=True)
    print(f"MEASUREMENT COMPLETE RV2 {args.law}: {len(rows)} steady-state VST3 tones, {len(checks)} scored/observed checks.",flush=True)

if __name__=="__main__":main()
