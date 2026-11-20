# 609

Circuit reconstruction of the 33609/JD discrete diode-bridge limiter/compressor. The [reference notes](docs/reference.md) identify the drawing revision, modeling assumptions and bench conditions.

The [SPICE fixture](circuit/609.cir) connects two EX11475 boards, the EX10340-C discrete output amplifiers, all six switch assemblies per channel, the front-panel in/out and attack switches, a stereo link relay and two meter loads. It uses a fixed +24 V supply.

Run the fixture with Python 3, NumPy and ngspice.

```sh
python tools/measure.py --ac
python tools/measure.py --level 0 --level 10 --set CEL=1 --set CRL=4
python tools/measure.py --level 10 --level 20 --set LEL=1
```

The compressor attack switch defaults to the 33609/JD position and offers the existing 7.5 and 15 ms alternatives. The corresponding SPICE detents are `CATKL=0/1/2` and `CATKR=0/1/2`. See [the JD timing bench](docs/jd-timing.md) for the test conditions and results.

The [dynamics measurement workflow](https://github.com/jerrykilgore8995-dotcom/609/actions/workflows/measure-dynamics.yml)
reports compressor ratios at 0/+10 dBu and limiter ratios at +10/+20 dBu.
Compressor recovery is read at 63% gain recovery. The limiter report includes
control-voltage recovery and the handbook's output settling test for attack.

The PSU Noise switch adds a modeled output noise floor, with 6 dB/octave
filter skirts at 40 Hz and 15 kHz and coherent 60/120 Hz hum across channels.
For comparison in SPAN at 44.1 kHz, use a 4,096-point Hann FFT with 0 dB/octave
display slope. The pink-noise target is −96 dBFS per bin near 1 kHz and the
hum amplitudes are −78 and −86 dBFS.

## Windows evaluation

The VST3 embeds the component-level nodal solver. The build derives its circuit and operating point from the SPICE deck, and generates the sparse solve schedule at 44.1, 48 and 96 kHz. Host-rate integration and numerical device approximations can differ from ngspice's adaptive transient results.

The VST3 runs independent 231-equation solvers for the left and right channels. A stereo track uses two workers and a mono track uses one. The plugin uses a three-block delay. At 44.1 kHz and 512 samples the host compensates 1,536 samples (34.8 ms). If the worker misses its deadline, the affected samples fall back to delay-aligned dry audio. Offline renders wait for the solved samples. The [nodal Windows workflow](https://github.com/jerrykilgore8995-dotcom/609/actions/workflows/nodal.yml) publishes the build and verifies offline renders against the native channel solvers.

To build locally, install Python 3, NumPy and ngspice, then run:

```sh
cmake -S plugin -B build-plugin -DCMAKE_BUILD_TYPE=Release
cmake --build build-plugin --config Release --target Compressor609_VST3 Vst3Smoke
```

## Presets and JD conformance

The VST3 footer includes a [30-preset bank](docs/presets.md) with documented 33609 applications and 609 starting points. The [JD measurement notes](docs/jd-stock-bounded.md) describe the stock ratio ladder, measured confidence, and how to run a resumable comparison between live SPICE and the loaded VST3.
