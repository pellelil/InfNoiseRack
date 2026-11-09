# Cross-fade switches
Before diving into the Cross-Fade Switches, the On/Off-Switch, Bernoulli Switch and Combine, it’s worth mentioning that the Infinite-Noise plugin also includes dedicated [Cross-fade modules](commonCrossFade.md), which can double as switches thanks to their built-in A/B toggle buttons and their input can be set to toggle on trigger-detection. Similarly, the [CV Toggle](CvToggle8.md) module have 8 sections which can function as a two-input/one-output switch, allowing you to toggle between an On and Off stage via a gate/trigger input. Additionally, the compare-modules [LMCP2, VCMP1 and VCMP2 Mk I](commonCompare.md) can also act as switches, as they can switch between the "True" and "False" input signals based on incoming signals. 

Among the two Cross-Fade Switch modules described here, one features (up to) 4 inputs that can be **switched/cross-faded** to a single output, while the other has a single input that can be switched/cross-faded across (up to) 4 outputs. At the top of these two modules, a knob, CV-input, and trim control determine the switch operation. Next to the knob, there’s a cross-fade button—when enabled (green), the module will smoothly cross-fade between inputs/outputs. When disabled, it will simply switch between them. **By default, both modules operate in switch mode**, so if you want to activate cross-fade functionality, you need to press this button. By default, the switch CV-input selects ports based on the CV level and trim setting, but a small button next to the CV-input lets you switch to trigger mode (red). In this mode, each trigger pulse advances the switch to a new port. The context menu allows you to configure how the switch advances, with options including:

+ **Next** (moves forward through ports in order)
+ **Prev** (moves backward through ports)
+ **Ping-pong** (alternates between forward and backward movement)
+ **Random** (selects any port, including the currently active one)
+ **Random-Diff** (selects a random port, ensuring it is different from the current one)

When trigger mode (red) is active, the trim knob is ignored (and covered by an overlay), meaning its position has no effect on the selection. When operating in switch mode with trigger input, the Reset input (trigger) can reset the selection. When not in trigger mode, the Reset input is covered instead. A reset pulse will move the switch knob fully counterclockwise, forcing the selection to return to the first input/output, regardless of the trigger-order selected in the context menu. However, if the module is not configured for switch operation with trigger input, the reset function is ignored.

Below the reset input, a 3-way switch allows you to limit the selection to 2, 3, or 4 ports. For example, if set to 3-port mode, the full range of the switch knob/CV-input will cycle through only ports 1 to 3.

At the bottom of the module, small indicator lights next to the four input/output ports provide visual feedback of the signal levels. When cross-fading, two lights may illuminate at different intensities to indicate a transition between ports. In switch mode (default), only one light is fully on — unless a **Fade time** other than 0 is set in the context menu, in which case the lights follow the internal mix while a switch fade is in progress. Default fade time is **0** (instant switch), with the same 0 / 1-2-5 series as other Infinite-Noise fixed times, up to 10 s. **S-curve fade** (off by default) eases that mix. This internal fade applies only in switch mode; the panel cross-fade button still interpolates adjacent ports from the knob/CV as before. If a new port is selected before a fade finishes, the mix retargets toward the new port from its current amount.

Both Cross-Fade Switch 4to1 and 1to4 support polyphonic signals:

+ When feeding a polyphonic signal into the 1to4 switch, each output will carry as many channels as the input.
+ When inputting polyphonic signals into the 4to1 switch, all inputs should ideally have the same number of channels, as the output will adopt the highest channel count of the inputs (you can however mix monophonic and polyphonic signals).

The switch/cross-fade CV-input is monophonic, meaning all polyphonic channels switch/cross-fade in unison. If a polyphonic signal is fed into the switch/cross-fade CV-input, only its first channel is used—the same applies to the Reset input.

**TIP**: If you need to switch stereo audio signals there are multiple ways to do it (as you could argu that the switch modules are mono in nature). However as the switch-modules support polyphonic signals, you can use a [Poly-Stereo](PolyStereo.md) to **encode** separate/monophonic left/right audio-signals into a compound 2-channel polyphonic signal, then pass this signal to one of the switch-modules, and once switched **decode** it back to separate left/right audio-signals.

**TIP**: Using the cross-fade-switches in fade operation you might want to use an [ADR Envelope](ADREnvelope.md) to generate the fade input. Feeding a single trigger into its Phase input in trigger mode, will either trigger an Attack or a Release, where the Envelope output with either transions from R.level to A.level or from A.level to R.level. Its A./R.time and A./R.shape knobs determine how fast- and how the fade will occure. *Using the phase-input of the Envelope in gate-mode will work equally well, where a high gate will begin the attack and a low-gate will begin the release.*

Cross-fade switch modules: [Cross-fade switch 4to1](CrossFadeSwitch4to1.md), [Cross-fade switch 1to4](CrossFadeSwitch1to4.md).

[Go back to modules overview](plugin.md#modules)
