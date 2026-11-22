#!/usr/bin/env python3
"""Short, equal-conditions low-frequency manufacturer bridge check (100Hz, 1kHz)."""
import subprocess,csv,math,sys,json
from pathlib import Path
host,bundle,dest=sys.argv[1:]
dest=Path(dest);dest.mkdir(parents=True,exist_ok=True)
rows=[]
for hz in (100,1000):
 for level in (12,18):
  for mode,ci,li in (('through',0,0),('compressor',1,0),('limiter',0,1),('both',1,1)):
   controls=[15,2,0,0,22,0,ci,li,0,0,hz,0,level,0,1.2,44100]
   p=subprocess.run([host,bundle,'--jd-case-80',*map(str,controls)],capture_output=True,text=True,timeout=210)
   if p.returncode:raise RuntimeError(p.stderr[-1500:])
   bins=[complex(*map(float,z.split())) for z in p.stdout.splitlines() if len(z.split())==2]
   if len(bins)!=80:raise RuntimeError(f'{hz} {level} {mode}: {len(bins)} bins')
   base=abs(bins[0]);valid=min(80,int(22049/hz))
   thd=100*math.sqrt(sum(abs(x)**2 for x in bins[1:valid]))/base
   row=dict(frequency_Hz=hz,input_dBu=level,mode=mode,output_dBu=round(24+20*math.log10(base),6),thd_percent=round(thd,8),k2_dBc=round(20*math.log10(abs(bins[1])/base),4),k3_dBc=round(20*math.log10(abs(bins[2])/base),4))
   rows.append(row);print(row,flush=True)
with (dest/'100hz-1khz.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
(dest/'100hz-1khz.json').write_text(json.dumps(rows,indent=2))
