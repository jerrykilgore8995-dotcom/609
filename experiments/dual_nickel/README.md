# Dual-Nickel Magnetic Circuit — development branch

**Scope:** this is the two-transformer audio-color engine only. The 33609JD
native engine and 609 VST3 sources are untouched. The zener limiter itself
and a separate JUCE plug-in have not yet been integrated.

## Implemented DSP

- 1:1 49%-nickel-*alloy-inspired* character input stage and 1:1
  high-nickel Jensen JT-11-BMCF-*inspired* low-distortion output stage.
- Magnetization state with Langevin anhysteretic curve, reversible and
  irreversible Jiles–Atherton-style terms, domain-wall pinning, branch
  direction and stateful minor-loop response.
- Implicit electric/magnetic coupling via Faraday's law and KCL of the
  primary/secondary windings. Primary and secondary copper resistance,
  leakage inductance, shunt winding capacitance, finite core-loss
  resistance, source resistance, external load and winding current
  history are in the **same** implicit solve.
- A separate actual `chowdsp_wdf` RC receiving network, including correct
  series adaptor, polarity inverter and ideal source reflection wiring.
  **Not** yet a completely WDF-native magnetic multiport network: the
  coupled nonlinear core uses MNA-style equations and safeguarded Newton
  iterations with high-precision bisection fallback. That distinction is
  intentional and must not be described as full-WDF.
- Drive -12 to +18 dB before input core; interstage -24 to +24 dB and
  makeup -12 to +24 dB before output core; gain compensation after the
  output core, linked by default, can be unlinked.
- Internal scale: normalized digital peak 1.0 represents a +24 dBu
  full-scale RMS sine; nominal +4 dBu is -20 dBFS peak.

## Measured simulation baseline (GitHub Linux runner)

Latest fully measured reference before phase/report additions:
[CI run](https://github.com/jerrykilgore8995-dotcom/609/actions/runs/45250003735).
At +4 dBu:
| Frequency | Character input THD | Clean output THD |
|---|---:|---:|
| 20 Hz | 1.4827% | 0.004164% |
| 50 Hz | 0.39862% | 0.000715% |
| 100 Hz | 0.12605% | about 0.00081% |
| 1 kHz | 0.001714% | 0.0000020% |

One transformer channel: 1.55 ms per 512 samples on that runner at
44.1 kHz, release build. Four equivalent channels would estimate about
6.2 ms before the limiter; this estimate is **not** a measured stereo
plugin benchmark and does not include oversampling, host or limiter.

The CTest suite checks finite behavior, abnormal peaks, clean-core THD,
response magnitude within +/-1 dB across the tested 20 Hz–10 kHz tones,
and the two-stage linked/unlinked routing path. Raw THD is measured with
a 44.1 kHz sampled, integer-cycle harmonic projection (K2–K20 within
Nyquist). Phase is reported using sine/cosine projections. Very small
THD values are limited by the solver and measurement precision.

## Hardware fidelity limitations

Neither core's real B-H curve, actual winding parasitics, magnetic
dimensions, nor Jensen phase/THD curves have been fitted to physical
hardware data. Parameters are **engineering estimates**, not factory
specifications. The current clean-model 20 Hz THD is roughly twice the
0.002% typical Jensen figure referenced in the design discussion.
It would be inaccurate to claim this is an exact Jensen or N49
recreation. Full load sweeps, 20 kHz magnitude and phase verification,
multi-tone IMD, sample-rate/oversampling alias tests and actual SER8
real-time VST3 testing remain necessary.

## Build

```sh
cmake -S experiments/dual_nickel -B build/dual-nickel -DCMAKE_BUILD_TYPE=Release
cmake --build build/dual-nickel --config Release
ctest --test-dir build/dual-nickel --output-on-failure
./build/dual-nickel/nickel_smoke
```

The standalone target fetches `chowdsp_wdf` from upstream. Pin the library
to a tested commit before a release build.

## Planned limiter integration

8:1 circuit-derived zener limiting, threshold control,
1/8/15 ms attack (8 default), 50/100/250/500/1000 ms
recovery (100 default), and a large ascending 0–20 dB
gain-reduction meter in a **new** JUCE plug-in target. None of these are
implemented or claimed tested here.

## Option C prototype (revised direction)

The new `DiscreteOutputDriver.hpp` and `test_option_c.cpp` implement an **isolated** clean input-transformer / discrete makeup-output-driver candidate. The existing `DualTransformerGain` class and its previous test fixtures remain for regression and comparison, but are **not the desired final production topology**. The final path is:

Input Drive → clean source driver → existing 49%-nickel input transformer → receiver/coupling → [C4-style band and optical gain cell, not yet implemented] → makeup → NPN+PNP emitter-follower output → series output capacitor → linked final trim. No output transformer.

This is a transistor-model **engineering prototype**, not a characterized real device or finished optical compressor. The BJT currents are Shockley/exponential with emitter resistors and a bounded nodal solve. Explicit capacitor state models are in the signal path, with ESR and a simple dielectric relaxation branch; the dielectric model needs hardware calibration. Bias and transistor parameters are provisional. The clean stage is intended to be measured at +4 dBu nominal line level with 10 kΩ and 600 Ω loads, and tested at increased levels and changed coupling capacitance. It does not claim a validated +24 dBu clean headroom on ±15 V rails.

The C4-style dynamic low-mid circuit, selectable RC sidechain HPF, and LED/LDR optical compressor are separate future circuit modules. Do not mistake the current `processOpticalOutput` integration point for a modeled optical gain cell.

## 2N5088 / 2N5087 output stage and load switch

Option C's `DiscreteOutputDriver.hpp` now has a **600-ohm / 10-kohm load
selector** (`setOutputLoadOhms(600)` / `setOutputLoadOhms(10000)`).
The load affects the KCL of the emitter followers, the current demand, and the
output electrolytic coupling-cap voltage/phase. The default is 10 kohm.
It does **not** select between two fabricated harmonic curves.

The transistor pair is 2N5088 (NPN) and 2N5087 (PNP), one each **per
channel**, in a complementary push-pull emitter-follower configuration.
The preliminary stage uses +/-15 V rails, 22-ohm emitter resistors,
47-uF input and 100-uF output coupling caps, source/load impedances,
series ESR, and a simplified dielectric-relaxation branch.

`2N5088_2N5087_models.lib` contains complete published SPICE cards
collected from the KiCad SPICE library (NSC-attributed 2N5088 and
Fairchild-attributed 2N5087). See:
https://github.com/kicad-spice-library/KiCad-Spice-Library/blob/master/Models/Transistor/BJT/BJT.lib
These are **not device-characterized measurements of the particular
transistors in this hypothetical circuit**.

`option_c_reference.cir` is the **ngspice** transistor-level reference
with both loads. Its base bias is currently idealized (+/-0.73 V) for
comparison; it does not yet include a complete thermal-bias circuit.
The *real-time* C++ processing uses Is, BF, IKF, VAF and some parasitic
resistances from the cards but omits complete Gummel-Poon reverse-mode,
charge-storage and depletion-capacitance dynamics. Accordingly it MUST
NOT be called full-SPICE-accurate or manufacturer-validated. Next step:
compare 600/10k sweeps against the SPICE reference, then fit and test
the BJT nodal solver at +4, +12, +18, +22 dBu and 20 Hz–20 kHz,
including device bias/collector-current limits and thermal behavior.
The selected load is an **internal modeled impedance**; it does not
change physical interface impedance at the DAW's hardware output.
