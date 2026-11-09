# Sign4 modules (Mk I and Mk II)
The [Sign](Sign.md) module features nine distinct processing sections, each applying a different transformation to a single input. In contrast, the Sign4 Mk I and Mk II modules each contain four sections (labeled A, B, C, and D), but instead of performing different operations, all four sections apply the same transformation. The operation is determined by a three-way toggle switch at the top of the module. The input signals can be monophonic or polyphonic, with the number of channels preserved across the inputs and outputs.

**TIP**: The Mk I and Mk II versions together provide most of the functionality of the original Sign module, except for the +5V and -5V operations. The +5V operation converts a bipolar signal (-5V to +5V) into a unipolar signal (0V to 10V), while -5V shifts a unipolar signal (0V to 10V) into a bipolar range (-5V to +5V). If you need to add or subtract 5V, you can achieve the same result using the [Tweak-4](Tweak4I.md) module by simply applying a +5V or -5V offset to the input signal.

Sign4 modules: [Sign4 Mk I](Sign4I.md), [Sign4 Mk II](Sign4II.md).

[Go back to modules overview](plugin.md#modules)
