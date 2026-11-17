"""Measure the 110 Hz limiter-only benchmark in the SPICE circuit."""

import argparse
import json

import numpy as np

from measure import simulate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ngspice', default='ngspice')
    args = parser.parse_args()
    peak = .775 * np.sqrt(2) * 10
    trace = simulate({'FREQ': 110, 'AMP_L': peak, 'LEL': 1, 'LTL': 9, 'LRL': 1, 'FASTL': 0},
                     '''tran 11u .605 .39 11u
wrdata trace.dat v(olh,oll)
''', args.ngspice)
    time = .4 + np.arange(8820) / 44100
    signal = np.interp(time, trace[:, 0], trace[:, 1])
    spectrum = np.abs(np.fft.rfft(signal))
    values = {str(k): float(20*np.log10(spectrum[22*k]/spectrum[22])) for k in range(2, 21, 2)}
    print(json.dumps(values, indent=2))


if __name__ == '__main__':
    main()
