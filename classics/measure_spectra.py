"""Steady-state spectra for the 2264 generated nodal solver."""
import json, math, subprocess, sys
from pathlib import Path
import numpy as np
RATE=44100
EXE=Path(sys.argv[1]).resolve()
FREQS=(100,1000,5000,10000)
def run(mode,hz,level,limiter=False):
    args=[str(EXE),"--trace",str(int(limiter)),"4","1","9" if limiter else "11","0",
          str(level-20),str(level),"1.5","0.15",str(hz)]
    p=subprocess.run(args,capture_output=True,text=True,check=True,timeout=180)
    y=np.fromstring(p.stdout,sep=" ")
    failures=int(p.stderr.strip() or "0")
    if failures: raise RuntimeError(f"{failures} nodal solve failures")
    n=round(RATE*.1); end=round((.3+1.5)*RATE); segment=y[end-n:end]
    t=np.arange(n)/RATE; harmonics=[]
    for k in range(1,min(20,int(20000/hz))+1):
        w=2*np.pi*k*hz*t
        harmonics.append(complex(2*np.dot(segment,np.cos(w))/n,-2*np.dot(segment,np.sin(w))/n))
    fund=abs(harmonics[0])
    return {"mode":mode,"frequency_hz":hz,"input_dBu":level,"fundamental_peak":fund,
            "thd_percent":100*math.sqrt(sum(abs(z/fund)**2 for z in harmonics[1:])),
            "harmonics_dBc":[round(20*math.log10(max(abs(z/fund),1e-15)),3) for z in harmonics[1:]],
            "fundamental_phase_deg":math.degrees(np.angle(harmonics[0])),"solver_failures":failures}
results={"note":"Generated nodal solver waveform, not host-loaded VST3. Compressor-only and limiter-only. Analysis window is final 100ms of high-level burst.","measurements":[]}
for mode,lim,level in (("compressor",False,10),("limiter",True,20)):
    for hz in FREQS: results["measurements"].append(run(mode,hz,level,lim))
Path("2264-nodal-spectra.json").write_text(json.dumps(results,indent=2))
print(json.dumps(results,indent=2))
