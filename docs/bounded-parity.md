# 609 bounded model evaluation

The Windows evaluation uses the bounded audio engine for playback and offline rendering. The component-level nodal solver and SPICE deck remain the two comparison references. The evaluation keeps the original parameter IDs, faceplate and PSU Noise switch.

## Results at 44.1 kHz

| Test | Reference difference or residual |
| --- | --- |
| 20 kHz through amplitude | Legacy nodal −2.57 dB, SPICE −0.43 dB. Even the amplitude that minimizes the worst relative error differs from the two references by about 12.3%. |
| Noisy 83 Hz stereo burst, linked compressor | The legacy nodal and SPICE renders differ by 1.76 dB at the 95th percentile of 256-sample RMS windows. The bounded candidate differs from SPICE by about 2.45 dB. |
| Noisy 83 Hz stereo burst, linked limiter | The candidate improves the 95th-percentile difference from SPICE from 0.95 dB to 0.74 dB. |
| Noisy 83 Hz stereo burst, linked compressor and limiter | The candidate improves the 95th-percentile difference from SPICE from 2.60 dB to 2.44 dB. |
| Held-out compressor tone cases | 1.22% fundamental amplitude error and 1.24 percentage points of THD error, both at the 95th percentile. |
| Held-out limiter tone cases | 1.57% fundamental amplitude error and 0.41 percentage points of THD error, both at the 95th percentile. |

The spectrally calibrated path uses measured complex residuals through K20 where the harmonic falls below Nyquist. Its frequency anchors are 40, 100, 300, 1,000, 3,000 and 8,000 Hz. The held-out tone cases use 70, 200, 700, 2,100 and 5,100 Hz with different threshold, ratio, limiter attack or makeup positions. Across 80 held-out compressor and limiter cases, individual harmonic amplitudes still exceed 5% error. The calibration anchors and the held-out tests assess selected combinations. They cannot certify every control combination.

The noisy bass burst contains a decaying 83 Hz fundamental, a quieter 1.83 kHz component and seeded broadband noise, with staggered left and right onsets. SPICE was driven by the same sampled inputs as piecewise-linear voltage sources with a maximum internal timestep of 11.34 µs. The reported window statistics omit the initial 100 ms and compare linked modes at the lowest compressor threshold, 4:1, and the +8 dBu limiter threshold.

The 5% target is a relative amplitude criterion. It corresponds to approximately −0.45 dB to +0.42 dB. The two reference models already exceed that separation at 20 kHz and on parts of the bass burst, and the current evaluation fails the overall parity target.

The GitHub build packages `circuit-comparison.txt` with harmonic measurements and held-out bass tones, the SPICE output, and the Windows host's paced playback and offline checks. The SPICE AC response can be checked independently with `python tools/measure.py --ac`.

## Finer-timestep reference check

The full nodal circuit measured −0.459 dB and −12.83° at 20 kHz with a 352.8 kHz internal timestep. SPICE measured −0.433 dB and −12.44°. The 44.1 kHz legacy nodal result was −2.574 dB and −3.33°. The comparison uses the same component models and source/load settings across nodal timestep tests.

For the linked compressor bass burst, the full nodal circuit at 176.4 kHz tracked the SPICE render within 0.052 dB at the 95th percentile of 256-sample RMS windows. Its normalized waveform residual was 4.88%. This test used linear interpolation between the original 44.1 kHz input samples, with outputs sampled on the original grid. The finer-grid reference significantly reduces the disagreement between the two full-circuit solvers.

At 1 kHz, the six SPICE and bounded transfer points tested were through at 0 and +9 dBu, compressor 4:1 at 0 and +10 dBu, and limiter at +10 and +20 dBu. The largest bounded fundamental amplitude difference from SPICE among those six points was 0.38%.

## Ratio-specific THD and bass recalibration

The 1 kHz compressor residuals now use the measured ratio and threshold detents. In the previous evaluation, the 0 dBu threshold shared the 4:1 residual across all ratios.

| Ratio | Full nodal THD | SPICE THD | Calibrated bounded THD |
| --- | ---: | ---: | ---: |
| 1.5:1 | 0.54130% | 0.54376% | 0.54162% |
| 2:1 | 0.52712% | 0.52999% | 0.52736% |
| 3:1 | 0.46988% | 0.47323% | 0.46979% |
| 4:1 | 0.40482% | 0.40831% | 0.40440% |
| 6:1 | 0.35262% | 0.35778% | 0.35182% |

