"""Measure the 609 reconstruction against the JD bench level and timing steps."""

import argparse
import json
import re
import subprocess
import tempfile
from pathlib import Path

import numpy as np

from measure import CIRCUIT


def simulate(settings, stop, signals, step=None, *, burst_ms=800):
    deck = (CIRCUIT / '609.cir').read_text().rsplit('.end', 1)[0]
    for key, value in settings.items():
        deck, count = re.subn(r'(?<=\s)' + key + r'=[^\s{}]+(?=\s)', f'{key}={value}', deck, count=1)
        if count != 1:
            raise ValueError(f'Unknown fixture parameter {key}')
    deck = re.sub(r'(?m)^\.include (\S+)', lambda m: f'.include "{CIRCUIT / m[1]}"', deck)
    if step:
        low, high = step
        off = (450 + burst_ms) / 1000
        source = f'BINPUT SL 0 V=sin(2*pi*1000*time)*({low}+({high-low})*(u(time-0.45)-u(time-{off})))'
        deck = re.sub(r'(?m)^VINL .*$', source, deck)

    nodes = {node for group in re.findall(r'v\(([^)]+)\)', signals) for node in group.split(',')}
    saved = ' '.join(f'v({node})' for node in sorted(nodes))
    with tempfile.TemporaryDirectory() as temporary:
        work = Path(temporary)
        fixture = deck + f'''
.options method=gear reltol=0.002
.save {saved}
.control
set wr_singlescale
set numdgt=12
tran 10u {stop} {0 if step else stop-0.11} 10u
wrdata trace.dat {signals}
quit
.endc
.end
'''
        (work / 'deck.cir').write_text(fixture)
        run = subprocess.run(['ngspice', '-b', 'deck.cir'], cwd=work,
                             capture_output=True, text=True, timeout=600)
        if run.returncode or 'aborted' in run.stdout + run.stderr or not (work / 'trace.dat').is_file():
            raise RuntimeError(f'ngspice exit {run.returncode}: {run.stderr[-2000:]}\n{run.stdout[-500:]}')
        trace = np.loadtxt(work / 'trace.dat')
        if trace[-1, 0] < stop-0.001:
            raise RuntimeError('Incomplete transient')
        return trace


def peak(level):
    return .775*np.sqrt(2)*10**(level/20)


def tone(settings, level):
    trace = simulate({**settings, 'AMP_L': peak(level)}, .7, 'v(olh,oll)')
    t = .6 + np.arange(10000)/100000
    y = np.interp(t, trace[:, 0], trace[:, 1])
    harmonics = np.abs(np.fft.rfft(y)*2/len(y))[100:2100:100]
    ratio = harmonics[1:]/harmonics[0]
    return {'input_dBu': level, 'output_dBu': float(20*np.log10(harmonics[0]/peak(0))),
            'THD_pct': float(100*np.linalg.norm(ratio)),
            'K2_K20_dBc': (20*np.log10(np.maximum(ratio, 1e-15))).tolist()}


def crossing(values, threshold):
    hits = np.flatnonzero(values >= threshold)
    return float(hits[0]+.5) if len(hits) else None


def times(series):
    idle = float(np.median(series[380:430]))
    driven = float(np.median(series[1170:1230]))
    if abs(driven-idle) < 1e-6:
        return dict(attack_63_ms=None, release_63_ms=None, release_90_ms=None)
    attack = (series[450:1250]-idle)/(driven-idle)
    release = (series[1250:]-driven)/(idle-driven)
    return dict(attack_63_ms=crossing(attack, .632),
                release_63_ms=crossing(release, .632),
                release_90_ms=crossing(release, .9))


def timing(mode, settings, recovery):
    compress = mode == 'comp'
    low, high = map(peak, (-30, 10) if compress else (10, 20))
    stop = 1.25 + (1, 1.8, 2.5, 4.5, 5.5, 8)[recovery-1]
    control = 'comp_env' if compress else 'limit_env'
    step_settings = {**settings, ('CML' if compress else 'LRL'): recovery}
    trace = simulate(step_settings, stop, f'v(olh,oll) v(xleft.xboard.{control})', (low, high))
    bins = np.arange(int(stop*1000)-1)
    t = np.arange(len(bins)*100)/100000
    y = np.interp(t, trace[:, 0], trace[:, 1]).reshape(-1, 100)
    envelope = np.interp(t, trace[:, 0], trace[:, 2]).reshape(-1, 100).mean(1)
    phase = 2*np.pi*np.arange(100)/100
    amplitude = 2*np.abs(y @ np.exp(-1j*phase))/100
    output = 20*np.log10(np.maximum(amplitude, 1e-15)/peak(0))
    gain = 20*np.log10(np.maximum(amplitude, 1e-15) /
                       np.where((bins >= 450) & (bins < 1250), high, low))
    before = float(np.median(output[380:430]))
    return dict(recovery_position=recovery, gain=times(gain), control=times(envelope),
                limiter_attack_within_1dB_ms=None if compress else
                crossing(before+1-output[450:1250], 0))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('comp', 'limit'))
    parser.add_argument('ratio', type=int, choices=range(1, 6))
    parser.add_argument('fast', type=int, choices=(0, 1))
    args = parser.parse_args()
    compress = args.mode == 'comp'
    settings = {'CEL': int(compress), 'LEL': int(not compress), 'CRL': args.ratio if compress else 4,
                'CML': 1, 'LRL': 1, 'FASTL': args.fast}
    results = dict(mode=args.mode, ratio_position=args.ratio, fast=args.fast,
                   tones=[], timing=[])
    for level in (0, 5, 10, 15, 20):
        result = tone(settings, level)
        results['tones'].append(result)
        print(json.dumps(result), flush=True)
    if not compress or args.ratio == 4:
        for recovery in range(1, 7):
            result = timing(args.mode, settings, recovery)
            results['timing'].append(result)
            print(json.dumps(result), flush=True)
    lo, hi = (0, 2) if compress else (2, 4)
    delta = results['tones'][hi]['output_dBu']-results['tones'][lo]['output_dBu']
    results['ratio_input_levels_dBu'] = [results['tones'][i]['input_dBu'] for i in (lo, hi)]
    results['ratio_output_delta_dB'] = delta
    results['ratio'] = 10/delta if delta > 0 else None
    Path('result.json').write_text(json.dumps(results, indent=2)+'\n')
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
