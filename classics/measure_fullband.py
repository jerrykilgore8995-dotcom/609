"""2264 nodal full-band response, phase and harmonic sweeps."""
import json, math, subprocess, sys
from pathlib import Path
import numpy as np
RATE=44100
EXE=Path(sys.argv[1]).resolve()
FREQS=(20,30,50,100,200,500,1000,2000,5000,8000,10000,12000,15000,18000,20000)

def capture(hz,level,comp,limit,ratio=5,crec=3,lrec=4,cth=11,lth=1):
    # Bench trace args: limiter flag, ratio, recovery, threshold, fast,
    # low dBu, high dBu, burst seconds, tail seconds, frequency, comp on, limiter on.
    values=[int(limit),ratio,crec if comp else lrec,cth if comp else lth,0,
            level-0.001,level,1.5,.15,hz,int(comp),int(limit)]
    p=subprocess.run([str(EXE),"--trace",*map(str,values)],capture_output=True,text=True,check=True,timeout=240)
    y=np.fromstring(p.stdout,sep=" ")
    fails=int(p.stderr.strip() or "0")
    n=round(RATE*.1); end=round(1.8*RATE); seg=y[end-n:end]
    t=np.arange(n)/RATE
    hs=[]
    for k in range(1,min(20,int(20000/hz))+1):
        a=2*np.dot(seg,np.cos(2*np.pi*k*hz*t))/n
        b=-2*np.dot(seg,np.sin(2*np.pi*k*hz*t))/n
        hs.append(complex(a,b))
    f=abs(hs[0])
    thd=100*math.sqrt(sum(abs(z/f)**2 for z in hs[1:]))
    return {"frequency_hz":hz,"input_dBu":level,"output_fundamental_peak":f,
            "relative_output_dB":20*math.log10(max(f,1e-15)),
            "phase_deg":math.degrees(np.angle(hs[0])),"thd_percent":thd,
            "K2_to_K20_dBc":[round(20*math.log10(max(abs(z/f),1e-15)),3) for z in hs[1:]],
            "solver_failures":fails}

modes=[("through",False,False,0),("compressor",True,False,10),
       ("limiter",False,True,20),("combined",True,True,20)]
result={"sample_rate":RATE,"analysis_window_ms":100,"modes":{}}
for name,comp,lim,level in modes:
    rows=[]
    for hz in FREQS:
        rows.append(capture(hz,level,comp,lim))
    ref=next(x["relative_output_dB"] for x in rows if x["frequency_hz"]==1000)
    for x in rows:
        x["response_relative_1k_dB"]=x["relative_output_dB"]-ref
    result["modes"][name]=rows
Path("2264-fullband-report.json").write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2))
