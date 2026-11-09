# Random-4
![Features](features/Polyphonic-Input--Output-green.svg)
![Features](features/Process--Quality-Auto-green.svg)
![Features](features/Quantize-Yes-green.svg)
![Features](features/Rate--Chaos-Yes-green.svg)
![Features](features/Internal--Slew-Yes-green.svg)<br>
This compact 2HP module generates four random outputs each time it receives a trigger. If no trigger input is provided, it operates with its built-in LFO, controlled by the frequency knob below the trigger input (chaos rate can be selected via the context menu). As described in [Common random features](commonRandom.md), the range knob defines the span of generated values, while the distribution knob adjusts how "likely" certain values appear (e.g., favoring the center, edges, min, or max). *Some general info regarding the Random-modules are listed in [Common random features](commonRandom.md).*

The module features various mode options, accessible via the context menu, allowing you to modify the Mn/Cn/Mx offset mode, adjust the distribution behavior, or enable/disable Forced Polarity (explained in [Common random features](commonRandom.md#forced-polarity)). Small green indicator lights on the panel illuminate to show active modes, reducing the need to open the menu for verification. The green light next to "Dist" indicates when Forced Polarity is enabled, but this only applies when Center/Edge distribution is active.

By default, **Polyphony** is **Auto**: each of the four outputs matches the number of channels on the Trigger input (or one channel if no trigger is connected, using the internal LFO). You can instead pick a fixed channel count (1–16) from the context menu; the module then always outputs that many channels regardless of the Trigger input. A small **red light** at the upper-left of the Trigger input is lit when a fixed count is selected. When a trigger cable is connected, each output channel is triggered independently from the corresponding trigger channel. *In total, this means the module can output up to 64 random values simultaneously (4 outputs × 16 channels).*

Via the context menu you can enable **Slew** by setting a fixed slewed time up to 10 seconds. **Adaptive** slew time matches the slew duration to the current LFO period (including Rate Chaos), or to the frequency of an incoming trigger interval. Especailly when using the internal LFO with adaptive slew time, and the slew shape set to **S-Curve** the random output will appear as smooth curves in stead of the default stepped output. The RGB light at the upper-right of the Trigger input shows the selected slew time (off at 0, green→red for longer times, blue for Adaptive).

![Screenshot of Random-4](module/Random4.png) 

**TIP**: The built-in slew is enough for smooth transitions between Random values, however you can instead process the output through a [Slew 2 or Slew 4](commonSlew.md) if you want separate rise/fall times or other slew-shapes. 

Other random modules: [Random Curve](RandomCurve.md), [Chaos](Chaos.md).

[Go back to modules overview](plugin.md#modules)
