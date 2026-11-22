#!/usr/bin/env python3
"""Nodal VST3 RV2 timing comparison: 1ms RMS envelopes, attack and release 63%/90%.
This is output-gain behavior NOT the Neve internal control-voltage factory instrument.
"""
import argparse,csv,math,subprocess,statistics,json
from pathlib import Path

RATE=44100
def measure(host,bundle,mode,attack,recovery,burst,stop,high,threshold,ratio):
    cmd=[str(host),str(bundle),"--jd-timing",mode,str(attack),str(recovery),str(burst),str(stop),str(high),str(threshold),str(ratio)]
    process=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,bufsize=65536)
    bins={}
    for line in process.stdout:
        try:
            ts,amplitude=map(float,line.split())
        except ValueError:
            continue
        idx=int(ts*1000+1e-7)
        a,n=bins.get(idx,(0.,0))
        bins[idx]=(a+amplitude*amplitude,n+1)
    errors=process.stderr.read()
    if process.wait()!=0:raise RuntimeError("Failed: "+" ".join(cmd)+" "+errors[-600:])
    if not bins: raise RuntimeError("Empty timing trace")
    return {i:24+20*math.log10(max(1e-14,math.sqrt(ss/n)*math.sqrt(2))) for i,(ss,n) in bins.items() if n>=30}

def average(x,lo,hi):
    values=[x.get(i) for i in range(int(lo*1000),int(hi*1000))]
    values=[v for v in values if v is not None and math.isfinite(v)]
    return statistics.median(values) if values else float("nan")

def crossing(x,t0,t1,initial,final,fraction):
    delta=final-initial
    if not math.isfinite(delta) or abs(delta)<0.5:return None
    for i in range(round(t0*1000),round(t1*1000)-3):
        series=[x.get(j) for j in range(i,i+4)]
        if any(v is None for v in series):continue
        progress=[(v-initial)/delta for v in series]
        if all(v>=fraction for v in progress):
            return round(i-t0*1000,3)
    return None

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("host");ap.add_argument("bundle");ap.add_argument("out");ap.add_argument("--law",required=True)
    a=ap.parse_args()
    out=Path(a.out);out.mkdir(parents=True,exist_ok=True)
    rows=[]
    cases=[]
    # Fast and slow limiter, all six limiter recoveries. Use threshold +8 dBu (internal position 9).
    for rec in range(1,7):
        for attack in (0,1):
            for burst in (100,500):
                stop=.45+burst/1000+(6.5 if rec>=5 else (3. if rec==4 else 1.8))
                cases.append(("limit",attack,rec,burst,stop,20.,9,3))
    # Compressor 3 attack positions, all 6 release positions, two signal durations, ratio 3:1.
    for rec in range(1,7):
        for attack in range(3):
            for burst in (100,500):
                stop=.45+burst/1000+(7.5 if rec>=5 else (4.5 if rec==4 else 3.5))
                cases.append(("comp",attack,rec,burst,stop,10.,1,3))
    for k,(mode,attack,rec,burst,stop,high,threshold,ratio) in enumerate(cases,1):
        envelope=measure(a.host,a.bundle,mode,attack,rec,burst,stop,high,threshold,ratio)
        start=.45;end=.45+burst/1000
        # small pre-window; for 100ms burst use its last 20ms.
        before=average(envelope,.32,.43)
        during=average(envelope,max(start+.03,end-.03),end-.005)
        after=average(envelope,max(end+1.0,stop-.2),stop-.04)
        attack63=crossing(envelope,start,end,before,during,.63212)
        attack90=crossing(envelope,start,end,before,during,.9)
        release63=crossing(envelope,end,stop,during,before,.63212)
        release90=crossing(envelope,end,stop,during,before,.9)
        # Control behavior requires gain relative to input, not raw output level
        # Output baseline differs from output during by input step plus compression.
        # Attack: measure dB gain change from dry input step in first 20ms vs final driven.
        # The waveform step itself contributes to output attack. Report this as an explicit proxy.
        row=dict(LAW=a.law,mode=mode,attack=attack,recovery=rec,burst_ms=burst,
                 pre_output_dBu=round(before,4),driven_output_dBu=round(during,4),late_output_dBu=round(after,4),
                 audio_attack63_ms=attack63,audio_attack90_ms=attack90,
                 audio_release63_ms=release63,audio_release90_ms=release90,
                 note="RMS audio output response includes input step; not factory detector CV trace")
        rows.append(row)
        if k%10==0:print(f"TIMING {a.law}: {k}/{len(cases)} measured",flush=True)
    with (out/"timing-raw.csv").open("w",newline="") as f:
        writer=csv.DictWriter(f,rows[0].keys());writer.writeheader();writer.writerows(rows)
    print(json.dumps({"law":a.law,"measurements":len(rows),"limiter_fast":[r for r in rows if r["mode"]=="limit" and r["recovery"]==1 and r["burst_ms"]==500],
                      "compressor_position":[r for r in rows if r["mode"]=="comp" and r["recovery"]==1 and r["burst_ms"]==500]},indent=2))
if __name__=="__main__":main()
