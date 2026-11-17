"""Measure the bass limiter-only benchmarks in the SPICE circuit."""

import argparse
import json

import numpy as np

from measure import simulate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ngspice', default='ngspice')
    args = parser.parse_args()
    peak = .775 * np.sqrt(2) * 10
    results = {}
    for f in (65, 110, 250):
        trace = simulate({'FREQ': f, 'AMP_L': peak, 'LEL': 1, 'LTL': 9, 'LRL': 1, 'FASTL': 0},
                         """tran 11u .605 .39 11u
wrdata trace.dat v(olh,oll)
""", args.ngspice)
        time = .4 + np.arange(8820) / 44100
        signal = np.interp(time, trace[:, 0], trace[:, 1])
        spectrum = np.abs(np.fft.rfft(signal))
        bin_ = f // 5
        results[f] = {k: float(20*np.log10(spectrum[bin_*k]/spectrum[bin_])) for k in range(2, 21, 2)}
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
