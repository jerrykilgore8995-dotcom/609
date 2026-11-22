#!/usr/bin/env python3
"""Summarize C13/C14 voltage step responses; report settling conventions separately."""
import csv,glob,math,statistics,pathlib
out=pathlib.Path("results/control-voltage")
summary=[]
for path in sorted(out.glob("*.csv")):
    if path.name=="summary.csv":continue
    parts=path.stem.split("_")
    if len(parts)!=4:continue
    kind,rec,atk,case=parts
    with path.open() as f: rows=list(csv.DictReader(f))
    samples=[(float(r["time_s"]),float(r["control_voltage_V"])) for r in rows]
    def med(lo,hi):
        vals=[v for t,v in samples if lo<=t<hi]
        return statistics.median(vals) if vals else float("nan")
    before=med(.52,.62)
    final=med(samples[-1][0]-.18,samples[-1][0]-.02)
    delta=final-before
    row=dict(mode=kind,recovery=int(rec),attack=int(atk),step=case,pre_V=before,final_V=final,delta_V=delta)
    for fraction in (.63212,.9,.95,.99):
        target=before+fraction*delta
        crossing=None
        for t,v in samples:
            if t<.65:continue
            if ((v-before)/delta if abs(delta)>1e-9 else 0)>=fraction:
                crossing=(t-.65)*1000;break
        row["t"+str(round(100*fraction))+"_ms"]=round(crossing,2) if crossing is not None else ""
    # settling to within 1% of endpoint for 20 consecutive approx-1ms samples
    settled=None
    for i,(t,v) in enumerate(samples):
        if t<.65:continue
        if i+20<=len(samples) and all(abs(x-final)<=.01*abs(delta) for _,x in samples[i:i+20]):
            settled=round((t-.65)*1000,2);break
    row["settled_1pct_20ms"]=settled if settled is not None else ""
    summary.append(row)
    print(row,flush=True)
if summary:
    with (out/"summary.csv").open("w",newline="") as f:
        writer=csv.DictWriter(f,summary[0].keys());writer.writeheader();writer.writerows(summary)
