# Nickel Opto — research-grounded analog character work

## Status and fidelity boundaries
This note records circuit-design principles from original manufacturers and
actual measured behavior, not a claim to reverse-engineer other plugin source
code. Nickel Opto is an original 49%-nickel-inspired magnetic model, NSL-32
LED/CdS optical reference and +/-15 V 2N5088/2N5087 nodal BJT output.
Other optocell choices remain future separate measured device models.
No waveform "warmth" injection is authorized: harmonics, noise and phase
must follow nonlinear **electrical** circuit behavior and be validated.

## What well-regarded analog modeling teaches us

### Universal Audio LA-2A / T4
UA documents distinct initial and long-tail photoconductive recovery,
including approximately 60 ms for the first half of recovery and long
program-dependent seconds-long tails for the T4. Signal history matters.
This is a **T4 EL-panel assembly**, not an interchangeable LED vactrol:
a future T4 mode requires a new driver/light source circuit, not just
substituting time constants in the NSL-32.

https://help.uaudio.com/hc/en-us/articles/215779663-A-History-of-the-Teletronix-LA-2A-Leveling-Amplifier
https://help.uaudio.com/hc/en-us/articles/4419496124180-Teletronix-LA-2A-Leveler-Collection-Manual

### Softube Tube-Tech CL 1B
The manufacturer's manual describes a combined attack/release mode in which
short peaks recover differently from sustained compression. This reinforces
the value of separate light/recovery states and a real sidechain network.
It does **not** justify implementing an arbitrary programmable release
envelope after the optical stage.

https://www.softube.com/eu/user-manuals/tube-tech-cl-1b-and-cl-1b-mk-ii-compressors

### Kazrog True Iron
Models multiple transformers and makes virtual input impedance an important
variable, with adjustable oversampling. This supports our drive/load
characterization and 44.1/96 kHz aliasing measurements. The model's
harmonics should follow magnetic current, frequency and load, not fixed
K2/K3 amplitudes added after a clean signal.

https://kazrog.com/collections/audio-plugins/products/true-iron
https://www.barryrudolph.com/recall/manuals/kazrog_true_iron.pdf

### Klanghelm MJUC / Softube FET Compressor
Klanghelm links saturation character and transformer/gain stages to
compression dynamics and gain staging. Softube notes that circuit quirks
and non-ideal hardware behavior contribute to audible character. Neither
is a reason to add noise or artificial saturation in the absence of
electrical/measurement evidence.

https://klanghelm.com/contents/products/MJUC.html
https://www.softube.com/user-manuals/fet-compressor

## Current NSL-32 reference and calibration method
Actual NSL-32 published conditions:
- Typical LED/CdS resistance: 500 ohms at 20 mA
- 3.5 ms to 63% of illuminated conductance, LED at 16 mA
- About 500 ms to 100 kohms after LED removed following 16 mA
- >=500 kohms OFF resistance 10 seconds after LED shutoff.

Datasheet:
https://www.advancedphotonix.com/media/pages/our-products/optocouplers-optoisolators/nsl-32/25647b03b2-1718287446/ds-nsl-32.pdf

A minimum dark resistance is not the dark asymptote. The **reference
LdrCell-only test** bypasses the compressor's BJT LED current limiter so
it can inject exactly 16 mA, then measures rise and recovery electrically.

The new model uses two conductance populations:
1. A fast photoconductive population with ~3.5 ms rise and
   calibrated ~85 ms initial dark decay
2. A low-amplitude *trap* population that needs ~700 ms of
   illumination to charge and ~1.5 s to recover.

The trap contribution is ~0.5% of the full-light conductance. Its values
are fitted approximations, *not uniquely identifiable from the NSL-32
datasheet alone*. Compare 150/500/1200/5000 ms exposure at 4/8/16 mA
and explicitly record resistance at 100/500/1000/10000 ms after shutoff.
The compressor burst test also compares GR tails following 150 and
1200 ms musical bursts.

## Current sound-engineering priorities

1. **Program dependence without pumping**: preserve the two-population
   light-trap memory, and inspect short and long notes at equivalent GR.
2. **Dynamic operating point**: retain authentic post-LDR feedback by default;
   selectable feedforward uses its own real input resistor pad and
   diode-feedback knee. Check burst overshoot and level-vs-GR ratios.
3. **Magnetic color**: validate frequency-, source-load- and drive-dependent
   K2–K20 and intermodulation; 100 Hz and 1 kHz need not saturate equally.
4. **Output electronics**: compare 600-ohm versus 10k loads using the
   same nodal 2N5088/2N5087 BJT, caps, resistor and rail equations.
5. **Linear parasitics**: verify large coupling capacitor ESR, effective
   high-pass corner, and phase response at low frequencies, not
   arbitrary all-pass or post-EQ correction.
6. **Nonlinearity aliasing**: analyze 44.1k/96k oversampling and IMD.
   K3 at 10 kHz is above Nyquist at 44.1k, so a K2-only FFT does
   not prove that high-level aliases are harmless.
7. **Practical mixes**: bass long notes, 100–250Hz low-end, 120–200Hz
   palm-muted bursts; measure stability of stereo linked gain and low-mid
   phase reconstruction.
8. **Real-time budget**: SER8/Reaper at 44.1kHz, 512 samples = 11.61ms
   maximum block deadline. Linux average CPU benchmarks do not
   establish Windows worst-case CPU.

Only adopt a tonal or timing change after an A/B measurement with the
original circuit held constant. Keep the original 609 mainline untouched.
