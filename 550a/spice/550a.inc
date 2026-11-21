* Vintage API 550A (613-759), high-level strap. SPICE reconstruction.
* Ideal +/-16 V rack supply is provided by the fixture. Board rectifiers remain.
.include discrete-opamps.lib
.include device-models.lib

* A3-A6. Values from the original inset and TB550A DVF BOM.
.model BF5088 NPN(IS=5.911f BF=680 VAF=82.8 IKF=.053 Rb=10 RC=.66 RE=.23 CJE=4p CJC=3p TF=.35n TR=40n)
.model BF5087 PNP(IS=6.51f BF=380 VAF=52 IKF=.055 RB=12 RC=.87 RE=.24 CJE=7p CJC=4p TF=.60n TR=60n)
.model SUPPLY_RECT D(IS=14n N=1.95 RS=.033 CJO=26p TT=2.2u BV=400 IBV=10u)
.model PANEL_LED D(IS=50p N=3.1 RS=8 CJO=20p)

.subckt DVF IN OUT VP VN GND params: EXTRA=0
R39K feed drive 39k
R36K feed bias 36k
R2K2 bias VN 2.2k
R430 VP feed 430
R200 sink VN 200
QP VN IN drive BF5087
QU feed drive OUT BF5088
QL OUT bias sink BF5088
C39 IN sink 39p
C22 IN OUT {EXTRA}
.ends DVF

* Four equal EA2503 windings. DCR from the GroupDIY EA2503 measurement.
* The legacy single-winding impedance observation gives about 1 H at 100 Hz
* if treated as mostly inductive. K is an effective estimate pending short tests.
.subckt EA2503 P N HI TAP LO
RP P WP 8.4
LP WP N 1.014
R1 HI W1 8.3
L1 W1 J1 1.014
R2 J1 W2 8.3
L2 W2 TAP 1.014
R3 TAP W3 8.3
L3 W3 LO 1.014
K1 LP L1 .999985
K2 LP L2 .999985
K3 LP L3 .999985
K4 L1 L2 .999985
K5 L1 L3 .999985
K6 L2 L3 .999985
.ends EA2503

* Gain detent wafer A. The separate mirrored rails implement reciprocal
* injection into the two sides of the amplifier feedback network.
.subckt GAINA LT LB RT RB ZERO W params: G=0
RL5 LT l4 3480
RL4 l4 l3 3320
RL3 l3 l2 4530
RL2 l2 LB 4870
RR5 RT r4 3480
RR4 r4 r3 3320
RR3 r3 r2 4530
RR2 r2 RB 4870
RS1 W LB {G==-5?0.01:1e10}
RS2 W l2 {G==-4?0.01:1e10}
RS3 W l3 {G==-3?0.01:1e10}
RS4 W l4 {G==-2?0.01:1e10}
RS5 W LT {G==-1?0.01:1e10}
RS6 W ZERO {G==0?0.01:1e10}
RS7 W RT {G==1?0.01:1e10}
RS8 W r4 {G==2?0.01:1e10}
RS9 W r3 {G==3?0.01:1e10}
RS10 W r2 {G==4?0.01:1e10}
RS11 W RB {G==5?0.01:1e10}
.ends GAINA

* Gain wafers B and C use one symmetric resistor string.
.subckt SHUNT W T params: G=0 R1=1180 R2=1780 R3=2100 R4=4530
R1 T t2 {R1}
R2 t2 t3 {R2}
R3 t3 t4 {R3}
R4 t4 t5 {R4}
RS1 W T {abs(G)==5?0.01:1e10}
RS2 W t2 {abs(G)==4?0.01:1e10}
RS3 W t3 {abs(G)==3?0.01:1e10}
RS4 W t4 {abs(G)==2?0.01:1e10}
RS5 W t5 {abs(G)==1?0.01:1e10}
.ends SHUNT

* The original frequency rotors connect cumulative groups of capacitors.
* Detent 1 connects all five. Detent 5 retains only C5.
.subckt CAPWAFER A W params: F=1 C1=1n C2=1n C3=1n C4=1n C5=1n
C1 A p1 {C1}
C2 A p2 {C2}
C3 A p3 {C3}
C4 A p4 {C4}
C5 A p5 {C5}
RS1 W p1 {F<=1?0.01:1e10}
RS2 W p2 {F<=2?0.01:1e10}
RS3 W p3 {F<=3?0.01:1e10}
RS4 W p4 {F<=4?0.01:1e10}
RS5 W p5 {F<=5?0.01:1e10}
.ends CAPWAFER

.subckt API550A HI1 HI2 OUTH OUTL VINP VINN GND params: LG=0 MG=0 HG=0 LF=1 MF=1 HF=1 LS=0 HS=0 BP=0 EQ=1
DCR1 VINP VP SUPPLY_RECT
DCR2 VN VINN SUPPLY_RECT
C35 GND VN 22u
C36 VP GND 22u
R100 VINN tally 3.9k
D1 GND led PANEL_LED
RS1LED led tally {EQ==1?0.01:1e10}
R1 HI1 inp 11k
R2 inp GND 7.5k
RS_IN2 HI2 inp 0.01
C44 inp GND 47p
XA1 inp a1minus a1out VP VN GND DOA2520_10P
R78 a1out a1minus 2.2meg
R71 hf_inv a1minus 1k
* Early factory compensation values from the service schematic.
C37 a1out hf_inv 10p

