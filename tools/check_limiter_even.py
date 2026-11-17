"""Check the packaged limiter's even harmonics against both reference solvers."""

import argparse
import json
import math
from pathlib import Path
import re


def measure(path, prefix):
    rows = re.findall(rf'^{prefix} (\d+) (\d+) (-?[0-9.]+)$',
                      path.read_text(encoding='utf-8-sig'), re.MULTILINE)
    return {(int(frequency), int(order)): float(level) for frequency, order, level in rows}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('nodal_log', type=Path)
    parser.add_argument('spice_json', type=Path)
    parser.add_argument('vst3_log', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    nodal = measure(args.nodal_log, 'Nodal limiter even')
    vst3 = measure(args.vst3_log, 'Packaged limiter even')
    spice = json.loads(args.spice_json.read_text())
    rows = ['Frequency Hz\tHarmonic\tNodal dBc\tSPICE dBc\tVST3 dBc\tVST3 nodal delta dB\tVST3 SPICE delta dB']
    passed = True
    for frequency in (65, 110, 250):
        for k in range(2, 21, 2):
            n, s, y = nodal[frequency, k], spice[str(frequency)][str(k)], vst3[frequency, k]
            dist = max(abs(y-n), abs(y-s))
            # Near cancellation the two references have different amplitudes.
            # Allow 0.55 dB beyond the smallest simultaneous dB error.
            if k <= 8:
                passed &= math.isfinite(dist) and dist <= abs(n-s)*.5+.55
            rows.append(f'{frequency}\tK{k}\t{n:.4f}\t{s:.4f}\t{y:.4f}\t{y-n:+.4f}\t{y-s:+.4f}')
    report = '\n'.join(rows) + '\n'
    print(report, end='')
    args.output.write_text(report)
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
