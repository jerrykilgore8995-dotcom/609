"""Measure the 550A SPICE reference and the generated discrete circuit renderer."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import itertools
import json
from pathlib import Path
import subprocess
import tempfile

import numpy as np

HERE = Path(__file__).resolve().parent
NAMES = ("LG", "MG", "HG", "LF", "MF", "HF", "LS", "HS", "BP", "EQ")
DEFAULT = (0, 0, 0, 1, 1, 1, 0, 0, 0, 1)
CENTERS = ((50, 100, 200, 300, 400), (400, 800, 1500, 3000, 5000),
           (5000, 7000, 10000, 12500, 15000))
DECI = (-12, -9, -6, -4, -2, 0, 2, 4, 6, 9, 12)
BASE_RATE = 44100
PERIOD = 16384
FETCH = np.unique(np.floor(np.r_[np.geomspace(30, 20000, 112), np.ravel(CENTERS), 1000, 50, 15000]
                           * PERIOD / BASE_RATE + .5).astype(int))
RANGE = FETCH * BASE_RATE / PERIOD


def settings(**overrides):
    return tuple(overrides.get(k, default) for k, default in zip(NAMES, DEFAULT, strict=True))


def cases(quick):
    yield "flat", DEFAULT
    yield "filter", settings(BP=1)
    yield "out", settings(LG=5, MG=-5, HG=5, EQ=0)
    yield "mixed", settings(LG=-2, MG=3, HG=2, LF=3, MF=4, HF=2, HS=1)
    for band, (gain, frequency) in enumerate(zip(NAMES[:3], NAMES[3:6], strict=True)):
        for g, f in itertools.product((-5, -1, 1, 5) if quick else tuple(x for x in range(-5, 6) if x),
                                      (1, 5) if quick else (1, 2, 3, 4, 5)):
            yield f"{gain}/{g}/{f}", settings(**{gain: g, frequency: f})
            if band in (0, 2):
                shelf = "LS" if band == 0 else "HS"
                yield f"{gain}/{g}/{f}/shelf", settings(**{gain: g, frequency: f, shelf: 1})


def spice(exe, controls, instructions, *, level=0, frequency=1000):
    source = (HERE / "spice/550a.cir").read_text().rsplit(".end", 1)[0].splitlines()
    source[2] = ".param " + " ".join(f"{key}={value}" for key, value in zip(NAMES, controls, strict=True))
    source[2] += f" LOAD=600 AMP={.775 * np.sqrt(2) * 10**(level/20)} FREQ={frequency}"
    source = [line for line in source if not line.startswith(("XRIGHT ", "RREFR ", "RLOADR ", "RSR ", "VINR "))]
    with tempfile.TemporaryDirectory() as path:
        path = Path(path)
        data = path / "response.txt"
        circuit = path / "fixture.cir"
        circuit.write_text("\n".join(source) + f"""
