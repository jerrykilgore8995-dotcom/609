"""Measure the JD distortion conditions and export a second nodal reference deck."""

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import shutil

import numpy as np

from measure import CIRCUIT, fixture_source, simulate

CALIBRATED = {'BRIDGE_DRIVE': .645, 'BRIDGE_COMP': 1.115}
CASES = (
    ('through', 9, {}),
    ('compressor', 0, {'CEL': 1, 'CTL': 2, 'CRL': 5, 'CGL': 11, 'CML': 3}),
    ('limiter_4', 22, {'LEL': 1, 'LTL': 1, 'LRL': 4}),
    ('limiter_8', 22, {'LEL': 1, 'LTL': 9, 'LRL': 4}),
    ('limiter_15', 22, {'LEL': 1, 'LTL': 23, 'LRL': 4}),
    ('hot_compressor', 24, {'CEL': 1, 'CTL': 16, 'CRL': 3}),
)


def record(case, profile, ngspice):
    _, level, controls = case
    reference = .775 * np.sqrt(2)
    amplitude = reference * 10 ** (level / 20)
    trace = simulate({'AMP_L': amplitude, **controls, **profile}, '''
tran 5u .903 .698 5u
wrdata trace.dat v(olh,oll) v(xleft.xboard.preamp) v(xleft.xboard.linesend) v(xleft.xboard.padh,xleft.xboard.padl)
''', ngspice)
    time = .7 + np.arange(32000) / 160000
    results = {}
    for column, name in enumerate(('output', 'amp_input', 'line_send', 'bridge'), start=1):
        samples = np.interp(time, trace[:, 0], trace[:, column])
        harmonics = 2 * np.abs(np.fft.rfft(samples)[200:4200:200]) / len(samples)
        results[name] = dict(level_dBu=float(20 * np.log10(harmonics[0] / reference)),
                             THD_pct=float(100 * np.linalg.norm(harmonics[1:]) / harmonics[0]),
                             harmonic_dBc=(20 * np.log10(harmonics[1:] / harmonics[0])).tolist())
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ngspice', default='ngspice')
    parser.add_argument('--calibrated-deck', type=Path)
    args = parser.parse_args()
    if args.calibrated_deck:
        shutil.copytree(CIRCUIT, args.calibrated_deck, dirs_exist_ok=True)
        (args.calibrated_deck / '609.cir').write_text(fixture_source(CALIBRATED) + '\n.end\n')
    results = {}
    with ThreadPoolExecutor(max_workers=4) as workers:
        futures = {(key, case[0]): workers.submit(record, case, profile, args.ngspice)
                   for key, profile in (('character', {}), ('calibrated', CALIBRATED))
                   for case in CASES}
        for (key, name), future in futures.items():
            results.setdefault(key, {})[name] = future.result()
    bench = results['calibrated']
    if not (.36 <= bench['limiter_8']['output']['THD_pct'] <= .45
            and bench['compressor']['output']['THD_pct'] <= .20
            and bench['through']['output']['THD_pct'] <= .075
            and abs(bench['through']['output']['level_dBu'] - 9) <= .16
            and abs(bench['limiter_8']['output']['level_dBu'] - 8) <= .20):
        raise SystemExit(f'Calibrated JD reference is outside the bench limits: {bench}')
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
