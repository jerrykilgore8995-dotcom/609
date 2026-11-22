# MKIV 33609JD — multi-trim calibration experiment

**Status:** Measurements only; working MKIV remains unchanged. All experiments use genuine SPICE netlist components compiled into the nodal VST3 and are isolated to the GitHub Actions runner.

## Critical correction: compressor distortion fixture

The earlier broad test labeled `comp6+9` as an 800 ms recovery THD measurement **but actually invoked compressor recovery detent 0 (100 ms)**. It produced 0.490675% THD at 200 Ω. That should **not be compared to Neve's 0.20% compressor THD limit at 800 ms recovery**.

The corrected separate 200 Ω test uses 1kHz, ratio 6:1, CT −18 dBu, **800 ms** recovery, +20 dB makeup, +9 dBu input; it measures **0.0690172% THD**, so it passes the 0.20% maximum at this input level. The handbook's compressor THD line does not specify input level. The limiter at +22 dBu and +8 dBu threshold, 800ms recovery, measures **0.5205246%** versus the 0.45% reference, still a **proxy** because the handbook prints −18 dBu threshold for the limiter even though its rotary positions span +4 to +15 dBu.

## Actual physical trim positions and their MKIV equivalents

| Component | VST3 netlist variable | Existing baseline | Role |
|---|---|---:|---|
| EX11475 RV1 (5k) | PRESET | 0.226248 (22.6248%) | Audio path gain (also compressor sidechain drive) |
| EX11475 RV2 (5k) | LAW | 0.35 (1.75kΩ) original, **0.04 (200Ω)** candidate | Shared diode bridge control law |
| EX11475 RV3 (10k) | MTR | 0.5 | Meter indication/calibration |
| EX11475 RV4 (10k) | LIMCAL | 0.166966 (1.66966kΩ) | Limiter threshold and sidechain sensitivity |
| EX11475 RV5 (50k) | COMPCAL | 0.4073 (20.365kΩ) | Compressor sidechain threshold/sensitivity |
| EX10340C RV1 (5k) | BIAS | 0.445245 | Audio amplifier output bias/symmetry |
| PSU RV1 | fixed 24V supply | not exposed | Hardware regulator, not a VST3 calibration pot |

Neve handbook identifies RV1 audio unity, RV2 law, RV3 meter, RV4 limiter +8dBu alignment, and RV5 on the board drawing; the exact independent sidechain effect of RV5 is taken from the MKIV circuit.

## Independent 11-variant sensitivity screen — ALL VST3 builds successful

All variants hold RV2=200Ω (`LAW=0.04`), using Neve's 1kHz, threshold −20 dBu, compressor 100ms, 0→+10 dBu factory ratio procedure. 1kHz compressor THD uses *correct* 800ms recovery, 6:1, −18dBu threshold, +20dB makeup, +9 dBu input. Limiter THD uses +22dBu input, 800ms recovery, +8dBu limiter threshold.

| Trial | Changed netlist trim | All 5 comp ratio tests | Average abs ratio output-step error | Comp THD % | Limiter THD % | Limiter +8 set at +20 input (dBu) |
|---|---|---|---:|---:|---:|---:|
| baseline | unchanged others | 5/5 | 0.13210 dB | 0.06902 | 0.52052 | +8.0246 |
| RV1 lower | PRESET .2000 | 5/5 | 0.12574 | 0.06912 | 0.54100 | +8.0278 |
| RV1 higher | PRESET .2525 | 5/5 | 0.13843 | 0.06890 | 0.50075 | +8.0294 |
| amplifier bias lower | BIAS .395245 | 5/5 | 0.13217 | 0.06954 | 0.52042 | +8.0291 |
| amplifier bias higher | BIAS .495245 | 5/5 | 0.13212 | 0.06879 | 0.52061 | +8.0224 |
| compressor calibration lower | COMPCAL .3273 | 5/5 | 0.12112 | 0.06900 | 0.52056 | +8.0327 |
| compressor calibration higher | COMPCAL .4873 | 5/5 | 0.17130 | 0.06892 | 0.52051 | +8.0290 |
| limiter sensitivity lower | LIMCAL .126966 | 5/5 | 0.13219 | 0.06902 | 0.48206 | +7.7519 |
| limiter sensitivity higher | LIMCAL .206966 | 5/5 | 0.13211 | 0.06902 | 0.55817 | +8.2812 |
| meter lower | MTR .3 | 5/5 | 0.13230 | 0.06906 | 0.52062 | +8.0281 |
| meter higher | MTR .7 | 5/5 | 0.13200 | 0.06896 | 0.52038 | +8.0242 |

**Findings:** none of these **individual** trims achieves ≤0.45% limiter THD at the tested proxy setting. RV4 can reduce THD at the expense of incorrect limiter output calibration, and the meter trim does not materially influence either. RV1 audio gain is the most promising additional calibration direction: raising PRESET from .226248 to .2525 raises the through-path output gain by approximately +0.140 dB and reduces limiter THD by ~0.020 percentage points, while preserving all ratio test passes. The original through path at 1kHz was approximately −0.609 dB relative to input, so raising RV1 offers a plausible route toward unity with improved THD.

## 200–300Ω RV2 + RV1 combined optimization

Running workflow [MKIV joint RV2 RV1 gain and limiter optimization](https://github.com/jerrykilgore8995-dotcom/609/actions/runs/45019567905) tests RV2 at 200, 250, and 300Ω combined with the RV1 audio gain preset near unity, varying limiter and compressor sensitivity separately. Do not assume any combination meets all factory specs until its full suite is measured.

## Baseline constraints and coverage

- Factory compressor 0→10 dBu: **5/5 pass** at 200Ω.
- Factory limiter +10→20 dBu test: 200Ω produces ~0.005dB output rise at limiter +8dBu threshold (within published 0.0–0.2dB range, though near lower edge).
- 200Ω limiter +8dBu output with +20dBu drive: +8.025 dBu, near perfect.
- In previous 776-condition/setting VST3 sweeps of original and 200Ω, compressor ratio response, limiter threshold all detents, measured active tone phase, THD at a wide set of detents, gain range, auto/fixed recovery and both-sections mode were recorded.
- RV2=200Ω accelerates some **output gain** limiter release 15–28% relative to original RV2. Those are not direct control-voltage factory time constants.
- Supply noise remains unchanged under the internal noise probe; that is **not** the manual's 600Ω terminated full-band output noise.
- Genuine amplifier standalone 75Ω-loaded +18dBu power/clipping and THD specifications, amplifier DC operating point, physical power supply alignment, and factory meter needle calibration cannot be certified solely by a packed VST3 tone host.

**No definitive joint 'all Neve specs pass' claim until all relevant tests and exact operating conditions pass.**
