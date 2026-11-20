"""Measure and compare live SPICE and loaded VST3 for combined 609 settings."""

import argparse
from concurrent.futures import ProcessPoolExecutor, as_completed
import hashlib
from itertools import combinations
import json
from math import prod
from pathlib import Path
import subprocess
from time import perf_counter

import numpy as np

from jd_timing import crossing
from measure import CIRCUIT
from measure_dynamics import peak, simulate
from measure_jd_grid import SOURCE_DB, run_tones

# Include the switch positions behind bypassed sections. They still have
# physical loading and some combinations can leave a measurable residual.
AXES = (16, 5, 6, 11, 23, 6, 2, 2, 2, 3)
FIXED_TIMING_AXES = (16, 5, 1, 11, 23, 1, 2, 2, 1, 1)


def positions(index, axes):
    values = []
    for size in reversed(axes):
        index, value = divmod(index, size)
        values.append(value)
    return tuple(reversed(values))


def axes_for(fixed_timing, both_enabled):
    axes = FIXED_TIMING_AXES if fixed_timing else AXES
    return (*axes[:6], 1, 1, *axes[8:]) if both_enabled else axes


def controls(index, fixed_timing=False, both_enabled=False):
    values = positions(index, axes_for(fixed_timing, both_enabled))
    return (*values[:6], 1, 1, *values[8:]) if both_enabled else values


def settings(p):
    ct, cr, cm, cg, lt, lr, ce, le, fast, attack = p
    return dict(CTL=ct+1, CRL=cr+1, CML=cm+1, CGL=cg+1, LTL=lt+1, LRL=lr+1,
                CEL=ce, LEL=le, FASTL=fast, CATKL=attack)


def plugin_case(host, bundle, p, frequency, low, high, burst, stop):
    command = [str(host), str(bundle), '--jd-case',
               *map(str, (*p, frequency, low, high, burst, stop))]
    result = subprocess.run(command, text=True, capture_output=True, check=True, timeout=90)
    values = np.fromstring(result.stdout, sep=' ').reshape(-1, 2)
    if not len(values) or not np.all(np.isfinite(values)):
        raise ValueError('VST3 returned no usable measurement')
    return values


