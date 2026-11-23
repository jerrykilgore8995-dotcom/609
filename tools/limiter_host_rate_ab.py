#!/usr/bin/env python3
"""Measure rendered 609 plugin THD at 44.1k and 192k, same limiter settings.

Measure the final VST3 output, not the internal 88.2k solver output.
44.1k uses K2-K22; 192k uses K2-K80. Both use 1kHz.
"""
import json,math,pathlib,subprocess,sys
host,bundle,model,folder=sys.argv[1:]
folder=pathlib.Path(folder);folder.mkdir(parents=True,exist_ok=True)
cases=[]
for rate,last_harmonic in ((44100,22),(192000,80)):
    # Vst3Smoke.cpp --jd-case-80 outputs K1 through K80 at either host rate.
    # settings are zero-indexed JUCE parameter indices; compressor enabled=0,
    # compressor makeup setting=0, limiter enabled=1, LT +8 (index 8), LR 800 (index 3).
    settings=[6,4,3,0,8,3,0,1,0,0,1000,0,22,0,1.2,rate]
    cmd=[host,bundle,"--jd-case-80",*map(str,settings)]
    p=subprocess.run(cmd,capture_output=True,text=True,timeout=900)
    (folder/f"raw-{model}-{rate}.stdout.txt").write_text(p.stdout)
    (folder/f"raw-{model}-{rate}.stderr.txt").write_text(p.stderr)
    if p.returncode:raise RuntimeError(f"{rate} host returned {p.returncode}: {p.stderr[-3000:]}")
    rows=p.stdout.strip().splitlines()
    if len(rows)!=80:raise RuntimeError(f"{rate}: expected 80 complex lines, got {len(rows)}")
    bins=[]
    for row in rows:
        parts=row.split()
        if len(parts)!=2:raise RuntimeError(f"{rate}: unexpected harmonic row {row!r}")
        bins.append(complex(*map(float,parts)))
    fundamental=abs(bins[0])
    if fundamental<=0:raise RuntimeError(f"{rate}: missing fundamental")
    ratios={k:abs(bins[k-1])/fundamental for k in range(2,last_harmonic+1)}
    thd=100*math.sqrt(sum(v*v for v in ratios.values()))
    shared=100*math.sqrt(sum(ratios[k]**2 for k in range(2,21)))
    result={
        "model":model,"host_rate_hz":rate,
        "internal_solver_rate_hz":88200 if rate==44100 else 192000,
        "frequency_hz":1000,"input_dBu":22,
        "compressor_enabled":False,"compressor_makeup_dB":0,
        "limiter_enabled":True,"limiter_threshold_dBu":8,"limiter_recovery_ms":800,
        "end_to_end_plugin_output":True,
        "harmonics_used":f"K2-K{last_harmonic}",
        "measurement_bandwidth_hz":last_harmonic*1000,
        "thd_percent":thd,"common_band_K2_K20_thd_percent":shared,
        "harmonic_dBc":{f"K{k}":20*math.log10(max(ratios[k],1e-20)) for k in range(2,last_harmonic+1)},
        "note":("Output of decimated 44.1kHz plugin, Nyquist=22.05kHz"
                if rate==44100 else "Output of 192kHz plugin, Nyquist=96kHz; K80=80kHz")
    }
    (folder/f"limiter-{model}-{rate}.json").write_text(json.dumps(result,indent=2))
    print(json.dumps({k:v for k,v in result.items() if k!="harmonic_dBc"},indent=2),flush=True)
    cases.append(result)
summary={"model":model,"thd_44100_percent":cases[0]["thd_percent"],
         "thd_192000_percent":cases[1]["thd_percent"],
         "shared_K2_K20_44100_percent":cases[0]["common_band_K2_K20_thd_percent"],
         "shared_K2_K20_192000_percent":cases[1]["common_band_K2_K20_thd_percent"],
         "delta_full_band_percentage_points":cases[1]["thd_percent"]-cases[0]["thd_percent"]}
(folder/f"comparison-{model}.json").write_text(json.dumps(summary,indent=2))
print("COMPARISON",json.dumps(summary,indent=2),flush=True)
