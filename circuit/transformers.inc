* T2, series/series T1452 reference. CJ reports 67 H / 16.4 H,
* 328 ohms / 112 ohms and about 8.2 mH measured or estimated from the
* low-turns side. K accounts for about 33 mH referred to the high-turns side.
* CP and CS are effective differential capacitances for the loaded JD.
.subckt TF10016 P N S T
RP1 P IP 164
RP2 N IN 164
LP IP IN 67
RS1 S IS 56
RS2 T IT 56
LS IS IT 16.4
K1 LP LS 0.999755194
CP P N 120p
CS S T 850p
.ends TF10016

* T1, CJ's Marinair 10468/T1444 winding data used as the TF10015 reference.
* The JD parallel secondary and +6 dB stage imply the parallel/parallel
* equivalent. Refer the measured series inductances and primary leakage
* to the parallel connection: 32/4 H, 125.6/4 H, and 6/4 mH.
* DCR is 23.7 || 24.0 ohms and 139.5 || 136.0 ohms.
.subckt TF10015 P N S T
RP P IP 11.9245283
LP IP N 8
RS S IS 68.8638838
LS IS T 31.4
K1 LP LS .999906245605
CP P N 106p
CS S T 142p
.ends TF10015

* T3, TF12009 / VT26247 reference with 3 / 34.5 ohm measured winding DCR.
* 2.2 H is a provisional open-circuit primary estimate. Fit the loaded
* response with one effective leakage term, encoded in K (16.2 uH primary).
* The secondary inductance fits the drawing's +8 dB stage at 1 kHz
* with the 1.2k board shunt and 600-ohm external load.
.subckt TF12009 P N S T
RP P IP 3
LP IP N 2.2
RS1 S IS 17.25
RS2 T IT 17.25
LS IS IT 18.31735
K1 LP LS .999996318175
CP P N 110p
CS S T 180p
.ends TF12009
