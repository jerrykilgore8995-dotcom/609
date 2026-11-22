#!/usr/bin/env python3
"""Measure limiter factory attack via output return to 1dB after +10 to +20dBu step."""
import csv, math, statistics, subprocess, sys
from pathlib import Path
host,bundle,dest=sys.argv[1:4];out=Path(dest);out.mkdir(parents=True,exist_ok=True)
def trace(attack):
    cmd=[host,bundle,'--jd-timing','limit',str(attack),'1','500','1.7','20','9','3']
    p=subprocess.Popen(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    d={}
    for line in p.stdout:
        z=line.split()
        if len(z)!=2:continue
        try:t,v=map(float,z)
        except ValueError:continue
        index=math.floor(t*1000+1e-5);ss,n=d.get(index,(0.,0));d[index]=(ss+v*v,n+1)
    err=p.stderr.read()
    if p.wait()!=0: raise RuntimeError(err)
    return {k:24+20*math.log10(max(1e-14,math.sqrt(ss/n)*math.sqrt(2))) for k,(ss,n) in d.items() if n>=30}
rows=[]
for fast in (0,1):
    v=trace(fast)
    pre=statistics.median(v[i] for i in range(380,430) if i in v)
    final=statistics.median(v[i] for i in range(820,930) if i in v)
    early=[(i,v.get(i)) for i in range(448,473)]
    first=None
    for k in range(450,485):
        if all(i in v and abs(v[i]-final)<=1. for i in range(k,k+4)):
            first=k-450;break
    rows.append({'limiter':'fast' if fast else 'slow','expected_ms':2 if fast else 4,
                 'measured_within_1dB_ms':first,'pre_dBu':pre,'final_dBu':final,
                 'within_factory_tolerance':(first is not None and abs(first-(2 if fast else 4))<=1),
                 'early_output_level':str(early)})
    print(rows[-1],flush=True)
with (out/'factory-attack.csv').open('w',newline='') as f:
    w=csv.DictWriter(f,rows[0].keys());w.writeheader();w.writerows(rows)
