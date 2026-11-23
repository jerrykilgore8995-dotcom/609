# Dual-Nickel / Zener Limiter – isolated prototype

This branch is **not the existing 609 compressor**. The original native 33609/JD engine and Plugin.cpp are left unchanged.

## Committed signal-flow design
Input drive -> 1:1 N49 49%-nickel-alloy CHARACTER transformer -> receiver/termination -> **8:1 zener limiter (to be implemented)** -> independent interstage/makeup gain -> 1:1 Jensen JT-11-BMCF-inspired clean output transformer -> clean final trim.

The input drive defaults to 0 dB, range -12..+18 dB. The final trim links at **-inputDrive** by default, and may be unlinked. This trim is deliberately after the output transformer; the interstage/makeup control drives the output transformer independently. The future limiter will offer 1/8/15 ms attack (8 default), and 50/100/250/500/1000 ms recovery (100 default), 8:1 fixed. Meter will ascend from 0 to 20 dB GR.

## Present code and limits
`NickelTransformer.hpp` is an initial real-time-safe **reduced nonlinear coupled magnetizing-branch prototype** (backward-Euler nonlinear current/KCL solve). It establishes the two transformers' distinct operating behavior and the requested routing and gain linking. It is **not** a complete WDF / Jiles-Atherton implementation and **not** a Jensen or N49 hardware-accurate emulation. Magnetic constants, winding values and saturation current are provisional engineering guesses.

TODO before claiming analog accuracy:
1. Implement physically parameterized Jiles-Atherton magnetic field, remanence and minor loops with stable implicit solution; couple it into WDF or validated equivalent MNA.
2. Add winding copper resistance by winding, leakage inductances, interwinding capacitance, finite core-loss branch, source and load, optional damped Zobel.
3. Fit N49 magnetic curves against 49% nickel material data and ultimately a measured 1:1 hardware reference. Do not conflate 49% nickel alloy with nickel-steel laminate blend.
4. Parameterize Jensen JT-11-BMCF from documented measurements and verify LF THD, phase, HF FR and load dependency.
5. Implement the distinct zener control circuit, 8:1 knee/ratio, calibrated attack and release and 0–20 dB GR meter.
6. Add the new JUCE plugin target and UI without altering the 609 plugin; 2x/4x oversampling and anti-aliasing plus automated tests at 44.1 kHz.
7. Check gain linking under limiting and makeup-drive; compensated drive does not guarantee constant perceived loudness when saturation or dynamics change.

## Standalone smoke test
Compile with C++20: `c++ -std=c++20 -O2 -I. test_nickel.cpp -o test_nickel && ./test_nickel`

This checks for finite output on a 100 Hz, 44.1 kHz test tone only — not calibration.
