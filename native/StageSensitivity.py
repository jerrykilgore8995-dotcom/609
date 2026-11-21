from pathlib import Path
import math,re,sys
source=Path(sys.argv[1]).read_text()
destination=Path(sys.argv[2])
variant=sys.argv[3]
def zero_cap(s, tag):
    before=s
    lines=[]
    for line in s.splitlines():
        if line.strip().endswith('// '+tag):
            line=re.sub(r'^(\s*c\.capacitors\.push_back\(\{[^,]+,[^,]+,)\s*[-+0-9.eE]+(\}\);.*)$',r'\g<1> 0\2',line)
        lines.append(line)
    result='\n'.join(lines)+'\n'
    if result==before:raise RuntimeError('Missing cap '+tag)
    return result
if variant=='no-zobel':
    source=zero_cap(source,'xleft.xboard.c19')
elif variant=='no-T2-winding-cap':
    for cap in ('xt2.cp','xt2.cs'):source=zero_cap(source,'xleft.xboard.'+cap)
elif variant=='ideal-T2-coupling':
    value=math.sqrt(67*16.4)*.99999999
    source=source.replace('33.140037419795696',format(value,'.17g'))
elif variant=='no-T1-winding-cap':
    for cap in ('xt1.cp','xt1.cs'):source=zero_cap(source,'xleft.xboard.'+cap)
elif variant=='ideal-T1-coupling':
    value=math.sqrt(8*31.4)*.99999999
    source=source.replace('15.847804264312817',format(value,'.17g'))
elif variant!='baseline':raise RuntimeError(variant)
destination.parent.mkdir(parents=True,exist_ok=True)
destination.write_text(source)
print(variant, 'generated',len(source),'bytes')
