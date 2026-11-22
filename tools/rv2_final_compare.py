#!/usr/bin/env python3
"""Aggregate actual nodal-VST3 RV2 conformance CSVs, no assumptions of pass beyond manual tolerances."""
import csv,json,sys,statistics,math
from pathlib import Path
root=Path(sys.argv[1]);output=Path(sys.argv[2]);output.mkdir(parents=True,exist_ok=True)
def read(name):
    with (root/name).open(newline="",encoding="utf-8") as f:return list(csv.DictReader(f))
def fl(x):
    try:return float(x)
    except (ValueError,TypeError):return float("nan")
stock=read("stock/all-vst3-tones.csv");new=read("tuned/all-vst3-tones.csv")
checks1=read("stock/factory-and-range-checks.csv");checks2=read("tuned/factory-and-range-checks.csv")
key=lambda x:(x["Group"],x["Case"])
a={key(x):x for x in stock}; b={key(x):x for x in new}
assert len(a)==len(b) and a.keys()==b.keys(),f"Mismatched cases: stock{len(a)} tuned{len(b)}"
changes=[]
for k in a:
    x=a[k];y=b[k]
    changes.append(dict(Group=k[0],Case=k[1],ThresholdC_dBu=y["ThresholdC_dBu"],Ratio=y["Ratio"],
        ThresholdL_dBu=y["ThresholdL_dBu"],Input_dBu=y["Input_dBu"],Frequency_Hz=y["Frequency_Hz"],
        OriginalOutput_dBu=x["Output_dBu"],TunedOutput_dBu=y["Output_dBu"],
        DeltaOutput_dB=round(fl(y["Output_dBu"])-fl(x["Output_dBu"]),6),
        OriginalTHD_pct=x["THD_pct"],TunedTHD_pct=y["THD_pct"],
        DeltaTHD_pct=round(fl(y["THD_pct"])-fl(x["THD_pct"]),7),
        K2Original_dBc=x["K2_dBc"],K2Tuned_dBc=y["K2_dBc"],
        K3Original_dBc=x["K3_dBc"],K3Tuned_dBc=y["K3_dBc"]))
def export(name,rows):
    with (output/name).open("w",newline="",encoding="utf-8") as f:
        w=csv.DictWriter(f,fieldnames=rows[0].keys());w.writeheader();w.writerows(rows)
export("matched-parameter-differences.csv",changes)
lookup=lambda rows:{(x["Category"],x["Case"]):x for x in rows}
ca,cb=lookup(checks1),lookup(checks2)
comparison=[]
for k,x in ca.items():
    y=cb[k]
    comparison.append(dict(Category=k[0],Case=k[1],Original=fl(x["Measured"]),
        Tuned=fl(y["Measured"]),Delta=round(fl(y["Measured"])-fl(x["Measured"]),5),
        Nominal=x["FactoryNominal"],Tolerance=x["Tolerance"],OriginalPass=x["Pass"],
        TunedPass=y["Pass"],Method=x["Method"],Note=x["Note"]))
export("factory-check-comparison.csv",comparison)
def by(group):
    return [x for x in changes if x["Group"]==group]
def summary(group):
    rows=by(group)
    if not rows:return {}
    delta=[fl(x["DeltaOutput_dB"]) for x in rows]
    pairs=[(fl(x["DeltaTHD_pct"]),x) for x in rows]
    return dict(group=group,n=len(rows),max_abs_output_change_dB=round(max(map(abs,delta)),5),
                mean_abs_output_change_dB=round(statistics.mean(map(abs,delta)),5),
                max_abs_thd_change_percentage_points=round(max(abs(x[0]) for x in pairs),5),
                highest_tuned_thd_pct=round(max(fl(x["TunedTHD_pct"]) for x in rows),5),
                largest_thd_case=max(rows,key=lambda x:abs(fl(x["DeltaTHD_pct"])))["Case"])
def check(cat):
    return [x for x in comparison if x["Category"]==cat]
