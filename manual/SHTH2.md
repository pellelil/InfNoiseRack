# S&H/T&H-2
![Features](features/Polyphonic-Input--Output-green.svg)
![Features](features/Process--Quality-Auto-green.svg)
![Features](features/Quantize-Yes-green.svg)
![Features](features/Rate--Chaos-Yes-green.svg)<br>
The SHTH2 module operates in Sample-and-Hold (S&H), Track-and-Hold (T&H), or Hold-and-Track (H&T) mode, as selected via the three-way switch. If a Clock input is provided, the module will use it to determine when to sample or hold/track the input signal. Otherwise, the built-in LFO takes over, with its frequency adjustable via the knob near the Hold input. *Some general info regarding the S&H/T&H/H&T are listed in [Common S&H features](commonShth.md).*

The Clock inputs in both sections support polyphonic signals, **allowing you to sample/track/hold individual channels independently**. If you provide a monophonic Clock signal, all channels will follow that same Clock input. When using a polyphonic Clock signal, make sure it has at least as many channels as the signal input. Likewise, when using a polyphonic Clock together with the internal noise generator (i.e., no external input connected), the module will automatically match the number of output channels to the number of channels present in the Clock input.

If no external input signal is supplied, the module generates random values using its internal white noise generator, which by default produces values in the range of -5V to +5V. However, this range can be adjusted in the context menu. The noise generator initially outputs a monophonic random signal, but it can be configured to produce a polyphonic signal with up to 16 channels, where each channel generates a unique random value.

![Screenshot of SHTH2](module/SHTH2.png) 

**TIP**: You can set different sample-and-hold frequencies in the **A** and **B** sections, then route the **A** and **B** outputs into a single [Bernoulli Switch](BernoulliSwitch.md). In this setup, the Bernoulli Switch determines whether the output comes from the “fast” or the “slow” sample-and-hold path. This allows you to create a signal that "randomly" alternates between frequently changing and Infrequently changing values, with the probability controlled by the Bernoulli Switch.

Other S&H modules: [S&H/T&H-2x4](SHTH2x4.md), [Sample and Update](SampleAndUpdate.md), [Stack](Stack.md), [Stack expander](StackExpander.md), [Stack bank expander](StackBankExpander.md).

[Go back to modules overview](plugin.md#modules)
