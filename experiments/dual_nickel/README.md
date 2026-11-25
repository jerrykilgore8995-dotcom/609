# Nickel Opto — Option C guitar-bus / bass optical compressor

Development branch: `feature/dual-nickel-zener-prototype`. **The original 609
plugin, native DLL and production sources on main are not modified.** The branch
name is historical; the current design is no longer a dual-transformer zener
limiter.

## Production-intended mono signal path

1. Input Drive -18 to +18 dB (clean source driver)
2. 1:1 49%-nickel-alloy-*inspired* magnetic input transformer (Jiles–Atherton
   state, nonlinear implicit magnetic KCL, resistive/capacitive winding loading
   and a chowdsp_wdf receiver)
3. Switchable C4-*style* single dynamic low-mid band, 80–400 Hz center,
   adjustable width, threshold, max attenuation, attack and release;
   phase-aligned three-band Linkwitz–Riley-4 topology
4. Sidechain: passive loaded-pot RC high-pass (40–300 Hz), diode full-wave
   rectifier, physical timing capacitors with variable charge/discharge
   resistances, LED driver amplifier
5. LED/CdS photocell attenuation: actual diode Shockley current solution,
   photoconductance with history-dependent light-state charge and release,
   resistor-network KCL; not a synthetic distortion curve
6. Makeup gain
7. 2N5088 NPN + 2N5087 PNP complementary class-AB emitter follower on
   +/-15 V rails, **five-node nonlinear MNA** (base, emitter and output
   potentials), model-card transport/charge terms, coupling capacitors with
   ESR, dielectric-history approximation, and selectable modeled 600-ohm /
   10-kohm load
8. Post-circuit Output Trim with Input-Drive-linked gain compensation by
   default; link can be disabled

Stereo: two independent magnetic/audio circuits, matched and **linked low-mid
detectors and optical detectors** to avoid image movement. Mono: only one
processing channel and the left half of the VST3 editor. The new VST3 is
`juce_plugin/` and is an independent target/product ID.

## Main source files

- `NickelTransformer.hpp`: magnetic input, and **legacy** dual-transformer
  regression code. The new `MonoEngine` uses just the input transformer.
- `OpticalCell.hpp`: optocell, LED diode, LDR KCL, physical sidechain.
- `DynamicLowMid.hpp`: active, phase-aligned low-mid multiband compression.
- `NodalBjtOutput.hpp`: **current** five-node BJT nonlinear solver.
- `DiscreteOutputDriver.hpp`: **legacy reduced BJT reference**, not used
  in the new Nickel Opto engine.
- `2N5088_2N5087_models.lib`: published NSC/Fairchild-attributed full
  SPICE card parameters.
- `option_c_reference.cir`: transistor-level ngspice reference circuit.
- `AnalogOptoProcessor.hpp`: full mono and stereo DSP wiring.
- `juce_plugin/Plugin.cpp`: VST3 processing, parameters and half-width
  mono / dual-panel stereo GUI.
- `test_nodal_bjt.cpp`, `test_analog_opto.cpp`, `test_opto_character.cpp`:
  nodal current/charge, optocell, detector, crossover, stereo and CPU checks.

## Build and validation

Standalone DSP:
```sh
cmake -S experiments/dual_nickel -B build/nickel -DCMAKE_BUILD_TYPE=Release
cmake --build build/nickel --parallel 2
ctest --test-dir build/nickel --output-on-failure
```

Windows VST3:
```powershell
cmake -S experiments/dual_nickel/juce_plugin -B build/nickel-vst3 -G "Visual Studio 17 2022" -A x64
cmake --build build/nickel-vst3 --config Release --parallel 2
```

GitHub Actions:
- `Nickel Opto circuit validation` (Linux C++, ngspice reference, CTest,
  complete mono/stereo full DSP benchmarks).
- `Nickel Opto VST3 Windows build` (JUCE 8 Windows x64 build and artifact).
- Legacy dual-output-transformer tests remain in source for prior comparison
  but are intentionally not used by the new analog opto production path.

## Calibration and fidelity limitations