report={"original_case_count":len(stock),"tuned_case_count":len(new),
        "factory_ratios":check("comp-ratio"),"limiter_factory_ratio":check("limiter-ratio"),
        "limiter_alignment":check("limiter-alignment"),
        "factory_thd_proxies":check("thd"),
        "frequency_max_errors":{mode:max(abs(fl(x["Tuned"])) for x in check(mode)) if check(mode) else None for mode in ["through-frequency"]},
        "makeup":check("makeup"),
        "group_summaries":[summary(x) for x in sorted(set(r["Group"] for r in changes))],
        "largest_absolute_output_changes":sorted(changes,key=lambda x:abs(fl(x["DeltaOutput_dB"])),reverse=True)[:20],
        "largest_absolute_thd_changes":sorted(changes,key=lambda x:abs(fl(x["DeltaTHD_pct"])),reverse=True)[:20],
        "limiter_20dBu_threshold_comparison":[x for x in changes if x["Group"]=="limiter-all-threshold-attack-thd" and x["Input_dBu"]=="20" and x["Case"].endswith("fast0")],
        "limitations":["THD and FR at finite tone points only; user requested all detents, measured coverage is stated in CSV",
                       "No external physical 600ohm terminated noise measurement or internal calibration-control-voltage tap",
                       "All THD proxy measurements are not identical to factory handbook conditions",
                       "Limiter ratio factory validity only when fully driven (threshold +4/+8 at +10 input)"]}
try:
    timings=[read("timing-stock/timing-raw.csv"),read("timing-tuned/timing-raw.csv")]
    before={(r["mode"],r["attack"],r["recovery"],r["burst_ms"]):r for r in timings[0]}
    after={(r["mode"],r["attack"],r["recovery"],r["burst_ms"]):r for r in timings[1]}
    assert before.keys()==after.keys()
    timing_comparison=[]
    for k,x in before.items():
        y=after[k]
        row=dict(mode=k[0],attack=k[1],recovery=k[2],burst_ms=k[3])
        for v in ("pre_gain_dB","driven_gain_dB","late_gain_dB","audio_attack63_ms","audio_attack90_ms","audio_release63_ms","audio_release90_ms"):
            vx=fl(x[v]);vy=fl(y[v])
            row["Original_"+v]=x[v];row["Tuned_"+v]=y[v]
            row["Delta_"+v]=round(vy-vx,3) if math.isfinite(vx) and math.isfinite(vy) else None
        timing_comparison.append(row)
    export("time-domain-comparison.csv",timing_comparison)
    report["timing_count"]=len(timing_comparison)
    report["timing_limiter"]= [x for x in timing_comparison if x["mode"]=="limit" and x["burst_ms"]=="500"]
    report["timing_compressor"]= [x for x in timing_comparison if x["mode"]=="comp" and x["burst_ms"]=="500" and x["attack"]=="0"]
except FileNotFoundError:
    report["timing_status"]="Awaiting both timing artifacts"
(output/"rv2-complete-comparison.json").write_text(json.dumps(report,indent=2))
with (output/"rv2-complete-summary.txt").open("w") as f:
    print("MKIV RV2 200 ohm vs original: identical case-by-case nodal VST3 comparisons",file=f)
    print(f"Test tones: {len(stock)} per resistor position, matched 1:1",file=f)
    for cat in ("factory_ratios","limiter_factory_ratio","limiter_alignment","factory_thd_proxies"):
        print("\\n"+cat,file=f)
        for x in report[cat]:print(f"{x['Case']}: original {x['Original']:.5f}, RV2=200 {x['Tuned']:.5f}, change {x['Delta']:+.5f}",file=f)
    print("\\nSubgroup largest changes:",file=f)
    for row in report["group_summaries"]: print(row,file=f)
    print("\\nTop changed output levels:",file=f)
    for row in report["largest_absolute_output_changes"][:15]:print(f"{row['Group']}/{row['Case']} input {row['Input_dBu']} from {row['OriginalOutput_dBu']} to {row['TunedOutput_dBu']} delta {row['DeltaOutput_dB']}dB",file=f)
    if "timing_count" in report:print("\\nTiming cases compared:",report["timing_count"],file=f)
print((output/"rv2-complete-summary.txt").read_text()[:16000])
