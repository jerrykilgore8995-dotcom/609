# 550A Nodal

Windows x64 VST3 of the vintage five-frequency 550A. The EQ retains the three paired gain and frequency selectors, low and high peak/shelf switches, independent filter switch and the EQ In switch. One modeled circuit runs per audio channel. Stereo channels share the panel settings. Full scale is +24 dBu. Use a modern x64 processor with AVX2.

## Circuit and sources

The main circuit follows the [613-759 550A drawing](https://ms-tas.com/api-550-eq-schematics/). The [API service schematic](https://audiocircuit.dk/downloads/api/API-550-eq-sm.pdf) supplies the 10 pF and 47 pF early compensation pair and the broader switch and connector context. The [TB550A assembly guide](https://ericbeam.com/wp-content/uploads/2019/12/TB550A-Assembly-Guide.pdf) corroborates the resistor ladders and three-transistor buffers. Gain and frequency positions use the actual signal topology of the vintage switch wafers. The frequency rotors combine capacitor banks as they rotate. Each capacitor and switch contact is stamped separately.

- The rack supply is ideal ±16 V. The onboard supply rectifiers and reservoirs remain. The fixture uses the high-level input and output configuration with a 40 Ω source and 600 Ω load. The original external gain strap is open.
- Both amplifier sockets use the 525 project's discrete 2520 compensation variant. The component library also includes its JE-990 for reference work. The 2520's MZ2361 compact model was aligned near the upper part of Motorola's [1.24–1.38 V forward specification at 10 mA](https://picosystems.jp/public/MOTOROLA_Master_Selection_Guide_1975.pdf) to calibrate the output-stage crossover behavior.
- T1 follows the four equal-winding [EA2503 connection](https://www.capi-gear.com/catalog/images/gallery/2503/2503-specs.pdf), with [measured winding resistances](https://groupdiy.com/threads/build-capi-553f-500-series-lc-equalizer-official-support-thread.54724/page-3). The 1.014 H estimate uses a [legacy 2503 winding impedance measurement](https://groupdiy.com/threads/triad-hs-29-innerstage-transformer.57092/) at 100 Hz. The transformer uses a linear coupled-inductance model with estimated leakage. An original 550A hardware capture and the EA2503's nonlinear core curves remain unavailable.
- The discrete device cards and the switch reconstruction constitute a documented circuit model. Several vintage semiconductor parameters were sourced from contemporary compact equivalents. An individual factory module can differ with its board issue and part tolerances.

The editor's geometry and markings follow the [vintage five-point faceplate](https://retrogearshop.com/products/api-550a-vintage). The product identifier is retained and the manufacturer logo is omitted. The controls are vector drawings.

## Verification

The dedicated 550A workflow generates the operating point and deck from ngspice. It verifies the nodal solver and hosts the compiled Windows VST3. The through-path gates are ±0.5 dB at 30 Hz–20 kHz and THD below 0.05% at +4 dBu and 0.07% at +18 dBu, as in the [API specification](https://apiaudio.com/docs/manuals/TCS_user_2016-02-01.pdf). THD is measured through the eighth harmonic within the 80 kHz measurement bandwidth. Each response curve must match SPICE within 10% in linear amplitude and 10° after removing the plugin's fixed resampling delay. Steady-state THD is checked within 10% relative to SPICE. The sweep includes all single-band gain and frequency detents, both shelf modes, the filter, EQ out and a mixed setting.

The solver uses the same flattened circuit as the reference. It integrates the reactive parts with trapezoidal companion models. At 44.1 kHz it solves internally at 132.3 kHz and reports 96 samples of delay. The workflow archives the measurement JSON and a capture of the actual plugin editor.
