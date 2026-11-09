# Poly-Value Compare
![Features](features/Polyphonic-Input--Output-green.svg)<br>
Similar to the Poly-Logical Compare module, Poly-Value Compare by default compares the values of all channels within a single polyphonic input-signal. It features 6 outputs, which by default are clamped within -12V to +12V, but this setting can be adjusted or disabled via the context menu. *Some general info regarding the Poly-tools are listed in [Common poly-tool features](commonPoly.md).*

Outputs:
+ **MIN**: The lowest value among all channels.
+ **MAX**: The highest value among all channels.
+ **NtZ**: The value closest to 0V, whether positive or negative.
+ **FfZ**: The value farthest from 0V, whether positive or negative.
+ **AVG**: The average of all channel values (basically a "mix").
+ **R/S**: Range/Sum output. By default this outputs the range between min and max ("max-min"). Via the context menu you can switch this output to Sum mode, where all channel values are added.

The small button next to the reset-input controls the **reset-mode**, By default the button is released (dim) in which case the reset-mode will be set to **Each cycle**. In this mode all internal values are reset in each cycle, hence e.g. the Min-output will output the lowest detected channel-value in this/current cycle. Pressing the reset-button (red) it will toggle the reset-mode to **Trigger-only**. In this mode, the internal values are only reset whenever the module receives a reset-trigger. Hence in trigger-only mode the module can be used to monitor the lowest/highest/average value over time (since last reset).

By default, the module operates in **Monophonic Output mode**, where each output is monophonic. For example, the **MIN** output will produce a single value representing the lowest value across all input channels. Using the 2-way output switch, the module can be set to **Polyphonic Output mode**. This mode should only be used when the reset mode is set to **Trigger-only**. In this configuration, all outputs become polyphonic, with each output containing the same number of channels as the input signal. For example, if you input a 4-channel signal, the **MIN** output will also have 4 channels. Each channel will hold the lowest value recorded for its corresponding input channel since the last reset (e.g. channel-1 of the Min-output will indicate the lowest value recorded from the channel-1 input - since the last reset - channel-2 will hold the lowest value recorded from channel-2 ... and so on).

![Screenshot of Polyphonic Value-compare](module/PolyVCMP.png) 

**TIP**: If you want to exclude some of the channels from the compare operation, simply pass the signal through one of the Poly-Tweak modules before this module, and use it to exclude any unwanted channels. If you want to exclude the last n channels (e.g. only use the first 4 channels of a signal), many of the other poly-tool modules can be used (e.g. a Poly-Shuffle module with it's channel knob set to 4, will exclude all channels after 4, hence only output channel 1-4).

**TIP**: If you need to find the lowest/highest value from multiple monophonic signals, instead of chaining multiple [VMCP2 Mk. II](VCMP2II.md) modules, merge those signals into a polyphonic signal using Poly-Merge, then send its Poly output into PolyVCmp to extract the MIN/MAX outputs.

Other poly-tool modules: [Poly-Merge](PolyMerge.md), [Poly-Split](PolySplit.md), [Poly-Stereo](PolyStereo.md), [Poly-Quad](PolyQuad.md), [Poly-Shuffle](PolyShuffle.md), [Poly-Tweak Mk I](PolyTweakI.md), [Poly-Tweak Mk II](PolyTweakII.md), [Poly-Logical Compare](PolyLCMP.md), [Poly-Offset](PolyOffset.md), [Poly-Scale](PolyScale.md).

[Go back to modules overview](plugin.md#modules)
