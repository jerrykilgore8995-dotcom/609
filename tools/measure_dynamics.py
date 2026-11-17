import sys,json,re,tempfile,subprocess
from pathlib import Path
import numpy as np
sys.path.insert(0,'tools')
from measure import CIRCUIT
mode,pos,fast=sys.argv[1],int(sys.argv[2]),int(sys.argv[3])
settings={'CEL':int(mode=='comp'),'LEL':int(mode=='limit'),'CRL':pos if mode=='comp' else 4,'CML':1,'LRL':1,'FASTL':fast}
def run(settings, stepped=False):
    s=(CIRCUIT/'609.cir').read_text().rsplit('.end',1)[0]
    for k,v in settings.items():
        s,n=re.subn(r'(?<=\\s)'+k+r'=[^\\s{}]+(?=\\s)',f'{k}={v}',s,count=1)
        assert n==1,k
    s=re.sub(r'(?m)^\\.include (\\S+)',lambda m:f'.include "{CIRCUIT/m[1]}"',s)
    if stepped:
        s=re.sub(r'(?m)^VINL .*$', 'BINPUT SL 0 V=sin(2*pi*1000*time)*(0.1095873+ (3.465-0.1095873)*(u(time-0.3)-u(time-1.3)))',s)
    with tempfile.TemporaryDirectory() as d:
        p=Path(d)
        (p/'deck.cir').write_text(s+'\n.options method=gear reltol=0.002\n.control\nset wr_singlescale\n'+('tran 10u 8.3 0 10u' if stepped else 'tran 10u .7 .59 10u')+'\nwrdata trace.dat v(olh,oll)\nquit\n.endc\n.end\n')
        r=subprocess.run(['ngspice','-b','deck.cir'],cwd=d,capture_output=True,text=True,timeout=600)
        if r.returncode or 'aborted' in r.stdout+r.stderr: raise RuntimeError(r.stdout[-2000:]+r.stderr[-2000:])
        a=np.loadtxt(p/'trace.dat')
        if a[-1,0] < (8.299 if stepped else .699): raise RuntimeError('incomplete transient')
        return a
def tone(level):
    a=run({**settings,'AMP_L':.775*np.sqrt(2)*10**(level/20)})
    t=.6+np.arange(10000)/100000
    y=np.interp(t,a[:,0],a[:,1])
    h=np.abs(np.fft.rfft(y)*2/len(y))[100:2100:100]
    return {'input_dBu':level,'output_dBu':float(20*np.log10(h[0]/(.775*np.sqrt(2)))),'THD_pct':float(100*np.linalg.norm(h[1:])/h[0]),'K2_K20_dBc':(20*np.log10(np.maximum(h[1:]/h[0],1e-15))).tolist()}
def timing(recovery):
    a=run({**settings,('CML' if mode=='comp' else 'LRL'):recovery},True)
    t=np.arange(830000)/100000
    y=np.interp(t,a[:,0],a[:,1]).reshape(-1,100)
    phase=2*np.pi*np.arange(100)/100
    amp=2*np.hypot(y@np.sin(phase),y@np.cos(phase))/100
    inp=np.where((np.arange(len(amp))>=300)&(np.arange(len(amp))<1300),3.465,.1095873)
    g=20*np.log10(np.maximum(amp,1e-15)/inp)
    lo=float(np.median(g[200:290])); hi=float(np.median(g[1200:1290])); gr=lo-hi
    def cross(arr,threshold,less):
        ix=np.flatnonzero(arr<=threshold if less else arr>=threshold)
        return float(ix[0]+.5) if len(ix) else None
    return {'recovery_position':recovery,'GR_dB':gr,'attack_63_ms':cross(g[300:1300],lo-.632*gr,True) if gr>1 else None,'attack_90_ms':cross(g[300:1300],lo-.9*gr,True) if gr>1 else None,'release_63_ms':cross(g[1300:],hi+.632*gr,False) if gr>1 else None,'release_90_ms':cross(g[1300:],hi+.9*gr,False) if gr>1 else None}
out={'mode':mode,'ratio_position':pos,'fast':fast,'tones':[],'timing':[],'errors':[]}
for level in (0,5,10,15,20):
    try: out['tones'].append(tone(level))
    except Exception as e: out['errors'].append(str(e))
    print(json.dumps(out),flush=True)
if mode=='limit' or pos==4:
    for recovery in range(1,7):
        try: out['timing'].append(timing(recovery))
        except Exception as e: out['errors'].append(str(e))
        print(json.dumps(out),flush=True)
if len(out['tones'])==5:
    delta=out['tones'][-1]['output_dBu']-out['tones'][-3]['output_dBu']
    out['ratio_10_to_20']=10/delta if abs(delta)>1e-9 else None
Path('result.json').write_text(json.dumps(out,indent=2))
print(json.dumps(out,indent=2))
