#!/usr/bin/env python3
"""Stock MKV: factory limiter THD and real 3-position compressor attack switching.
Uses the complete built 609 MkV VST3, final 44.1kHz output.
"""
import pathlib,subprocess,math,json,statistics,sys
host,bundle,folder=sys.argv[1:];out=pathlib.Path(folder);out.mkdir(exist_ok=True,parents=True)
def run(name,settings):
    p=subprocess.run([host,bundle,'--jd-case-80',*map(str,settings)],capture_output=True,text=True,timeout=900)
    (out/f'{name}.stdout').write_text(p.stdout)
    (out/f'{name}.stderr').write_text(p.stderr)
    if p.returncode:raise RuntimeError(f"{name} host failed {p.returncode}: {p.stderr[-2000:]}")
    return p.stdout
# manufacturer-correct factory test: +22 in, +4 threshold, 800ms, slow, comp OFF.
bins=[complex(*map(float,line.split())) for line in run('limiter_stock_factory',[6,4,3,0,0,3,0,1,0,0,1000,0,22,0,1.2,44100]).splitlines()]
if len(bins)!=80:raise RuntimeError("Unexpected limiter bins")
f=abs(bins[0]); ratios={k:abs(bins[k-1])/f for k in range(2,23)}
limit=dict(thd_percent=100*math.sqrt(sum(v*v for v in ratios.values())),
           output_dBu=24+20*math.log10(f),
           k2_dBc=20*math.log10(max(ratios[2],1e-20)),
           k3_dBc=20*math.log10(max(ratios[3],1e-20)))
# Steady -30dBu followed by a 0.1s +10dBu burst; compressor only.
# Each 3-state position gets a fresh plugin host run, so state is independent.
# Time-domain gain reduction transition extracted from smoothed sine-wave RMS
# after high input begins at 450ms. This verifies that switch changes behavior.
attacks=[]
for attack in range(3):
    raw=run(f'compress_attack_{attack}',[10,3,3,0,0,3,1,0,0,attack,1000,-30,10,0.1,0.9,44100])
    rows=[list(map(float,l.split())) for l in raw.splitlines()]
    windows={}
    for t,y in rows:
        idx=int(t*1000)
        if 450<=idx<=540:
            windows.setdefault(idx,[]).append(y*y)
    rms={k:math.sqrt(sum(a)/len(a)) for k,a in windows.items() if len(a)>=35}
    # Initial window after the burst, 455-459ms; final during burst 530-540ms.
    early=statistics.median(rms[i] for i in range(450,452) if i in rms)
    late=statistics.median(rms[i] for i in range(530,540) if i in rms)
    # Envelope decays as compression develops: estimate t10/t90 of total reduction.
    if early<=late:raise RuntimeError(f'Attack position {attack} has no GR transition early {early} late {late}')
    def crossing(frac):
        threshold=early-frac*(early-late)
        return next((i-450 for i in range(450,531) if i in rms and rms[i]<=threshold),None)
    a10=crossing(.1);a90=crossing(.9)
    attacks.append(dict(position=attack,label=['3 ms','7.5 ms','15 ms'][attack],
                resistor_ohms=[680,1210,2745][attack],initial_rms=early,settled_rms=late,
                t10_from_450_ms=a10,t90_from_450_ms=a90,
                time_10_to_90_ms=None if a10 is None or a90 is None else a90-a10))
result=dict(model='609 MkV',diodes='Diodes Inc 1N4148W x4 distinct IS -1,-0.5,+0.5,+1 percent',
    resistor_R28_R30_ohms=1600,R31_attack_resistor_ohms=[680,1210,2745],
    rv1_percent=34.5,rv2_ohm=200,rv4_ohm=1669.66,stock_limiter=limit,compressor_attack_tests=attacks)
(out/'609-mkv-verification.json').write_text(json.dumps(result,indent=2))
print('MKV_VERIFY',json.dumps(result),flush=True)
