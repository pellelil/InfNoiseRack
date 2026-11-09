# Cross-fade switch 1to4
![Features](features/Polyphonic-Input--Output-green.svg)
![Features](features/Internal--Fade-Yes-green.svg)<br>
This module allows you to switch or cross-fade a single input signal across up to four outputs. When the cross-fade button (next to the switch knob) is activated (green), the module smoothly transitions between outputs. When deactivated, it functions as a simple switch, selecting only one output at a time (or blending over the context-menu **Fade time** if that is not 0). *Some general info regarding the Cross-fade switches are listed in [Common cross-fade switch features](commonSwitch.md).*

A red button next to the switch/cross-fade CV input enables trigger mode. When activated, the CV input is treated as a trigger, cycling through outputs based on the "Trigger Order" setting, which can be adjusted via the context menu. As noted in [Common cross-fade switch features](commonSwitch.md), the reset input is only functional when the module is configured for switch mode with trigger input, in which case the trim-knob is also ignored (and covered by an overlay). When not in trigger mode, the Reset input is covered instead.

An input cable is not required if you want to route a fixed voltage. By default the CV input is normalized to 0V when no cable is connected. Via the context menu you can assign a custom normalization voltage, then use the switch/cross-fade to send that fixed level to the selected output(s).

![Screenshot of Cross-fade Switch 1to4](module/CrossFadeSwitch1to4.png) 

Other cross-fade switch modules: [Cross-fade switch 4to1](CrossFadeSwitch4to1.md).

[Go back to modules overview](plugin.md#modules)
