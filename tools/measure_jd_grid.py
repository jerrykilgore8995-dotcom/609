"""Measure native calibration anchors from the live 609 SPICE circuit."""

import argparse
from concurrent.futures import ProcessPoolExecutor, as_completed
import json
from pathlib import Path
import re
import shutil
import tempfile

import numpy as np
import measure

COMP_LEVELS = (-16, -10, -6, -3, 0, 3, 6, 9, 12, 18, 24, 30, 36, 44)
LIMIT_LEVELS = (-12, -6, -3, 0, 1, 2, 4, 6, 10, 16, 23, 32)
SOURCE_DB = 0.577
RMS_REF = 0.775
MAX_INPUT = 25.0


def run_tones(ngspice, settings, frequency, levels, *, dwell, timestep):
    period = 1 / frequency
    warm = max(0.45, dwell)
    boundaries = [0, *(warm + i * dwell for i in range(len(levels) - 1))]
    stop = boundaries[-1] + (warm if len(levels) == 1 else dwell)
    peaks = [RMS_REF * np.sqrt(2) * 10 ** ((v + SOURCE_DB) / 20) for v in levels]
    terms = [f'{p - prior:.12g}*u(time-{start:.8f})'
             for start, p, prior in zip(boundaries[1:], peaks[1:], peaks)]
    amplitude = '+'.join([f'{peaks[0]:.12g}', *terms]).replace('+-', '-')
    replacement = f'BINPUT SL 0 V=sin(2*pi*{frequency}*time)*({amplitude})'
    scenario = measure.CIRCUIT
    with tempfile.TemporaryDirectory() as root:
        deck = Path(root)
        for path in scenario.glob('*.inc'):
            shutil.copyfile(path, deck / path.name)
        top = re.sub(r'(?m)^VINL .*$', replacement, (scenario / '609.cir').read_text())
        lines = (line for line in top.splitlines() if not line.startswith(('XRIGHT', 'RRELAY_LINK')))
        (deck / '609.cir').write_text('\n'.join(lines) + '\n')
        measure.CIRCUIT = deck
        try:
            trace = measure.simulate({'FREQ': frequency, **settings}, f"""
save v(ilh) v(ill) v(olh) v(oll)
set numdgt=11
tran {timestep}u {stop:.8f} 0 {timestep}u
wrdata trace.dat v(ilh,ill) v(olh,oll)
""", ngspice)
        finally:
            measure.CIRCUIT = scenario
    if trace[-1, 0] < stop - 0.00001:
        raise RuntimeError(f'Incomplete SPICE trace at {frequency} Hz, {settings}')
    duration = round(min(.060, dwell / 2) / period) * period
    n = round(duration / period) * 256
    step = np.arange(n) * duration / n
    bins = np.arange(1, 21) * round(duration / period)
    windows = [*boundaries[1:], stop]
    result = []
    for end in windows:
        # Leave one sample before the step and keep harmonic phase referenced
        # to the source oscillator for comparisons across frequency.
        start = end - duration
        t = start + step
        input_, output = [np.interp(t, trace[:, 0], trace[:, index]) for index in (1, 2)]
        phase = np.exp(-2j * np.pi * frequency * start * np.arange(1, 21))
        incoming = np.fft.rfft(input_)[bins] * phase * (2 / n)
        outgoing = np.fft.rfft(output)[bins] * phase * (2 / n)
        dbu = lambda z: float(20 * np.log10(abs(z) / (RMS_REF * np.sqrt(2))))
        transfer = outgoing[0] / incoming[0]
        result.append({'input': dbu(incoming[0]), 'output': dbu(outgoing[0]),
                       'transfer': [transfer.real, transfer.imag],
                       'spectrum': [[z.real, z.imag] for z in outgoing / abs(outgoing[0])]})
    return result


def measure_row(task):
    ngspice, mode, attack, detent, threshold, freq, offsets, dwell, step = task
    compressor = mode == 'comp'
    settings = ({'CEL': 1, 'CRL': detent, 'CTL': threshold} if compressor else
                {'LEL': 1, 'FASTL': attack, 'LTL': threshold} if mode == 'limit' else {})
    mark = (-20 + 2 * (threshold - 1) if compressor else
            4 + 0.5 * (threshold - 1) if mode == 'limit' else 0)
    drives = [min(mark + offset, MAX_INPUT) for offset in offsets]
    levels = sorted(set(drives))
    observations = run_tones(ngspice, settings, freq, levels, dwell=dwell, timestep=step)
    by_level = dict(zip(levels, observations))
    return {'mode': mode, 'attack': attack, 'ratio': detent, 'threshold': threshold,
            'frequency': freq, 'offsets': offsets, 'observations': [by_level[v] for v in drives]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--ngspice', default='ngspice')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--section', choices=('gain', 'spectrum'), default='gain')
    args = parser.parse_args()
    if args.section == 'gain':
        levels = COMP_LEVELS
        tasks = [('comp', 0, r, t, 1000, levels, .16, 22) for r in range(1, 6) for t in range(1, 17)]
        tasks += [('limit', a, 4, t, 1000, LIMIT_LEVELS, .16, 22) for a in range(2) for t in range(1, 24)]
    else:
        frequencies = (100, 1000, 5000, 10000)
        levels = (-9, -3, 0, 3, 6, 12, 20, 30)
        tasks = [('comp', 0, r, t, f, levels, .10, min(10, 35000/f))
                 for r in range(1, 6) for t in (1, 6, 11, 16) for f in frequencies]
        tasks += [('limit', a, 4, t, f, levels, .10, min(10, 35000/f))
                  for a in range(2) for t in (1, 9, 23) for f in frequencies]
        tasks += [('through', 0, 4, 1, f, (-18, 0, 9, 18, 22), .10, min(10, 35000/f))
                  for f in frequencies]
    with ProcessPoolExecutor(args.jobs) as pool, args.output.open('w') as output:
        futures = [pool.submit(measure_row, (args.ngspice, *task)) for task in tasks]
        for future in as_completed(futures):
            output.write(json.dumps(future.result(), separators=(',', ':')) + '\n')
            output.flush()


if __name__ == '__main__':
    main()
