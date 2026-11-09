# Merge/Mult-4
![Features](features/Polyphonic-Input--Output-green.svg)
![Features](features/Quantize-Yes-green.svg)<br>
The merge section in the top includes four inputs and a single output. By default, all inputs are **summed** together and sent to the output. However, using the small button, you can switch to **Mix-mode**, where the output is an averaging mix of the active inputs rather than a direct sum. Regarding polyphony, the output will have the same number of channels as the input with the most channels. If, for example, one input has 8 channels and another has 4 channels, the output will have 8 channels. However, the last 4 channels of the 8-channel input will be merged with 0V since the second signal only has 4 channels. *Some general info regarding these modules are listed in [Common merge/mult features](commonMergeMult.md).*

The mult section in the bottom has a single input and four outputs. If no cable is inserted into the mult input, it automatically normalizes to the output of the merge section, making it easy to distribute the merged signal to multiple destinations. All four outputs replicate the input signal exactly, and the number of polyphonic channels matches the input. By default, all outputs are clipped to -12V/+12V, but this can be adjusted or disabled via the context menu.

![Screenshot of Merge/Mult-4](module/MergeMult4.png) 

Other merge/mult modules: [Mult2x4](Mult2x4.md), [Merge2x4](Merge2x4.md).

[Go back to modules overview](plugin.md#modules)
