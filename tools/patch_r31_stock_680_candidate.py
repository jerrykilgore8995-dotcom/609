#!/usr/bin/env python3
"""Patch only JD stock attack resistor R31 at index zero, preserving 7.5/15ms extended detents."""
import pathlib,re,sys
p=pathlib.Path(sys.argv[1]);source=p.read_text()
matches=list(re.finditer(r'^.*c\.contact\.push_back\(.*// x(left|right)\.xboard\.r31\s*$',source,re.M))
if len(matches)!=2:raise RuntimeError(f"Expected left/right R31, found {len(matches)}")
for m in reversed(matches):
 line=m.group(0)
 old='0.0026666666666666666'
 if line.count(old)!=2:raise RuntimeError(f'Unexpected R31 contact structure: {line}')
 val=format(1/680,'.17g')
 updated=line.replace(old,val)
 source=source[:m.start()]+updated+source[m.end():]
 print(m.group(1),updated,flush=True)
p.write_text(source)
