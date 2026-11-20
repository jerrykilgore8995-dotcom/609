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

The transformer equivalents include winding resistance, coupling and, where supported by the source, winding capacitance. They model incremental inductance at a fixed operating condition. Core saturation and hysteresis need hardware measurements, particularly for the DC-biased LO1166. The split-winding leakage and LO1173 magnetizing branch are estimates. Vintage semiconductor models in `parts.inc` are provisional compact models.

The 2254 uses the early L/10004 circuit with an added compressor threshold trim in the existing detector feed. The B192 limiter input loading and control taper are effective alignment values. The drawing's 120 Ω reading did not give a working +8 dBu limiter in the first pass. The de-ess switch, program meter selection and external duck input are omitted. The makeup switch uses the resistor bank from L/10004. Panel labels describe intended calibration and still need verification.

The 2264 motherboard scan calls out EX20127, EX20128 and EX20129 for the rotary subassemblies. The first pass uses related 609-family ladders and estimated recovery values. Limiter calibration, interstage termination and amplifier bias need per-revision confirmation. Stereo link uses the MKII's 10 Ω Norton equivalent with one sample of delay between the channel solvers.

The current bench checks finite output and convergence under combined compression and limiting. Hardware comparison of the full ratio curves, timing, THD and frequency response remains open.

## Build

Generate an operating point and deck for each circuit from the repository root.

```text
python classics/seed.py classics/circuit/2254.cir decks/2254/seed.raw
python native/compile_deck.py classics/circuit/2254.cir decks/2254/seed.raw decks/2254/Deck.hpp
python classics/seed.py classics/circuit/2264.cir decks/2264/seed.raw
python native/compile_deck.py classics/circuit/2264.cir decks/2264/seed.raw decks/2264/Deck.hpp
cmake -S classics -B build -A x64 -DDECK_DIR=/absolute/path/to/decks
cmake --build build --config Release --target Classic2254_VST3 Classic2264_VST3 Bench2254 Bench2264
```
