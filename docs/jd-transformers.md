# 609 input and output transformers

The 33609JD drawing identifies the signal input transformer as T2/TF10016 and the line output transformer as T3/TF12009. The [JD handbook](https://www.thehistoryofrecording.com/Manuals/Neve/Neve%2033609jd.pdf) specifies ±0.5 dB from 20 Hz to 20 kHz relative to 1 kHz. The model is checked with the full channel, its original interstage transformer, and the fixture's 40 Ω source and 600 Ω external termination.

## Winding data

- **Input T2.** CJ's [T1452/31267 teardown](https://groupdiy.com/threads/neve-mic-and-line-input-data.52339/) and [numerical winding summary](https://groupdiy.com/threads/lundahl-combo-transformer.67567/) give 2,710 and 1,350 turns, 67 and 16.4 H, and 328 and 112 Ω with both windings in series. Each side of the balanced winding gets half of the corresponding DCR. CJ's 8.2 mH leakage figure is stated with the transformer reversed. Interpreting it as referred to the driven low-turns side gives approximately 33 mH on the high-turns side. The original test frequency and drive level were unspecified. The model encodes the resulting total leakage in the coupling coefficient, using $L_\mathrm{sc} = L_\mathrm{p}(1-k^2)$.
- **Output T3.** A [TF12009/VT26247 owner measured](https://groupdiy.com/threads/neve-vt26247-tf12009-transformers.11870/) 3 and 34.5 Ω across the two windings. The [VTB9056 replacement guide](https://www.barryrudolph.com/recall/manuals/carnhill_design_guide.pdf) gives 3 and 30 Ω with a nominal +8 dB voltage gain, consistent with the [JD builder's approximately 1:2.5 connection](https://groupdiy.com/threads/official-33609-builders-tread-see-1st-page-for-updates.26218/page-32). The model keeps the winding resistance from the actual TF12009 report. The 2.2 H primary is the supplied LO1173-family starting estimate. The output leakage is an effective 16.2 µH referred to the primary, carried over from the earlier loaded-response fit. CJ's [LO2567 data](https://groupdiy.com/threads/mo-metal.4242/) describe a different transformer and give a comparison point for the family.

The winding capacitances represent effective differential values for this circuit and load. The fitted 850 pF input secondary term absorbs the distributed winding and shield effects seen by the differential circuit. It is distinct from a measured interwinding or shield capacitance. The two models are linear. A calibrated core model would require the appropriate core and distortion-versus-level measurements.

## Full-channel spot checks

The revised bench uses PRESET 0.62453 to restore unity at 1 kHz, LIMCAL 0.35 to align the +8 dBu limiter detent at +20 dBu input, and COMPCAL 0.821 to restore the previous +4 dBu, 4:1 compressor result at the 0 dBu threshold detent. These are fixture calibrations for the changed transformer loading.

| SPICE result | Previous | Revised |
| --- | ---: | ---: |
| 20 Hz magnitude relative to 1 kHz | −0.133 dB | +0.137 dB |
| 20 kHz magnitude relative to 1 kHz | −0.440 dB | −0.435 dB |
| 20 kHz phase | −12.44° | −38.89° |
| +4 dBu compressor output, 4:1, 0 dBu threshold | −0.335 dBu | −0.335 dBu |
| +4 dBu compressor THD at 1 kHz, same setting | 0.189% | 0.176% |
| +20 dBu limiter output, +8 dBu threshold, 50 ms | +8.008 dBu | +8.002 dBu |
| +22 dBu limiter THD at 1 kHz, +8 dBu threshold, 800 ms | 1.040% | 0.885% |

Input levels are fixture-generator levels. The magnitude falls within the handbook's tolerance on the 20 Hz to 20 kHz AC sweep. The phase figures are simulated. The revised limiter still exceeds the handbook's 0.45% THD ceiling at the closest comparable fixture condition. That ceiling and the lack of an independent loaded transformer phase trace limit how far the revised reference can be used to calibrate the existing bounded VST3.
