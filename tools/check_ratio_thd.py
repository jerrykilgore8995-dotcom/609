"""Verify compressor THD at 1 kHz and +10 dBu, with a 0 dBu threshold."""

import argparse
import json
import math
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('nodal_log', type=Path)
    parser.add_argument('spice_directory', type=Path)
    parser.add_argument('--vst3-log', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    comparison = re.findall(r'^Ratio THD ([1-5]) ([0-9.]+) ([0-9.]+)$',
                            args.nodal_log.read_text(), re.MULTILINE)
    settings = {int(index): (float(full), float(bounded)) for index, full, bounded in comparison}
    rows = ['Ratio\tNodal THD %\tSPICE THD %\tBounded THD %\tNodal error %\tSPICE error %']
    accepted = len(comparison) == 5
    packaged = {}
    if args.vst3_log:
        vst3 = re.findall(r'^Packaged ratio THD ([1-5]) ([0-9.]+)$',
                          args.vst3_log.read_text(encoding='utf-8-sig'), re.MULTILINE)
        packaged = {int(index): float(thd) for index, thd in vst3}
        accepted &= len(vst3) == 5
        rows[0] += '\tPackaged THD %\tPackaged nodal error %\tPackaged SPICE error %'
    for index, label in enumerate(['1.5:1', '2:1', '3:1', '4:1', '6:1'], start=1):
        if index not in settings:
            raise ValueError(f'Missing THD measurement for ratio {label}')
        full, bounded = settings[index]
        data = json.loads((args.spice_directory / f'ratio-spice-{index}.json').read_text())
        spice = data[0]['THD_pct']
        if any(not math.isfinite(n) or n <= 0 for n in (full, spice, bounded)):
            raise ValueError(f'Invalid THD measurement for ratio {label}')
        nodal_error = 100*(bounded/full-1)
        spice_error = 100*(bounded/spice-1)
        accepted &= max(abs(nodal_error), abs(spice_error)) <= 5
        row = f'{label}\t{full:.6f}\t{spice:.6f}\t{bounded:.6f}\t{nodal_error:+.3f}\t{spice_error:+.3f}'
        if args.vst3_log:
            thd = packaged.get(index, math.nan)
            vst_nodal_error = 100*(thd/full-1)
            vst_spice_error = 100*(thd/spice-1)
            accepted &= math.isfinite(thd) and max(abs(vst_nodal_error), abs(vst_spice_error)) <= 5
            row += f'\t{thd:.6f}\t{vst_nodal_error:+.3f}\t{vst_spice_error:+.3f}'
        rows.append(row)
    result = '\n'.join(rows) + '\n'
    args.output.write_text(result)
    print(result, end='')
    return 0 if accepted else 1


if __name__ == '__main__':
    raise SystemExit(main())
