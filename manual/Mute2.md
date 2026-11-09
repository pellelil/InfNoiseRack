# Mute 2
![Features](features/Polyphonic-Input--Output-green.svg)
![Features](features/Internal--Fade-Yes-green.svg)<br>
While the Manual-Mute 8 module is entirely knob-operated, the Mute-2 module (2HP) offers both manual and CV-controlled muting across two independent sections. Each section features a latchable mute button and a CV input for external control. Adjacent to the mute CV input, a mode selection button toggles between gate mode (green) and trigger mode (red). By default, in gate mode, the section will mute when a high gate signal is received. However, via the context menu, you can invert this behavior, making the section mute when receiving a low gate instead. In trigger mode, each incoming trigger pulse toggles the mute state on or off. Each section also has a small red LED next to the output port, which illuminates when that section is muted. *Some general info regarding these manual-CV modules are listed in [Common manual-control features](commonManCV.md).*

In the top section of the module, a global mute button and CV input allow you to mute both sections simultaneously. This is useful when working with stereo signals, ensuring that both the left and right channels are muted at the same time. If no input signal is connected to the B-section, it will automatically normalize to the A-section’s input. This is useful when routing the same signal to two different destinations, while still allowing independent muting. Unlike the ManMute8, the Mute-2 module does not allow setting a custom mute-output value—muted outputs default to 0V, but this can be adjusted via the context menu.

![Screenshot of Mute-2](module/Mute2.png)

When the mute CV input is set to trigger mode, the module internally tracks the current mute state. If a section has been toggled to the mute state, you can reset it back to unmuted by disconnecting and reconnecting the trigger input or by initializing the module. If the "Both" section has a trigger input connected, you may also need to disconnect/reconnect that cable as well.

Via the context menu you can set a **Fade time** (0 / 1-2-5 series up to 10 s). Default is **0**, which is the instant mute. A non-zero time cross-fades between the live input and the mute voltage over that duration. **S-curve fade** (off by default) eases the mix. While fading, the red mute lights follow the mix instead of jumping instantly.

If multiple mute buttons or CV signals are used, they will be logically OR’ed together. This means that whether a mute button is pressed manually or a mute signal is sent via CV, the section will mute. Similarly, if either the individual A/B mute control or the global "Both" mute control is activated, the section will mute. Since the "Both" section and the individual A/B sections operate independently, it's recommended to mute either at the global level or per section—not both at the same time, as this may cause confusion. If uncertain, refer to the small red LED next to the output port to check the mute state.

Other manual-control modules: [Manuel Trigger, Gate and CV](ManTrGtCv.md), [Manuel Push 2](ManPush2.md), [Manuel Trigger 8](ManTrigger8.md), [Manuel Gate 8](ManGate8.md), [Manuel CV 8 Mk I](ManCV8I.md), [Manuel CV 8 Mk II](ManCV8II.md), [Manuel Mute 8](ManMute8.md), [CV-Toggle 8](CvToggle8.md).

[Go back to modules overview](plugin.md#modules)
