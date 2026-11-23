#!/usr/bin/env python3
"""Measure every RV1 setting: limiter & compressor THD, output level, ratio check.
Use actual 44.1kHz final VST3 output; sum K2-K22 only (22kHz).
"""
import json,math,pathlib,subprocess,sys
host,bundle,pct,folder=sys.argv[1:]
folder=pathlib.Path(folder);folder.mkdir(parents=True,exist_ok=True)
def measure(name,source,compressor,limiter):
    args=[6,3,3,0,8,3,int(compressor),int(limiter),0,0,1000,0,source,0,1.2,44100]
    cmd=[host,bundle,'--jd-case-80',*map(str,args)]
    p=subprocess.run(cmd,capture_output=True,text=True,timeout=900)
    (folder/f"{name}.stdout").write_text(p.stdout)
    (folder/f"{name}.stderr").write_text(p.stderr)
    if p.returncode:raise RuntimeError(name+f": host exit={p.returncode}, stderr={p.stderr[-2000:]}")
    lines=p.stdout.strip().splitlines()
    if len(lines)!=80:raise RuntimeError(f"{name}: expected 80 harmonics got {len(lines)}")
    h=[complex(*map(float,line.split())) for line in lines]
    fund=abs(h[0])
    if not math.isfinite(fund) or fund<=0:raise RuntimeError(f"{name}: invalid fundamental {fund}")
    harmonics={k:abs(h[k-1])/fund for k in range(2,23)}
    return {"input_dBu":source,"output_dBu":24+20*math.log10(fund),
            "thd_percent":100*math.sqrt(sum(x*x for x in harmonics.values())),
            "harmonic_dBc":{f"K{k}":20*math.log10(max(1e-20,v)) for k,v in harmonics.items()}}
lim=measure("limiter_22dBu",22,False,True)
comp4=measure("comp_4dBu",4,True,False)
comp10=measure("comp_10dBu",10,True,False)
through=measure("through_4dBu",4,False,False)
result={"rv1_percent":int(pct),"rv2_ohms":200,"rv4_ohms":1669.66,
  "diodes":"Diodes Incorporated 1N4148W x4","attack_resistors_ohms":[1600,1600],
  "limiter":lim,"compressor_4dBu":comp4,"compressor_10dBu":comp10,
  "through_4dBu":through,
  "comp_6dB_input_step_output_dB":comp10["output_dBu"]-comp4["output_dBu"]}
(folder/f"rv1-{pct}.json").write_text(json.dumps(result,indent=2))
print("RV1_RESULT",json.dumps({
 "pct":int(pct),"lim_thd":lim["thd_percent"],"lim_out":lim["output_dBu"],
 "comp4_thd":comp4["thd_percent"],"comp4_out":comp4["output_dBu"],
 "comp10_thd":comp10["thd_percent"],"comp10_out":comp10["output_dBu"],
 "comp_step":result["comp_6dB_input_step_output_dB"],
 "through4_thd":through["thd_percent"],"through4_out":through["output_dBu"]}))
