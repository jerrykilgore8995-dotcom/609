#!/usr/bin/env python3
"""Calculate Q12 bias, C14 2kHz ripple/current, and bridge balance from native nodal probe."""
import csv,json,math,sys
from pathlib import Path
import numpy as np
file=Path(sys.argv[1]); out=Path(sys.argv[2]); label=sys.argv[3]
with file.open() as f:
    rows=list(csv.DictReader(f))
if len(rows)<1000:raise RuntimeError("Incomplete nodal recording")
keys=rows[0].keys()
a={k:np.array([float(row[k]) for row in rows]) for k in keys}
t=a['time_s']; fs=1/np.median(np.diff(t))
def summary(x):
    x=np.asarray(x);ac=x-np.mean(x)
    def component(hz):
        return float(2*abs(np.mean(ac*np.exp(-2j*np.pi*hz*(t-t[0])))))
    return {"mean":float(np.mean(x)),
            "rms_ac":float(np.sqrt(np.mean(ac**2))),
            "peak_to_peak":float(np.ptp(x)),
            "ripple_2khz_peak":component(2000)}
def diff(a,b,r):
    return summary(1000*(a-b)/r) # mA
def get(name):return a[name]
r={
 "R10_supply_mA":diff(get('scv'),get('ldet_v'),240),
 "R27_Q7_emitter_mA":diff(get('scv'),get('ql7e'),47),
 "R28_Q12_emitter_mA":diff(get('ql12e'),get('attack_tap'),1600),
 "R30_C14_charge_mA":diff(get('attack_tap'),get('c14'),1600),
 "R36_bridge_control_mA":diff(get('link'),get('law_top'),1000),
 "RV2_bridge_control_mA":diff(get('law_top'),get('vctrl'),200.001),
 "R45_bridge_shunt_mA":diff(get('vctrl'),get('law_knee'),220),
 "C14_capacitive_current_mA":summary(1000*10e-6*np.gradient(get('c14'),t))
}
r["C14_net_current_vs_R30_mA"]={
    "mean_difference":float(np.mean(1000*10e-6*np.gradient(get('c14'),t)
        -1000*(get('attack_tap')-get('c14'))/1600))
}
# Q12 emitter is connected to R28, so emitter current is directly measurable;
# base current may vary considerably with gain and current.
q12_be=get('ql12b')-get('ql12e')
q12_ce=get('scv')-get('ql12e')
bridge_center=(get('padh')+get('padl'))*.5
result={
 "variant":label,"analysis":"native 88.2kHz nodal voltages at 1kHz +22dBu, limiter +8dBu, compressor OFF",
 "rate_of_logged_samples":fs,
 "observed_window_seconds":float(t[-1]-t[0]),
 "q12_vbe":summary(q12_be),
 "q12_vce":summary(q12_ce),
 "q12_fraction_vbe_above_0p55V":float(np.mean(q12_be>.55)),
 "c14_voltage":summary(get('c14')),
 "vctrl_voltage":summary(get('vctrl')),
 "bridge_pad_differential":summary(get('padh')-get('padl')),
 "bridge_pad_center_minus_vctrl_half":summary(bridge_center-get('vctrl')*.5),
 "bridge_pad_center":summary(bridge_center),
 "branch_currents_mA":r,
 "warnings":["DC and transient are distinct: Q12 may be OFF with no drive.",
  "C14 ripple is a direct time-domain measurement; harmonic content is not a factory THD pass measurement.",
  "Bridge diode models are matched, but voltage symmetry alone does not prove identical instantaneous diode currents.",
  "Changing Q12 BF may also change gain reduction; compare output_dBu from the paired VST3 THD test."]
}
out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2),flush=True)
