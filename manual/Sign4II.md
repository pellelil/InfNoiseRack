# Sign4 Mk II
![Features](features/Polyphonic-Input--Output-green.svg)<br>
Similar to the Sign4 Mk I, the Sign4 Mk II features four independent sections (A, B, C, and D), each with its own input and output. The three-way switch at the top determines how the input signal is processed for each section. By default, when the switch is set to **Abs** (Absolute Value Mode), the module outputs the absolute value of the input, converting negative values into positive values. In **Ct-** mode (Cut Negative), all negative values are replaced with 0V, effectively removing the negative portion of the signal. For example, when processing a bipolar sine wave, the module will retain the positive half-cycle, while the negative half-cycle is flattened to 0V. Conversely, in **Ct+** mode (Cut Positive), all positive values are replaced with 0V, allowing only the negative portion of the input signal to pass through. *Some general info regarding these modules are listed in [Common Sign4 features](commonSign4.md).*

![Screenshot of Sign-4II](module/Sign4II.png) 

**TIP**: When using this module with bipolar signals (-5V to +5V), you effectively reduce the dynamic range by half, as either the negative or positive portion of the signal is removed. If you need to adjust the dynamic range, you can process the output through one of the [Tweak modules or the Auto-Scale4 module](commonTweak.md) for scaling and offset adjustments.

**TIP**: Passing a bipolar signal through this module in **Cut−** mode (where all negative values are set to 0V) and then into a CV input - of another module - ensures it only drives the parameter in the positive direction—equivalent to turning the associated knob clockwise (only positive values and 0V). Conversely, using **Cut+** mode (where all positive values are set to 0V) restricts the signal to negative values, effectively driving the parameter in the opposite direction—like turning the knob counterclockwise. In both cases, if the input is a standard bipolar waveform (without PWM), the output will be 0V for half of the waveform’s cycle, since one half of the phase is "removed" (changed to 0V).

Other Sign modules: [Sign](Sign.md), [Sign4 Mk I](Sign4I.md).

[Go back to modules overview](plugin.md#modules)
