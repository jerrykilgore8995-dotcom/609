"""Export the complete operating point, including winding currents."""

import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from measure import CIRCUIT

output = Path(sys.argv[1]).resolve()
circuit = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else CIRCUIT
output.parent.mkdir(parents=True, exist_ok=True)
source = (circuit / '609.cir').read_text().rsplit('.end', 1)[0]
source = re.sub(r'(?m)^\.include (\S+)', lambda m: f'.include "{circuit / m[1]}"', source)
if output.exists():
    output.unlink()
with tempfile.TemporaryDirectory() as temporary:
    work = Path(temporary)
    (work / 'seed.cir').write_text(source + f'''
.control
set filetype=ascii
op
write {output.as_posix()} all
quit
.endc
.end
''')
    subprocess.run([os.environ.get('NGSPICE', 'ngspice'), '-b', 'seed.cir'], cwd=work, check=True)
if not output.exists():
    raise RuntimeError('ngspice did not write an operating point')
