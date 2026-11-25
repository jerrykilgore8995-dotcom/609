# Nickel Opto — selectable optical cells and fidelity notes

This implementation has four operational choices in the VST3 dropdown:
**NSL-32** (default), **VTL5C2**, **T4B-style**, and **VTL5C1**.
The VTL5C1 was added on request after an investigation of fast CdS recovery. All options
attenuate the same modeled resistor-network audio node: they differ in
how they generate illumination, translate illumination to resistance,
and retain photo-carrier history.

## VTL5C1 (Excelitas / PerkinElmer)
Published 25 C points: 1mA = 20 kohm, 10mA = 600 ohm,
40mA = 200 ohm, dark resistance 50 Mohm, 2.5ms turn-on (63%),
35ms maximum to 100 kohm after the LED is switched off.
Reference: https://www.farnell.com/datasheets/87223.pdf

The model uses the resistance/current anchors with log interpolation,
its own light and trap states, and a fast recovery calibrated to the
100-kohm endpoint rather than treating 35ms as an exponential time
constant. The user-adjustable detector RC attack (10-100ms) and release
(50-2000ms) operate **only** in VTL5C1 mode. The other LED cartridges
select fixed 15ms/250ms detector resistors; actual optical GR onset
and recovery remain program- and cell-dependent. T4B is a separate
AC EL panel with its own dynamics.

## Photocell harmonic distortion

