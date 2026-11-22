#!/usr/bin/env python3
"""Independent manufacturer 1N4148 bridge model comparison and RV4 sweep.
NXP=official 1N4148 SOD27 (.MODEL + 5.827G manufacturer reverse shunt).
Diodes=official Diodes Incorporated 1N4148W SOD123 (.MODEL only).
OnSemi=onsemi-attributed model from LTspice standard.dio (NOT independently verified
as an official Onsemi released model).
No other circuit parts are changed. On physical parts, matched means same diode
family and common nominal model, NOT four measured individual devices.
"""
import pathlib,sys,re
model,rv4=sys.argv[1:]
rv4=float(rv4)
assert model in ('nxp','diodes','onsemi')
assert 500 <= rv4 <= 4000
root=pathlib.Path("circuit")
de=root/"devices.inc";ex=root/"ex11475.inc";main=root/"609.cir"
d=de.read_text();s=ex.read_text();c=main.read_text()
assert '.model D1N4148_NXP' in d
assert sum(z.strip().endswith('D1N4148_NXP') for z in s.splitlines() if z.startswith(('D14 ','D15 ','D16 ','D17 ')))==4
if model == "diodes":
  d+="\n* Diodes Incorporated manufacturer 1N4148W.spice.txt\n* https://www.diodes.com/spice/download/1589/1N4148W.spice.txt\n.model D1N4148_ALT D(IS=10.4n RS=51.5m BV=75 IBV=1u CJO=2p M=.333 N=2.07 TT=5.76n)\n"
elif model == "onsemi":
  d+="\n* LTspice standard.dio onsemi-attributed (not verified manufacturer release)\n.model D1N4148_ALT D(IS=2.52n RS=.568 N=1.752 CJO=4p M=.4 TT=20n)\n"
if model!="nxp":
  for num in range(14,18):
    old=f'D{num} '
    lines=s.splitlines()
    count=0
    for i,line in enumerate(lines):
      if line.startswith(old):
        assert line.endswith('D1N4148_NXP')
        lines[i]=line.replace('D1N4148_NXP','D1N4148_ALT')
        count+=1
    assert count==1
    s='\n'.join(lines)+'\n'
  s='\n'.join(line for line in s.splitlines() if not re.match(r'^R_D1[4-7]_NXP\b',line))+'\n'
for p,original in [(ex,s),(main,c)]:
  assert original.count('LIMCAL=0.166966') in (1,2), (p,original.count('LIMCAL=0.166966'))
  original=original.replace('LIMCAL=0.166966',f'LIMCAL={rv4/10000:.8f}')
  if p==ex:s=original
  else:c=original
de.write_text(d);ex.write_text(s);main.write_text(c)
print('Variant',model,'RV4 ohm',rv4,'diodes',[(x.split()[0],x.split()[-1]) for x in s.splitlines() if re.match(r'^D1[4-7] ',x)])