* Switch S2 opens/grounds the common return of the discrete band-pass ladder.
C1 a1out b1 4.7u
R13 b1 bpground 910
C2 b1 b2 .47u
R14 b2 bpground 9.1k
R16 b2 b3 560
C3 b3 bpground .01u
R17 b3 b4 5.6k
C4 b4 bpground .001u
R15 bpground GND 330k
R107 a1out bpground 18k
RS2 bpground GND {BP==1?0.01:1e10}
R99 b4 a6in 1k
R51 a6in GND 2.2meg
XA6 a6in a6out VP VN GND DVF

R8 a6out hlt 6.98k
R7 a1out hrt 6.98k
XHGA hlt hf_bus hrt hf_inv hzero hselect GAINA params: G={HG}
R48 hzero href 10k
R75 hf_bus a4in 1k
R81 a4in GND 2.2meg
XA4 a4in a4out VP VN GND DVF

XHGB hcap hb SHUNT params: G={HG}
R42 hb hgnd 3.92k
XHGC hscap hc SHUNT params: G={HG} R1=2740 R2=3830 R3=5360 R4=8060
R47 hc hgnd 6.65k
C38 GND hgnd_ac 120u
RS1H hgnd hgnd_ac {EQ==1?0.01:1e10}
RS8D href a6out {HG<0?0.01:1e10}
RS8E href a1out {HG>0?0.01:1e10}
XHFA hcap href CAPWAFER params: F={HF} C1=1n C2=680p C3=330p C4=330p C5=1.2n
XHFB hcap hpeak CAPWAFER params: F={HF} C1=1n C2=680p C3=390p C4=330p C5=1n
XHFC hscap hshelf CAPWAFER params: F={HF} C1=1.8n C2=1.5n C3=680p C4=470p C5=2.2n
RS5P hselect hpeak {HS==0?0.01:1e10}
RS5S hselect hshelf {HS==1?0.01:1e10}

R59 a4out mlt 6.98k
R58 drive mrt 6.98k
XMGA mlt mid_l mrt mid_r mzero mselect GAINA params: G={MG}
R52 mzero mref 10k
R73 mid_l a3in 1k
R77 mid_r a5in 1k
C56 mid_l a4out 47p
C43 mid_r drive 47p
R49 a3in GND 2.2meg
R82 a5in GND 2.2meg
XA3 a3in a3out VP VN GND DVF
XA5 a5in a5out VP VN GND DVF params: EXTRA=22p
RS10C mref a4out {MG<0?0.01:1e10}
RS10D mref drive {MG>0?0.01:1e10}
RS1M1 mselect mpeak {EQ==1?0.01:1e10}
XMFA mcap mpeak CAPWAFER params: F={MF} C1=22n C2=10n C3=5.6n C4=2.2n C5=3.3n
XMFB mcap mref CAPWAFER params: F={MF} C1=22n C2=10n C3=5.6n C4=2.2n C5=3.3n
XMGB mcap mb SHUNT params: G={MG}
R68 mb mgnd 3.92k
RS1M2 mgnd GND {EQ==1?0.01:1e10}

R33 a3out llt 6.98k
R32 a5out lrt 6.98k
XLGA llt lf_pos lrt lf_neg lzero lselect GAINA params: G={LG}
R50 lzero lref 10k
R53 lf_pos a2plus 1k
R79 a2plus GND 2.2meg
R72 lf_neg a2minus 1k
C39 lf_neg a5out 47p
C40 a2plus GND 47p
XA2 a2plus a2minus a2out VP VN GND DOA2520_10P
R80 a2out a2minus 1.5meg
C61 a2out a2minus 10p
XLGB lcap lb SHUNT params: G={LG}
R22 lb lgnd 3.92k
XLGC lscap lc SHUNT params: G={LG} R1=2550 R2=3920 R3=4420 R4=9310
R27 lc lgnd_ac 6.34k
C14 lgnd lgnd_ac 22u
RS1L lgnd GND {EQ==1?0.01:1e10}
RS6D lref a3out {LG<0?0.01:1e10}
RS6E lref a5out {LG>0?0.01:1e10}
XLFA lcap lref CAPWAFER params: F={LF} C1=.15u C2=.082u C3=.027u C4=.015u C5=.039u
XLFB lcap lpeak CAPWAFER params: F={LF} C1=.15u C2=.082u C3=.027u C4=.015u C5=.039u
XLFC lscap lref CAPWAFER params: F={LF} C1=.047u C2=.022u C3=.0068u C4=.0047u C5=.01u
RS4P lselect lpeak {LS==0?0.01:1e10}
RS4S lselect lscap {LS==1?0.01:1e10}

R106 a2out drive 200
C41 a2out primary 470u
R101 a2out primary 47
XT1 primary GND OUTH lowtap OUTL EA2503
R76 a4out trim 56
.ends API550A
