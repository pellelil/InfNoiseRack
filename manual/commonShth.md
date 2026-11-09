# Sample-and-hold / Track-and-hold / Hold-and-track
The Infinite-Noise plugin includes two SHTH modules capable of performing traditional Sample-and-Hold (**S&H**/**SH**) along with Track-and-Hold (**T&H**/**TH**), and Hold-and-Track (**H&T**/**HT**) operations. Sample-and-Hold typically responds to a trigger input *(short signal)*, determining when to sample a new input value. In contrast, Track-and-Hold and Hold-and-Track operate based on a gate signal *(typical "longer" signal)*, altering behavior depending on whether the clock-gate is high or low. The three modes function as follows:

+ **Sample-and-Hold (S&H)**: A high trigger samples the input value, which is then held until the next trigger occurs.
+ **Track-and-Hold (T&H)**: While the gate is high, the input is continuously tracked (passed through). When the gate goes low, the last value is held.
+ **Hold-and-Track (H&T)**: While the gate is high, the last input value is held. When the gate goes low, the input is tracked (passed through).

**TIP**: Since T&H and H&T typically rely on gate signals, they can also be controlled using trigger signals by passing the trigger through an [On/Off Switch](OnOffSwitch.md) module. This On/Off-Switch should be configured to toggle between "ON" (10V) and "OFF" (0V), which is equivalent to gate high-/low-levels. Routing this toggled output into the Hold input of an SHTH module allows the module to alternate between tracking and holding with each trigger received by the On/Off-Switch. Alternative a [Flip-Flop](FlipFlop.md#flip-flop) in T-mode (with the trigger input into the "T/D" port) can also be used (each trigger-input will toggle the Q output high/low).

If no external Clock input is connected, both SHTH modules features an internal LFO that can drive the Clock-rate. When active, the LFO frequency is adjustable via the knob next to the Clock input. Since T&H and H&T operate on gates rather than triggers, the internal LFOs generates a pulse wave where the high and low phases are equal by default (50% duty cycle). However, using the context menu, this ratio can be adjusted in 10% increments, from 10%/90% to 90%/10%. Additionally, in the SHTH2x4 module, if a Clock signal is plugged into the A-section, it will be normalized to the B-section, unless a separate Clock signal is supplied. This allows a single Clock input to control all 8 sample/hold pairs (4 in the A-section and 4 in the B-section).

The context menu also offers a **Rate Chaos** setting (per section) for the internal LFO, which randomizes the speed of each individual cycle so the sampling/holding happens slightly - or wildly - irregularly instead of at a perfectly steady rate. This option appears on several other modules too and is described in the [main manual](commonFeatures.md#rate-chaos). *The Rate Chaos is only used with the internal LFO, hence not in use when a Clock-input is connected.*

When generating a **polyphonic output**, there are three ways the module determines the number of channels. These are evaluated in priority order (top to bottom):
* **Polyphonic input**: If the **A input** is polyphonic, the A output will match its number of channels.
* **Polyphonic clock trigger**: If no A input is connected, the module uses its built-in noise generator, and the output channel count will match the number of channels in the clock trigger.
* **Context menu setting**: If neither an A input nor a clock-input is connected, the output channel count is determined by the context menu (default is a single channel).

By default, the built-in noise source generates bipolar random values (**white noise**) between −5V and +5V. You can change this to other ranges—such as 0V to 10V for unipolar signals—via the context menu. For "other kinds of noise" you can connect the output from a VCV Noise module (which can both do white-, pink-, red-, vilolet-, blue-, gray- and black-noise). If you need more control over the range or distribution of the random values, you may want to use a [Random-4](Random4.md) module instead, which also supports trigger-based operation or its built-in LFO.

**TIP**: For probabilistic tracking or holding, route the Clock signal through a [Bernoulli Switch](BernoulliSwitch.md) module. By connecting the signal to the Clock input of the Bernoulli Switch, and then routing either the "A" or "B" output (from the "→A/B" section) into the Clock input of the SHTH module, you can introduce randomness into whether the module tracks or holds (producing longer/shorter track/hold-times).

S&H modules: [S&H/T&H-2](SHTH2.md), [S&H/T&H-2x4](SHTH2x4.md), [Sample and Update](SampleAndUpdate.md), [Stack](Stack.md), [Stack expander](StackExpander.md), [Stack bank expander](StackBankExpander.md).

[Go back to modules overview](plugin.md#modules)
