# Sign4 Mk I
![Features](features/Polyphonic-Input--Output-green.svg)<br>
The Sign4 Mk I module provides three types of signal inversion, selectable via a three-way toggle switch at the top. By default, the module operates in **!Gt** (Gate Inversion) mode, where a high-gate is converted into a low-gate, and vice versa. Any input at or above 1V is considered a high-gate, and the module outputs 10V for high-gates and 0V for low-gates. Both the detection threshold and output levels can be customized via the context menu. In this mode, the module functions identically to the "!Gate" output of the original Sign module. Each of the four sections (A, B, C, and D) operates independently and supports both monophonic and polyphonic signals. For example, if a monophonic signal is connected to Input A, the A output will remain monophonic. If an 8-channel polyphonic signal is fed into Input B, the corresponding B output will also be 8-channel polyphonic. *Some general info regarding these modules are listed in [Common Sign4 features](commonSign4.md).*

Switching the module to **×-1** mode will multiply the input by -1, effectively inverting a bipolar signal (e.g., -3V becomes +3V). Selecting the **!R** mode (inversion within a defined Range) will invert values within a specified range, which can be adjusted via the context menu. By default, the inversion applies to a unipolar 0V to 10V range, meaning an input of 3V would be transformed into 7V (e.g. useful for flipping an uniplolar envelope-signal).

![Screenshot of Sign-4I](module/Sign4I.png)

**TIP**: The [logical compare modules](commonCompare.md) of the Infinite-Noise plugin lets you invert the logical inputs either with a knob directly on the panel or via the context menu. If you are using other compare modules without this feature, you can use a Sign4I to invert up to 4 signals - simply by using the !Gt output - before passing them on to other modules.

**TIP**: If you need to convert incoming signals into gates without inverting them, simply set the three-way switch to "!Gt" mode and adjust the context menu settings so that high-gates output 0V and low-gates output 10V. This configuration makes the module behave like the "Gate" output of the original Sign module.

Other Sign modules: [Sign](Sign.md), [Sign4 Mk II](Sign4II.md).

[Go back to modules overview](plugin.md#modules)
