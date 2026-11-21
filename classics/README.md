# 2254 and 2264 nodal previews

These are two independent circuit models. Both use the 609 MKII sparse Newton solver, generated elimination schedules and stereo workers. The SPICE fixture also runs directly in ngspice. Each VST3 supports mono, dual mono and stereo link, with separate compressor and limiter controls. At 44.1 kHz, the benchmark measures 512-sample blocks with both sections in circuit.

## Drawing references

| Model | Signal and control circuits | Iron |
|---|---|---|
| 2254 | L/10004 with B183, B185, B191 and the B192 limiter amplifier | 31267 input, centre-tapped 10468 interstage, reverse-driven centre-tapped 31267 detector, gapped LO1166 |
| 2264 | EL10017 BA337 motherboard and EX10440/A BA440A line amplifier | 31267 input, series 10468 interstage, ungapped LO1173 |

- [2254 L/10004 and card drawings](https://www.benmook.com/tech/documents/Schematics%202/Schematics%201/Neve2254lim%20sch.pdf)
- [2264 EL10017](https://www.benmook.com/tech/documents/Schematics%202/Schematics%201/Neve2264lim.pdf)
- [BA440A schematic and documentation](https://www.technicalaudio.com/neve/neve_pdf/0440_BA440_class_AB_line_amplifier_doc_4pages.pdf)
- [CJ 31267 winding measurements](https://groupdiy.com/threads/neve-mic-and-line-input-data.52339/), [10468 measurements](https://groupdiy.com/threads/marinair-10468-t1444-green-meanie-input.13601/) and [LO1166 teardown](https://groupdiy.com/threads/lo-1166-gapped-neve-output-iron.6628/) and [bench measurements, posts 11–12](https://groupdiy.com/threads/mo-metal.4242/)
- [Neve transformer wiring and ratings](https://technicalaudio.com/reading/neve_xfmrs/neve_edo7113.html) and [Carnhill VTB9056 reference](https://www.barryrudolph.com/recall/manuals/carnhill_design_guide.pdf)

## Calibration status

The working decks are restricted to circuit development. Vintage semiconductor compact models have yet to be checked against measured curves. The parts found in the service drawings are listed in the decks.

| Transformer | Evidence applied | Still needed for release |
|---|---|---|
| 31267 | CJ's series/series 67 H and 16.4 H, 328 Ω and 112 Ω, and 8.2 mH leakage referred to the 16.4 H side | Capacitance, driven-core curves, and per-half leakage for the centre-tapped wiring in L/10004 |
| 10468 | CJ's 9.46 H and 9.55 H primary halves, 32 H series primary, 125.6 H series secondary, 6 mH series primary leakage, DCR, winding capacitance, and the approximately 110 pF between winding groups | Bench confirmation of the mutual terms in the 2254 centre-tapped connection and large-signal core behavior |
| LO1166 | CJ's gapped specimen #2, 833 mH primary, 1.93 H with secondaries in series and 14.5 Ω primary. The [LO1166/A bench sheet](https://fr.scribd.com/doc/207518597/Marinair-LO1166-audio-transformer-data) identifies 43.6 Ω end-to-end at output pins 8 and 5. | Leakage, capacitance and magnetizing response with the 2254's standing collector current |
| LO1173 | Neve EDO 71/73 connection and rated gain; VTB9056 replacement guide as a comparison | Original 2264 LO1173 winding resistance, open/short inductance and capacitance or a measured equivalent model for that part |

`iron.inc` labels interim values explicitly. The LO1166/A open-circuit measurements cannot establish core behavior at the 2254 operating point. The 2254 B183 gain and B192 limiter trimmers are recalibrated after changing the output winding values. The LO2567 teardown covers a different transformer and cannot supply LO1173 winding data.

The 2254 uses the early L/10004 circuit. A detented trim in the B191 input attenuation and feedback sets the host compressor threshold. The B192 limiter input loading and control taper are effective alignment values. The drawing's 120 Ω reading did not give a working +8 dBu limiter in the first pass. The De-Ess switch now stamps the B191 feedback network. Program meter modes use input/output levels and control voltage at the UI, rather than a B192 meter-driver simulation. The external duck input is omitted. The makeup switch uses the resistor bank from L/10004. Panel labels describe intended calibration and still need verification.

The 2264 motherboard scan calls out EX20127, EX20128 and EX20129 for the rotary subassemblies. The 2264 threshold begins with a 609-family ladder. The ratio, attack and recovery elements are effective values fitted to the published 2264 response. They do not represent a recovered EX20127–EX20129 parts list. Limiter calibration, interstage termination and amplifier bias need per-revision confirmation. Stereo link uses the MKII's 10 Ω Norton equivalent with one sample of delay between the channel solvers.

The current bench checks finite output and convergence under combined compression and limiting. At the +8 dBu limiter setting, the corrected SPICE decks produced these 1 kHz results:

| Model | Out at +10 dBu in | Out at +20 dBu in |
|---|---:|---:|
| 2254 | +8.00 dBu | +7.91 dBu |
| 2264 | +7.87 dBu | +8.00 dBu |

The earlier 2254 compressed far past its steady-state setpoint. During a burst, charging the B191 envelope pulled down the rail after the 470 Ω feed resistor and shifted the BCY70 base reference. The compact-model circuit drove the bridge past 40 dB of reduction. The clamp diodes now reference the rail on the supply side of the resistor. The discrete charging followers, timing capacitor, feedback path and diode bridge remain in the solve. Initial-law, top-end-law and timing resistors use effective values fitted to the modeled loop and are documented in the deck. The calibration covers the benchmark stimuli below. Hardware THD, threshold accuracy over the entire control range and 100 µs fast limiter attack need separate verification.

## Faceplates

The 2254 editor traces the [first 2254's panel photograph](https://postfade.co.uk/wp-content/uploads/2022/02/1969-Neve-2254-Compressor-Limiter-900px-85f.jpg). The 2264 editor uses the [2264 manual photograph and recall sheet](https://www.allfordj.ru/upload/iblock/407/rukovodstvo_polzovatelya_ams_neve_2264a.pdf). Both draw the hardware controls and markings without the Neve badge. For the 2254, knob centres, screws and meter placement are taken from the panel plane of the [early hardware photo](https://www.attackmagazine.com/reviews/the-best/top-20-best-hardware-compressors-ever-made/10/). The 2264 dial pointers use the detent angles on the factory recall sheet, including the separate arcs for concentric controls. Each stereo instance shows two modules and each mono instance shows one. On the 2264, matching A/A or B/B positions link the two modules. The 2254's DAW-only stereo link, makeup and compressor threshold are host parameters.

## Validation and build

The Windows runner builds both candidates independently. Each VST3 download requires clean SPICE operating checks, attack and fixed release timing and effective ratio within 10% of the benchmark targets, and a clean mono and linked-stereo processing benchmark. The 2254 normal limiter attack check uses a 2 kHz carrier to shorten the envelope measurement window. The 1 kHz fast limiter result is comparative and does not verify the 100 µs marking.

The dynamics probe uses a 1 kHz carrier and a centred, one-cycle fundamental envelope. Ratio is the output change from +10 to +20 dBu input at a fixed threshold. Attack is the time from that step to 1 dB above the new plateau, with the compressor on 6:1 for the timing reference. Release is the time to recover 63.2% of the gain change in dB. A 0.55 s burst checks the fixed releases. The Auto checks compare 0.1 and 3 s bursts. The 2264 recovery network keeps each Auto capacitor on a separate switched branch so SPICE and the generated solver use the same capacitance. A detent expression on a single capacitor would have stayed fixed at its default value in the generated solver. Timing values are tied to these stimuli and measurement definitions.

```text
python classics/read_dynamics.py 2264 build/Release/Bench2264.exe --require-close
```

```text
python classics/verify_spice.py
```

Once the SPICE behavior and transformer evidence are verified, generate an operating point and deck for each circuit from the repository root.

```text
python classics/seed.py classics/circuit/2254.cir decks/2254/seed.raw
python native/compile_deck.py classics/circuit/2254.cir decks/2254/seed.raw decks/2254/Deck.hpp
python classics/seed.py classics/circuit/2264.cir decks/2264/seed.raw
python native/compile_deck.py classics/circuit/2264.cir decks/2264/seed.raw decks/2264/Deck.hpp
cmake -S classics -B build -A x64 -DDECK_DIR=/absolute/path/to/decks
cmake --build build --config Release --target Classic2254_VST3 Classic2264_VST3 Bench2254 Bench2264
```

## Factory test comparison

The Windows workflow produces a JSON response and distortion report from the generated solver. It also captures the actual JUCE mono editor. The existing dynamics benchmark tests attack, ratio and release independently.

The 2264 targets are from the [2264 Issue 1.4 manual, page 10](https://www.allfordj.ru/upload/iblock/407/rukovodstvo_polzovatelya_ams_neve_2264a.pdf). At 1 kHz, the through and 6:1 compressor tests set the output to +9 dBu, the compressor uses 800 ms recovery, and the limiter tests +20 dBu in at a +4 dBu setting and 800 ms recovery. The respective THD maxima are 0.05%, 0.2%, and 0.4%. The source and load in the deck are 40 Ω and 600 Ω. dBu and dBm have the same numerical value at the 600 Ω load to within 0.01 dB.

The original 2254 and later 2254/E and 2254/R have different control sets and attack specifications. The 2254 report uses +8 dBu output for a comparison with published 2254-series figures, and reports the actual levels and settings. It does not attribute later revisions' specifications to L/10004.

The response sweep drives a 0 dBu tone with both dynamics sections out and the circuitry in. Phase is relative to the input sine, with no DAW buffering. At 44.1 kHz the BDF2 integration attenuates the top octave more than the SPICE AC model. Higher solve rates can be measured with the audit tool and are diagnostic only. Leakage and magnetizing assumptions still affect the analog result, particularly at the LO1173 and the 2264 interstage. The audit reports these discrepancies without masking them in the release timing gate.
