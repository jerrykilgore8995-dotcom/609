"""Steady-state spectra for the 2264 generated nodal solver."""
import json, math, subprocess, sys
from pathlib import Path
import numpy as np

RATE=44100
EXE=Path(sys.argv[1]).resolve()
FREQS=(100,1000,5000,10000)

def run(mode, hz, level, limiter=False):
    # Bench trace: limiter, ratio, recovery, threshold, fast, low, high,
    # burst duration, tail, carrier frequency.
    args=[str(EXE),"--trace",str(int(limiter)),"4","1",
          "9" if limiter else "11","0",str(level),str(level),
          "1.5","0.15",str(hz)]
    p=subprocess.run(args,capture_output=True,text=True,check=True,timeout=180)
    y=np.fromstring(p.stdout,sep=" ")
    if p.stderr.strip() not in ("","0"): raise RuntimeError(f"solver failures {p.stderr}")
    # Analyze final 100 ms, coherent for each test frequency.
    n=round(RATE*.1); y=y[-n:]; t=np.arange(n)/RATE
    out=[]
    for k in range(1,min(20,int(20000/hz))+1):
        w=2*np.pi*k*hz*t
        c=2*np.dot(y,np.cos(w))/n
        s=2*np.dot(y,np.sin(w))/n
        out.append(complex(c,-s))
    fund=abs(out[0]); thd=100*math.sqrt(sum(abs(z/fund)**2 for z in out[1:]))
    return {"mode":mode,"frequency_hz":hz,"input_dBu":level,
            "fundamental_peak":fund,"thd_percent":thd,
            "harmonics_dBc":[20*math.log10(max(abs(z/fund),1e-15)) for z in out[1:]],
            "fundamental_phase_deg":math.degrees(np.angle(out[0]))}

results={"note":"Generated nodal Solver, not the host-loaded VST3 wrapper. Bench trace currently supports compressor-only or limiter-only, not both engaged.","measurements":[]}
for mode,lim,level in (("compressor",False,10),("limiter",True,20)):
    for hz in FREQS:
        results["measurements"].append(run(mode,hz,level,lim))
Path("2264-nodal-spectra.json").write_text(json.dumps(results,indent=2))
print(json.dumps(results,indent=2))
