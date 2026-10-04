# Mixer modules
This section covers the dedicated mixer modules in Infinite-Noise—modules built for combining multiple signals into a mixed output, rather than treating mix as a side feature of attenuverting or VCA processing. The Manual Mix 4 family currently provides mono and stereo four-input mixers with per-input gain, a master mix level, shared averaging and unity mix modes, and optional per-section inverted amplification. Mk I is fully manual; Mk II and Stereo add amplification CV inputs with chainable normalization.

Beside these dedicated mixer modules described below, a few [Tweak modules](Tweak.md) also include averaging mix: [Tweak-2 Mk II](Tweak.md#tweak-2-mk-ii) (2 signals), [Tweak-4 Mk II](Tweak.md#tweak-4-mk-ii) (up to 4), and [Tweak-8](Tweak.md#tweak-8) (up to 8).

## Manuel Mix 4 Mk I
![Features](https://img.shields.io/badge/Polyphonic-Input--Output-green.svg?style=flat-square)
![Features](https://img.shields.io/badge/Attenuate-2x-green.svg?style=flat-square)<br>
The Manual-Mix4 Mk I is a compact, 2HP 100% manually controlled mixer featuring four inputs and one mix output. Each input has an individual mix knob that allows amplification within a range of 0% to 200%, with the default setting at 100%. At the top of the module there is a master-mix knob, which controls the overall amplification of the mixed output, also adjustable between 0% and 200%. This module supports polyphonic signals, but if inputs contain different numbers of channels, the output will adopt the highest channel count among them. If mixing monophonic and polyphonic signals, the monophonic signal will be applied equally to each polyphonic channel. However, if multiple polyphonic signals are used, they should ideally contain the same number of channels for proper mixing.

The mixer can operate in two distinct modes (however defaults to averaging mix). Next to the "MIX" label at the bottom there is a light indicating the active mix mode (dim=Averaging mix, Red=Unity mix):
+ **Averaging mix**: All inputs are summed (according to mix-knobs) and then divided by the number of inputs in use. So if only connecting inputs 1-3 the output will be as follows (I#=Input #, M#=Mix-knob #, and MM = Master mix-knob): "Output = ((I1 x M1 + I2 x M2 + I3 x M3) / 3) x MM". The reason for the "/ 3" is because only 3 inputs are in use in this example. The benefit of the Averaging mix is that you typically don't need to make huge adjustments to master mix-level as you add/remove connections.
+ **Unity mix**: All inputs are simply summed (according to mix-knobs). So if only connecting inputs 1-3 the output will be as follows (I#=Input #, M#=Mix-knob #, and MM = Master mix-knob): "Output = (I1 x M1 + I2 x M2 + I3 x M3) x MM". As you add/remove connections you typically need to adjust the master mix-knob. However even if you only mix in a fraction of some of the inputs, you should be able to reach the desired final output level. *Remember to disable/change the default +/-12V clipping if you plan to exceed this range for some reason.*

Each of the four mix knobs can be set to **inverted** amplification via the context menu (a small red light beside the knob indicates inversion).

![Screenshot of Manual Mix-4 Mk I](module/ManMix4I.png)

**TIP**: ManMix4 Mk I has manual knobs only—no amplification CV inputs. If you need CV-controlled mixing levels, use [Manuel Mix 4 Mk II](#manuel-mix-4-mk-ii) (mono) or [Manuel Mix 4 Stereo](#manuel-mix-4-stereo) (stereo). If you only need to amplify the mixed output using CV, you can feed the output to a [VCA-2](Tweak.md#vca-2) module. For mixing up to 8 mono-signals or up to 4 stereo-signals with per-signal CV, consider a [Tweak-8](Tweak.md#tweak-8) or [Tweak-4II](Tweak.md#tweak-4-mk-ii) module instead.

## Manuel Mix 4 Mk II
![Features](https://img.shields.io/badge/Polyphonic-Input--Output-green.svg?style=flat-square)
![Features](https://img.shields.io/badge/Attenuate-2x-green.svg?style=flat-square)<br>
The 4HP Manual Mix 4 Mk II is similar to the Mk I, but adds amplification CV inputs and a separate output for each of the four sections (A–D) in addition to the mix output at the bottom. Mix-knob CV inputs are normalized to the previous section (A normalizes to 0V; B to A; C to B; D to C). Because there are no trim knobs, red/green toggle buttons beside the normalization arrows control whether each downstream input follows the previous one—green means enabled (default), red means disabled.

Since the amplification CV is without trim, it should be applied with the exact input range. The amplification knobs default to the center position (100%), so with the knob at that position a +5V input would be equivalent to the knob being set to 200%, or a −5V input would be equivalent to setting the knob to 0%. If you instead turn the knob fully counter-clockwise to 0%, a 5V amplification input would be the same as setting the knob to 100%, and a 10V amplification input would be the same as setting the knob to 200% (10V input range = full knob range).

The **individual section outputs** (A–D) carry each input multiplied by its own knob/CV gain only—they are not scaled by the master knob or mix-mode averaging/unity logic. The **mix output** sums all connected inputs (with per-section gain), applies averaging or unity scaling, then applies the master knob/CV gain.

Like Mk I, the module supports **averaging** and **unity** mix modes (context menu; light beside the mix output: dim=Averaging, Red=Unity—see Mk I above). Each mix knob can also be **inverted** via the context menu.

![Screenshot of Manual Mix-4 Mk II](module/ManMix4II.png)

## Manuel Mix 4 Stereo
![Features](https://img.shields.io/badge/Polyphonic-Input--Output-green.svg?style=flat-square)
![Features](https://img.shields.io/badge/Attenuate-2x-green.svg?style=flat-square)<br>
The Manual-Mix4st is the stereo variant: four stereo input sections (A through D), each with left and right polyphonic inputs, and mixed left/right outputs at the bottom (no per-section outputs). It shares the Mk II amplification CV inputs and normalization buttons (B→A, C→B, D→C; see Mk II above for CV range and behavior).

By default—indicated by green lights between the left and right input columns—if you supply only a left input (e.g. a mono signal) and leave the right input unplugged, the right channel is normalized to the left. This **mono-to-stereo** behavior can be disabled in the context menu (lights turn red).

The module supports **averaging** and **unity** mix modes and per-section **inverted** amplification like Mk I (context menu). The mix-mode light is centered between the left and right mix outputs at the bottom.

![Screenshot of Manual Mix-4ST](module/ManMix4st.png)

[Go back to modules overview](manual.md#modules)
