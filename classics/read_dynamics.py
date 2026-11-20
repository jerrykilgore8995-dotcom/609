"""Measure calibrated ratio and transients on the generated 44.1 kHz solver."""
import argparse
import math
from pathlib import Path
import subprocess

import numpy as np

RATE = 44100


def measurement(executable, limiter, ratio, recovery, threshold, fast, low, high, burst, tail):
    values = (int(limiter), ratio, recovery, threshold, int(fast), low, high, burst, tail, 1000)
    result = subprocess.run([str(executable), "--trace", *map(str, values)], text=True,
                            capture_output=True, check=True, timeout=180)
    y = np.fromstring(result.stdout, sep=" ")
    n = round(RATE / 1000)
    phase = 2 * math.pi * 1000 * np.arange(n) / RATE
    sine = np.correlate(y, np.sin(phase), "valid")
    cosine = np.correlate(y, np.cos(phase), "valid")
    amplitude = 2 / n * np.hypot(sine, cosine)
    envelope = 24 + 20 * np.log10(np.maximum(amplitude, 1e-10))
    time = (np.arange(len(envelope)) + n / 2) / RATE
    stop = .3 + burst
    before = float(np.median(envelope[(time >= .25) & (time < .299)]))
    after = float(np.median(envelope[(time >= stop - .05) & (time < stop - .001)]))
    release_floor = after - (high - low)
    release_band = release_floor + .63212056 * (before - release_floor)
    peak = float(np.min(envelope[(time >= .301) & (time < stop - .001)]))

    def first(mask, origin):
        hits = time[mask]
        return 1000 * (hits[0] - origin) if len(hits) else math.nan

    attack = first((time >= .3005) & (time < stop) & (envelope <= after + 1), .3)
    release = first((time >= stop + .0005) & (envelope >= release_band), stop)
    gain_change = high - low + before - after
    if gain_change <= 1 or int(result.stderr.strip()):
        attack = release = math.nan
    return before, after, attack, release, peak, int(result.stderr.strip())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model", choices=("2254", "2264"))
    parser.add_argument("executable", type=Path)
    parser.add_argument("--require-close", action="store_true")
    args = parser.parse_args()
    old = args.model == "2254"
    exe = args.executable.resolve()
    fixed_limit = (100, 200, 800) if old else (50, 100, 200, 800)
    fixed_comp = (400, 800, 1500) if old else (100, 400, 800, 1500)
    checks = []

    def check(label, actual, expected, failures, *, minimum=False):
        error = (expected - actual) / expected if minimum else abs(actual - expected) / expected
        checks.append((label, math.isfinite(actual) and error <= .1 and failures == 0))

    for detent, nominal in enumerate((1.5, 2, 3, 4, 6), 1):
        lo, hi, att, _, peak, failures = measurement(exe,False,detent,1,11,False,10,20,.9,.1)
        ratio = 10/(hi-lo) if hi > lo else math.nan
        check(f"comp {nominal:g}:1", ratio, nominal, failures)
        if detent == 5:
            check("comp attack at 6:1", att, 5 if old else 3, failures)
        print(f"COMP {nominal:g}:1 | {ratio:.2f}:1 | attack {att:.2f} ms | "
              f"output {lo:+.2f}/{hi:+.2f} dBu | burst minimum {peak:+.2f} dBu | {failures} fails", flush=True)
    for fast in ((False, True) if old else (False,)):
        lo, hi, att, _, peak, failures = measurement(exe,True,4,1,9,fast,10,20,.65,.1)
        limit_ratio = 10/max(abs(hi-lo), 1e-8)
        check("limit ratio", limit_ratio, 100, failures, minimum=True)
        if not old or not fast:
            check("limit attack", att, 1 if old else 4, failures)
        print(f"LIMIT {'FAST' if fast else 'NORMAL'} | {limit_ratio:.0f}:1 | attack {att:.2f} ms | "
              f"output {lo:+.2f}/{hi:+.2f} dBu | burst minimum {peak:+.2f} dBu | {failures} fails", flush=True)
    for lim, positions in ((False, fixed_comp), (True, fixed_limit)):
        for detent, expected in enumerate(positions, 1):
            _, _, _, release, _, failures = measurement(exe,lim,4,detent,9 if lim else 11,False,
                                                         0 if lim else -12,10,.55,2.8)
            check(f"{'limit' if lim else 'comp'} recovery {expected} ms", release, expected, failures)
            print(f"{'LIMIT' if lim else 'COMP'} REC {expected} ms | {release:.2f} ms | {failures} fails", flush=True)
        for auto in range(1 if old else 2):
            for burst in (.1, 3.0):
                _, _, _, release, _, failures = measurement(exe,lim,4,len(positions)+1+auto,9 if lim else 11,
                                                             False,0 if lim else -12,10,burst,7.0)
                if not old:
                    expected = ((100, 2000), (50, 5000))[auto][burst > .1]
                    check(f"{'limit' if lim else 'comp'} auto {auto+1} {burst:g}s", release, expected, failures)
                print(f"{'LIMIT' if lim else 'COMP'} AUTO {auto+1} {burst:g}s | "
                      f"{release:.2f} ms | {failures} fails", flush=True)

    missed = [label for label, close in checks if not close]
    if missed:
        print("Outside 10% or invalid: " + ", ".join(missed))
    return int(args.require_close and bool(missed))


if __name__ == "__main__":
    raise SystemExit(main())
