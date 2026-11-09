# Merge2x4
![Features](features/Polyphonic-Input--Output-green.svg)
![Features](features/Quantize-Yes-green.svg)<br>
This 2 HP module have 2 sections, each with 4 inputs and a single output (both of these sections are processed individually). The default behavior is to **sum** (add) all inputs together before outputting the combined signal. By default, the output is clipped to -12V/+12V, but this can be modified or disabled via the context menu. Additionally, you can switch the module to **Mix-mode**, in which case the sum of inputs is divided by the number of active signals, producing an averaging mix output instead of a sum. *Some general info regarding these modules are listed in [Common merge/mult features](commonMergeMult.md).*

In terms of polyphony, the module determines the number of output channels based on the input with the highest polyphony count. If, for example, you connect one input signal with 8 channels and another with 4 channels, the output will have 8 channels. However, the last 4 channels of the 8-channel signal will be merged with 0V, since the second signal only has 4 channels. However, if merging an 8-channel polyphonic signal with a monophonic signal, the monophonic signal will be added to all 8 channels of the polyphonic signal.

![Screenshot of Merge2x4](module/Merge2x4.png) 

Other merge/mult modules: [Mult2x4](Mult2x4.md), [Merge/Mult-4](MergeMult4.md).

[Go back to modules overview](plugin.md#modules)
