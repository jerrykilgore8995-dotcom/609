"""Compare the Windows solver outputs with their SPICE operating points."""

import json
from pathlib import Path
import re
import sys


def main(directory):
    cases = {"through": (0, 0.05), "compress": (10, 0.15), "limit": (20, 0.15)}
    passed = True
    for name, (drive, tolerance) in cases.items():
        expected = next(row for row in json.loads((directory / "deck" / f"{name}.json").read_text())
                        if row["input_dBu"] == drive)
        actual = (directory / f"{name}-native.txt").read_text()
        match = re.search(r"([+-]?\d+\.\d+) dBu out", actual)
        if not match:
            raise ValueError(f"Missing native result for {name}")
        difference = float(match[1]) - expected["output_dBu"]
        print(f"{name}: native minus SPICE {difference:+.3f} dB at {drive:+} dBu input")
        passed = abs(difference) <= tolerance and passed
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main(Path(sys.argv[1])))
