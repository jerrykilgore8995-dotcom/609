"""Measure the generated classic circuit with a 600-ohm output load."""
import argparse
import json
import math
from pathlib import Path
import subprocess

import numpy as np

from spice_response import BANDS

RATE = 44100


def tone(executable, mode, frequency, level, *, rate=RATE, threshold=11, recovery=1, makeup=1):
    burst = 2.5 if mode != 2 else .8
    compress, limit = int(mode == 0), int(mode == 1)
    args = (limit, 5, recovery, threshold, 0, level - 20, level, burst, .08, frequency,
            compress, limit, makeup, rate)
    result = subprocess.run([str(executable), "--trace", *map(str, args)],
                            text=True, capture_output=True, check=True, timeout=120)
    y = np.fromstring(result.stdout, sep=" ")
    end = round((.3 + burst) * rate)
    count = rate // 5
    if y.size < end:
        raise ValueError("Node solver returned a truncated waveform")
    x = y[end - count:end]
    t = (end - count + np.arange(count)) / rate
    harmonics = [
        2 / count * np.dot(x, np.exp(-2j * np.pi * frequency * k * t))
        for k in range(1, min(20, 20000 // frequency) + 1)
    ]
    # The phase of a unit sine is -90 degrees in the complex projection.
    phase = (math.degrees(np.angle(harmonics[0])) + 270) % 360 - 180
    output = 24 + 20 * math.log10(abs(harmonics[0]))
    return {
        "input_dBu": level,
        "output_dBu": output,
        "gain_dB": output - level,
        "phase_deg": phase,
        "thd_percent": (100 * math.sqrt(sum(abs(h / harmonics[0]) ** 2 for h in harmonics[1:]))
                        if len(harmonics) > 1 else None),
        "failures": int(result.stderr.strip()),
    }


def target_output(executable, mode, level, **controls):
    """Adjust the test generator to the specified steady-state output."""
    source = level if mode == 2 else 18
    point = tone(executable, mode, 1000, source, **controls)
    for _ in range(6):
        error = level - point["output_dBu"]
        if abs(error) < .05 or point["failures"]:
            break
        multiplier = 6 if mode == 0 else 1
        adjusted = min(26, max(-60, source + error * multiplier))
        if adjusted == source:
            break
        source = adjusted
        point = tone(executable, mode, 1000, source, **controls)
    return point


def measure(model, executable, rate):
    bands = [dict(frequency_hz=f, **tone(executable, 2, f, 0, rate=rate)) for f in BANDS]
    reference = bands[BANDS.index(1000)]["gain_dB"]
    for row in bands:
        row["relative_dB"] = row["gain_dB"] - reference
    limit = model == 2264
    # Working reference for the early 2254 uses +8 dBu output. See README.
    reference_output = 9 if limit else 8
    through = target_output(executable, 2, reference_output, rate=rate)
    compressor = target_output(executable, 0, reference_output, recovery=3 if limit else 2,
                               rate=rate, threshold=11 if limit else 15, makeup=5 if limit else 1)
    limiter = tone(executable, 1, 1000, 20, threshold=1, recovery=4 if limit else 3, rate=rate)
    mode_results = {"through": through, "compressor": compressor, "limiter": limiter}
    spec = (.05, .2, .4) if limit else (.08, .2, None)
    for name, row, maximum in zip(mode_results, mode_results.values(), spec, strict=True):
        row["reference_thd_percent"] = maximum
        if name != "limiter":
            row["reference_output_dBu"] = reference_output
            row["output_within_0p05_dB"] = abs(row["output_dBu"] - reference_output) < .05
        row["passes_thd_limit"] = (row["thd_percent"] <= maximum and row["failures"] == 0
                                   and row.get("output_within_0p05_dB", True)
                                   if maximum is not None else None)
        print(f"{model} {name} | {row['input_dBu']:+.2f} in / {row['output_dBu']:+.2f} out dBu"
              f" | THD {row['thd_percent']:.4f}% | {row['failures']} fails", flush=True)
    for row in bands:
        print(f"{model} through {row['frequency_hz']:5d} Hz | {row['relative_dB']:+.3f} dB"
              f" | phase {row['phase_deg']:+.2f} deg | {row['failures']} fails", flush=True)
    audit = {"model": model, "rate": rate, "load_ohms": 600, "response": bands, "modes": mode_results}
    if model == 2254:
        thresholds = []
        for position in range(1, 18):
            point = tone(executable, 1, 1000, 20, threshold=position, recovery=3, rate=rate)
            target = 4 + .5 * (position - 1)
            thresholds.append({"detent_dBu": target, "output_dBu": point["output_dBu"],
                               "failures": point["failures"]})
        audit["limiter_thresholds"] = thresholds
    return audit


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model", type=int, choices=(2254, 2264))
    parser.add_argument("executable", type=Path)
    parser.add_argument("--rate", type=int, choices=(44100, 88200, 176400), default=RATE)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--spice-reference", type=Path)
    args = parser.parse_args()
    document = measure(args.model, args.executable.resolve(), args.rate)
    passed = True
    if args.spice_reference:
        reference = json.loads(args.spice_reference.read_text())["response"]
        for point, target in zip(document["response"], reference, strict=True):
            if point["frequency_hz"] != target["frequency_hz"]:
                raise ValueError("SPICE and nodal frequency grids do not match")
            point["spice_relative_dB"] = target["relative_dB"]
            point["spice_phase_deg"] = target["phase_deg"]
            point["error_dB"] = point["relative_dB"] - target["relative_dB"]
            point["error_phase_deg"] = (point["phase_deg"] - target["phase_deg"] + 180) % 360 - 180
        amplitude_error = max(abs(p["error_dB"]) for p in document["response"])
        phase_error = max(abs(p["error_phase_deg"]) for p in document["response"])
        passed = amplitude_error <= .65 and phase_error <= 10
        passed &= all(p["failures"] == 0 for p in document["response"])
        document["spice_parity"] = {"max_error_dB": amplitude_error,
                                    "max_error_deg": phase_error, "passed": passed}
        print(f"SPICE parity | {amplitude_error:.3f} dB | {phase_error:.2f} deg | passed: {passed}")
    args.output.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
