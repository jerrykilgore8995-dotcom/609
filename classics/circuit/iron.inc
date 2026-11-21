* 31267/T1452, series/series, CJ's measured winding data.
.subckt L31267 P N S T
RPH P PH 164
RPL N PL 164
LP PH PL 67
RSH S SH 56
RSL T SL 56
LS SH SL 16.4
KPS LP LS .999755194
.ends L31267

* Same winding with the low-turns side centre tap brought out.
* The 16.4 H end-to-end inductance is split into two coupled 4.1 H halves.
.subckt L31267_CT P N S MID T
RPH P PH 164
RPL N PL 164
LP PH PL 67
RSH S SH 56
RSL T SL 56
LSH SH MID 4.1005
LSL MID SL 4.1005
KPH LP LSH .999755
KPL LP LSL .999755
KSS LSH LSL .999756
.ends L31267_CT

* 10468: series windings, from CJ 32 H / 125.6 H and 6 mH shorted.
.subckt L10468 P N S T
RP P PP 47.7
LP PP N 32
RS S SS 275.5
LS SS T 125.6
KPS LP LS .999906245605
CP P N 106p
CS S T 142p
* CJ measured about 110 pF between the two winding groups. Distribute
* the lumped capacitance equally across the balanced winding ends.
CPS P S 27.5p
CPT P T 27.5p
CNS N S 27.5p
CNT N T 27.5p
.ends L10468

* 2254 B185 primary has a DC centre tap to bias the emitter followers.
.subckt L10468_CT P MID N S T
RPH P PH 23.7
RPL N PL 24
LPH PH MID 9.46
LPL MID PL 9.55
RS S SS 275.5
LS SS T 125.6
* The two half-primary mutual terms sum to the measured 6 mH
* series-primary short-circuit leakage. KPP recovers the measured 32 H.
KPH LPH LS .9195147631
KPL LPL LS .9151717145
KPP LPH LPL .6833322242
CP P N 106p
CS S T 142p
* CJ measured about 110 pF between the two winding groups. Distribute
* the lumped capacitance equally across the balanced winding ends.
CPS P S 27.5p
CPT P T 27.5p
CNS N S 27.5p
CNT N T 27.5p
.ends L10468_CT

* Gapped LO1166/A. CJ's first specimen measured 833 mH on the
* primary, 1.93 H with the secondary windings strapped in series, and
* 14.5 ohms on the primary (GroupDIY "Mo Metal", posts 11 and 12).
* The separate LO1166/A bench sheet identifies 43.6 ohms across pins
* 8 and 5. Keep that explicitly identified end-to-end measurement.
* Leakage factor and magnetizing behavior under DC are still provisional.
.subckt LO1166 P N S T
RP P PP 14.5
LP PP N .833
RS S SS 43.6
LS SS T 1.93
KPS LP LS .9997
.ends LO1166

* LO1173: VTB9056 family DCR, nominal +8 dB rated voltage gain.
* Magnetizing inductance, leakage and capacitances are equivalent-model values.
.subckt LO1173 P N S T
RP P PP 3
LP PP N 2.2
RS S SS 30
LS SS T 15.57
KPS LP LS .9999
CP P N 110p
CS S T 180p
.ends LO1173

* B185 limiter detector: drive the 31267 low-turns side and
* take the high-turns winding in two halves (L/10004 pins 2/5).
.subckt L31267_REVERSE_CT P N S MID T
RPH P PH 56
RPL N PL 56
LP PH PL 16.4
RSH S SH 164
RSL T SL 164
LSH SH MID 16.7509
LSL MID SL 16.7509
KPH LP LSH .999755
KPL LP LSL .999755
KSS LSH LSL .999785
.ends L31267_REVERSE_CT
