# Poly-Shuffle
![Features](features/Polyphonic-Input--Output-green.svg)<br>
This module has two sections, each serving a different purpose. The top section allows you to set the number of output channels (either **removing** channels from the signal or **adding** additional ones), while the lower section lets you change the order of the channels (for example, **shuffle** them). *Some general info regarding the Poly-tools are listed in [Common poly-tool features](commonPoly.md).*

At the top of the module, you’ll see 16 indicator lights—one for each possible output channel. When a polyphonic signal is connected to the **Poly** input and the channel knob is set to more than one channel, multiple lights will illuminate. These lights will transition from **green** (representing the first input channel) to **red** (representing the last output channel). Channels that are not included in the output remain dim. *These lights are described in more detail below.*

![Screenshot of PolyShuffle](module/PolyShuffle.png) 

### Channel count
By default, the **channel mode** button above the channel knob is **pressed (green)**, indicating **Automatic mode**. In this mode, the channel knob automatically follows the number of channels in the input signal. As a result, the module outputs the same number of channels as the input (or a single channel if no input is connected).

When the channel mode button is toggled to its **released (dim)** state, the module switches to **Manual mode**. In this mode, you can freely set the number of output channels using the knob (1-16). If you select a lower channel count than the input, the last channels are removed (for example, reducing an 8-channel signal to 4 channels by removing channels 5–8). If you instead select a higher channel count than the input, additional channels are added to the output. For example, if you input a 4-channel signal and set the output to 8 channels, channels 1–4 will match the input, while channels 5–8 will be generated.

When increasing the channel count, the module defaults to **Value mode**, where added channels take the value set by the value knob (default is 0 V). Pressing the small button next to the **Vl/Rp** (Value/Repeat) indicator-light switches to **Repeat mode**. In Repeat mode, the input channels are repeated (cloned) as needed. For example, with a 4-channel input and 8-channel output, channel 1 and 5 will both output input channel 1, channel 2 and 6 will output input channel 2, and so on. If a monophonic signal is used in Repeat mode, it will simply be repeated across all output channels. *If no input signal is provided, all output channels will take the value dialed-in by the value-knob, no matter if value mode or repeat mode is selected.*

Regarding the 16 channel indicator lights: their **brightness indicates which channels are available** from the input. If the channel knob is set to match (or be lower than) the number of input channels, all active lights will display **full brightness**. If the knob is set higher than the number of input channels, the additional channels will be shown at **reduced brightness (50%)**, still fading from green to red according to channel order *(see below)*.

### Channel order
The label-numbers **1–16** shown at the top of the module represent the **output channels**, while the color of each light indicates which **input channel** is mapped to that output. By default (and after a **Reset**), the channel order matches the input signal. The light next to channel 1 will be fully **green**, indicating that input channel 1 is mapped to output channel 1. The light next to the last active output channel (for example, channel 4 if the channel knob is set to 4) will be fully **red**, indicating the last input channel. The colors in between will gradually transition from green to red. As mentioned above, if you set a higher channel count than the number of channels available in the input signal, the lights corresponding to the added channels will be shown at reduced brightness (50%).

Hovering the mouse over these lights will display which input channel is assigned to each output channel. For example, the label “3” refers to output channel 3, and if the tooltip of the light shows “channel 1,” it means that output channel 3 is currently using the value from input channel 1.

At the bottom of the module, you’ll find a 3-way **order switch** (for selecting the order mode), along with its associated trigger input and button. When a trigger is received (or the small button is pressed), the channel order changes according to the selected mode:
* **Next**: Channels shift in an upward direction. For example, channel 1 outputs what was previously on channel 2, channel 2 outputs what was previously on channel 3, and so on.
* **Prev**: Channels shift in a downward direction. For example, channel 3 outputs what was previously on channel 2, channel 2 outputs what was previously on channel 1, and so on.
* **Shuffle**: Channels are rearranged in a random order each time a trigger is received.

**TIP**: If a downstream module (connected to the Poly output of **Poly-Shuffle**) only uses channel 1, **Poly-Shuffle** can be used as a probability switch. The probability is determined by the number of input channels and the selected number of output channels. For example, if you input a 2-channel signal and set the module to output 8 channels, then shuffle the channels, there is a **1/8 chance** that output channel 1 contains input channel 1. There is a **1/4 chance** that it contains either input channel 1 or 2, and a **3/4 chance** that it contains neither (e.g. 0V by default). 

**TIP**: Similar to the previous tip, you can use two Poly-Shuffle modules in series to both reorder channels and introduce a probability that individual channels will pass through. For example, start with a 4-channel signal and feed it into the first Poly-Shuffle. Set its output channel count to a fixed 16 channels and enable channel shuffling. Then route this 16-channel output into a second Poly-Shuffle, but set its output channel count to 4 channels. In this configuration, each of the original four channels has a 25% chance of being included in the final output, while the "surviving channels" are also randomly reordered. The result is a signal where channels are both randomly selected and randomly shuffled.

Other poly-tool modules: [Poly-Merge](PolyMerge.md), [Poly-Split](PolySplit.md), [Poly-Stereo](PolyStereo.md), [Poly-Quad](PolyQuad.md), [Poly-Tweak Mk I](PolyTweakI.md), [Poly-Tweak Mk II](PolyTweakII.md), [Poly-Logical Compare](PolyLCMP.md), [Poly-Value Compare](PolyVCMP.md), [Poly-Offset](PolyOffset.md), [Poly-Scale](PolyScale.md).

[Go back to modules overview](plugin.md#modules)
