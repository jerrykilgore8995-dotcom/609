# 33609JD MKIV — RV2 200 Ω comprehensive factory audit

**Scope:** Investigation-only matched comparison of the original RV2=1750 Ω (LAW=0.35) and candidate RV2=200 Ω (LAW=0.04), using the actual Windows nodal VST3. **No permanent change to the working MKIV.**

## Manufacturer conditions
Reference: [33609/JD technical handbook](https://thehistoryofrecording.com/Manuals/Neve/Neve%2033609jd.pdf).
- Compressor factory ratio: −20 dBu threshold, 100 ms recovery, 1 kHz, input 0→+10 dBu, no makeup.
- Limiter threshold +8 dBu factory alignment: +20 dBu, 1 kHz input, 50 ms recovery.
- Limiter hard limiting ratio: +10→+20 dBu input, target 0.1 ±0.1 dB output rise.
- Compressor THD: ≤0.2% at 6:1, maximum +20dB makeup, 800ms recovery, −18dBu threshold.
- Limiter THD: ≤0.45% at +22 dBu input, recovery 800ms (handbook's stated −18dBu threshold is incompatible with the +4 to +15 dBu limiter threshold range, so a corresponding VST3 test must be called a proxy).
- Through response: 20Hz–20kHz ±0.5dB relative to 1kHz; noise: special 600Ω terminated wideband output test (not equivalent to internal PSU noise).

## Confirmed previous nodal-VST3 factory ratio calibration
| Ratio | Nominal output rise | Stock (1750Ω) | 200Ω | Pass with 200Ω |
|---|---:|---:|---:|---|
| 1.5:1 | 6.5 ±1dB | 8.62dB | 6.56369dB | Yes |
| 2:1 | 5.0 ±1dB | 6.08dB | 5.16391dB | Yes |
| 3:1 | 3.5 ±1dB | 3.91dB | 3.45767dB | Yes |
| 4:1 | 2.5 ±0.5dB | 3.11dB | 2.69432dB | Yes |
| 6:1 | 1.5 ±0.5dB | 2.11dB | 1.69625dB | Yes |

The final ratio-row label differs between factory handbook revisions (5:1 printed versus 6:1 at actual selector); the measured 6:1 test retains the official last-row output-rise tolerance.

## Limiter alignment and ratio change, actually measured
- +20 dBu, limiter +8dBu setting: original +8.1472dBu output, tuned +8.0256dBu. Candidate is **closer to +8** by ~0.122dB.
- Limiter +10→+20dBu output rise: original 0.1154288dB, tuned 0.0062729dB; **both within [0,0.2]dB factory limit**, but tuned is closer to its strict lower edge.
- This is evidence RV2 also changes the limiter, not just the compressor.

## VST3 audio-envelope dynamic comparison (500ms burst)
These are 90%-of-gain-reduction-recovery audio times, **NOT Neve internal sidechain/control-voltage factory timings**.

| Limiter recovery | Stock | RV2 200Ω | Result |
|---|---:|---:|---|
| 50ms | 46ms | 37ms | ~20% faster |
| 100ms | 93ms | 79ms | ~15% faster |
| 200ms | 191ms | 159ms | ~17% faster |
| 800ms | 769ms | 639ms | ~17% faster |
| Auto 1 | 1689ms | 1221ms | ~28% faster |
| Auto 2 | 5149ms | 4180ms | ~19% faster |

With 3:1 compressor, −20dBu threshold and the same +10dBu high burst, measured audio gain reduction rose from ~17.3dB to ~22.6dB with 200Ω. Thus the static ratio pass **does not imply unchanged absolute gain reduction or release**.

## Solver and PSU noise
Both have zero mismatched samples under the numeric tolerance in offline mono/stereo solver-parity tests, including compressor-only, limiter-only, combined. This is **not real-time deadline certification**. The internal PSU-noise calibration remained identical: −95.6761dBFS 1kHz pink probe, −78.0161dBFS at 60Hz, −86.0388dBFS at 120Hz. This is not the manufacturer's 600Ω terminated wideband output-noise test.

## Full 776-tones-per-resistor characterization

The matched 776-case compressor ratio/threshold and limiter threshold/attack/level, makeup, THD harmonics, frequency, phase, and combined-mode sweep is running in GitHub Actions. Its dataset/results will be incorporated below after successful completion; do not infer passing performance from the absence of data.

- [Full matched VST3 sweep](https://github.com/jerrykilgore8995-dotcom/609/actions/runs/45011289650)
- [Matched timing and PSU noise audit](https://github.com/jerrykilgore8995-dotcom/609/actions/runs/45011293692)
- [Prior RV2 optimization investigation](https://github.com/jerrykilgore8995-dotcom/609/blob/609-rv2-investigation-only/docs/rv2-calibration-investigation.md)

**Preservation:** All measurement workflows use original generated decks and temporary builds. The original MKIV branch DSP remains at RV2=1750Ω.
