# Poly-Logical Compare
![Features](features/Polyphonic-Input-orange.svg)<br>
Unlike the "standard" [Logical compare modules](commonCompare.md), which compare channels between multiple inputs, the Poly-Logical Compare module compares all channels within the same/single polyphonic signal. If you input a polyphonic signal with up to 16 channels, this module evaluates logical conditions across all active channels. For example, if a 4-channel polyphonic signal is provided, the outputs behave as follows:

+ **AND** Output: High only if all 4 channels are detected as high-gates.
+ **OR** Output: High if at least 1 channel is high (1 or more are high).
+ **XOR** Output: High only if exactly 1 channel is high.
+ **NAND** Output: High when less than 4 channels are high.
+ **NOR** Output: High if all 4 channels are low (0 channels are high).
+ **XNOR**: Output: High when 0, or more than 1 channel is high.

At the top of the module, 16 indicator lights display the status of each channel. A green light indicates a high-gate detection, while a red light means the channel is low-gate. By default, a channel is detected as high when its voltage is ≥1V, but this threshold can be adjusted via the context menu. Likewise, output voltages default to 10V for high and 0V for low, but these values can also be customized. *Some general info regarding the Poly-tools are listed in [Common poly-tool features](commonPoly.md).*

![Screenshot of Polyphonic logical-compare](module/PolyLCMP.png) 

By default, the **OR** output activates when one or more channels are high, and **XOR** activates when exactly one channel is high. However, these conditions can be customized using the context menu.

+ The OR output can be set to trigger only when a specific number of channels (1-16) are high (e.g., if at least 3 channels are high).
+ The XOR output can be configured to activate when exactly a specified number of channels are high (e.g., precisely 3 channels - no less, no more).

If these defaults are modified, a small red indicator light next to the OR or XOR labels will illuminate, signaling non-standard behavior. However, the exact settings will only be visible in the context menu. The NOR and XNOR outputs always provide negated versions of the OR and XOR outputs. This means that if you modify the OR condition, NOR will follow the inverse rule, and the same applies to XOR and XNOR. For example, if OR is set to activate when 3 or more channels are high, then NOR will activate when fewer than 3 channels are high.

**TIP**: If you pass the signal through one of the PolyTweak modules before feeding it into this module, you can invert selected channels (when in "Gate" mode) and use the Excl/Val switch to either remove channels from the logical comparison or overwrite specific channel values. For instance, setting channels values to 0V ensures they are detected as low-gates, while setting them to 10V guarantees they are read as high-gates. 

Other poly-tool modules: [Poly-Merge](PolyMerge.md), [Poly-Split](PolySplit.md), [Poly-Stereo](PolyStereo.md), [Poly-Quad](PolyQuad.md), [Poly-Shuffle](PolyShuffle.md), [Poly-Tweak Mk I](PolyTweakI.md), [Poly-Tweak Mk II](PolyTweakII.md), [Poly-Value Compare](PolyVCMP.md), [Poly-Offset](PolyOffset.md), [Poly-Scale](PolyScale.md).

[Go back to modules overview](plugin.md#modules)
