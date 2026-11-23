#!/usr/bin/env python3
"""Read-only 1kHz limiter transient / current audit of the existing 609 SPICE deck.

Requires a test checkout after diodesinc_bridge_fixture.py has selected a
bridge model. Does not edit or approximate a circuit element or diode model.
Reports average, RMS, AC ripple of control voltages and currents after 1s
settling, the waveform THD in the available 1k-20k Nyquist harmonics, and
actual current differences between the limiter driver and bridge bias nodes.
"""
import json,math,re,subprocess,sys
from pathlib import Path
import numpy as np
name=sys.argv[1]
root=Path.cwd()
circuit=root/"circuit"
out=root/"results"
out.mkdir(parents=True,exist_ok=True)
src=(circuit/"609.cir").read_text()
# The existing factory harness fixes all circuit values. Only test stimulus
# and the front-panel limiter controls are selected.
src,n=re.subn(r"(?m)^\.param AMP_L=\S+ AMP_R=\S+ FREQ=\S+ SOURCE=\S+ LOAD=\S+",
              ".param AMP_L={:.15g} AMP_R=0 FREQ=1000 SOURCE=40 LOAD=600".format(
                 0.775*10**(22/20)*2**0.5),src)
assert n==1
src,n=re.subn(r"(?m)^\.param CTL=.*$",
 ".param CTL=1 CRL=1 CML=1 CGL=1 LTL=9 LRL=4 CEL=0 LEL=1 FASTL=0 CATKL=0",src)
assert n==1
src=src.rsplit(".end",1)[0]
probes=["v(xleft.xboard.limit_env)","v(xleft.xboard.ql12e)",
        "v(xleft.xboard.attack_tap)","v(xleft.xboard.link)",
        "v(xleft.xboard.vctrl)","v(xleft.xboard.law_top)",
        "v(xleft.xboard.law_knee)","v(xleft.xboard.padh)",
        "v(xleft.xboard.padl)","v(xleft.xboard.amp_out)",
        "v(olh)","v(oll)",
        "v(xleft.xboard.scv)","v(xleft.xboard.ldet_v)",
        "v(xleft.xboard.ql7e)","v(xleft.xboard.audiov)",
        "v(xleft.xboard.buffv)","v(xleft.xboard.ql11c)",
        "v(xleft.xboard.ql7b)","v(xleft.xboard.zref)"]
# Restrict simulation output to the last 200 ms, after 1 s conditioning.
data=(out/("limiter-wave-"+name+".dat")).resolve()
net=(circuit/("diag-limiter-"+name+".cir"))
net.write_text(src+"\n.control\nset wr_singlescale\ntran 10u 1.2 1.0 10u\n"
        +"wrdata "+str(data)+" "+" ".join(probes)+"\nquit\n.endc\n.end\n")
p=subprocess.run(["ngspice","-b",net.name],cwd=circuit,capture_output=True,text=True,timeout=3600)
(out/("limiter-ngspice-"+name+".log")).write_text(p.stdout+"\nSTDERR:\n"+p.stderr)
if p.returncode or not data.exists():
    raise RuntimeError("Ngspice transient failed: "+p.stdout[-3000:]+p.stderr[-3000:])
array=np.loadtxt(data)
# ngspice wr_singlescale should produce one time column plus one per probe.
if array.ndim!=2 or array.shape[1]!=len(probes)+1:
    raise RuntimeError(f"Unexpected wrdata shape {array.shape}; expected {len(probes)+1} columns")
t=array[:,0]; readings={p:array[:,i+1] for i,p in enumerate(probes)}
dt=float(np.median(np.diff(t)))
def stats(signal):
    signal=np.asarray(signal)
    centered=signal-np.mean(signal)
    times=t-t[0]
    def amplitude(hz):
        return 2*abs(np.mean(centered*np.exp(-2j*np.pi*hz*times)))
    return dict(mean=float(np.mean(signal)),rms_ac=float(np.sqrt(np.mean(centered**2))),
                peak_to_peak=float(np.ptp(signal)),ripple_2khz_peak=float(amplitude(2000)))
outv=readings["v(olh)"]-readings["v(oll)"]
def harmonic(signal,hz):
    return 2*abs(np.mean(signal*np.exp(-2j*np.pi*hz*t)))
fundamental=harmonic(outv,1000)
harmonics={f"K{k}":float(harmonic(outv,1000*k)/fundamental) for k in range(2,21)}
thd=100*math.sqrt(sum(v*v for v in harmonics.values()))
node_stats={p:stats(v) for p,v in readings.items()}
v=readings
def cur(a,b,r):
    return stats((v[a]-v[b])*1000/r)
currents={
 "R27_Q7_emitter_mA":cur("v(xleft.xboard.scv)","v(xleft.xboard.ql7e)",47),
 "R28_Q12_to_C14_mA":cur("v(xleft.xboard.ql12e)","v(xleft.xboard.attack_tap)",1600),
 "R30_attack_mA":cur("v(xleft.xboard.attack_tap)","v(xleft.xboard.limit_env)",1600),
 "R36_bridge_driver_mA":cur("v(xleft.xboard.link)","v(xleft.xboard.law_top)",1000),
 "RV2_bridge_driver_mA":cur("v(xleft.xboard.law_top)","v(xleft.xboard.vctrl)",200.001),
 "R45_bridge_mA":cur("v(xleft.xboard.vctrl)","v(xleft.xboard.law_knee)",220),
 "R29_law_mA":cur("v(xleft.xboard.law_knee)", "v(xleft.xboard.law_knee)", 3300),
 "R10_detector_supply_mA":cur("v(xleft.xboard.scv)","v(xleft.xboard.ldet_v)",240),
 "R44_audio_supply_mA":cur("v(xleft.xboard.audiov)","v(xleft.xboard.buffv)",820)
}
# R44 cannot be computed without VCC in transient; report only DC separately.
del currents["R44_audio_supply_mA"]
del currents["R29_law_mA"]
result=dict(model=name,stimulus="1kHz +22dBu compressor OUT limiter IN threshold +8dBu recovery 800ms",
            sample_rate_simulation_hz=1/dt,measurement_window_s=float(t[-1]-t[0]),
            thd_1khz_to_20khz_percent=thd,
            note="Transient band ends at 20kHz; do not compare this figure to Neve's 80kHz THD spec.",
            harmonics_ratio=harmonics,node_stats=node_stats,currents_mA=currents)
(out/("limiter-current-wave-"+name+".json")).write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2))
