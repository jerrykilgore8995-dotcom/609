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
- [CJ 31267 winding measurements](https://groupdiy.com/threads/neve-mic-and-line-input-data.52339/), [10468 measurements](https://groupdiy.com/threads/marinair-10468-t1444-green-meanie-input.13601/) and [LO1166 teardown](https://groupdiy.com/threads/lo-1166-gapped-neve-output-iron.6628/)
- [Neve transformer wiring and ratings](https://technicalaudio.com/reading/neve_xfmrs/neve_edo7113.html) and [Carnhill VTB9056 reference](https://www.barryrudolph.com/recall/manuals/carnhill_design_guide.pdf)

## Calibration status

The working decks are restricted to circuit development. Vintage semiconductor compact models have yet to be checked against measured curves. The parts found in the service drawings are listed in the decks.

| Transformer | Evidence applied | Still needed for release |
|---|---|---|
| 31267 | CJ's series/series 67 H and 16.4 H, 328 Ω and 112 Ω, and 8.2 mH leakage referred to the 16.4 H side | Capacitance, driven-core curves, and per-half leakage for the centre-tapped wiring in L/10004 |
| 10468 | CJ's 9.46 H and 9.55 H primary halves, 32 H series primary, 125.6 H series secondary, 6 mH series primary leakage, DCR and winding capacitance | Bench confirmation of the mutual terms in the 2254 centre-tapped connection and large-signal core behavior |
| LO1166 | CJ's gapped construction and the [LO1166/A bench specimen](https://fr.scribd.com/doc/207518597/Marinair-LO1166-audio-transformer-data) for ratio, DCR and unloaded impedance | Leakage, capacitance and magnetizing response with the 2254's standing collector current |
| LO1173 | Neve EDO 71/73 connection and rated gain; VTB9056 replacement guide as a comparison | Original 2264 LO1173 winding resistance, open/short inductance and capacitance or a measured equivalent model for that part |

`iron.inc` labels interim values explicitly. The LO1166/A zero-bias test cannot establish core behavior at the 2254 operating point. The LO2567 teardown covers a different transformer and cannot supply LO1173 winding data.

The 2254 uses the early L/10004 circuit with an added compressor threshold trim in the existing detector feed. The B192 limiter input loading and control taper are effective alignment values. The drawing's 120 Ω reading did not give a working +8 dBu limiter in the first pass. The De-Ess switch now stamps the B191 feedback network. Program meter modes use input/output levels and control voltage at the UI, rather than a B192 meter-driver simulation. The external duck input is omitted. The makeup switch uses the resistor bank from L/10004. Panel labels describe intended calibration and still need verification.

The 2264 motherboard scan calls out EX20127, EX20128 and EX20129 for the rotary subassemblies. The first pass uses related 609-family ladders and estimated recovery values. Limiter calibration, interstage termination and amplifier bias need per-revision confirmation. Stereo link uses the MKII's 10 Ω Norton equivalent with one sample of delay between the channel solvers.

The current bench checks finite output and convergence under combined compression and limiting. At the +8 dBu limiter setting, the corrected SPICE decks produced these 1 kHz results:

| Model | Out at +10 dBu in | Out at +20 dBu in |
|---|---:|---:|
| 2254 | +8.00 dBu | +7.91 dBu |
| 2264 | +7.87 dBu | +8.00 dBu |

The 2254 still over-attenuates at the 6:1 setting. It produced −27.60 dBu at +10 dBu input and −17.67 dBu at +20 dBu input in the late measurement window. The 2264 compressor passed this operating sanity check, but that does not verify its individual rotary assemblies or hardware ratio curves. Hardware comparison of the full ratio curves, timing, THD and frequency response remains open.

## Faceplates

The 2254 editor traces the [first 2254's panel photograph](https://postfade.co.uk/wp-content/uploads/2022/02/1969-Neve-2254-Compressor-Limiter-900px-85f.jpg). The 2264 editor uses the [2264 manual photograph and recall sheet](https://www.allfordj.ru/upload/iblock/407/rukovodstvo_polzovatelya_ams_neve_2264a.pdf). Both draw the hardware controls and markings without the Neve badge. Each stereo instance shows two modules and each mono instance shows one. On the 2264, matching A/A or B/B positions link the two modules. The 2254's DAW-only stereo link, makeup and compressor threshold are host parameters.

## Validation and build

The latest source has its Windows build behind a manual workflow and the SPICE operating checks. The earlier runner timing used preliminary circuits and preceded the hardware-faceplate editor.

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
