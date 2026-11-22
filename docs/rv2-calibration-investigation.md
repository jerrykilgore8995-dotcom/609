# MKIV nodal RV2 law-correction calibration — investigation only

**Status:** Research and measurements only. No change to the working MKIV's DSP, original `circuit/609.cir`, solver constants, or released plugin. All trials were generated in temporary GitHub Actions jobs on this separate branch.

## Circuit trace and identification

- `circuit/609.cir` defines `.param ... LAW=0.35` and passes `LAW` to both `CH609` channels.
- `circuit/ex11475.inc` defines `RV2 LAW_TOP VCTRL {5000*LAW+0.001}`: a **5 kΩ law-correction adjustment**.
- Associated series/nonlinear components: R36 1 kΩ, R45 220 Ω, R29 3.3 kΩ, and D10 1N4148. Control voltage is downstream of the compressor's TR9 and merges with limiter control via the shared bridge driver.
- **Original nodal value** `LAW=0.35` = **1,750.001 Ω**.
- RV2 is **baked into the generated nodal Deck.hpp** by `native/compile_deck.py`, not exposed as a front-panel or runtime plugin knob.
- Neve's 33609/JD handbook, compressor section, identifies RV2/R36/R45/R29/D10 as the *law correction characteristic* for driving the gain control diode bridge. The **selected ratio** is established upstream by D12, D3–D6, R26 and the compressor ratio switch. Thus RV2 adjusts the shared compression transfer characteristic and need not adjust each nominal ratio independently.
- A separate model resistor `RV5 QC15C R1_IN {50000*COMPCAL+0.001}` is in the compressor sidechain; existing `COMPCAL=0.4073` corresponds to **20,365 Ω** and was **not adjusted** in this investigation.

Source: https://thehistoryofrecording.com/Manuals/Neve/Neve%2033609jd.pdf and repository schematic.

## Factory test methodology

Each LAW trial: temporarily replace both `LAW=0.35` occurrences in the **workflow checkout only**, run `tools/native_seed.py` with ngspice, regenerate `Deck.hpp` from the updated component-level netlist using `native/compile_deck.py`, build the Windows MKIV VST3 and its Vst3Smoke measurement host, then capture 1 kHz steady-state spectra through the **actual built VST3**. Input levels: **0 and +10 dBu**, compressor threshold **−20 dBu**, compressor recovery **100 ms**, 0 dB makeup, limiter disengaged, sample rate **44.1 kHz**, 1.2 s settled tone, five ratio positions. Output increase is the delta in fundamental amplitude (dB), compared to Neve's factory targets. Only RV2 changed.

The original **1,750 Ω** trial reproduced the existing MKIV ratio results (8.62, 6.08, 3.91, 3.11, 2.11 dB), validating baseline comparability.

## Complete nodal VST3 measurements

All figures below are **output change (dB) for the 0→+10 dBu input step**:

| RV2 (Ω) | LAW | 1.5:1 | 2:1 | 3:1 | 4:1 | 6:1 | Factory passes / 5 |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 0 | 0.00 | 5.66 | 4.65 | 3.23 | 2.54 | 1.59 | 5 |
| **200** | **0.04** | **6.56** | **5.16** | **3.46** | **2.69** | **1.70** | **5** |
| 350 | 0.07 | 7.04 | 5.39 | 3.55 | 2.76 | 1.75 | 5 |
| 500 | 0.10 | 7.39 | 5.54 | 3.61 | 2.82 | 1.81 | 5 |
| 600 | 0.12 | 7.57 | 5.62 | 3.64 | 2.85 | 1.84 | 4 |
| 750 | 0.15 | 7.81 | 5.71 | 3.69 | 2.89 | 1.88 | 4 |
| **1750 (original)** | **0.35** | **8.62** | **6.08** | **3.91** | **3.11** | **2.11** | **1** |
| 2750 | 0.55 | 8.98 | 6.39 | 4.14 | 3.31 | 2.29 | 1 |
| 3750 | 0.75 | 9.19 | 6.69 | 4.36 | 3.50 | 2.44 | 1 |
| 4750 | 0.95 | 9.32 | 6.96 | 4.57 | 3.67 | 2.58 | 0 |
| Factory nominal | — | 6.50 | 5.00 | 3.50 | 2.50 | 1.50 | — |
| Factory tolerance | — | ±1.00 | ±1.00 | ±1.00 | ±0.50 | ±0.50 | — |

**Note:** the final row of the handbook sometimes prints 5:1 despite the actual switch and later descriptions identifying a 6:1 setting; the existing MKIV measurement fixture and user's project convention use 6:1 with the published 1.5±0.5 dB target.

## Best calibration among measured trials: **RV2 ≈ 200 Ω (LAW = 0.04)**

| Position | Nominal dB output rise | Measured with 200 Ω | Error from nominal | Factory result |
|---|---:|---:|---:|---|
| 1.5:1 | 6.50 | 6.56 | +0.06 dB | PASS |
| 2:1 | 5.00 | 5.16 | +0.16 dB | PASS |
| 3:1 | 3.50 | 3.46 | −0.04 dB | PASS |
| 4:1 | 2.50 | 2.69 | +0.19 dB | PASS |
| 6:1 | 1.50 | 1.70 | +0.20 dB | PASS |

At 200 Ω, all five are within factory tolerances, maximum absolute nominal error **0.20 dB**, mean absolute nominal error **0.13 dB**, versus approximately **0.97 dB** for the original 1750 Ω (approximately **87% reduction** in this five-point mean absolute dB error). Ratios are compared by manufacturer-measured output steps, **not** percent error in the nominal ratio itself.

- Near-zero RV2 (0 Ω) also passes all five positions, but puts the 1.5:1 result close to its **lower** tolerance limit (5.66 vs 5.50 dB), and is the end-stop of the physical trim.
- 350/500 Ω pass all five but have substantially larger errors at 1.5:1 and 2:1; 600 Ω already fails 1.5:1.
- 200 Ω is the **best among tested values** by the mean and maximum absolute deviation from the nominal factory targets. A finer test around 150–200 Ω could identify a slightly better mathematical optimum, but the 200 Ω trial already comfortably meets **every published factory tolerance**.

## Important limitations and safeguards

1. RV2 is part of the **shared compressor/limiter diode-bridge control**. A compressor-ratio pass **does not by itself prove** the limiter threshold, ratio, THD, gain reduction, or dynamics are unaffected. Those need to be checked before adopting RV2 as a permanent calibration. Test host includes limiter probes but this report draws no limiter conformance conclusion from them.
2. Changing RV2 affects static control-law behavior and may also change dynamic behavior at different input levels; measurements here prove the five **specific factory 0/+10 dBu tests**, not global accuracy across all thresholds, input levels, signal histories, or attack/recovery combinations.
3. Per source circuit, `COMPCAL`/RV5 is a separate compressor-sidechain adjustment not touched here. It might be useful if one later needs a compressor-only refinement rather than a shared-bridge correction.
4. **No working circuit or shipped plugin was modified.** Do **not** merge this investigation as a calibration change. The VST3 trial DLLs and generated decks were created solely inside GitHub Actions test jobs.

## Reproducibility

- Original broad sweep: https://github.com/jerrykilgore8995-dotcom/609/actions/runs/44979868306
- Refined 0–600 Ω sweep: https://github.com/jerrykilgore8995-dotcom/609/actions/runs/44980539331
- The workflow source is `.github/workflows/609-rv2-investigation.yml`, on `609-rv2-investigation-only`.
