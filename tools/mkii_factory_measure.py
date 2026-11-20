"""609 MKII repeatable SPICE verification: factory limiter, response and harmonics."""
import json, math
import numpy as np
from measure_dynamics import simulate, peak

def measure(settings, level, hz, stop=0.75):
    settings={**settings,"AMP_L":peak(level),"FREQ":hz}
    trace=simulate(settings,stop,"v(olh,oll)")
    # Last 100 ms, sampled at 100 kHz. Analyze only harmonics up to 20 kHz.
    n=10000
    t=np.arange(n)/100000 + stop-.1
    y=np.interp(t,trace[:,0],trace[:,1])
    harmonics=[]
    for k in range(1,min(20,int(20000/hz))+1):
        coefficient=2*np.dot(y,np.exp(-2j*np.pi*k*hz*t))/n
        harmonics.append(abs(coefficient))
    fundamental=harmonics[0]
    dbcs=[round(20*math.log10(max(a/fundamental,1e-15)),3) for a in harmonics[1:]]
    return {"frequency_Hz":hz,"generator_dBu":level,
            "output_dBu":round(20*math.log10(fundamental/peak(0)),4),
            "THD_percent":round(100*math.sqrt(sum((a/fundamental)**2 for a in harmonics[1:])),5),
            "K2_up_to_20kHz_dBc":dbcs}

def main():
    # COMP out, limiter in, +8 dBu limiter detent and 800 ms recovery.
    factory={"CEL":0,"LEL":1,"LTL":9,"LRL":4,"FASTL":0}
    results={"description":"Original component-level SPICE fixture; NOT a measured packaged DLL",
             "factory_limiter":measure(factory,22.581,1000),
             "through":[],"compressor":[],"limiter":[]}
    for hz in (100,1000,5000,10000):
        results["through"].append(measure({"CEL":0,"LEL":0},4,hz))
        results["compressor"].append(measure({"CEL":1,"LEL":0,"CRL":4,"CTL":11,"CML":1,"CATKL":0},10,hz))
        results["limiter"].append(measure(factory,20,hz))
    with open("mkii-factory-measurements.json","w") as f: json.dump(results,f,indent=2)
    print(json.dumps(results,indent=2),flush=True)

if __name__=="__main__":
    main()
