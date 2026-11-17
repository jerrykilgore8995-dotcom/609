"""Measure the complete circuit at a defined source and load impedance."""

import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile

import numpy as np


CIRCUIT = Path(__file__).resolve().parents[1] / 'circuit'


def simulate(settings, command, ngspice):
    source = (CIRCUIT / '609.cir').read_text().rsplit('.end', 1)[0]
    for parameter, value in settings.items():
        source, changes = re.subn(r'(?m)(?<=\s)' + parameter + r'=[^\s{}]+(?=[\s])',
                                  lambda _: f'{parameter}={value}', source, count=1)
        if changes != 1:
            raise ValueError(f'Unknown fixture parameter {parameter}')
    source = re.sub(r'(?m)^\.include (\S+)', lambda m: f'.include "{CIRCUIT / m[1]}"', source)
    with tempfile.TemporaryDirectory() as temporary:
        work = Path(temporary)
        (work / 'deck.cir').write_text(source + '\n.options method=gear reltol=0.002\n.control\nset wr_singlescale\n' + command + '\nquit\n.endc\n.end\n')
        run = subprocess.run([ngspice, '-b', 'deck.cir'], cwd=work, capture_output=True, text=True, timeout=180)
        if run.returncode or 'aborted' in run.stdout + run.stderr or not (work / 'trace.dat').is_file():
            raise RuntimeError(run.stdout[-4000:] + run.stderr[-4000:])
        return np.loadtxt(work / 'trace.dat', ndmin=2)


def tone(dbu, settings, ngspice):
    rms = .775 * 10 ** (dbu / 20)
    trace = simulate({'AMP_L': rms * np.sqrt(2), **settings}, '''
tran 10u .70 .59 10u
wrdata trace.dat v(olh,oll) v(xleft.xboard.amp_out) v(xleft.xboard.vctrl) v(lctl) v(meterl)
''', ngspice)
    t = .600 + np.arange(10000) / 100000
    y = np.interp(t, trace[:, 0], trace[:, 1])
    harmonics = np.abs(np.fft.rfft(y) * (2 / len(y)))[100:2100:100]
    return {'input_dBu': dbu, 'output_dBu': float(20 * np.log10(harmonics[0] / (.775 * np.sqrt(2)))),
            'THD_pct': float(100 * np.linalg.norm(harmonics[1:]) / harmonics[0]),
            'amplifier_bias_V': float(np.mean(trace[:, 2])), 'diode_control_V': float(np.mean(trace[:, 3])),
            'link_V': float(np.mean(trace[:, 4])), 'meter_V': float(np.mean(trace[:, 5]))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ngspice', default='ngspice')
    parser.add_argument('--set', action='append', default=[], metavar='KEY=VALUE')
    parser.add_argument('--level', action='append', type=float)
    parser.add_argument('--ac', action='store_true')
    args = parser.parse_args()
    settings = dict(item.split('=', 1) for item in args.set)
    if args.ac:
        trace = simulate(settings, '''
ac dec 36 20 30k
wrdata trace.dat db(v(olh,oll))
''', args.ngspice)
        magnitudes = np.interp(np.log([20, 30, 50, 100, 1000, 5000, 10000, 20000]), np.log(trace[:, 0]), trace[:, 1])
        print(json.dumps(dict(zip([20, 30, 50, 100, 1000, 5000, 10000, 20000], magnitudes)), indent=2))
    else:
        print(json.dumps([tone(level, settings, args.ngspice) for level in args.level or [0]], indent=2))


if __name__ == '__main__':
    main()
