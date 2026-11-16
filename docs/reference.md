# 33609/JD circuit reference

The [33609/JD Technical Handbook](https://www.thehistoryofrecording.com/Manuals/Neve/Neve%2033609jd.pdf) is the controlling source. Page numbers below refer to the PDF.

| PDF pages | Drawing or purpose |
| --- | --- |
| 3–4 | Manufacturer bench procedures and tolerances |
| 6–7 | Channel harness, control connections, stereo relay and signal levels |
| 9–14 | Rotary switch contacts and resistor, capacitor and diode networks |
| 17–18 | EX11475 board and component-level connections |
| 19–21 | 10340-C amplifier description, assembly and transistor schematic |
| 25 | Dimensional reference for the faceplate and its legends |

The circuit connects both channels and the gain-reduction meters. The 1N4153 bridge is the discrete-diode option printed on EX11475. The netlist uses the EX10340-C drawing pin labels 1/2 for inputs, 5 for output, 6 for B+ and 3 for B−. A paragraph in the handbook calls pin 6 the output. The marked drawings take precedence for the ratio control, whose highest position is labeled 6:1. The threshold and recovery switch drawing identifiers on the sheets differ from their ordering in the introductory list.

The fixed supply is +24 V. The bench has a balanced 40 Ω source and 600 Ω load. Switch networks retain their actual contacts and individual components. Bypass and stereo link are modeled with relay contact resistances. The meter winding is represented by a nominal 2.2 kΩ load and the isolated T1 primary uses an insulation resistance to reference its otherwise floating DC common mode. External relay controls can be selected by the simulator parameters ACTIVE and LINKED.

The device parameters for the legacy semiconductors and electrical constants of the three audio transformers are initial engineering estimates. The winding configurations are from EX11475. Exact transformer impedance, phase, saturation, and device noise require measured data. Bench targets from the handbook can constrain the estimates, with individual model uncertainty retained.
