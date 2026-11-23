#!/usr/bin/env python3
"""Measure both limiter and compressor at every RV2/RV4 grid point.

Final VST3 sample rate 44.1kHz (internal 88.2kHz); only harmonics <=22kHz.
The compressor reference is independently checked at +4 and +10dBu.
Identical settings, source and normalization at all grid points.
"""
import sys,math,subprocess,json
from pathlib import Path
if len(sys.argv)!=6:raise SystemExit("Usage: test.py HOST VST3 RV2 RV4 OUTPUT")
host,bundle,rv2,rv4,out=sys.argv[1:]
out=Path(out);out.mkdir(parents=True,exist_ok=True)
def sample(label,level,compressor,limit):
    settings=[6,3,3,0,8,3,int(compressor),int(limit),0,0,1000,0,level,0,1.2,44100]
    cmd=[host,bundle,'--jd-case-80',*map(str,settings)]
    p=subprocess.run(cmd,capture_output=True,text=True,timeout=900)
    (out/f"{label}.stdout").write_text(p.stdout)
    (out/f"{label}.stderr").write_text(p.stderr)
    if p.returncode:raise RuntimeError(f"{label} host failure {p.returncode}: {p.stderr[-4000:]}")
    lines=p.stdout.strip().splitlines()
    if len(lines)!=80:raise RuntimeError(f"{label}: 80 bins expected, got {len(lines)}")
    h=[complex(*map(float,s.split())) for s in lines]
    f=abs(h[0])
    if not math.isfinite(f) or f<=0:raise RuntimeError("Invalid fundamental")
    q={k:abs(h[k-1])/f for k in range(2,23)}
    return dict(label=label,input_dBu=level,thd_percent=100*math.sqrt(sum(z*z for z in q.values())),
                output_dBu=24+20*math.log10(f),
                k2_dBc=20*math.log10(max(1e-20,q[2])),
                k3_dBc=20*math.log10(max(1e-20,q[3])),
                harmonics_dBc={f"K{k}":20*math.log10(max(1e-20,z)) for k,z in q.items()})
a=sample("limiter_22dBu",22,False,True)
b=sample("compressor_4dBu",4,True,False)
c=sample("compressor_10dBu",10,True,False)
# Increase of output in response to a 6dB input step; this detects any change
# in compressor ratio/knee without requiring unmeasured upstream references.
result=dict(rv2_ohms=int(rv2),rv4_ohms=int(rv4),sample_rate_hz=44100,
   upper_harmonic=22,limiter=a,compressor_4dBu=b,compressor_10dBu=c,
   compressor_output_step_for_6dB_input_dB=c['output_dBu']-b['output_dBu'])
(out/f"rv2-{rv2}-rv4-{rv4}.json").write_text(json.dumps(result,indent=2))
print(json.dumps({k:v for k,v in result.items() if k not in ('limiter','compressor_4dBu','compressor_10dBu')},indent=2))
print("LIMITER_THDPCT",a["thd_percent"],"LIMITER_OUTDBU",a["output_dBu"])
print("COMP4_THDPCT",b["thd_percent"],"COMP4_OUTDBU",b["output_dBu"])
print("COMP10_THDPCT",c["thd_percent"],"COMP10_OUTDBU",c["output_dBu"])
