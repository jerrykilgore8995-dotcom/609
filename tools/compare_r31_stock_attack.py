#!/usr/bin/env python3
"""Single controlled R31 A/B on packaged VST3 compressor. Do not edit any DSP."""
import csv,sys,math,statistics
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from rv2_timing_audit import measure,average,crossing
host,bundle,folder,variant=sys.argv[1:]
folder=Path(folder);folder.mkdir(parents=True,exist_ok=True)
rows=[]
for attack in (0,1,2):
 for burst in (100,500):
  recovery=1;stop=3.2+burst/1000
  e=measure(host,bundle,"comp",attack,recovery,burst,stop,10.,1,4)
  st=.45;end=st+burst/1000
  gain={i:v- (10 if st<=i/1000<end else -30) for i,v in e.items()}
  pre=average(gain,.32,.43)
  during=average(gain,max(st+.03,end-.03),end-.005)
  after=average(gain,stop-.32,stop-.07)
  r=dict(variant=variant,attack_detent=attack,burst_ms=burst,R31_stock_ohm=375 if variant=="375" else 680,
    baseline_gain_dB=round(pre,3),during_gain_dB=round(during,3),late_gain_dB=round(after,3),
    gain_change_dB=round(during-pre,3),
    attack63_ms=crossing(gain,st+.002,end,pre,during,.6321205588),
    attack90_ms=crossing(gain,st+.002,end,pre,during,.9),
    release63_ms=crossing(gain,end+.002,stop,during,pre,.6321205588),
    release90_ms=crossing(gain,end+.002,stop,during,pre,.9))
  rows.append(r);print(r,flush=True)
with (folder/"r31-compressor-attack.csv").open("w",newline="") as f:
 w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
