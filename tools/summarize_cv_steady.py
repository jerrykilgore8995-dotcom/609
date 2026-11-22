#!/usr/bin/env python3
"""Steady-conditioned capacitor-voltage recovery diagnostics.
No unsubstantiated 'Neve 90%' endpoint claim. Phase-bin averaged waveforms.
"""
import csv,math,pathlib,statistics
root=pathlib.Path("results/cv-steady")
rows=[]
for fn in sorted(root.glob("*.csv")):
 if fn.name=="summary.csv":continue
 parts=fn.stem.split("_")
 if len(parts)!=4:continue
 mode,rec,attack,step_type=parts
 with fn.open(newline="") as f:
  points=[(float(x["time_s"]),float(x["control_voltage_V"])) for x in csv.DictReader(f)]
 step=8.
 def median(a,b):
  q=[v for t,v in points if a<=t<b]
  return statistics.median(q) if q else float('nan')
 def mean(a,b):
  q=[v for t,v in points if a<=t<b]
  return sum(q)/len(q) if q else float('nan')
 before=mean(step-.30,step-.10)
 pre_earlier=mean(step-.70,step-.50)
 final=mean(points[-1][0]-.35,points[-1][0]-.15)
 final_earlier=mean(points[-1][0]-.75,points[-1][0]-.55)
 delta=final-before
 # Average 10ms windows to reject detector ripple/1kHz waveform.
 bins={}
 for t,v in points:
  if t<step: continue
  k=int((t-step)*100)
  total,n=bins.get(k,(0.,0)); bins[k]=(total+v,n+1)
 smoothed=[(k*10.,ss/n) for k,(ss,n) in sorted(bins.items()) if n>=5]
 row=dict(mode=mode,recovery=int(rec),attack=int(attack),step=step_type,pre_V=round(before,5),
  pre_drift_V_400ms=round(before-pre_earlier,5),final_V=round(final,5),
  late_drift_V_400ms=round(final-final_earlier,5),delta_V=round(delta,5))
 for frac in [.5,.6321205588,.9,.95,.99]:
  v=next((t for t,y in smoothed if abs(delta)>.01 and ((y-before)/delta)>=frac),None)
  row[f"t{round(frac*100)}_ms"]=v if v is not None else ""
 # Strict: terminal reference not established while drifting > 1 percent of voltage change
 stable=(abs(before-pre_earlier)<.01*abs(delta) and abs(final-final_earlier)<.01*abs(delta)) if abs(delta)>.01 else False
 row["steady_endpoints"]=stable
 # Charge/discharge average in first 10,50,100ms for visual audit
 for ms in [10,50,100,200,400,800,1500,3000]:
  row[f"CV_{ms}ms"]=round(mean(step+ms/1000.,step+(ms+20)/1000.),5)
 rows.append(row)
 print(row,flush=True)
if rows:
 with (root/"summary.csv").open("w",newline="") as f:
  w=csv.DictWriter(f,rows[0].keys());w.writeheader();w.writerows(rows)
