# 609

Circuit reconstruction of the 33609/JD discrete diode-bridge limiter/compressor. The [reference notes](docs/reference.md) identify the drawing revision, modeling assumptions and bench conditions.

The [SPICE fixture](circuit/609.cir) connects two EX11475 boards, the EX10340-C discrete output amplifiers, all six switch assemblies per channel, the front-panel in/out and attack switches, a stereo link relay and two meter loads. It uses a fixed +24 V supply.

Run the fixture with Python 3, NumPy and ngspice.

```sh
python tools/measure.py --ac
python tools/measure.py --level 0 --level 10 --set CEL=1 --set CRL=4
python tools/measure.py --level 10 --level 20 --set LEL=1
```

The circuit is undergoing bench validation. See the reference notes for the remaining transformer and semiconductor characterization.

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

This branch evaluates a bounded model at 44.1 kHz and 512 samples. It adds zero samples of host latency. The compressor models the charging and recovery of the timing capacitor. The limiter uses a peak and smoothed detector, and the audio path uses measured harmonic residuals for coherent cycles.

Download `609-Bounded-Parity-Evaluation-Windows-x64-VST3` from a passing [Windows workflow](https://github.com/jerrykilgore8995-dotcom/609/actions/workflows/native.yml) on this branch. The workflow checks the installed VST3 in a paced Windows test host against an 11.61 ms callback budget and includes circuit and SPICE results. See [bounded parity results](docs/bounded-parity.md) for the remaining differences and the conflicting reference measurements.
