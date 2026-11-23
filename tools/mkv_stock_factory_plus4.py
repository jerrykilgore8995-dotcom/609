#!/usr/bin/env python3
"""Exactly one stock MKV Neve JD limiter THD measurement, +4dBu threshold."""
import json,math,pathlib,subprocess,sys
host,bundle,folder=sys.argv[1:];out=pathlib.Path(folder);out.mkdir(parents=True,exist_ok=True)
# control order: compress threshold, ratio, recovery, gain,
# limiter threshold, recovery, compressor in, limiter in, fast attack,
# compressor attack, freq, low, high, burst, duration, sample rate
# limit index 0 = +4 dBu; release index 3 = 800ms; fast=0 = slow.
args=[6,4,3,0,0,3,0,1,0,0,1000,0,22,0,1.2,44100]
p=subprocess.run([host,bundle,"--jd-case-80",*map(str,args)],capture_output=True,text=True,timeout=900)
(out/"stock_mkv_factory_plus4.stdout").write_text(p.stdout)
(out/"stock_mkv_factory_plus4.stderr").write_text(p.stderr)
if p.returncode:raise RuntimeError(f"VST3 test failed {p.returncode} {p.stderr[-2500:]}")
lines=p.stdout.strip().splitlines()
if len(lines)!=80:raise RuntimeError(f"Expected 80 bins; got {len(lines)}")
h=[complex(*map(float,line.split())) for line in lines]
fund=abs(h[0]); assert math.isfinite(fund) and fund>0
harm={k:abs(h[k-1])/fund for k in range(2,23)}
result={"circuit":"Stock MKV factory calibration","diodes":"Diodes Inc 1N4148W x4","R28_ohm":1600,"R30_ohm":1600,
"R31_fast_ohm":680,"RV1_percent":34.5,"RV2_ohm":200,"RV4_ohm":1669.66,"RV5_percent":30,
"input_dBu":22,"tone_hz":1000,"limiter_threshold_dBu":4,"limiter_recovery_ms":800,"attack":"slow",
"limiter_on":True,"compressor_on":False,"host_hz":44100,"internal_hz":88200,
"THD_K2_to_K22_percent":100*math.sqrt(sum(v*v for v in harm.values())),
"output_dBu":24+20*math.log10(fund),"K3_dBc":20*math.log10(max(harm[3],1e-20)),
"harmonics_dBc":{f"K{k}":20*math.log10(max(v,1e-20)) for k,v in harm.items()}}
(out/"stock_mkv_plus4_factory_thd.json").write_text(json.dumps(result,indent=2))
print("STOCK_MKV_FACTORY_RESULT",json.dumps(result))