These local tests used 1 kHz, +10 dBu input, a 0 dBu threshold, 100 ms recovery, zero makeup and the limiter off. THD includes K2 through K20. The Windows pipeline repeats the measurement through the packaged VST3 and checks a 5% relative error limit against both references. See `ratio-thd.tsv` in the archive for the build-specific measurements.

The bass changes move the modeled output coupling after the time-varying gain and add a bounded correction for low-frequency envelope shape. The correction uses causal envelope, polarity and stereo features calibrated against a mix of finer-timestep nodal and SPICE renders.

| Bass test | Prior window-level p95 error | Calibrated window-level p95 error | Normalized waveform error |
| --- | ---: | ---: | ---: |
| Noisy 83 Hz burst, linked compressor vs SPICE | 2.45 dB | 1.79 dB | 14.92% → 11.50% |
| Noisy 83 Hz burst, both sections vs SPICE | 2.44 dB | 1.79 dB | 14.92% → 11.42% |
| Six bursts withheld from fitting, mean vs finer-timestep nodal | 1.30 dB | 1.22 dB | 8.19% → 7.46% |

Individual bass fixtures can still differ, and some became slightly worse. The largest p95 difference among the six withheld cases was 3.75 dB. The updated 5% check applies to the five reported THD measurements at the stated settings.

A second set of six bass fixtures was generated after the calibration was selected. Mean window-level p95 error against the finer-timestep nodal solver fell from 2.65 dB to 2.40 dB. Mean normalized waveform error fell from 11.01% to 9.74%. The worst case used 6:1, a −20 dBu threshold, 100 ms recovery and stereo link. SPICE confirmed the discrepancy. The calibrated build's window p95 was 10.53 dB for that stimulus, with the largest deviations around −56 dBFS in the decay. The bounded detector's left timing capacitor peaked at 11.01 V against 19.11 V in SPICE during the first burst. The finer-timestep nodal and SPICE waveforms agreed within 0.091 dB at p95 for that test.

## Limiter-only even-harmonic calibration

This build adds a time-dependent complex even-harmonic residual to the limiter-only bass path. The correction covers approximately 34–400 Hz and applies when the signal is coherent. The gain-reduction trajectory, compressor path and original odd-harmonic synthesis are unchanged. Regression renders for through, compressor-only and combined modes matched the previous engine sample for sample. The largest fundamental amplitude change across the limiter tone grid was 0.0035 dB.

The 110 Hz benchmark uses +20 dBu input, a +8 dBu threshold, normal attack, 50 ms recovery and the 400–600 ms measurement window.

| Harmonic | Previous bounded | Full nodal | SPICE | Revised bounded |
| --- | ---: | ---: | ---: | ---: |
| K2 | −42.98 dBc | −63.22 dBc | −65.31 dBc | −64.37 dBc |
| K4 | −54.95 dBc | −64.70 dBc | −65.93 dBc | −65.34 dBc |
| K6 | −59.55 dBc | −69.02 dBc | −70.51 dBc | −69.77 dBc |
| K8 | −66.43 dBc | −73.38 dBc | −75.88 dBc | −74.68 dBc |

The [normal-attack sweep](limiter-even-110-normal.csv) and [fast-attack sweep](limiter-even-110-fast.csv) give K2 through K20 for both references and both bounded builds. They cover all 23 limiter thresholds, all six recoveries, both attack positions and two input levels per setting, +8 and +12 dB above threshold. Among 5,389 even bins with either reference above −85 dBc, 42.2% could mathematically fall within 7% amplitude of both references. The revised model met that tolerance on 99.2% of those compatible bins. Across all reported bins, median absolute even-harmonic error dropped from 6.99 to 0.77 dB against the nodal solver and from 6.98 to 0.74 dB against SPICE.

The two 110 Hz K2 references differ by 2.09 dB at the benchmark. The smallest possible maximum relative amplitude error is about 12%, so the calibration balances the two targets. A separate 48-case intermediate-frequency check gave a median even-harmonic error of 4.22 dB against the nodal solver, down from 7.79 dB. The 5–7% target across unmeasured input levels, frequencies and the entire plugin remains unverified. The compressor's documented deep linked 6:1 burst discrepancy is outside this limiter-only change.
