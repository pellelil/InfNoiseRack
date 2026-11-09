# Poly-Tweak Mk I
![Features](features/Polyphonic-Input--Output-green.svg)<br>
The Poly-Tweak Mk I module allows you to invert individual channel values, and enables you to mute/remove channels from an output. By default, before making any adjustments, the module simply outputs a copy of the input signal since none of the channels are inverted, and all channels are enabled. *Some general info regarding the Poly-tools are listed in [Common poly-tool features](commonPoly.md).*

By default, the module outputs the same number of channels as the input (unless channels are disabled in **Exclude mode**, described later). However, you can override this behavior via the context menu by selecting a specific polyphonic count (it defaults to **Auto**). If you choose a polyphonic count lower than the number of input channels, the remaining channels will be removed. If you select a higher count, additional channels will be added and set to 0V. Regardless of the settings, the module will always output at least one channel.

Both sections include 16 buttons. In the top section, these invert individual channels; in the bottom section, they disable individual channels. Each section also has an **All** button. By default, this button toggles all 16 channel buttons. However, the context menu provides options to change its behavior so it can instead set all 16 buttons either **On** or **Off**. The context menu also includes separate commands to set all buttons to **On** or **Off** without changing the current behavior of the **All** button.

At the top of the module, you will find inversion controls for each channel. Pressing a button will invert the associated channel, turning the button red. However, for inversion to work, the module must understand the range of incoming values. A three-way switch below the inversion buttons defines the inversion range:

+ **Bipolar** (-5V to +5V): Values are inverted symmetrically around 0V (e.g. inverting 3 to -3).
+ **Unipolar** (0V to +10V): Values are inverted symmetrically around 5V (e.g. inverting 3 to 7).
+ **Gate Mode**: Inputs are treated as high/low gates, where values ≥1V (by default) are considered high and values below 1V are low. In this mode, inversion flips high gates to low (default: 0V) and low gates to high (default: 10V). The threshold for detecting high gates and output levels for high/low gates can be adjusted via the context menu.

When in gate mode, inverted channels always output inverted gates. Non-inverted channels, by default, also output as gates (for example, an input of 2V will produce a 10V output, since any signal ≥ 1V is treated as a high gate). However, the context menu allows you to change this behavior so that non-inverted channels pass the signal through unchanged (for example, 2V will remain 2V as long as the channel is not inverted).

Below the inversion section, you find the the Disable section, which allows you to disable or enable individual channels. By default, all channels are enabled (buttons appear dim). Pressing a button toggles it red, which indicate that channel is disabled. The context menu also includes options to enable or disable all 16 channels simultaneously. When disabling a channel, two different behaviors can be selected via a two-way switch:

+ **Val** Mode (Default): Disabled channels output a fixed voltage set by the "Dis. Value" knob, which ranges from -10V to +10V (default: 0V). *Hence this can also be regarded as a mute-option, where disabled channels are "muted" (set to 0V).*
+ **Excl** Mode: Disabled channels are completely removed from the output. For example, if you input a 16-channel signal and disable every odd-numbered channel, the output will only contain the 8 non-disabled channels (hence output as an 8-channel polyphonic signal). *If you exclude all channels, the module with output a monophonic 0V signal.*

![Screenshot of PolyTweak Mk I](module/PolyTweakI.png) 

**TIP**: PolyTweak can be used before the [Poly-Logical Compare](PolyLCMP.md) or [Poly-Value Compare](PolyVCMP.md) modules to exclude or invert channel values before sending the signal for logical- or value comparisons.

**TIP**: Even without an input signal, the module can be used to generate a polyphonic gate signal. Using the context menu, you can define a fixed number of channels (e.g. 16). All channels default to 0V (low gate). In **Gate mode**, pressing the buttons to invert specific channels will turn those channels into high gates (10V by default). *If needed, you can also use the **Disable** section to introduce a third possible value. In **Value mode**, the value knob determines the voltage assigned to disabled channels, allowing more flexible signal states beyond simple high/low gates.*

**TIP**: Similar to the previous tip, this module can also ge used to geneate a polyphonic signal where all channels have the same *(custom defined)* fixed value. Via the context menu you select a fixed polyphony (e.g. 8 channels), next you press the "All" button in the Disable section (in value mode)  to "disable" all channels. Finally you use the Dis.value knob to dial-in the desired fixed value (e.g. 5V). *Using this example the "Poly" output will output an 8-channel polyphonic signal, where all channels have the value 5V.*

Other poly-tool modules: [Poly-Merge](PolyMerge.md), [Poly-Split](PolySplit.md), [Poly-Stereo](PolyStereo.md), [Poly-Quad](PolyQuad.md), [Poly-Shuffle](PolyShuffle.md), [Poly-Tweak Mk II](PolyTweakII.md), [Poly-Logical Compare](PolyLCMP.md), [Poly-Value Compare](PolyVCMP.md), [Poly-Offset](PolyOffset.md), [Poly-Scale](PolyScale.md).

[Go back to modules overview](plugin.md#modules)
