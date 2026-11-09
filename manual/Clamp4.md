# Clamp 4
The previous sections covered the Tweak modules first, followed by the VCA modules. This section introduces additional modules that, while not part of either the Tweak or VCA families, are still designed for amplification purpose.

![Features](features/Polyphonic-Input--Output-green.svg)
![Features](features/Oversample-2x-green.svg)<br>
This 2HP clamp module features four inputs and four outputs, with Min and Max knobs at the top, allowing you to define a voltage range between -10V and +10V. If an input voltage exceeds the specified range, the signal is clamped to fit within it. Each input has an indicator light that illuminates whenever an input value exceeds the defined range, dimming gradually.

If the Min knob is set to a value greater than the Max knob, the signal is not only clamped but also **inverted** within the defined range. When this occurs, a small red light next to the Min knob is illuminated to indicate inversion mode. By default, the module clamps signals within a -5V to +5V range, but preset voltage ranges can be selected via the context menu, or you can manually adjust them as needed.

Between the **Min** and **Max** knobs, you’ll find a small **Link** button that lets you link the Max knob to the Min knob. When enabled, the Max knob mirrors the Min knob. For example, if you set the Min knob to **-3 V**, the Max knob will automatically be set to **+3 V**. This makes it very easy to dial in a symmetrical bipolar range.

The module has three operational modes:
+ **Clamp Mode** (Default):  In this mode, all incoming signals are clamped to ensure they remain within the range set by the Min and Max knobs. If inversion mode is activated (Min > Max), the signal will also be inverted.
+ **Diff Mode**: Instead of clamping signals, Diff modes monitors- and output how much the input signal **overflows** the range defined by the min-/max-knobs. For example, with a range of -5V to +5V, an input of +6V produces an output of +1V, while an input of -7.5V results in an output of -2.5V. By default, this output is signed, but the context menu allows switching to absolute difference mode instead. *Note: Inversion mode is only available in Clamp Mode and does not apply to Diff Mode*.
+ **Gate Mode**: In this mode, the module outputs a 10V gate whenever the input exceeds the specified range (default behavior). This behavior can be inverted using the context menu, making the gate output high while the input is within the defined range instead. Additionally, the gate’s high and low voltage levels can be adjusted via the context menu. *Note: Inversion mode is only available in Clamp Mode and does not apply to Gate Mode*.

**TIP**: While in Diff mode, if you want to isolate the "peaks" of a sine wave from an LFO outputting a bipolar -5V to +5V signal, you can set Min/Max to -3V/+3V in Diff Mode. The output will only contain the "peaks" but within a range of -2V to +2V. If you need to rescale this to a range from -5V to +5V, you could manually adjust it with a Tweak module, but an [Auto-Scale 4](AutoScale4.md) can automatically adjust the scaling.
    
![Screenshot of Clamp 4](module/Clamp4.png)

Other amplification modules: [Auto-Scale 4](AutoScale4.md).

[Go back to modules overview](plugin.md#modules)
