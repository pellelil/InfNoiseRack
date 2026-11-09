# VCA-2
![Features](features/Polyphonic-Input--Output-green.svg)
![Features](features/Attenuate-1x-green.svg)<br>
As the name suggests, this 2 HP module contains two independent Voltage-Controlled Amplification (VCA) sections labeled A and B. Each section features a knob and CV input to control amplification. The knob allows manual adjustment of amplification from 0% to 100%, while the CV input provides dynamic control. The incoming CV signal is added to the knob's setting but is always clamped within the 0% to 100% range. If additional amplification is needed, consider using a Tweak module, which supports scaling up to 10x. *Some general info regarding the VCA-modules are listed in [Common VCA features](commonVca.md).*

The default scaling mode is linear, but this can be modified via the context menu. Both sections share the same scaling mode. As indicated by the arrow markings, the CV input for amplification in section B is normalized to the CV input of section A. This allows the same amplification signal to be applied to both sections, which is particularly useful for stereo signals (left/right channels).

At the top of the **B section**, you’ll find a small toggle button that controls **link mode**. When enabled (**green**), the amplify and trim knobs in the B section follow the settings of the A section. This makes it easy to apply identical amplification to both sections—for example, when using a single **VCA-2** module to process the left and right channels of a stereo signal. As indicated by the small arrowheads, the CV input in the B section is normalized to the CV signal connected to the A section. If you do not want to use this CV signal in the B section, simply leave the B-section trim knob at **0** (default/center position).

![Screenshot of VCA-2](module/VCA2.png)

Other VCA modules: [VCA-4 Mk I](VCA4I.md), [VCA-4 Mk II](VCA4II.md).

[Go back to modules overview](plugin.md#modules)