def compare_tone(host, bundle, ngspice, p, frequency, level):
    timestep = min(5, 30000/frequency)
    point = run_tones(ngspice, settings(p), frequency, [level], dwell=.9, timestep=timestep)[0]
    phase = point['spectrum']
    reference = np.array([complex(*k) for k in phase])
    measured = plugin_case(host, bundle, p, frequency, level, point['input'], 0, .9)
    if len(measured) != 20:
        raise ValueError(f'Expected twenty harmonic bins, got {len(measured)}')
    bounded = measured[:, 0] + 1j*measured[:, 1]
    hmax = int(min(20, 21609 // frequency))
    reference, bounded = reference[:hmax], bounded[:hmax]
    level_out = float(24 + 20*np.log10(abs(bounded[0])))
    bounded /= abs(bounded[0])
    phase_error = float(np.angle(bounded[0]/reference[0], deg=True))
    thd = lambda x: float(100*np.linalg.norm(x[1:]))
    return dict(frequency=frequency, input_dBu=point['input'],
                spice_out_dBu=point['output'], vst3_out_dBu=level_out,
                gain_error_dB=level_out-point['output'], phase_error_deg=phase_error,
                spice_transfer_dB=point['output']-point['input'], vst3_transfer_dB=level_out-point['input'],
                spice_phase_deg=float(np.angle(1j*reference[0], deg=True)),
                vst3_phase_deg=float(np.angle(1j*bounded[0], deg=True)),
                spice_thd_pct=thd(reference), vst3_thd_pct=thd(bounded),
                spice_harmonics=[[float(z.real), float(z.imag)] for z in reference],
                vst3_harmonics=[[float(z.real), float(z.imag)] for z in bounded])


def envelope(trace, high, low, burst, voltage_scale):
    t = np.arange(int((trace[-1, 0]-.001)*1000)*100)/100000
    y = np.interp(t, trace[:, 0], trace[:, 1]).reshape(-1, 100)
    basis = np.exp(-2j*np.pi*np.arange(100)/100)
    output = 20*np.log10(np.maximum(2*abs(y@basis)/(100*voltage_scale), 1e-15))
    on, off = 450, 450+burst
    drive = np.where((np.arange(len(output))>=on)&(np.arange(len(output))<off), high, low)
    gain = output-drive
    a = float(np.median(gain[380:430]))
    b = float(np.median(gain[off-20:off-5]))
    if abs(b-a)<0.25:
        return dict(idle_gain_dB=a, driven_gain_dB=b, attack63_ms=None, release63_ms=None)
    rise = (gain[on:off]-a)/(b-a)
    fall = (gain[off:]-b)/(a-b)
    return dict(idle_gain_dB=a, driven_gain_dB=b, attack63_ms=crossing(rise,.63212,hold=2),
                release25_ms=crossing(fall,.25), release63_ms=crossing(fall,.63212),
                release90_ms=crossing(fall,.9),
                retained300=float(1-np.median(fall[290:310])))


def compare_burst(host, bundle, ngspice, p, low, high, burst):
    stop = .45+burst/1000 + (6. if max(p[2],p[5])>=3 else 2.)
    spice = simulate(settings(p), stop, 'v(olh,oll)',
                     (peak(low+SOURCE_DB), peak(high+SOURCE_DB)), burst_ms=burst, ngspice=ngspice)
    plugin = plugin_case(host, bundle, p, 1000, low, high, burst/1000, stop)
    return dict(burst_ms=burst, before_dBu=low, after_dBu=high,
                spice=envelope(spice, high, low, burst, peak(0)),
                vst3=envelope(plugin, high, low, burst, 10**(-24/20)))


def run(task):
    key, p, host, bundle, ngspice, frequencies, levels, bursts, transitions = task
    start = perf_counter()
    tones = [compare_tone(host, bundle, ngspice, p, f, v) for f in frequencies for v in levels]
    pairs = combinations(levels,2) if transitions else ((levels[0], high) for high in levels[1:])
    timing = [compare_burst(host, bundle, ngspice, p, low, high, b)
              for low, high in pairs for b in bursts]
    return dict(index=key, controls=settings(p), tones=tones, timing=timing,
                elapsed_seconds=round(perf_counter()-start,3))


def digest(paths):
    sha = hashlib.sha256()
    for path in paths:
        sha.update(path.name.encode())
        with path.open('rb') as source:
            for chunk in iter(lambda: source.read(2**20),b''):
                sha.update(chunk)
    return sha.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('host', type=Path)
    parser.add_argument('bundle', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--start', type=int, default=0)
    parser.add_argument('--count', type=int, default=1)
    parser.add_argument('--quick', action='store_true', help='Exercise three explicit combined-control cases')
    parser.add_argument('--frequencies', nargs='+', type=float, default=[100,1000,5000,10000])
    parser.add_argument('--levels', nargs='+', type=float, default=list(range(-36,25)))
    parser.add_argument('--bursts', nargs='*', type=int, default=[100,500,800])
    parser.add_argument('--ngspice', default='ngspice')
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--both-enabled', action='store_true',
                        help='Sweep only with compressor and limiter switched in')
    parser.add_argument('--fixed-timing', action='store_true',
                        help='Use the JD attack and shortest fixed recovery for the steady control grid')
    parser.add_argument('--all-transitions', action='store_true',
                        help='Exercise every pair of sampled input levels in the burst tests')
    args = parser.parse_args()
    total = prod(axes_for(args.fixed_timing,args.both_enabled))
    if args.start < 0 or args.count < 1 or args.start+args.count > total:
        parser.error(f'Choose a shard within the {total} single-channel control combinations')
    if not all(20<=f<=20000 and f%5==0 for f in args.frequencies):
        parser.error('Use frequencies divisible by 5 Hz in the 20 Hz–20 kHz range')
    if min(args.levels)<-80 or max(args.levels)>24 or args.levels!=sorted(args.levels):
        parser.error('Input levels must be sorted from −80 through +24 dBu')
    if args.bursts and (len(args.levels)<2 or min(args.bursts)<50):
        parser.error('Timing runs require two input levels and bursts of at least 50 ms')
    # The run header protects resumptions from silently mixing builds or plans.
    bundle_files = sorted(p for p in args.bundle.rglob('*') if p.suffix in ('.dll','.so','.vst3') and p.is_file())
    if args.bundle.is_file():
        bundle_files = [args.bundle]
    if not bundle_files:
        parser.error('The VST3 module could not be found in the bundle')
    fingerprints = dict(deck=digest(sorted(CIRCUIT.glob('*.*'))), vst3=digest(bundle_files),
                        host=digest([args.host]))
    header = dict(frequencies=args.frequencies, levels=args.levels, bursts=args.bursts,
                  total_combinations=total, quick=args.quick, fixed_timing=args.fixed_timing, both_enabled=args.both_enabled,
                  all_transitions=args.all_transitions, fingerprints=fingerprints)
    done = set()
    if args.output.is_file():
        with args.output.open() as old:
            if json.loads(old.readline()) != header:
                parser.error('The existing results use different sampling settings')
            done = {json.loads(line)['index'] for line in old}
    else:
        args.output.write_text(json.dumps(header)+'\n')
    cases = (enumerate(((9,2,0,1,8,0,1,1,0,0),(10,2,4,3,16,4,1,1,1,1),
                        (8,2,5,5,14,5,1,1,0,2)), -3) if args.quick else
             ((i,controls(i,args.fixed_timing,args.both_enabled)) for i in range(args.start,args.start+args.count)))
    common = (args.host.resolve(), args.bundle.resolve(), args.ngspice,
              args.frequencies, args.levels, args.bursts, args.all_transitions)
    with ProcessPoolExecutor(args.jobs) as pool, args.output.open('a') as output:
        pending = {}
        for key, p in cases:
            if key in done:
                continue
            pending[pool.submit(run,(key,p,*common))] = key
            if len(pending) >= args.jobs*2:
                future=next(as_completed(pending))
                output.write(json.dumps(future.result())+'\n')
                output.flush()
                del pending[future]
        for future in as_completed(pending):
            output.write(json.dumps(future.result())+'\n')
            output.flush()


if __name__ == '__main__':
    main()
