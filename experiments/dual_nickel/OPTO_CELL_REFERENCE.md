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

## Cell-origin distortion and even-order harmonic caveat

The PerkinElmer photocell catalog states that photocell voltage
dependence produces distortion, and with simultaneous DC and AC cell
voltage both odd and even harmonics can appear:
https://www.alldatasheet.com/html-pdf/84387/PERKINELMER/VTL5C1/25005/41/VTL5C1.html

The signal-path LDR is nominally unbiased in Nickel Opto. It is
incorrect to claim a large inherent K2 with no voltage asymmetry or
DC bias measured across that cell. The modeled shunt conduction is
I = G(light)*V*(1+a*(V/10)^2), solved in node KCL, with provisional
a=0.002. This is an **unvalidated low-level placeholder** that makes
the photocell non-ohmic while preserving zero-bias inversion symmetry.
It creates K3 at zero bias; K2 can occur when DC is present across
that same nonlinear material. We do NOT add an unphysical DC voltage
or a fixed K2 synthesizer. Exact harmonic strengths of NSL-32,
VTL5C1, VTL5C2 and T4B require measured cell voltage-vs-current and
bias curves; this model is not yet a calibrated match.

The `measure_all_opto_cells.cpp` bench reports device resistance,
compression knee and local ratios, 63% optical GR attack, 50% and
10% optical GR recovery, and K2-K10 with zero and 2V DC bias. For
T4B, this harmonic bench includes EL modulation and should not be
misread as a measurement of intrinsic dark CdS only.

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
