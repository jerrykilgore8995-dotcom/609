* Working device parameters. See docs/reference.md for model status.
* Legacy device fits represent nominal behavior pending IV / capacitance validation.
.model QBC184C NPN(IS=7f BF=455 NF=1 VAF=75 IKF=0.11 ISE=18f NE=1.55 BR=6 VAR=20 CJE=11p VJE=.7 MJE=.33 CJC=4p VJC=.6 MJC=.33 TF=.45n TR=50n RB=45 RC=.8 RE=.2)
.model QBC214C PNP(IS=12f BF=460 NF=1 VAF=45 IKF=.09 ISE=25f NE=1.55 BR=7 VAR=14 CJE=12p VJE=.7 MJE=.33 CJC=5p VJC=.6 MJC=.33 TF=.65n TR=70n RB=45 RC=1 RE=.25)
.model QBC441 NPN(IS=40f BF=120 NF=1 VAF=65 IKF=.45 BR=7 VAR=17 CJE=90p CJC=24p TF=1.8n TR=120n RB=11 RC=.18 RE=.1)
.model QBC461 PNP(IS=55f BF=110 NF=1 VAF=45 IKF=.4 BR=6 VAR=14 CJE=100p CJC=29p TF=2.4n TR=140n RB=13 RC=.2 RE=.12)
.model QMJE371 PNP(IS=70f BF=75 NF=1 VAF=58 IKF=.5 BR=4 VAR=16 CJE=135p CJC=56p TF=9n TR=340n RB=10 RC=.24 RE=.1)
.model JJ109 NJF(VTO=-2.9 BETA=.0025 LAMBDA=.015 RD=2 RS=2 IS=2f CGS=13p CGD=5p PB=.9 FC=.5)
.model D1N4148 D(IS=2.52n N=1.752 RS=.568 CJO=4p VJ=.75 M=.4 TT=4n BV=100 IBV=.1u)
* NXP Semiconductors official 1N4148 SPICE model (SOD27 axial).
* Identical model for four matched bridge diodes D14,D15,D16,D17.
* Source NXP 1N4148.prm, reproduced at https://github.com/PySpice-org/PySpice/blob/master/examples/libraries/diode/switching/1N4148.lib
* NXP also provides 5.827G shunt R1 in each diode subcircuit; represented explicitly on EX11475.
.model D1N4148_NXP D(IS=4.352n N=1.906 BV=110 IBV=100u RS=0.6458 CJO=0.7048p VJ=0.869 M=0.03 FC=0.5 TT=3.48n)
.model DBAX13 D(IS=2.5n N=1.72 RS=.65 CJO=4p VJ=.74 M=.4 TT=4n BV=75 IBV=.1u)
.model DAA144 D(IS=.65u N=1.36 RS=3.2 CJO=3p VJ=.4 M=.33 BV=60 IBV=10u)
.model DBAT42 D(IS=11n N=1.085 RS=2.6 CJO=7p VJ=.43 M=.37 BV=30 IBV=10u)
.model DBZX79C2V7 D(IS=1.2n N=1.52 RS=15 CJO=290p VJ=.64 M=.34 BV=2.7 IBV=5m)

* MkV physical diode D14: Diodes Inc 1N4148W with -1% IS production variation; all other manufacturer parameters unchanged
.model DI_1N4148W_14 D(IS=10.29600n RS=51.5m BV=75.0 IBV=1.00u CJO=2.00p M=0.333 N=2.07 TT=5.76n)

* MkV physical diode D15: Diodes Inc 1N4148W with -0.5% IS production variation; all other manufacturer parameters unchanged
.model DI_1N4148W_15 D(IS=10.34800n RS=51.5m BV=75.0 IBV=1.00u CJO=2.00p M=0.333 N=2.07 TT=5.76n)

* MkV physical diode D16: Diodes Inc 1N4148W with +0.5% IS production variation; all other manufacturer parameters unchanged
.model DI_1N4148W_16 D(IS=10.45200n RS=51.5m BV=75.0 IBV=1.00u CJO=2.00p M=0.333 N=2.07 TT=5.76n)

* MkV physical diode D17: Diodes Inc 1N4148W with +1% IS production variation; all other manufacturer parameters unchanged
.model DI_1N4148W_17 D(IS=10.50400n RS=51.5m BV=75.0 IBV=1.00u CJO=2.00p M=0.333 N=2.07 TT=5.76n)
