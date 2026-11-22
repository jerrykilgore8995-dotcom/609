#!/usr/bin/env python3
"""Measurement-only candidate: EX11475/J drawing shows R28=R30=1k6.
Preserve existing series/short topology, controls, law, bias, and rest of deck.
"""
import pathlib,re,sys
p=pathlib.Path(sys.argv[1]);s=p.read_text()
for n in ('r28','r30'):
 for side in ('xleft','xright'):
  pat=re.compile(r'(c\.resistors\.push_back\(\{\d+,\s*-?\d+,\s*)([^,]+)(,\s*0\}\);\s*//\s*'+side+r'\.xboard\.'+n+r'\b)')
  s,count=pat.subn(lambda m:m.group(1)+format(1/1600,'.17g')+m.group(3),s)
  if count!=1:raise RuntimeError(f'expected one {side}.{n}, got {count}')
p.write_text(s)
print('Patched measured candidate only: R28=1600 ohm, R30=1600 ohm each channel')
