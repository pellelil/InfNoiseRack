# Merge/Mult
Although basic Merge/Mult modules are not strictly necessary in VCV Rack—since multiple cables can be connected to the same output, and multiple signals can be merged into the same input, Merge/Mult modules still offer advantages. They can help reduce cable clutter, making it easier to route signals efficiently. For example, instead of running multiple long cables across the rack, a single cable can carry a signal to a Mult module, which then distributes it to multiple nearby destinations. Additionally, the Infinite-Noise Merge modules offer more than just standard summing. They allow customized signal mixing, including controls over negative values and merge behavior, which are useful for certain processing needs.

A merger functions as a **precision adder**, meaning all input signals are summed together at the output (however, by default clamped between -12V and +12V). If the sum of the signals is negative, the output will also be negative. To control this behavior, the **negative-mode** setting (found in the context menu) allows you to choose how negative values are handled:

+ **Signed**: Default mode, where Negative remains negative (keeps their sign).
+ **Zero**: Converts all negative values to 0V.
+ **Absolute-input**: Converts negative input-values to positive (flipping polarity of negative) - before summing/mix.
+ **Absolute-output**: Converts negative output-values to positive (flipping polarity of negative) - after summing/mix.

The merge-sections have a small button that lets you toggle **merge-mode**. By default this button is unpressed which means the default merge/sum-mode is active (inputs are summed), and the module functions as a precision adder. Pressing this button, it will turn green, and toggle to **mix-mode**, where the module insted will perform an averaging mix of the supplied inputs. E.g. if you supply a signal with 4 volt, and another of 3, it will output 3.5 ((4+3)/2 = 3.5). *Remember in merge-mode, that output ports are by default clamped to the range -12v/+12v, so with many/high inputs, you increase the chance the output is being clamped.*

**TIP**: Since Merge modules can function as mixers, they can be used to create custom random distributions from random noise sources. For example, feeding two random/noise outputs (e.g., from a [a SHTH2 module](SHTH2.md)) into a Merge module set to mix-mode results in a **triangular distribution**. Normally, each random input ranges from -5V to +5V, and mixing them averages the values. This creates a higher probability of values being near 0V, while extreme values (close to -5V or +5V) occur less frequently. By mixing three or more random signals instead of two, the output distribution approximates a standard **bell curve (normal distribution)**. If the inputs are scaled to -1V to +1V, the result can be used for 1V/oct control, creating "random melodies" with a natural pitch distribution—favoring the root note more often than full octave jumps.

For musical applications, the Merge module includes an optional quantization feature (enabled via the context menu) that snaps output values to the nearest note (1/12V per semitone). This can be used to generate random note sequences centered around a fixed root note (set via an external offset voltage). However, if you need the output to conform to a specific musical scale/key, you still need to pass it through a dedicated quantizer module, where you can select scale/key.

Merge/Mult modules: [Mult2x4](Mult2x4.md), [Merge2x4](Merge2x4.md), [Merge/Mult-4](MergeMult4.md).

[Go back to modules overview](plugin.md#modules)
