# 609 JD timing bench

The [AMS Neve J/JD specification](https://manuals.plus/m/c8dd092f5ff2e928ebc6d1080d51aa86a61e311b21215b097e1eef9191f0231e.pdf) gives **3 ms** for the stock compressor and **2 ms fast / 4 ms normal** for the limiter. The 7.5 and 15 ms compressor options are extensions. In sessions with a saved attack detent, the stored index still selects the corresponding time.

The [JD technical handbook and switch drawings](https://www.thehistoryofrecording.com/Manuals/Neve/Neve%2033609jd.pdf) give two ways to characterize A1 and A2. The switch drawings give the nominal pairs **100 ms / 2 s** and **50 ms / 5 s** for both sections. The handbook gives single recovery figures under its bench conditions. The measured recovery from a burst depends on how long the second capacitor had to charge, its diode conduction state, and the control voltage at the new input level.

The SPICE reconstruction uses an effective R31 of 320 Ω for the 3 ms position. The EX11475 drawing specifies 560 Ω. The effective value corrects the attack of the reconstructed transistor and transformer models in the fixture. The limiter retains the R30 shunt in the drawing and the two measured charging resistances. The A1/A2 positions use 33/42 µF with 60/120 kΩ nominal long paths. This gives open-path products of 1.98 and 5.04 seconds before diode conduction and loading. The bounded engine preserves the compressor's feedback detector for fixed recovery and carries a separate voltage history for automatic compressor recovery. Limiter dynamics use a peak and smoothed detector with their own automatic history.

## Measurement

The 1 kHz stimulus uses the fixture's generator dBu convention. Compressor timing uses a −30 to +10 dBu step at a −20 dBu threshold and 4:1. Attack and recovery read 63.2% of the change in gain dB. The JD limiter fixture steps +10 to +20 dBu and back at the +8 dBu detent. Limiter attack reads when the output settles within 1 dB of its pre-step level. Limiter fixed recovery reads 90% of the change in the SPICE control voltage. The VST3 exposes audio, so its release is compared with the SPICE output gain trajectory. Three consecutive 1 ms cycles confirm release and limiter settling crossings.

| Attack setting | SPICE attack, ms | Loaded VST3, ms |
| --- | ---: | ---: |
| Compressor JD | 3.00 | 3.11 |
| Compressor 7.5 ms | 7.18 | 7.62 |
| Compressor 15 ms | 14.53 | 15.28 |
| Limiter fast | 1.97 | 2.29 |
| Limiter normal | 3.96 | 4.19 |

| Recovery position | SPICE, ms | Loaded VST3, ms |
| --- | ---: | ---: |
| Compressor 100 | 100.87 | 100.20 |
| Compressor 400 | 401.16 | 402.09 |
| Compressor 800 | 799.42 | 802.67 |
| Compressor 1500 | 1501.09 | 1502.42 |
| Limiter 50 | 24.50 | 23.55 |
| Limiter 100 | 49.56 | 48.45 |
| Limiter 200 | 99.24 | 98.23 |
| Limiter 800 | 399.29 | 397.74 |

The table uses 63.2% output gain recovery for both engines, with JD compressor attack and normal limiter attack. At the limiter's SPICE control node the 90% times were 50.08, 101.37, 203.20 and 816.19 ms. The stepped-sine numbers describe the stated fixture. Detector overdrive and recovery history can change the measured trajectory at other levels.

The published [33609/N drum-bus capture](https://www.amazona.de/test-ams-neve-33609-n-limiter-und-kompressor/) gives a separate program-material reference. On the 3:1, fast, 100 ms example, the bounded engine at about 6.3 dB peak reduction differed from the hardware envelope by 0.38 dB median and 1.39 dB at the 95th percentile in 10 ms RMS windows after median level matching. The hardware increased crest factor by 1.26 dB, versus 0.46 dB for the model. The clips are MP3s and the hardware converter calibration is unspecified.

The revised reference also retested the 1 kHz, +10 dBu compressor THD case at a 0 dBu threshold. The bounded model and SPICE agreed to within 1% relative across the five ratios in the local comparison. The old through-path SPICE result measured about -0.44 dB at 20 kHz relative to 1 kHz. The limiter's 0.45% factory THD limit is a ceiling. At the assumed +8 dBu detent, the original diode model in SPICE measures about 1% at the handbook's +22 dBu input. The loaded VST3 measured 0.399% at the equivalent +22 dBu input-terminal condition using the bounded limiter's existing spectral correction.

The linked-burst nodal comparison still exceeded its envelope limits on the revised compressor. The 95th-percentile window-level errors were 1.64 dB on the cymbal fixture and 3.55 dB on the low-frequency fixture in compressor-only mode. Limiter-only errors were 0.64 and 0.73 dB.

## Reproduce

```sh
python tools/jd_timing.py --mode comp --attack 0 --bus --output jd-spice-comp.json
python tools/jd_timing.py --mode limit --attack 1 --bus --output jd-spice-limit.json
c++ -O3 -std=c++20 native/DynamicsProbe.cpp -o /tmp/609-timing
python tools/jd_timing.py --engine /tmp/609-timing --mode comp --attack 0 --bus --output jd-bounded.json
```

Use `--engine` with the compiled Vst3Smoke host and pass the VST3 bundle with `--plugin` to run the same fixture on the packaged binary. The workflows cover all six recoveries at each attack position and log additional 100 and 500 ms bursts. They report the factory step, time to partial recovery, and gain remaining 300 ms into recovery.