PerkinElmer's Figure 3 describes a residual second harmonic and suggests
a possible crossover effect near the origin of the photocell I–V curve.
Higher AC voltage can produce primarily odd orders. DC bias can increase
even-order distortion. [PerkinElmer optical catalog, pp. 35–36](https://ka-electronics.com/images/pdf/Perkin_Elmer_Photocell_Vactrol_Catalog.pdf)

The current Mk II shunt solve includes a symmetric film term plus a small
polarity-dependent photocurrent. At low voltage the contact term varies
quadratically with voltage and at higher voltage the polarity mismatch
approaches a bounded fraction of the photocurrent. At zero light the
contact term vanishes. The provisional asymmetry and crossover voltage
are shared across the four cartridges. They need separate fixed-light
bipolar I–V measurements for individual calibration.

The Mk II regression measures K2 and K4 with fixed illumination and no
input DC. Reversing the modeled contact mismatch reverses the phase of
K2. The legacy cartridge bench also measures how adding DC changes the
harmonics.

## VTL5C2 (Excelitas/PerkinElmer)

Manufacturer measurements at 25 C:
- 1 mA LED -> **5.5 kohm** typical cell resistance
- 10 mA LED -> **800 ohm**
- 40 mA LED -> **200 ohm**
- off resistance **>= 1 Mohm at 10 seconds**
- turn-on to 63% final resistance **3.5 ms** typical
- turn-off to 100 kohm **500 ms** maximum
- 5 pF cell output capacitance, 0.5 pF input/output coupling capacitance
- Low temperature coefficient, described as a *small light-history memory*.

Source: https://www.tme.eu/Document/80c214be211c02af0aa7b151bc4737ea/VTL5C1.pdf
Supplement: https://ka-electronics.com/images/pdf/Perkin_Elmer_Photocell_Vactrol_Catalog.pdf

We interpolate **conductance** through the published steady-state
resistance/current anchors using log-log interpolation; below 1 mA
we extrapolate toward darkness. This is intentionally NOT a linear
current-to-resistance or a generic change of attack/release coefficients.
There are separate fast carrier and small slow-trap populations; their
time constants must be calibrated further using the manufacturer's
response-time curves and individually measured units. The audio LDR node includes a 5pF VTL5C2 capacitance branch. The LED drive uses its
40 mA published limit.

## T4B-style (LA-2A family, not a Vactrol)

A T4 uses an **electroluminescent (EL) panel** and *two* CdS cells
illuminated by it (one for audio attenuation, one for GR metering).
It does **not** have a current-driven LED that can be exchanged with
a VTL5C2. The original LA-2A uses a 12AX7 control amplifier and a
6AQ5 high-voltage EL driver and supports program-dependent reduction.

References:
- https://help.uaudio.com/hc/en-us/articles/215779663-A-History-of-the-Teletronix-LA-2A-Leveling-Amplifier
- https://media.uaudio.com/assetlibrary/l/a/la-2a_manual.pdf
- https://thehistoryofrecording.com/Manuals/UREI/Urei%20LA-2A%20manual.pdf
- https://igsaudio.com/t4bx/

Current **T4B-style engineering prototype** implements:
- A separately routed **AC sidechain signal** feeding an EL panel
  electrical RC and high-voltage driver approximation, instead of
  routing rectified DC through an LED Shockley-current light source
- An approximate nonlinear AC voltage-to-phosphor brightness response
  with activation voltage and separate light rise/fall states
- Two separate CdS illumination/memory states, one for audio
  attenuation and one for the simulated GR photocell
- A fast first-stage recovery and slower CdS trap occupancy for
  short versus prolonged program, rather than a fixed digital
  release applied after the audio.

**Important limits**: the panel drive gain, voltage threshold,
CDS resistance-versus-EL-voltage characteristic, phosphor transient
and meter cell parameters are NOT yet grounded in a manufacturer
T4B voltage sweep, and this architecture uses an independent AC sidechain path
instead of Nickel Opto's LED diode-bridge detector. Consequently the
T4B-style attack/release settings are governed predominantly by the
EL/CDS physics, rather than falsely pretending that the old LED
detector RC can command any T4B recovery time.
It is not an exact LA-2A/T4B clone and does NOT claim the original
6AQ5/12AX7 tube driver, the T4 internal CdS material, matched
two-cell metering calibration, EL aging, or temperature response.
A true high-fidelity T4B match requires representative measured
panel excitation and both photocell responses for at least
short and sustained bursts, across temperature and level.
The T4B-style switch is an independently modeled initial architecture,
not a guessed resistor preset masquerading as a measured T4.

## Validation and integration

- `OpticalCell.hpp` owns the selectable cell physics.
- `AnalogOptoProcessor.hpp` wires a shared selection into mono and
  stereo optical engines. The change resets the cell state, avoiding
  leftover light-memory from another optical material.
- `juce_plugin/Plugin.cpp` owns the automation-capable dropdown.
  The current generic GUI is retained pending the user's preferred physical
  faceplate. The fast-cell attack/release knobs are disabled for other cells.
- `test_opto_variants.cpp` checks datasheet VTL5C2 points and
  its 40mA/500ms turn-off limit, a **120 Vrms AC / 1kHz** T4 panel
  turn-on/recovery and second photocell, and real runtime switching.
- Existing NSL-32 reference and 600/10k transistor/audio tests are
  unchanged and remain regression gates.

**Calibration to do**: run separate cell attack/release burst sweeps
for each input level and both feedback/feedforward settings, fit
published manufacturer time-response graphs and (for the T4)
reference optical assembly measurements. Quantify actual
compression ratio/threshold, GR overshoot, frequency response,
IMD and CPU, including source/sample-rate dependence. Do not
mistake nominal knob values for measured optical time constants.

## November 2026: sidechain high-frequency diagnosis

A shared digital implementation fault mixed the current input audio sample with
the *previous* post-attenuator output sample. At 20 kHz / 44.1 kHz, that
misaligned feedback sum produced severe phase cancellation, despite the
2N5088 follower and bridge detector being nearly flat in isolation.

Feedback is now evaluated using current input audio and the most recently
solved CdS attenuation (a slowly evolving physical state), in mono and
stereo. The optocell control remains causal without delaying the AUDIO
waveform through a non-existent physical one-sample path. The measured LED
feedback-mode optical GR at 1 kHz vs 20 kHz is within approximately 0.001 dB
at 44.1 kHz (+12 dBu isolated tests), versus several dB before.

T4B-style EL panel behavior was independently affected by a provisional
47-kohm driver source resistance feeding 5 nF EL capacitance (677 Hz
low-pass corner), which eliminated virtually all 20 kHz illumination.
A 100-ohm *effective* EL driver output impedance with voltage gain 24.5
replaces that speculative source value. These are fitted effective
circuit parameters, NOT claimed hardware component values.
This preserves 1 kHz optical GR while giving approximately 0.27 dB lower
20 kHz GR in high-level T4B feedback, and approximately 0.97 dB lower
20 kHz GR in T4B feedforward. The LA-2A factory R37 flat-sidechain
setting is the general response target, but T4B model fidelity still
requires measured EL illumination versus amplitude and frequency.

Sources:
- https://help.uaudio.com/hc/en-us/articles/4419496124180-Teletronix-LA-2A-Leveler-Collection-Manual
- https://www.uaudio.com/blogs/ua/la-2a-collection-tips-tricks

The standalone `measure_sidechain_20khz.cpp` verifies the transistor
buffer and diode storage at 44.1/96 kHz. The `measure_feedback_phase.cpp`
runs **all four cells** in both feedback/feedforward modes at 44.1/96 kHz
and tracks 1k/5k/10k/15k/20k gain reduction at two audio levels.
The `measure_full_path_grid.cpp` includes the actual transformer and
output BJT, plus frequency response, phase and THD. T4B device timing
regression now preserves the same panel excitation after drive-impedance
changes instead of confusing a stronger panel exposure with photocell speed.