This is a real stateful electrical DSP implementation, **not** a vendor
hardware clone. The nickel core lacks measured B/H geometry or samples.
The selected optical reference is NSL-32 family: published approximate 500
ohms at 20 mA, off-resistance 500 kohms, 3.5 ms rise and
approximately 500 ms decay to 100 kohms under specified datasheet
conditions. The transfer curve and multi-time-constant light-memory
states use engineering fitting assumptions; *full measured cell curves,
temperature, aging and lot-to-lot spread are not yet characterized*.

The MNA circuit stamps forward/reverse transistor transport, Early voltage,
high-injection current, depletion and transit-charge branches, series
base/emitter resistances, nodal current KCL and capacitive history.
Some full SPICE card secondary features (e.g. collector bulk node and
bias-dependent transit parameter XTF/VTF/ITF, temperature scaling)
are still simplified. Do not describe its output as **exactly** full
ngspice or as validated within 5% without an explicit numerical comparison.
The SPICE netlist contains idealized bias references, so separate bias,
DC operating point and temperature calibration are also required.

The optical attack/release knobs change detector RC resistances. The
actual gain-reduction timing is limited by nonlinear light rise,
multi-stage release and history. The optical ratio control changes
sidechain amplifier feedback; **actual steady-state compression slope
must be measured**, rather than assuming the printed ratio is exact.

An optocell by itself doesn't necessarily generate even-order harmonics.
Distortion must emerge from its actual nonlinear device/current/circuit
behavior, not injected K2 values.

Measured time in the GitHub Linux CI runner is **not** a Windows Reaper
or SER8 worst-case real-time result. 44.1k/512 processing budget is
approximately 11.61 ms. Dynamic control changes, 600-ohm loading,
stereo bursts, variable attack/recovery, multitone aliasing and final
sample-rate sweeps must be validated before an accuracy/performance release.

## References

- NSL-32 optocoupler datasheet (Silonex family):
  https://www.alldatasheet.fr/html-pdf/205826/SILONEX/NSL-32/293/1/NSL-32.html
- Published BJT cards:
  https://github.com/kicad-spice-library/KiCad-Spice-Library/blob/master/Models/Transistor/BJT/BJT.lib
- Andy Sneap's original description of single-band C4 guitar ducking:
  https://www.waves.com/metal-mixing-tips-andy-sneap

## Optical feedback-loop stabilization and selectable detector

The model started as **post-LDR feedback**, like the classic T4-style
topology. Fast (1 ms) detector attack with +12 dBu initially sent ~8 mA
through the LED, forcing ~16.9 dB peak GR before settling near 9.7 dB.
A **discrete shunt transistor LED current limiter**, represented by a
+15 V bias-divider reference, finite driver resistance and BJT junction
current in the LED KCL, reduced that 16.9 dB peak to ~10.7 dB while
preserving ~9.5 dB settled GR. The original one-millisecond timing was
still unstable around +8 dBu (~3.8 dB GR overshoot), so the validated
electrically controllable **Attack range is now 10–100 ms**. At 10 ms
the +4/+8/+12 dBu excess is ~0.14/0.93/0.91 dB; at the default 15 ms
it is ~0.08/0.57/0.77 dB. The release control remains 50–2000 ms;
actual CdS recovery is history-dependent and not equal to the knob value.

An attempted RC LED-current lead-compensation branch was **rejected**:
the measured overshoot became worse at 8 dBu and some slower attack
settings. It is not part of the present signal path.

The new **Feedforward** switch changes the detector's source to audio
*before* the opto attenuator (feedback remains the default). It also
switches a passive 3.3k/10k detector input resistor divider and a
**biased diode shunting the detector amplifier feedback resistor**
for a smooth, physically derived nonlinear sidechain knee. The
default 15 ms attack, 250 ms release steady GR values at
+4/+8/+12 dBu are feedback 2.34/5.77/9.08 dB and feedforward
1.57/5.16/5.71 dB. Diode model parameters and the LED current-limit
reference are development-calibrated, **not validated against a
specific hardware assembly**.

The dedicated `trace_opto_feedback.cpp` captures LED current,
detector capacitor voltage, light-dependent resistance and GR in
both modes. `test_opto_bursts.cpp` now includes overshoot and
feedforward dynamic-range regressions; do not replace them with
nominal-RC tests.
