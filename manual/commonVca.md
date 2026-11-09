# VCA modules
I chose to describe the VCA modules in this section of the manual because they function similarly to the Tweak modules discussed in [Common Tweak features](commonTweak.md). The primary difference is that Tweak modules can attenuvert in the range from -10x to +10x, while the VCA modules are designed strictly for attenuation, ranging from 0% to 100%. Additionally, Tweak modules allow for offsetting signals by adding or subtracting a fixed value, whereas VCA modules do not support this functionality. Essentially, Tweak modules offer more flexibility, but if you only need voltage-controlled attenuation, the VCA modules are the more appropriate choice. 

**Note**: Since the **VCA** covers a range from **0× to 1×**, while the **Tweak** modules by default cover a range from **−1× to +1×**, they respond differently to CV input. For example, if you set the VCA knob to **0×**, you need to apply **+10 V** to reach a gain of **1×**. In contrast, for a Tweak module, **0×** is at the center position (between −1× and +1×). This means you only need a **+5 V** CV signal to reach **1×** gain. The "idea is" that **10 V corresponds to a full knob range sweep**.

By default, the amplification scaling (0% to 100%) is set to linear mode. However, using the context menu, you can switch to exponential (exp) or logarithmic (log) scaling. A small indicator light next to the "Amp." label provides a visual cue for the active scaling mode. In linear mode, the light remains dim. When exponential scaling is selected, the light turns green. For logarithmic scaling, the light turns red. This allows you to quickly identify the current scaling mode without needing to open the context menu.

When **polyphonic** signals are fed into the VCA module, the output will contain the same number of channels. Additionally, you can input a polyphonic amplify signal, allowing each output channel to be amplified by a different amount. If the amplify input is monophonic however, all channels will be amplified by the same amount.

VCA modules: [VCA-2](VCA2.md), [VCA-4 Mk I](VCA4I.md), [VCA-4 Mk II](VCA4II.md).

[Go back to modules overview](plugin.md#modules)
