# Stock JD ladder and bounded calibration

The PL20233 drawing specifies a direct first tap and 820 Ω, 2.7 kΩ, 3.3 kΩ and 15 kΩ between the five taps. The stock switch, with the existing 1N4153 bridge, produces the following measurements at the 1 kHz, −20 dBu threshold, 100 ms recovery bench. The input levels are 0 and +10 dBu at the input terminals. [JD handbook and PL20233 drawing](https://media.soniccircus.com/productpdfs/Manuals/Neve/Neve_33609J_manual.pdf)

| Detent | SPICE output rise | Effective ratio |
| --- | ---: | ---: |
| 1.5:1 | 8.57 dB | 1.17:1 |
| 2:1 | 5.96 dB | 1.68:1 |
| 3:1 | 3.84 dB | 2.60:1 |
| 4:1 | 3.07 dB | 3.26:1 |
| 6:1 | 2.09 dB | 4.79:1 |

The bounded preview uses measured gain curves for all 80 compressor ratio/threshold positions and all 46 limiter attack/threshold positions. It uses coherent harmonic residuals at 100 Hz, 1 kHz, 5 kHz and 10 kHz, measured at selected levels and thresholds. The two sections were independently enabled for that fit. Their interaction, the effect of compressor makeup on limiter drive, and the full phase response remain unvalidated across the control combinations. The preview also fits fixed timing and the two automatic modes against stepped signals. The automatic measurements use 100 and 500 ms bursts.

## Combined-control parity bench

`tools/jd_conformance.py` maps each of the 17,487,360 single-channel combinations to a stable index. That includes both in/out switches, all the controls behind those switches, and all three compressor attack positions. For each index the bench measures live SPICE and the loaded VST3 with the same controls. The output includes fundamental level, phase error, THD, complex harmonics below Nyquist, and gain attack/recovery metrics. The default burst suite exercises each nonminimum input level from the minimum level. The sampling resolution is explicit in the output header. Use `--all-transitions` to exercise every pair of sampled levels. The header fingerprints the source deck, host and plugin to keep resumed runs comparable.

```sh
python tools/jd_conformance.py /path/to/Vst3Smoke /path/to/609.vst3   jd-00000000.jsonl --start 0 --count 32 --jobs 4
```

Rerunning the same command skips completed indexes. Each shard should use the same SPICE deck, VST3 version, levels, frequencies, and burst lengths. A finite input and frequency grid samples the continuous controls. At the default 61 levels and four frequencies, there are 4,266,915,840 steady-tone tests before timing or stereo configurations. A finished shard covers only the indexes and stimuli recorded in that shard. The `--fixed-timing` option holds the two attacks and recoveries at their default fixed settings, leaving 80,960 control states including the in/out switch combinations. Use `--fixed-timing --both-enabled` to address the 20,240 states with both sections in. Timing can be verified separately. Limiter recovery also changes steady harmonic content, so compare recovery variants at harmonic-sensitive settings. This bench is single-channel, with matching stereo and independent left/right combinations still to be measured.

The 1 kHz combined-control pilot exercised 3:1 compression with different thresholds, makeup settings, attack and recovery. The earlier preview build showed up to 0.26 dB of output-level error across its six tones, with considerably larger harmonic errors at some settings. This is verification of the measurement route and evidence that the isolated-section harmonic tables cannot certify combined parity. The measured mono limiter response from +10 to +20 dBu gives 49.66, 99.76, 201.34 and 804.20 ms of control-voltage recovery at its four fixed positions.

One cold-start 1 kHz SPICE tone in the combined pilot took 14.34 seconds in the local validation environment. At that measured rate, all 20,240 fixed-timing combined states at 61 levels and four frequencies would take about 102 days on eight workers, assuming ideal scaling. This is a runtime estimate for the current fixture.
