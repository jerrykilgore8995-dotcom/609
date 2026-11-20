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

* Secondary windings are strapped in parallel by the EX11475 PCB.
.subckt TF10015 P N S T
RP P IP 61
LP IP N 47
RS1 S IS1 140
RS2 S IS2 140
LS1 IS1 T 188
LS2 IS2 T 188
KPS1 LP LS1 .9999991
KPS2 LP LS2 .9999991
KSS LS1 LS2 .9999991
CP P N 130p
CS S T 95p
.ends TF10015

* T3, TF12009 / VT26247 reference with 3 / 34.5 ohm measured winding DCR.
* 2.2 H is a provisional open-circuit primary estimate. Fit the loaded
* response with one effective leakage term, encoded in K (16.2 uH primary).
* The 1:2.5 nominal ratio follows the JD circuit gain structure.
.subckt TF12009 P N S T
RP P IP 3
LP IP N 2.2
RS1 S IS 17.25
RS2 T IT 17.25
LS IS IT 13.75
K1 LP LS .999996318175
CP P N 110p
CS S T 180p
.ends TF12009
