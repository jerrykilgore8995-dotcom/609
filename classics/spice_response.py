"""Write the small-signal reference for the classic through circuit."""
import argparse
import json
import math
import os
import subprocess
import tempfile
from pathlib import Path

import numpy as np

from read_dynamics import RATE, analyze

NGSPICE = os.environ.get("NGSPICE", "ngspice")
BANDS = (20, 40, 80, 100, 200, 500, 1000, 2000, 5000, 10000, 15000, 18000, 20000)


def sweep(circuit):
    source = circuit.read_text().rsplit(".end", 1)[0]
    with tempfile.TemporaryDirectory() as directory:
        deck = Path(directory) / "response.cir"
        output = Path(directory) / "response.txt"
        points = "\n".join(
            f"ac lin 1 {frequency} {frequency}\n"
            "let output = v(olh)-v(oll)\n"
            f'wrdata {output.as_posix()} output\n'
            "destroy $curplot"
            for frequency in BANDS
        )
        deck.write_text(source + f"""
.control
set appendwrite
set wr_singlescale
set numdgt=12
{points}
quit
.endc
.end
""")
        subprocess.run([NGSPICE, "-b", str(deck)], cwd=circuit.parent, check=True, timeout=90)
        rows = [tuple(map(float, row.split())) for row in output.read_text().splitlines() if row.strip()]
    if len(rows) != len(BANDS):
        raise ValueError("ngspice did not return the requested sweep")
    reference = next(abs(complex(real, imag)) for f, real, imag in rows if f == 1000)
    return [
        {"frequency_hz": frequency, "relative_dB": 20 * math.log10(abs(complex(real, imag)) / reference),
         "phase_deg": math.degrees(math.atan2(imag, real))}
        for frequency, real, imag in rows
    ]



def normal_limiter_attack(circuit):
    frequency, low, high, burst = 2000, 10, 20, .65
    edge = .3
    stop = edge + burst
    duration = stop + .10
    full_scale = .775 * math.sqrt(2) * 10 ** (24/20)
    low_peak = full_scale * 10 ** ((low-24)/20)
    high_peak = full_scale * 10 ** ((high-24)/20)
    source = circuit.read_text().rsplit(".end", 1)[0]
    source = source.replace("FREQ=1000", f"FREQ={frequency}").replace("CEL=0 LEL=0", "CEL=0 LEL=1")
    source = source.replace(
        "VINL SL 0 SIN(0 {AMP_L} {FREQ}) AC 1",
        "VCARRIER carrier 0 SIN(0 1 {FREQ})\n"
        f"VGAIN level 0 PWL(0 {low_peak} {edge} {low_peak} {edge+1e-9} {high_peak} "
        f"{stop} {high_peak} {stop+1e-9} {low_peak})\n"
        "BINL SL 0 V=V(carrier)*V(level)",
    )
    with tempfile.TemporaryDirectory() as directory:
        deck = Path(directory) / "attack.cir"
        output = Path(directory) / "attack.txt"
        deck.write_text(source + f"""
.control
set wr_singlescale
tran 5u {duration} 0 3u
let output = v(olh)-v(oll)
wrdata {output.as_posix()} output
quit
.endc
.end
""")
        subprocess.run([NGSPICE, "-b", str(deck)], cwd=circuit.parent, check=True, timeout=150)
        t, v = np.loadtxt(output, unpack=True)
    if t[-1] < duration-1/RATE:
        raise ValueError("ngspice returned a truncated limiter transient")
    y = np.interp(np.arange(round(duration * RATE)) / RATE, t, v) / full_scale
    _, _, attack, _, _, _ = analyze(y, frequency, low, high, burst, 0)
    if not math.isfinite(attack):
        raise ValueError("could not measure the SPICE limiter attack")
    return attack


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("circuit", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    circuit = args.circuit.resolve()
    result = {"response": sweep(circuit)}
    if circuit.stem == "2254":
        result["normal_limiter_attack_ms"] = normal_limiter_attack(circuit)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
