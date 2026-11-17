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

## Native VST3 status

The `native` and `plugin` directories contain the stereo MNA engine and Windows VST3 editor. The [Windows workflow](https://github.com/jerrykilgore8995-dotcom/609/actions/workflows/native.yml) checks the driven solver before publishing an installable bundle. The solver prefers stable pivots, backtracks corrections at rectifier transitions and reuses recent solutions as initial estimates. The Windows workflow verifies driven operating points and paced playback before publishing.
