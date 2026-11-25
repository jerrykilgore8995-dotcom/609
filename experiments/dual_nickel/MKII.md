# Nickel Opto Mk II

Mk II is a separate VST3 with product code `NOm2`. Retained controls use their original parameter IDs. The original target, `NickelOpto`, still builds under its original name and product code. Mk II 2.0.5 runs the full signal path at four times the host rate. Polyphase allpass IIR oversampling wraps the wet and dry mix together, and its integer latency is reported to the host. The transformer uses a warm-start secant with a residual test and a bracketed fallback. Receiver and diode solves use bounded initial predictions, and LED current tables are built during preparation from the electrical model.

## Audio and control circuits

- **Input:** A 1:1 nickel transformer feeds a biased 2N5088 common collector. The transformer solve includes the receiver bias resistance, input capacitance and voltage-dependent base current. The emitter model uses junction voltage, emitter degeneration, high injection and Early effect. The coupling capacitor sees the optical divider's current small-signal input resistance, or the input of the optional buffered low-mid section. The 4.7 kΩ emitter resistor biases the receiver for more negative-swing headroom under optical load. The rail limits and loading are circuit design choices. The magnetic core and complete receiver combination still need bench measurements against a physical prototype.
- **Optical control:** Each channel computes its own sidechain from its own pre/post shunt signal. The stereo bus uses high selection after detection and feeds one shared LED driver voltage (LED cells) or illumination level (T4B-style) to both channels. Each output retains an independent audio circuit. The optional low-mid stage similarly links detected power. The VTL5C2 high-level diode tap now samples the actual pre-shunt node.
- **LED cells:** VTL5C1 and VTL5C2 retain their resistance/current anchors, with slower conductance onset under weak LED drive and a separate light-adaptation state. The adaptation is stronger in C1, which still has the faster intrinsic dark decay. Sub-mA transfer, short-term adaptation and exact time constants are engineering fits. NSL-32 targets the legacy Silonex revision. The Advanced Photonix Rev. C sheet gives materially different values and is a separate reference. The LED driver models finite source impedance, supply compliance and the transistor shunt across the diode load.
- **T4B-style:** The sidechain drives an AC EL model, which advances its own light state before stereo linking. The panel amplifier uses a feedforward pad and finite swing, with a gentler reduction control taper above the nominal sensitivity. Carrier onset is calibrated separately from slow trap occupancy, which increases recovery time following sustained exposure. A separate metering photocell feeds the analog-style needle. The digital GR readout remains a measurement of optical attenuation.
- **Photocell distortion:** The shunt solves a symmetric CdS film current plus a small polarity-dependent current, which follows illumination and approaches zero in the dark. PerkinElmer Figure 3 reports residual K2 and suggests crossover distortion as a possible explanation. Each cell has its own provisional mismatch, crossover and knee. Contact current is limited as photocell conductance rises and is driven by the actual attenuated shunt voltage, so stronger drive and deeper gain reduction can be measured independently. The exact mechanism and individual part magnitudes still need fixed-light, bipolar I-V measurements. [PerkinElmer optical catalog, pp. 35–36](https://ka-electronics.com/images/pdf/Perkin_Elmer_Photocell_Vactrol_Catalog.pdf)
- **Output:** Makeup Gain and Output Trim apply linear gain to the optical output. The sound engine can pass signals above digital full scale to the host. Input Drive, Makeup, Trim and Mix use 10 ms control ramps in Mk II.

The receiver uses a small circuit solve in the existing magnetics. The 525 repository has 2520 and JE-990 circuit references, including alternate 2520 compensation models. A discrete amplifier would also need the surrounding feedback, input-impedance and load networks specified. The single-transistor receiver gives the transformer a defined load.

## Reference designs

| Reference | Design influence |
| --- | --- |
| [UA LA-2A manual](https://media.uaudio.com/assetlibrary/l/a/la-2a_manual.pdf), [UA LA-3A manual](https://media.uaudio.com/assetlibrary/l/a/la-3a-manual.pdf) | Share the EL/CdS principle and program-dependent recovery. The surrounding amplifiers and panel excitation can change the behavior of the same type of attenuator. |
| [Tube-Tech CL 1B manual](https://www.tube-tech.com/wp-content/uploads/2020/05/Tube-Tech-Manual-CL-1B-200513.pdf) | The control bus and electrical timing circuits are distinct from the optical gain element. Fixed/Manual operation illustrates the interaction of peak response and sustained program. |
| [Buzz Audio SOC-1.1](https://www.buzzaudio.com/products/soc1.1.htm) | Independent waveform detection and soft linking are useful stereo reference designs. Mk II uses a high-select control bus. |
| [Manley ELOP manual](https://www.manley.com/s/melopb_manual.pdf), [ELOP+ manual](https://www.bhphotovideo.com/lit_files/782752.pdf) | The LED/LDR divider, finite driver swing under load and equal linked GR inform the design. The ELOP manual confirms solid-state LED drive. |
| [Silonex NSL-32 Rev. 1](https://media.digikey.com/pdf/Data%20Sheets/Photonic%20Detetectors%20Inc%20PDFs/NSL-32.pdf), [Advanced Photonix NSL-32 Rev. C](https://www.advancedphotonix.com/wp-content/uploads/2022/03/DS-NSL-32.pdf), [PerkinElmer optical catalog](https://ka-electronics.com/images/pdf/Perkin_Elmer_Photocell_Vactrol_Catalog.pdf) | Device resistances, test stimulus levels, rise and turn-off endpoints, and light history. A device resistance endpoint differs from a full compressor's GR recovery time. |
| [Waves CLA-2A](https://www.waves.com/plugins/cla-2a-compressor-limiter), [Waves CLA-3A](https://www.waves.com/plugins/cla-3a-compressor-limiter), [UAD LA-2A](https://help.uaudio.com/hc/en-us/articles/19378009641748-LA-2A-Tube-Compressor-Manual) | Public behavior references for separating amplifier coloration from optical timing. |
| [chowdsp_wdf](https://github.com/Chowdhury-DSP/chowdsp_wdf), [JUCE](https://juce.com/) | The original receiver uses WDF passives. Mk II solves the secondary load inside the existing nonlinear magnetic winding equations. JUCE supplies the VST3 wrapper, sample ramps and automation. |

## Build and bench

```sh
cmake -S experiments/dual_nickel -B build/mkii-bench -DCMAKE_BUILD_TYPE=Release
cmake --build build/mkii-bench --config Release --target nickel_opto_mkii
cmake -S experiments/dual_nickel/juce_plugin -B build/mkii-plugin -DCMAKE_BUILD_TYPE=Release
cmake --build build/mkii-plugin --config Release --target NickelOptoMkII_VST3
```

Run `nickel_opto_mkii` from the bench build. It checks equal L/R optical GR, phase and channel sensitivity, resistance endpoints, four-cell timing at 3 dB of GR, T4B memory, receiver loading and stereo processing cost. The Windows workflow publishes the engine measurements and VST3 separately. Measurements from simulation quantify the model, and the actual CdS parts, transformer and hardware drive circuits are needed for a hardware match.
