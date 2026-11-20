"""Generate an ASCII operating point for a classic circuit fixture."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("circuit", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
circuit = args.circuit.resolve()
output = args.output.resolve()
output.parent.mkdir(parents=True, exist_ok=True)
output.unlink(missing_ok=True)
source = circuit.read_text().rsplit(".end", 1)[0]
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / "seed.cir"
    path.write_text(source + f'''
.control
set filetype=ascii
op
write {output.as_posix()} all
quit
.endc
.end
''')
    subprocess.run([os.environ.get("NGSPICE", "ngspice"), "-b", str(path)],
                   cwd=circuit.parent, check=True)
if not output.is_file():
    raise SystemExit("ngspice did not produce the requested operating point")
