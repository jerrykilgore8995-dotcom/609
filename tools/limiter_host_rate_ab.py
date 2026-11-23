#!/usr/bin/env python3
"""Actual final 44.1 kHz VST3 limiter THD, 1 kHz, K2 through K22 (22 kHz).
No 192 kHz comparison, compressor test, frequency sweep, or circuit adjustment.
"""
import json,math,pathlib,subprocess,sys
host,bundle,model,folder=sys.argv[1:]
folder=pathlib.Path(folder);folder.mkdir(parents=True,exist_ok=True)
# 44.1kHz final host output (88.2kHz solver and halfband decimation).
rate=44100
# Vst3Smoke positions are zero-based for each JUCE selector; compressor
# enable=0, makeup knob index=0, limiter enable=1, threshold=+8dBu,
# release=800ms, +22dBu source, 1kHz, 1.2s acquisition.
settings=[6,4,3,0,8,3,0,1,0,0,1000,0,22,0,1.2,rate]
cmd=[host,bundle,"--jd-case-80",*map(str,settings)]
p=subprocess.run(cmd,capture_output=True,text=True,timeout=900)
(folder/f"host-{model}.stdout.txt").write_text(p.stdout)
(folder/f"host-{model}.stderr.txt").write_text(p.stderr)
if p.returncode:raise RuntimeError(f"VST3 exit {p.returncode}: {p.stderr[-4000:]}")
rows=p.stdout.strip().splitlines()
if len(rows)!=80:raise RuntimeError(f"Expected 80 harmonic bins, got {len(rows)}: {p.stdout[-1000:]}")
bins=[complex(*map(float,row.split())) for row in rows]
fund=abs(bins[0])
if not math.isfinite(fund) or fund<=0:raise RuntimeError("Invalid 1kHz fundamental")
ratios={k:abs(bins[k-1])/fund for k in range(2,23)}
thd=100*math.sqrt(sum(v*v for v in ratios.values()))
result=dict(model=model,host_sample_rate_hz=44100,internal_solver_rate_hz=88200,
            measured_output="final decimated VST3",fundamental_hz=1000,
            input_dBu=22,limiter_threshold_dBu=8,limiter_recovery_ms=800,
            compressor_in=False,compressor_makeup_dB=0,limiter_in=True,
            harmonic_range="K2-K22",measurement_bandwidth_hz=22000,
            thd_22khz_percent=thd,\n            output_dBu=24+20*math.log10(fund),\n            dominant_k3_dBc=20*math.log10(max(ratios[3],1e-20)),
            k2_to_k20_percent=100*math.sqrt(sum(ratios[k]**2 for k in range(2,21))),
            harmonics_dBc={f"K{k}":20*math.log10(max(v,1e-20)) for k,v in ratios.items()})
(folder/f"limiter-22khz-{model}.json").write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2),flush=True)
