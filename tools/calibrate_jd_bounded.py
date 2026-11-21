"""Generate bounded transfer tables from measured JD SPICE anchors."""

import argparse
import json
from pathlib import Path
import re
import subprocess

import numpy as np

from measure_jd_grid import COMP_LEVELS

ROOT = Path(__file__).resolve().parents[1]


def read_rows(path):
    with path.open() as source:
        return [json.loads(line) for line in source]


def interpolate(x, y, anchors):
    """Use the last measured slope for anchors above the supported input."""
    unique, index = np.unique(np.asarray(x).round(2), return_index=True)
    y = np.asarray(y)[index]
    wanted = np.asarray(anchors)
    result = np.interp(wanted, unique, y)
    high = wanted > unique[-1]
    slope = np.clip((y[-1] - y[-2]) / (unique[-1] - unique[-2]), 0, 1)
    result[high] = y[-1] + (wanted[high] - unique[-1]) * slope
    return np.maximum(0, result)


def floats(values):
    return ','.join(f'{value:.8e}f' for value in values)


def gain_tables(rows, path):
    base_gain = -0.0288  # active 1 kHz terminal-to-terminal AC gain
    comp = {}
    limiter = {}
    for row in rows:
        observations = row['observations']
        inputs = [point['input'] for point in observations]
        reduction = [point['input'] + base_gain - point['output'] for point in observations]
        threshold = row['threshold']
        if row['mode'] == 'comp':
            if threshold != 1 or tuple(row['offsets']) != COMP_LEVELS:
                raise ValueError('Expected the four compressor endpoints from the reference detent')
            comp[row['ratio']] = np.asarray(reduction) - reduction[0]
        else:
            mark = 4 + (threshold - 1) * 0.5
            limiter[row['attack'], threshold] = interpolate(np.subtract(inputs, mark), reduction, row['offsets'])
    text = path.read_text()
    comp_rows = ['    {' + floats(comp[ratio]) + '},' for ratio in range(1, 6)]
    text = re.sub(r'(inline constexpr float compressor\[5\]\[compLevelCount\]=\{).*?(\n\};)',
                  lambda match: match[1] + '\n' + '\n'.join(comp_rows) + match[2], text, count=1, flags=re.S)
    block = re.search(r'inline constexpr Point limiter\[2\]\[23\]\[12\]=\{(.*?)\n\};', text, re.S)
    points = iter(value for a in range(2) for t in range(1, 24) for value in limiter[a, t])
    table, count = re.subn(r'\{([-+.0-9e]+)f,', lambda _: '{' + floats([next(points)]) + ',', block[1])
    if count != 2 * 23 * 12:
        raise RuntimeError(f'Expected 552 limiter points, found {count}')
    text = text[:block.start(1)] + table + text[block.end(1):]
    text = text.replace('// Calibrated at 44.1 kHz using the full EX11475/EX10340C nodal solver.',
                        '// Gain tables follow the JD ngspice fixture with stock PL20233.')
    path.write_text(text)


def detector_phase(frequency):
    z = np.exp(-2j * np.pi * frequency / 44100)
    radius = np.exp(-2 * np.pi * 1.76037857 / 44100)
    dc = (radius * (1 - z) / (1 - radius * z)) ** 4
    first = (.87855588 - .20367940*z - .04843757*z*z) / (1 - .12000398*z + .04963765*z*z)
    second = (1 - .22176785*z + .05458820*z*z) / (1 - .48687517*z + .04769670*z*z)
    return np.angle(dc * first * second)


def spectrum_tables(rows, probe, path):
    requests = []
    reference = []
    for row in rows:
        for point in row['observations']:
            requests.append(f"{row['mode']} {row['attack']} {row['ratio']} {row['threshold']} "
                            f"{row['frequency']} {point['input']:.8f}")
            reference.append(point)
    run = subprocess.run([str(probe)], input='\n'.join(requests) + '\n',
                         capture_output=True, text=True, check=True)
    values = np.fromstring(run.stdout, sep=' ').reshape(-1, 20, 2)
    if len(values) != len(reference):
        raise RuntimeError('Bounded probe returned an incomplete harmonic table')
    spectra = values[:, :, 0] + 1j * values[:, :, 1]
    compiled = {}
    i = 0
    for row in rows:
        freq = row['frequency']
        indices = np.arange(2, 21)
        count = min(20, int(.49 * 44100 / freq)) - 1
        rotation = np.exp(-1j * indices * detector_phase(freq))
        anchors = []
        for point in row['observations']:
            previous = spectra[i]
            scale = 10 ** ((point['output'] - 24) / 20)
            data = np.asarray(point['spectrum'])
            target = scale * (data[:, 0] + 1j * data[:, 1])
            fundamental = abs(previous[0])
            # Normalize to the bounded wet fundamental, which scales the
            # coherent correction at runtime.
            delta = (target[1:] - previous[1:]) / max(fundamental, 1e-9) * rotation
            delta[count:] = 0
            if not np.all(np.isfinite(delta)):
                raise ValueError('Non-finite harmonic calibration')
            anchors.append(delta)
            i += 1
        compiled[row['mode'], row['attack'], row['ratio'], row['threshold'], freq] = anchors
    frequencies = (100, 1000, 5000, 10000)
    def bank(mode, keys):
        lines = []
        for attack, ratio, threshold in keys:
            bands = [compiled[mode, attack, ratio, threshold, freq] for freq in frequencies]
            values = []
            for anchors in zip(*bands):
                point = np.concatenate([anchor[: min(20, int(.49 * 44100 / freq)) - 1]
                                        for anchor, freq in zip(anchors, frequencies)])
                values.append('{' + ','.join(f'{v:.6e}f' for z in point for v in (z.real, z.imag)) + '}')
            lines.append('{' + ','.join(values) + '}')
        return ',\n'.join(lines)
    comp = [(0, r, t) for r in range(1, 6) for t in (1, 6, 11, 16)]
    limit = [(a, 4, t) for a in range(2) for t in (1, 9, 23)]
    through = [(0, 4, 1)]
    text = '#pragma once\n// Generated from the JD SPICE harmonic grid and bounded residual probe.\n'
    text += 'namespace mkii::native::measured {\n'
    for mode, keys, length in [('comp', comp, 8), ('limit', limit, 8), ('through', through, 5)]:
        text += f'inline constexpr float jd_{mode}[{len(keys)}][{length}][84]={{\n'
        text += bank(mode, keys) + '\n};\n'
    path.write_text(text + '}\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('gain', type=Path)
    parser.add_argument('--tables', type=Path, default=ROOT / 'native/RealtimeTables.hpp')
    parser.add_argument('--spectrum', type=Path)
    parser.add_argument('--probe', type=Path)
    parser.add_argument('--harmonics', type=Path, default=ROOT / 'native/JdSpectrumTables.hpp')
    args = parser.parse_args()
    if args.spectrum and not args.probe:
        parser.error('--spectrum requires --probe')
    gain_tables(read_rows(args.gain), args.tables)
    if args.spectrum:
        spectrum_tables(read_rows(args.spectrum), args.probe, args.harmonics)


if __name__ == '__main__':
    main()
