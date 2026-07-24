# Merge/Mult
Although basic Merge/Mult modules are not strictly necessary in VCV Rack—since multiple cables can be connected to the same output, and multiple signals can be merged into the same input, Merge/Mult modules still offer advantages. They can help reduce cable clutter, making it easier to route signals efficiently. For example, instead of running multiple long cables across the rack, a single cable can carry a signal to a Mult module, which then distributes it to multiple nearby destinations. Additionally, the Infinite-Noise Merge modules offer more than just standard summing. They allow customized signal mixing, including controls over negative values and merge behavior, which are useful for certain processing needs.

A merger functions as a **precision adder**, meaning all input signals are summed together at the output (however, by default clamped between -12V and +12V). If the sum of the signals is negative, the output will also be negative. To control this behavior, the **negative-mode** setting (found in the context menu) allows you to choose how negative values are handled:

+ **Signed**: Default mode, where Negative remains negative (keeps their sign).
+ **Zero**: Converts all negative values to 0V.
+ **Absolute-input**: Converts negative input-values to positive (flipping polarity of negative) - before summing/mix.
+ **Absolute-output**: Converts negative output-values to positive (flipping polarity of negative) - after summing/mix.

The merge-sections have a small button that lets you toggle **merge-mode**. By default this button is unpressed which means the default merge/sum-mode is active (inputs are summed), and the module functions as a precision adder. Pressing this button, it will turn green, and toggle to **mix-mode**, where the module insted will perform an averaging mix of the supplied inputs. E.g. if you supply a signal with 4 volt, and another of 3, it will output 3.5 ((4+3)/2 = 3.5). *Remember in merge-mode, that output ports are by default clamped to the range -12v/+12v, so with many/high inputs, you increase the chance the output is being clamped.*

**TIP**: Since Merge modules can function as mixers, they can be used to create custom random distributions from random noise sources. For example, feeding two random/noise outputs (e.g., from a [a SHTH2 module](Shth.md#shth-2paq)) into a Merge module set to mix-mode results in a **triangular distribution**. Normally, each random input ranges from -5V to +5V, and mixing them averages the values. This creates a higher probability of values being near 0V, while extreme values (close to -5V or +5V) occur less frequently. By mixing three or more random signals instead of two, the output distribution approximates a standard **bell curve (normal distribution)**. If the inputs are scaled to -1V to +1V, the result can be used for 1V/oct control, creating "random melodies" with a natural pitch distribution—favoring the root note more often than full octave jumps.

For musical applications, the Merge module includes an optional quantization feature (enabled via the context menu) that snaps output values to the nearest note (1/12V per semitone). This can be used to generate random note sequences centered around a fixed root note (set via an external offset voltage). However, if you need the output to conform to a specific musical scale/key, you still need to pass it through a dedicated quantizer module, where you can select scale/key.

## Mult2x4(p)
This 2 HP module have 2 sections, each with an input and 4 outputs, where the 4 outputs clones the inputs. If you don't input a signal into the input-port of the lower section, it will normalize to the input of the upper section. Hence the module easily doubles as mult with 1 input and 8 outputs. If the input is polyphonic (up to 16 channels), all outputs will preserve that polyphony. By default, all outputs are clipped to -12V/+12V to prevent excessive signal levels. However, this clipping can be adjusted or disabled via the context menu.

![Screenshot of Mult-2x4](module/Mult2x4.png) 

**TIP**: Typical in VCV when a module sets its outputs, other modules (connected via cables) wont see this signal until they are processed at the next cycle, hence while not its designed purpose, a Mult can also be used as a 1 cycle dela (in few/edge cases this might be exactly what you need).

## Merge2x4(pq)
This 2 HP module have 2 sections, each with 4 inputs and a single output (both of these sections are processed individually). The default behavior is to **sum** (add) all inputs together before outputting the combined signal. By default, the output is clipped to -12V/+12V, but this can be modified or disabled via the context menu. Additionally, you can switch the module to **Mix-mode**, in which case the sum of inputs is divided by the number of active signals, producing an averaging mix output instead of a sum.

In terms of polyphony, the module determines the number of output channels based on the input with the highest polyphony count. If, for example, you connect one input signal with 8 channels and another with 4 channels, the output will have 8 channels. However, the last 4 channels of the 8-channel signal will be merged with 0V, since the second signal only has 4 channels. However, if merging an 8-channel polyphonic signal with a monophonic signal, the monophonic signal will be added to all 8 channels of the polyphonic signal.

![Screenshot of Merge2x4](module/Merge2x4.png) 

## Merge/Mult-4(pq)
The merge section in the top includes four inputs and a single output. By default, all inputs are **summed** together and sent to the output. However, using the small button, you can switch to **Mix-mode**, where the output is an averaging mix of the active inputs rather than a direct sum. Regarding polyphony, the output will have the same number of channels as the input with the most channels. If, for example, one input has 8 channels and another has 4 channels, the output will have 8 channels. However, the last 4 channels of the 8-channel input will be merged with 0V since the second signal only has 4 channels.

The mult section in the bottom has a single input and four outputs. If no cable is inserted into the mult input, it automatically normalizes to the output of the merge section, making it easy to distribute the merged signal to multiple destinations. All four outputs replicate the input signal exactly, and the number of polyphonic channels matches the input. By default, all outputs are clipped to -12V/+12V, but this can be adjusted or disabled via the context menu.

![Screenshot of Merge/Mult-4](module/MergeMult4.png) 

[Go back to modules overview](manual.md#modules)