# 33609/JD distortion bench

The [33609/JD Technical Handbook](https://thehistoryofrecording.com/Manuals/Neve/Neve%2033609jd.pdf) gives a 0.45% maximum for the limiter at 1 kHz, +22 dBu input, 800 ms recovery, with compression out. The distortion line gives a −18 dBu limiter threshold, while the limiter scale runs from +4 to +15 dBu. This test uses the +8 dBu detent specified in the handbook's separate limiter alignment procedure and normal attack. The handbook's compressor line specifies 6:1, −18 dBu threshold, maximum makeup gain and 800 ms recovery, and omits the input level. This test takes 0 dBu from the handbook's separate ratio test. The through specification is 0.075% maximum at 1 kHz and +9 dBu with both dynamics sections out.

Both models use a 40 Ω balanced source and 600 Ω load. Plugin measurements use 44.1 kHz, +24 dBu at full scale and PSU Noise off. K2 through K20 are divided by the output fundamental and combined in quadrature after a 700 ms settling interval.

## Circuit diagnosis

At the +8 dBu limiter detent, Character SPICE measured 1.0541% at the diode bridge, 1.0504% at the amplifier input and 1.0505% at the output. At full scale in the 3:1 compressor measurement, SPICE measured 5.75% at the amplifier input and the output. The amplifier has the 15 kΩ feedback connection, C18, R40 and the switched makeup feedback network. The distortion observed at these levels originates predominantly in the bridge.

The alternative fit lowers T2 secondary drive and compensates with the T1 secondary while retaining the complete diode bridge, pads, buffer and amplifier. The through gain at 1 kHz remains within 0.06 dB of unity in the fit. The fitted secondary voltage ratios relative to the Character model are 0.645 for T2 and 1.115 / 0.645 for T1. These effective ratios constrain the estimated transformer model against the bench data. Actual JD winding constants and distortion at other conditions require hardware measurements.

## Plugin switch

**Calibrated Distortion** is off by default, which selects the original Character signal path and preserves the sound of saved sessions. Turn it on for the lower-drive fit. The VST3 saves and exposes the switch to host automation. Both modes run the Character envelope and meter model. The calibrated path uses a short parallel gain average and a lower nonlinear residual, with independent limiter even-harmonic attenuation. The control change is smoothed and both modes report zero samples of latency.

At the +8 dBu limiter detent the calibrated SPICE and nodal references measured 0.4380% and 0.4487%. The bounded engine measured 0.4389%, with K2 at −74.1 dBc and K3 at −47.4 dBc. At the compressor bench setting the three calibrated models measured about 0.06%. The +4 and +15 dBu limiter settings and the +24 dBu compressor check are included in the reports as separate operating points. The transfer curve and the complete harmonic spectrum have not been characterized against hardware at all settings.

## Reproduce

```sh
python tools/measure_jd_distortion.py --calibrated-deck build/calibrated-circuit
python tools/native_seed.py build/calibrated/seed.raw build/calibrated-circuit
python native/compile_deck.py build/calibrated-circuit build/calibrated/seed.raw build/calibrated/Deck.hpp
```

The Windows build measures both compiled circuit references and the packaged VST3. The artifact includes the six SPICE cases with per-node K2–K20 levels, the nodal and bounded comparisons, and the VST3 output and callback reports.
