# Output load, high-frequency response and stability baseline
**2026-11-24 | Diagnostics only; no DSP constants or circuit parts changed.**

The product topology is nickel *input* transformer -> optical shunt ->
2N5088/2N5087 emitter-follower makeup/output buffer. The 600/10k switch
changes the **transistor amplifier output termination**. Unlike the user's
Neve 33609JD/LO1166-style reference, this is **not a selectable output
transformer termination**. Consequently a 10k-vs-600 difference in
transformer secondary damping cannot arise from the present routing.

## Direct stage measurements (44.1k sample rate)

Source: `measure_stage_frequency_stability.cpp`, 0.2s coherent sine
windows, 0.2s settling; input at -18/+4/+12/+18 dBu, two loads,
stages input-only/output-only/full-chain with optical threshold at +18 dB
for nominally uncompressed tests. Each stage normalized to its 1 kHz
fundamental amplitude.

**At +4 dBu:**

| Stage | 10k: 10kHz / 20kHz (dB rel. 1kHz) | 600: 10kHz / 20kHz |
|---|---|---|
| Input nickel transformer | -0.0054 / -0.0137 | same (output switch not in its path) |
| Isolated 2N5088/5087 BJT | ~0 / -0.00016 | ~0 / -0.00019 |
| Complete circuit | -0.00547 / -0.01385 | -0.00546 / -0.01386 |

The **absolute gain** of the isolated BJT at +4 dBu/1kHz was
-0.03765 dB (10k) and -0.28062 dB (600), a -0.243 dB difference,
not a high-frequency tilt. At +18 dBu these were -0.03770 and
-0.34412 dB (difference -0.306 dB). Thus heavier loads change
the level and nonlinear current demand with drive, not the treble balance.

The transformer measured approximately -0.70 dB at 20 Hz vs 1 kHz
at +4 dBu, but was essentially flat through 20 kHz.
Pushing it to +18 dBu did **not** develop a measurable additional
high-frequency attenuation in this model.

## Ultrasonic sweep (96k sample rate, through 45kHz)

Source: `measure_ultrasonic_response.cpp`, coherent 0.1s windows,
+4/+18 dBu, three stages, both loads, 1k/10k/20k/30k/40k/45kHz.

| Stage, +4 dBu | 20kHz | 40kHz | 45kHz |
|---|---:|---:|---:|
| Nickel input (identical loads) | -0.011 dB | -0.030 dB | -0.032 dB |
| BJT, 10k | ~0 dB | ~0 dB | ~0 dB |
| BJT, 600 | ~0 dB | ~0 dB | ~0 dB |
| Full path, 10k | -0.011 dB | -0.030 dB | -0.032 dB |
| Full path, 600 | -0.011 dB | -0.030 dB | -0.032 dB |

At +18 dBu, complete chain shows only about +0.011 dB at 10 kHz
and -0.036 dB at 40–45 kHz. No modeled HF transformer-style
resonant peak appears up to 45kHz at 96kHz sampling.

## Finite-time stability and important qualifications

Source: same 44.1k diagnostics; direct impulse, 100ms DC step,
18kHz and 100Hz bursts at +4/+18 dBu; observed one-second runs.
- Every measured output remained finite.
- Isolated BJT MNA reported **zero** convergence failures.
- No persistent output after the 18kHz tone was removed.
- The 10k BJT's DC-step test retained a substantial **decaying,
  low-frequency** tail: +18 dBu, peak after 0.5s ~0.043 normalized
  (RMS ~0.0327). The 600-ohm setting had only 0.00045 peak.
  This reflects the *lightly loaded long RC coupling network*
  under a hard DC step and is **not evidence of sustained HF
  oscillation**. It deserves a separate time-constant check in the
  future. The complete path's 10k after-step tail was lower at ~0.00378.
- A discrete model with no cable capacitance and limited base/collector
  parasitic/topological detail **cannot establish** hardware stability
  for all reactive loads. No >45kHz test performed.

## Design interpretation

Transformer winding leakage L, distributed/shunt C and output loading
produce a resonant Q, so genuine lightly loaded output transformer
high-frequency peaking (and 600-ohm damping) is physically plausible.
The present circuit **has no output transformer**, and its output
BJT is a low-impedance emitter follower. Adding a digital high-shelf
because the Neve transformer did something similar would not be
faithful to this topology. Any future HF difference must be justified
by a modeled **physical** frequency-dependent output/termination
network with validated damping and phase/step response, not an
invented response curve.

References:
- Douglas Self, *The Design of Active Crossovers*, transformer
  termination discussion, Fig 17.5 (unloaded vs 10k/600 peaking)
- https://www.sound-au.com/articles/audio-xfmrs.htm
- https://groupdiy.com/threads/transformer-coupled-preamp-over-undershoots-square-wav.2050/

CI measurements:
https://github.com/jerrykilgore8995-dotcom/609/actions/runs/45353251412
https://github.com/jerrykilgore8995-dotcom/609/actions/runs/45353899423
