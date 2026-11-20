"""Check assembly-level operation in ngspice before preparing a release."""
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
import math
import os
from pathlib import Path
import re
import subprocess
import tempfile

CIRCUITS = Path(__file__).resolve().parent / "circuit"


@dataclass(frozen=True)
class Case:
    circuit: str
    level: float
    compressor: bool = False
    limiter: bool = False
    ratio: int = 4


def measure(case):
    fixture = (CIRCUITS / f"{case.circuit}.cir").read_text().rsplit(".end", 1)[0]
    amplitude = 0.775 * math.sqrt(2) * 10 ** (case.level / 20)
    fixture = re.sub(r"AMP_L=[\d.]+", f"AMP_L={amplitude:.12g}", fixture, count=1)
    fixture = fixture.replace("SIN(0 {AMP_L} {FREQ})", "SIN(0 {AMP_L} {FREQ} .2)")
    fixture = fixture.replace("CTL=1 CRL=1", f"CTL=11 CRL={case.ratio}")
    fixture = fixture.replace("CEL=0 LEL=0", f"CEL={int(case.compressor)} LEL={int(case.limiter)}")
    fixture += """
EVOUT PROBE 0 OLH OLL 1
.tran 15u 1.2 .4
.meas tran early RMS v(probe) from=.4 to=.5
.meas tran output RMS v(probe) from=1.1 to=1.2
"""
    if case.circuit == "2264" and case.limiter:
        fixture += (
            "ETR8 TR8_REVERSE 0 xleft.xboard.ql8b xleft.xboard.ql8c 1\n"
            ".meas tran reverse_max MAX v(tr8_reverse) from=.4 to=1.2\n"
        )
    saved = "v(probe) v(tr8_reverse)" if case.circuit == "2264" and case.limiter else "v(probe)"
    fixture += f".save {saved}\n.end\n"
    with tempfile.TemporaryDirectory() as directory:
        deck = Path(directory) / "test.cir"
        deck.write_text(fixture)
        result = subprocess.run([os.environ.get("NGSPICE", "ngspice"), "-b", str(deck)],
                                cwd=CIRCUITS, capture_output=True, text=True, timeout=150)
    if result.returncode:
        raise ValueError(result.stderr.strip())
    metrics = {k: float(v) for k, v in
               re.findall(r"(?m)^(early|output|reverse_max)\s*=\s*([-+\d.eE]+)", result.stdout)}
    if not {"early", "output"}.issubset(metrics) or min(metrics["early"], metrics["output"]) <= 0:
        raise ValueError("ngspice did not return a valid through or dynamics measurement")
    return metrics


def dbu(voltage):
    return 20 * math.log10(voltage / 0.775)


def main():
    cases = [
        Case(model, level, compressor, limiter, ratio)
        for model in ("2254", "2264")
        for level, compressor, limiter, ratio in (
            (0, False, False, 4),
            (10, False, True, 4),
            (20, False, True, 4),
            (10, True, False, 5),
            (20, True, False, 5),
        )
    ]
    with ThreadPoolExecutor(max_workers=4) as pool:
        jobs = {case: pool.submit(measure, case) for case in cases}
        results = {}
        for case, job in jobs.items():
            mode = "comp 6:1" if case.compressor else "limit +8" if case.limiter else "through"
            label = f"{case.circuit} {mode:9s} {case.level:+3g} dBu"
            try:
                metrics = job.result()
                results[case] = metrics
                message = f"{dbu(metrics['early']):+.2f} dBu early / {dbu(metrics['output']):+.2f} dBu late"
                if "reverse_max" in metrics:
                    message += f", TR8 C–B reverse peak {metrics['reverse_max']:+.2f} V"
                print(f"{label}  {message}", flush=True)
            except (ValueError, subprocess.TimeoutExpired) as error:
                print(f"{label}  failed measurement: {error}", flush=True)
    ok = len(results) == len(cases)
    for model in ("2254", "2264"):
        def get(case, metric):
            value = results.get(case, {}).get(metric, math.nan)
            return value

        through = get(Case(model, 0), "output")
        limits = [get(Case(model, level, limiter=True), "output") for level in (10, 20)]
        comp = [get(Case(model, level, compressor=True, ratio=5), "output") for level in (10, 20)]
        through_ok = through > 0 and abs(dbu(through)) <= 1
        limit_ok = all(volts > 0 and abs(dbu(volts) - 8) <= 1 for volts in limits)
        # Catch runaway envelopes or reverse output slope. This is a sanity check,
        # independent of the tighter eventual hardware tolerance for the ratio.
        comp_ok = all(volts > 0 and dbu(volts) > -12 for volts in comp) and comp[0] < comp[1]
        if model == "2264":
            # BC214 datasheet absolute collector-base reverse rating.
            reverse = [get(Case(model, level, limiter=True), "reverse_max") for level in (10, 20)]
            junction_ok = all(volts < 45 for volts in reverse)
            print(f"2264 TR8 collector-base voltage within rating: {junction_ok}")
            ok = ok and junction_ok
        ok = ok and through_ok and limit_ok and comp_ok
        print(f"{model} operating checks: through {through_ok}, limiter {limit_ok}, compressor {comp_ok}")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
