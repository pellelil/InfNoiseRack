# Cross-fade switch 4to1
![Features](features/Polyphonic-Input--Output-green.svg)
![Features](features/Internal--Fade-Yes-green.svg)<br>
This module allows you to switch or cross-fade between (up to) 4 input signals, with the selected signal being sent to a single output. When the cross-fade button (next to the switch knob) is activated (green), the module smoothly transitions between inputs, blending them together. When deactivated, the module functions as a simple switch, selecting only one input at a time (or blending over the context-menu **Fade time** if that is not 0). *Some general info regarding the Cross-fade switches are listed in [Common cross-fade switch features](commonSwitch.md).*

A red button next to the switch/cross-fade CV input enables **trigger mode**. When activated, the CV input is treated as a trigger, cycling through inputs based on the "Trigger Order" setting, which can be customized via the context menu. As noted in [Common cross-fade switch features](commonSwitch.md), the reset input is only functional when the module is configured for switch mode with trigger input, in which case the trim-knob is also ignored (and covered by an overlay). When not in trigger mode, the Reset input is covered instead.

Inputs are not even required if you want to switch between fixed voltage levels. By default, all four inputs are normalized to 0V when no cables are connected. However, via the context menu, you can assign a custom normalization voltage to each input. For example, combined with trigger mode, the module can act as a probability-based switch. You could configure three inputs to output 0V and one input to output a fixed 10V (or connect a signal to it). In this setup, using it as a 4-way switch, there would be a 1-in-4 (25%) chance that the output is 10V (or your signal), and a 75% chance that it remains at 0V. *If in fade mode, you can instead fade between fixed voltage levels.*

![Screenshot of Cross-fade Switch 4to1](module/CrossFadeSwitch4to1.png) 

**TIP**: For example, you can take four different outputs from an LFO module or four different curve outputs from the Random-Curve module and feed them into the Cross-Fade Switch 4to1 module. When configured for cross-fading, the switch/cross-fade knob and/or CV input can be used to dynamically transition between the four signals. Alternatively, you might want to cross-fade between inputs from different LFOs or random modules, each running at different frequencies or using different waveforms. This allows for more complex and evolving modulation, especially when using random modules with varying output ranges and/or distributions.

Other cross-fade switch modules: [Cross-fade switch 1to4](CrossFadeSwitch1to4.md).

[Go back to modules overview](plugin.md#modules)
