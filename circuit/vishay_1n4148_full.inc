* Vishay Semiconductors 1N4148 full SPICE3 subcircuit, document 85547 rev 1.1 27-Nov-2003.
* Model authored June 2001 by Arpad Buermen (Thomatronik GmbH).
* Source: https://datasheet.datasheetarchive.com/originals/library/Datasheet-028/DSA00500384.pdf
* Nominal temperature 27 C. Temperature coefficients rmod/rlmod evaluate at their
* published nominal reference temperature, so the native fixed-temperature circuit
* can represent them as ordinary resistors without altering their nominal values.
* All 1N4148 nonlinearity components included: junction, VF, nonlinear BF, I1,
* temperature-reference R1, transimpedance H2, RTF1, CTF, RTF2, DBV1/DBV2,
* and parallel leakage RL.
.subckt VISHAY4148 1 2
D_DIO 1 10 VISHAY_LEGD
.model VISHAY_LEGD D(IS=5.94633E-009 N=1.90777 RS=0.560464 EG=1.10452 XTI=3.99998 CJO=3.92E-012 VJ=10 M=0.25 FC=0.5 TT=1.357169E-008 BV=110 IBV=0.1)
V_F 10 20 0
* Original: bf 20 2 v=600*i(vf)/(abs(v(51)*10/v(30))+1)
* Native compiler exact nonlinear Jacobian supported as 'bvis' (voltage output).
BVIS 20 2 V=600*I(V_F)/(ABS(V(51)*10/V(30))+1)
I1 0 30 1e-3
R1 30 0 25.86417
H2 50 0 V_F 500
RTF1 50 51 10000
CTF 51 0 240p
RTF2 51 0 1E+007
DBV1 1 90 DBVMOD
DBV2 2 90 DBVMOD
.model DBVMOD D(IS=1e-15 N=2 RS=1 BV=110 IBV=0.1)
RL 1 2 1.1E9
.ends VISHAY4148
