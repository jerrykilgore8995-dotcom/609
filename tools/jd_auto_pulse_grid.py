#!/usr/bin/env python3
"""JD A1/A2 10/50/100/500/1000 ms pulse history; factory-calibrated VST3.
Read output gain, not CV. Explicitly report attack depth and relative recovery.
A1/A2 are multi-stage; a single number is not their complete recovery law.
"""
import argparse,csv,json,math,statistics,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from rv2_timing_audit import measure
B=(10,50,100,500,1000)
def median(x,indices):
    v=[x[i] for i in indices if i in x and math.isfinite(x[i])]
    return statistics.median(v) if v else None
def first_stable(x,end,start,stop,origin,baseline,fraction):
    delta=baseline-origin
    if abs(delta)<0.4:return None
    for i in range(int(start*1000),int(stop*1000)-3):
        if all(j in x and (x[j]-origin)/delta >= fraction for j in range(i,i+4)):
            return i-end
    return None
def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("host");ap.add_argument("bundle");ap.add_argument("out");ap.add_argument("--mode",choices=["comp","limit"],required=True)
    ap.add_argument("--limiter-attack",type=int,choices=[0,1],default=0)
    a=ap.parse_args()
    out=Path(a.out);out.mkdir(parents=True,exist_ok=True)
    allrows=[]
    for modeidx in (5,6):
      for burst in B:
        mode=a.mode
        islim=mode=="limit"
        attack=a.limiter_attack if islim else 0
        high=20 if islim else 10
        low=10 if islim else -30
        threshold=9 if islim else 1
        ratio=3 if islim else 4
        stop=.45+burst/1000+9.0
        envelope=measure(a.host,a.bundle,mode,attack,modeidx,burst,stop,high,threshold,ratio)
        startidx=450;endidx=450+burst
        gain={i:v-(high if startidx<=i<endidx else low) for i,v in envelope.items()}
        before=median(gain,range(320,430))
        # For even a 10-ms pulse, sample pre-release envelope in the final 3 ms;
        # additionally report peak gain reduction so the two-stage recovery's
        # program dependence is not normalized away.
        driven=median(gain,range(max(startidx+2,endidx-5),endidx-1))
        recovery_origin=median(gain,range(endidx+2,endidx+6))
        after=median(gain,range(int(stop*1000)-300,int(stop*1000)-80))
        peak=min((gain[i] for i in range(startidx+2,endidx-1) if i in gain),default=None)
        if None in (before,driven,recovery_origin,after,peak):
            raise RuntimeError(f"Missing data in {mode} A{modeidx-4} {burst} ms")
        effective=before-recovery_origin
        values=dict(mode=mode,recovery="A1" if modeidx==5 else "A2",pulse_ms=burst,
          limiter_attack="fast" if attack==1 else "normal" if islim else "JD-stock",
          input_low_dBu=low,input_high_dBu=high,compressor_threshold_dBu=-20 if not islim else None,
          limiter_threshold_dBu=8 if islim else None,
          compressor_ratio="6:1" if not islim else None,
          baseline_gain_dB=round(before,3),driven_end_gain_dB=round(driven,3),
          peak_gain_reduction_dB=round(before-peak,3),initial_recovery_gain_dB=round(recovery_origin,3),
          recovered_gain_at_end_dB=round(after,3),
          initial_recovery_depth_dB=round(effective,3),
          recovered_63_ms=first_stable(gain,endidx,(endidx+6)/1000,stop,recovery_origin,before,.6321205588),
          recovered_90_ms=first_stable(gain,endidx,(endidx+6)/1000,stop,recovery_origin,before,.9),
          recovered_95_ms=first_stable(gain,endidx,(endidx+6)/1000,stop,recovery_origin,before,.95))
        for t in (50,100,250,500,1000,2000,5000):
            g=median(gain,range(endidx+t,max(endidx+t+1,endidx+t+8)))
            values[f"gain_reduction_remaining_{t}ms_dB"]=round(before-g,3) if g is not None else None
        allrows.append(values)
        print(json.dumps(values),flush=True)
    with (out/f"{a.mode}-auto-pulse-grid.csv").open("w",newline="") as f:
        w=csv.DictWriter(f,fieldnames=list(allrows[0]));w.writeheader();w.writerows(allrows)
    (out/f"{a.mode}-auto-pulse-grid.json").write_text(json.dumps(allrows,indent=2))
if __name__=="__main__":main()