.control
set wr_singlescale
set numdgt=12
{instructions}
let output = v(olh) - v(oll)
wrdata {data.as_posix()} output
quit
.endc
.end
""")
        subprocess.run([exe, "-b", str(circuit)], cwd=HERE / "spice",
                       check=True, capture_output=True, timeout=180)
        return np.loadtxt(data)


def spice_curve(exe, controls):
    rows = spice(exe, controls, "ac dec 100 20 24000")
    return np.interp(RANGE, rows[:, 0], rows[:, 1] + 1j * rows[:, 2])


def render(exe, controls, rate, count, channels, frequency, amplitude):
    process = subprocess.run([exe, *map(str, (rate, count, channels, frequency, amplitude, *controls))],
                             check=True, capture_output=True, text=True, timeout=180)
    failures, latency, duration = map(float, process.stderr.strip().split())
    y = np.fromstring(process.stdout, sep=" ")
    if failures or y.size != count or not np.isfinite(y).all():
        raise ValueError("renderer returned incomplete or unstable audio")
    return y, int(latency), duration


def nodal_curve(exe, controls):
    amplitude = .0003
    y, latency, _ = render(exe, controls, BASE_RATE, 2 * PERIOD, 1, 0, amplitude)
    phase = np.pi * np.arange(FETCH.size) * (np.arange(FETCH.size) - 1) / FETCH.size
    reference = -1j * np.exp(1j * phase) * PERIOD * amplitude / 2
    correction = np.exp(2j * np.pi * FETCH * latency / PERIOD)
    return np.fft.rfft(y[-PERIOD:])[FETCH] * correction / reference


def harmonics(y, time, f):
    count = min(8, 80000 // f)
    bins = np.array([np.mean(y * np.exp(-2j * np.pi * k * f * time)) for k in range(1, count+1)])
    return float(100 * np.linalg.norm(bins[1:]) / abs(bins[0]))


def measure_thd(exe, rate, frequency, level, *, nodal):
    end = .4 if frequency <= 100 else .1
    window = .1 if frequency <= 100 else .05
    if nodal:
        count = round(rate * end)
        y, _, _ = render(exe, DEFAULT, rate, count, 1, frequency, 10**((level-24)/20))
        time = np.arange(count) / rate
        start = count - round(rate * window)
        return harmonics(y[start:], time[start:], frequency)
    rows = spice(exe, DEFAULT, f"tran 4u {end} 0 {min(2e-6,1/(frequency*140))}",
                 level=level, frequency=frequency)
    if rows[-1, 0] < end - 1e-6:
        raise ValueError("SPICE did not reach the distortion window")
    time = np.arange(round((end-window)*rate), round(end*rate)) / rate
    return harmonics(np.interp(time, rows[:, 0], rows[:, 1]), time, frequency)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stage", choices=("spice", "nodal"))
    parser.add_argument("executable")
    parser.add_argument("output", type=Path)
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--quick", action="store_true")
    parser.add_argument("--curves-only", action="store_true")
    parser.add_argument("--workers", type=int, default=2)
    options = parser.parse_args()
    nodal = options.stage == "nodal"
    source = json.loads(options.reference.read_text()) if options.reference else {}
    suite = list(cases(options.quick))
    curve = nodal_curve if nodal else spice_curve
    failed = []
    data = {"frequencies": RANGE.tolist(), "response": {}, "thd": {}}
    with ThreadPoolExecutor(options.workers) as pool:
        work = {name: pool.submit(curve, options.executable, ctrl) for name, ctrl in suite}
        for name, future in work.items():
            try:
                y = future.result()
                mag = abs(y)
                phase = np.angle(y, deg=True)
                row = {"gain": mag.tolist(), "phase": phase.tolist()}
                if nodal:
                    target = source["response"][name]
                    gain_error = float(np.max(abs(mag / np.array(target["gain"]) - 1)))
                    phase_error = float(np.max(abs((phase - target["phase"] + 180) % 360 - 180)))
                    row |= {"max_relative_gain_error": gain_error, "max_phase_error_deg": phase_error}
                    if gain_error > .10 or phase_error > 10:
                        failed.append(name)
                    print(f"{name:14} gain {gain_error:7.3%}, phase {phase_error:5.2f} deg", flush=True)
                data["response"][name] = row
            except (subprocess.SubprocessError, ValueError) as error:
                failed.append(name)
                print(f"{name}: {error}", flush=True)
        thd_cases = list(itertools.product((30, 1000, 10000) if options.quick else (30, 100, 1000, 5000, 10000, 20000), (4, 18)))
        if options.curves_only:
            thd_cases = []
        work = {f"{f}/{l}": pool.submit(measure_thd, options.executable, 176400, f, l, nodal=nodal)
                for f, l in thd_cases}
        for name, future in work.items():
            try:
                thd = future.result()
                ceiling = .05 if name.endswith("/4") else .07
                data["thd"][name] = thd
                passed = np.isfinite(thd) and thd <= ceiling
                if nodal:
                    reference = source["thd"][name]
                    passed &= abs(thd - reference) <= .10 * reference
                if not passed:
                    failed.append(f"THD {name}")
                print(f"{name:14} THD {thd:.5f}% {'PASS' if passed else 'FAIL'}", flush=True)
            except (subprocess.SubprocessError, ValueError) as error:
                failed.append(f"THD {name}")
                print(f"{name}: {error}", flush=True)
    if "flat" in data["response"]:
        mag = np.array(data["response"]["flat"]["gain"])
        flat = 20 * np.log10(mag / mag[np.argmin(abs(RANGE - 1000))])
        worst = float(np.max(abs(flat)))
        data["flat_max_deviation_dB"] = worst
        print(f"Flat response {worst:.3f} dB worst, {flat[-1]:+.3f} dB at 20 kHz", flush=True)
        if worst > .5:
            failed.append("Hardware flatness specification")
    data["failed"] = failed
    options.output.write_text(json.dumps(data, indent=2))
    return bool(failed)


if __name__ == "__main__":
    raise SystemExit(main())
