# 609 preset bank

The **Preset** box in the footer loads the compressor and limiter controls on both channels. The saved session stores the individual controls. The box shows the matching preset when those settings are recalled. PSU noise is a separate preference.

The presets are starting points for a signal calibrated to the 609’s +24 dBu full-scale reference. Adjust threshold against the gain-reduction meter and match output level when comparing. The names describe applications and the bank has no usage ranking.

## Published settings and session examples

- **CLA Parallel Drums** follows Chris Lord-Alge’s published settings of 0 dBu threshold, 2:1, 100 ms recovery, +8 dB makeup, unlinked channels and limiter out. It is intended for a parallel return. The original separate EQ and return fader remain outside the preset. [Puremix](https://www.puremix.com/blog/parallel-drum-compression-with-chris-lord-alge)
- **Subtle Stereo Glue**, **Classic Stereo Glue**, **Tempo Glue 400**, **Slow Song 800**, **Dense Mix**, **Parallel Drum Crush**, **Parallel Drum Weight**, **Transformer Pass** and **Limiter Insurance** adapt the Access Analog recipes for mix, drum and limiter applications. Their fixed thresholds are starting values. The 33609/N’s slow attack includes a detector filter, so the JD bank uses its own attack controls for the slow variants. [Access Analog](https://accessanalog.com/ams-neve-33609n-bus-compressor-explained/)
- **Sampled Drum and Loop Bus** takes its application from Philippe Zdar’s reported 33609 chain for the Beastie Boys’ “Make Some Noise.” The preset’s control positions are original starting values. [Sound On Sound](https://www.soundonsound.com/techniques/inside-track-beastie-boys-make-some-noise)
- **Lead Vocal Peak Catch**, **Tracking Vocal to Tape** and **Stereo Vocal Package** are adapted from published 33609 applications by Hamilton, Keith Olsen and Rich Costey, respectively. The sources describe the circuits’ roles and the preset values are starting points. [Mix vocal recording](https://www.mixonline.com/recording/recording-vocals-start-singer-366289) · [Mix June 2013](https://www.worldradiohistory.com/Archive-All-Audio/Mix-Magazine/10s/Mix-2013-06.pdf) · [Rich Costey interview](https://gearspace.com/threads/interview-with-rich-costey.1439519/)
- **Keys Bus** and **Backing Vocal Bus** adapt two applications Michael Brauer described for his 33609-equipped A bus on “Violet Hill.” External EQ and the parallel routing are session choices. The preset’s controls are original. [Interview excerpt](https://equipboard.com/pros/jonny-buckland-coldplay?gear=studio-equipment)
- **Fast Mastering Catch** adapts the low-gain-reduction, short-recovery compressor and limiter setup from the published stereo-bus demonstration. [Stereo-bus demonstration](https://www.youtube.com/watch?v=O4R-On4GS5k)
- The remaining presets are 609 starting points for the named sources. Actual knob ranges and the distinction between bus, track and mastering gain staging follow the published hardware and UAD documentation. [AMS Neve manual](https://www.ams-neve.com/wp-content/uploads/2022/01/33609N1.0usermanual.pdf) · [UAD manual](https://help.uaudio.com/hc/en-us/articles/33536131639956-Neve-Dynamics-Collection-Manual)

The exact starting controls are in [Presets.hpp](../plugin/Presets.hpp).
