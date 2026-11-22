"""Measure MKIV dynamic audio-envelope settling times from actual VST3 test-host traces.
The recovery figures are OUTPUT ENVELOPE proxies, not the Neve internal control-voltage factory time.
"""
from pathlib import Path
import csv
import math
import sys

def rms_bins(path):
    buckets={}
    with path.open(encoding="utf-8-sig") as f:
        for line in f:
            parts=line.split()
            if len(parts)!=2: continue
            try: t=float(parts[0]); y=float(parts[1])
            except ValueError: continue
            i=int(t*1000 + 1.e-7)
            ss,n=buckets.get(i,(0.,0))
            buckets[i]=(ss+y*y,n+1)
    return {i:24+20*math.log10(max(math.sqrt(ss/n)*math.sqrt(2),1.e-14))
            for i,(ss,n) in buckets.items() if n>25}

def avg(dbmap,start,end):
    vals=[dbmap.get(i) for i in range(int(start*1000),int(end*1000))]
    vals=[v for v in vals if v is not None and math.isfinite(v)]
    return sum(vals)/len(vals) if vals else float("nan")

def settling(bins,from_s,to_s,target,tol=1.0,window=4):
    start=int(from_s*1000)
    stop=int(to_s*1000)
    for i in range(start,stop-window):
        if all(j in bins and abs(bins[j]-target)<=tol for j in range(i,i+window)):
            return (i-start)
    return float("nan")

def main(folder):
    results=[]
    for path in sorted(folder.glob("*.txt")):
        if path.name=="psu-noise.txt": continue
        parts=path.stem.split("_")
        # e.g. limit_slow_r1_0 ; comp_a0_r1
        if len(parts)!=4: continue
        mode,attack,rec,duration=parts
        burst=.5; end=.95
        stop=float(duration)/10.
        bins=rms_bins(path)
        before=avg(bins,.35,.43)
        during=avg(bins,.82,.92)
        after=avg(bins,stop-.30,stop-.10)
        attack_time=settling(bins,.45,.75,during)
        release_time=settling(bins,end,min(stop-.1,end+5.5),before)
        results.append({"case":path.stem,"mode":mode,"attack_position":attack,
                        "recovery_position":rec,"preburst_dBu":round(before,3),
                        "burst_dBu":round(during,3),"postburst_dBu":round(after,3),
                        "audio_attack_within1dB_ms":attack_time,
                        "audio_release_within1dB_ms":release_time,
                        "note":"Audio envelope proxy; not internal factory CV calibration"})
    with (folder/"timing-summary.csv").open("w",newline="",encoding="utf-8") as f:
        if results:
            w=csv.DictWriter(f,fieldnames=results[0].keys());w.writeheader();w.writerows(results)
    print("MKIV VST3 audio-envelope timing measurements")
    for r in results:
        print(r["case"],"attack",r["audio_attack_within1dB_ms"],"ms;",
              "release",r["audio_release_within1dB_ms"],"ms;",
              "levels",r["preburst_dBu"],r["burst_dBu"],r["postburst_dBu"])
    print("NOTE: Neve recovery factory specification is based on internal control voltage, not merely output envelope.")
if __name__=="__main__":
    main(Path(sys.argv[1]))
