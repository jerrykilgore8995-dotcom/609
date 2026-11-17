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
