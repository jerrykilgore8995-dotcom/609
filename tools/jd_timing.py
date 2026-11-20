"""Compare isolated 609 compressor and limiter controls with the same stepped sine."""

import argparse
from concurrent.futures import ProcessPoolExecutor
import itertools
import json
from pathlib import Path
import subprocess

import numpy as np

from measure_dynamics import peak, simulate


def crossing(values, threshold, hold=3):
    """Interpolate between cycle centers and require a sustained crossing."""
    if len(values) < hold:
        return None
    above = np.convolve(values >= threshold, np.ones(hold, dtype=int), 'valid')
    hits = np.flatnonzero(above == hold)
    if not len(hits):
        return None
    i = hits[0]
    if not i:
        return 0.5
    return round(float(i - .5 + (threshold - values[i-1]) / (values[i] - values[i-1])), 2)


def timing(case, engine=None, plugin=None):
    mode, attack, recovery, burst, high, threshold, ratio = case
    limiter = mode == 'limit'
    off = 450 + burst
    stop = off / 1000 + (1., 1.8, 3., 6., 8., 12.)[recovery - 1]
    low = 10 if limiter else -30
    if engine:
        command = [str(engine)]
        if plugin:
            command += [str(plugin), '--jd-timing']
        result = subprocess.run([*command, *map(str, (mode, attack, recovery, burst, stop, high, threshold, ratio))],
                                capture_output=True, text=True, check=True, timeout=90)
        trace = np.fromstring(result.stdout, sep=' ').reshape(-1, 2)
        trace[:, 1] *= peak(24)
    else:
        settings = {'LEL': int(limiter), 'CEL': int(not limiter), 'FASTL': attack if limiter else 0,
                    'CATKL': 0 if limiter else attack, 'LRL': recovery, 'CML': recovery,
                    'LTL': threshold if limiter else 9, 'CTL': 1 if limiter else threshold, 'CRL': ratio}
        env = 'limit_env' if limiter else 'comp_env'
        trace = simulate(settings, stop, f'v(olh,oll) v(xleft.xboard.{env})',
                         (peak(low), peak(high)), burst_ms=burst)
    bins = np.arange(int(stop * 1000) - 1)
    drive = (bins >= 450) & (bins < off)
    t = np.arange(len(bins) * 100) / 100000
    y = np.interp(t, trace[:, 0], trace[:, 1]).reshape(-1, 100)
    basis = np.exp(-2j * np.pi * np.arange(100) / 100)
    amplitude = 2 * np.abs(y @ basis) / 100
    output = 20 * np.log10(np.maximum(amplitude, 1e-15) / peak(0))
    gain = output - np.where(drive, high, low)
    rec = dict(mode=mode, attack=attack, recovery=recovery, burst_ms=burst,
               high_dBu=high, threshold=threshold, ratio=ratio)

    def describe(values):
        idle = float(np.median(values[380:430]))
        driven = float(np.median(values[off-20:off-5]))
        if abs(driven-idle) < 1e-6:
            return dict(idle=idle, driven=driven)
        rise = (values[450:off]-idle)/(driven-idle)
        fall = (values[off:]-driven)/(idle-driven)
        return dict(idle=idle, driven=driven, attack63=crossing(rise, .63212, hold=2),
                    release25=crossing(fall, .25), release63=crossing(fall, .63212),
                    release90=crossing(fall, .9), release95=crossing(fall, .95),
                    retained300=round(float(1 - np.median(fall[290:310])), 4))

    rec['gain'] = describe(gain)
    if limiter:
        rec['settle1dB'] = crossing(np.median(output[380:430]) + 1 - output[450:off], 0)
    if not engine:
        envelope = np.interp(t, trace[:, 0], trace[:, 2]).reshape(-1, 100).mean(1)
        rec['control'] = describe(envelope)
    return rec


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', type=Path, help='Use the bounded-engine probe instead of ngspice')
    parser.add_argument('--plugin', type=Path, help='VST3 bundle when the engine is Vst3Smoke')
    parser.add_argument('--mode', choices=['comp', 'limit'], required=True)
    parser.add_argument('--attack', type=int, choices=range(3), required=True)
    parser.add_argument('--recovery', type=int, nargs='+', choices=range(1, 7), default=list(range(1, 7)))
    parser.add_argument('--bus', action='store_true', help='Include shorter automatic-recovery bursts and bus levels')
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.mode == 'limit' and args.attack == 2:
        parser.error('Limiter has two attack positions')
    if args.plugin and not args.engine:
        parser.error('--plugin requires --engine pointing to Vst3Smoke')
    limiter = args.mode == 'limit'
    cases = [(args.mode, args.attack, recovery, 800, 20 if limiter else 10, 9 if limiter else 1, 4)
             for recovery in args.recovery]
    if args.bus:
        ratios = (4,) if limiter else (3, 4)
        for ratio, recovery, burst in itertools.product(ratios, (5, 6), (100, 500)):
            cases.append((args.mode, args.attack, recovery, burst, 20 if limiter else 4,
                          9 if limiter else 10, ratio))
    engine = args.engine.resolve() if args.engine else None
    plugin = args.plugin.resolve() if args.plugin else None
    with ProcessPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(timing, cases, itertools.repeat(engine), itertools.repeat(plugin)))
    args.output.write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
