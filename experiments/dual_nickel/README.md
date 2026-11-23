# Dual Nickel Zener — magnetic circuit development

**This branch does NOT modify the existing 609 plugin.** The experimental transformer
module is separate. The earlier polynomial/nonlinear-inductor placeholder has been
replaced by an actual **stateful Jiles–Atherton equation** (anhysteretic Langevin,
irreversible domain-wall motion, reversible magnetisation, direction dependence).
A bounded implicit numerical solve couples it to driver source resistance and
the reflected secondary load. A **real chowdsp_wdf** resistor-capacitor network
handles downstream winding/capacitive parasitic filtering.

The magnetic and WDF sections are **not yet a single fully coupled multiwinding
WDF transformer**; that remains future engineering work. There are no claims of
physical transformer accuracy, and there is not yet a limiter or compiled VST3.
No hardware measurements or distortion validation have been run.

## Signal chain
Input drive -> 49%-nickel alloy-inspired nonlinear magnetic model ->
source/load coupling -> WDF parasitic section -> [8:1 zener limiter TODO] ->
independent interstage/makeup gain -> high-headroom 80%-nickel Jensen-inspired
magnetic model -> WDF parasitic section -> clean final trim.

The linked compensation is AFTER the output transformer. Set linked=false to
disable automatic -inputDrive compensation. Default linked=true.

## Dependencies / build
This is a standalone test project, separate from the existing JUCE CMake target.
CMake 3.22+, a C++20 compiler, and Git access for chowdsp_wdf are needed:

```
cmake -S experiments/dual_nickel -B build/dual-nickel -DCMAKE_BUILD_TYPE=Release
cmake --build build/dual-nickel --config Release
ctest --test-dir build/dual-nickel --output-on-failure
```

## Limitations requiring correction before production
1. Calibrate physical core parameters and dimensions, and correct input/output
   winding network (leakage, interwinding C, eddy-current loss, core loss).
2. Replace approximate fixed-point J-A state integration with converged stable
   integration and document rate-dependent/hysteretic minor-loop behavior.
3. Couple all magnetic and electrical WDF branches rather than using a downstream
   RC WDF section; evaluate passivity and CPU cost.
4. Pin chowdsp_wdf to tested tag/commit; check dependency headers/API.
5. Test against physical N49 and Jensen 1:1 data (THD K2–K20, IMD,
   FR, phase, burst response at 44.1/88.2 kHz).
6. Implement 8:1 Zener control circuit and calibrated 1/8/15ms attack,
   50/100/250/500/1000ms recovery, and ascending 0–20 dB GR meter.
7. Build a separate JUCE plugin rather than changing 609's native engine.

**No remote CI build or audio benchmark result exists for the new stage.**
