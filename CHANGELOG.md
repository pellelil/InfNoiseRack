# 2.0.3 (In development)
Ensured all module descriptions in manifest are identical to module descriptions in the manual. Also updated the manifest tags for several modules, e.g. LMCP2, VCMP1 and VCMP2 Mk I, are all able to work as 2-to-1 switches thanks to their "True"/"False" inputs in the top of the modules, but were not tagged as "Switch" in the maninfest. Various minor updates to the manual (e.g. added badges to highlight module features, and changed all module headers). Added support for multi-stage (color) push-buttons (needed one for the ADR Envelope).

## New modules
- [ADR Envelope](doc/ADREnvelope.md#adr-envelope): Attack-Delay-Release Envelope with attack/release time and shape (technically it is an Attack-Sustain-Delay-Release Envelope).

## Updated modules
- [Manuel Push 2](doc/ManCV.md#manuel-push-2): In previous release this module was not able to react to trigger-inputs as the manual otherwise stated. There is now a gate/trigger-mode switch next to the input that lets you select whether the input should be detected as a gate or a trigger (defaults to gate as previous version).
- [Cross-fade switch 4to1](doc/Switch.md#cross-fade-switch-4to1): Using the context menu you can now specify which volt-level the input should normalize to when not connected (e.g. if you need to switch/fade a fixed volt level to up to 4 different outputs). It defaults to 0V as previous version.

## Fixed modules
- [Simple LFO4-ss](doc/LFO.md#simple-lfo4-ss): Phase light for LFO-4 and sync lights were not set correctly.
- [Turing Machine](doc/TuringMachine.md#turing-machine): Min/Center/Max toggle-button appeared pressed (green) when adding module to rack. Min/Center/Max mode-lights changed to tiny-lights.
- [LFO1](doc/LFO.md#lfo1): Sync-output remained being monophonic even when the frequency-input was polyphonic.
- [Fold](doc/Fold.md#fold): Output port appeared to be monophonic ("red circle" was missing), but would output polyphonic signals.

# 2.0.2 
Fixed various warnings based on cppcheck report from VCV. Going forward I now have a local release-check script I can run, which monitors the various issues reported from VCV. So future releases should be less painfull.


# 2.0.1 (not included in VCV Library due various issues)
All panel SVG-files were simplified and reduced in file-size, but appears the same as before (e.g. a hidden layer inside the files were removed).

## Updated Modules
- [Poly-Merge](doc/PolyTools.md#poly-merge): Ligths (next to each input port) indicating channel count are now color-coded, whether input is availble (green) or whether it will simply output 0V (red). Ports that are not included in the output still shows as dimmed light. *Might have fixed a bug from 2.0.0, otherwise it was a bug caused by the new code that was fixed. Anyway no known bugs.*
- [Poly-Split](doc/PolyTools.md#poly-split): Output ports that have available input still shows with a green light (whether cable is connected or not). However lights for output ports without available input will either show as dimmed (when no cable is connected), or red (if a cable is connected). This works both in mono- and poly-mode. E.g. if you in Poly-mode input a 6-channel polyphonic cable, and only insert a cable into output port 8, it will still output a 8 channel signal. The lights for port 1-6 will be green (as input is available), however the lights for ports 7 and 8 will now show as red (as no input is available, and ports 7 and 8 will simply output 0V).


# 2.0.0 (First release - not included in VCV Library due to manifest issues)

## New Modules
As everying in this very-first release is new, it only consists of "New Modules" (nothing changed/fixed in this version). Here below is a list of all the modules included in this (1st) version.

- [Simple LFO4-ss](doc/LFO.md#simple-lfo4-ss)
- [Simple LFO4-st](doc/LFO.md#simple-lfo4-st)
- [Tiny LFO](doc/LFO.md#tiny-lfo)
- [LFO1](doc/LFO.md#lfo1)
- [Phase-Driven LFO](doc/LFO.md#phase-driven-lfo)
- [Tweak-2 Mk I](doc/Tweak.md#tweak-2-mk-i)
- [Tweak-2 Mk II](doc/Tweak.md#tweak-2-mk-ii)
- [Tweak-4 Mk I](doc/Tweak.md#tweak-4-mk-i)
- [Tweak-4 Mk II](doc/Tweak.md#tweak-4-mk-ii)
- [Tweak-8](doc/Tweak.md#tweak-8)
- [VCA-2](doc/Tweak.md#vca-2)
- [VCA-4 Mk I](doc/Tweak.md#vca-4-mk-i)
- [VCA-4 Mk II](doc/Tweak.md#vca-4-mk-ii)
- [Clamp 4](doc/Tweak.md#clamp-4)
- [Auto-Scale 4](doc/Tweak.md#auto-scale-4)
- [Manuel Trigger, Gate and CV](doc/ManCV.md#manuel-trigger-gate-and-cv)
- [Manuel Push 2](doc/ManCV.md#manuel-push-2)
- [Manuel Trigger 8](doc/ManCV.md#manuel-trigger-8)
- [Manuel Gate 8](doc/ManCV.md#manuel-gate-8)
- [Manuel CV 8 Mk I](doc/ManCV.md#manuel-cv-8-mk-i)
- [Manuel CV 8 Mk II](doc/ManCV.md#manuel-cv-8-mk-ii)
- [ManMix4 Mk I](doc/ManCV.md#manuel-mix-4-mk-i)
- [ManMix4 Mk II](doc/ManCV.md#manuel-mix-4-mk-ii)
- [Manuel Mix 4 Stereo](doc/ManCV.md#manuel-mix-4-stereo)
- [Mute 2](doc/ManCV.md#mute-2)
- [Manuel Mute 8](doc/ManCV.md#manuel-mute-8)
- [CV-Toggle 8](doc/ManCV.md#cv-toggle-8)
- [CV to Gate](doc/CvToGt.md#cv-to-gate)
- [CV to Gate/Trigger 8](doc/CvToGtTr8.md#cv-to-gatetrigger-8)
- [Mult2x4](doc/MergeMult.md#mult2x4)
- [Merge/Mult-4](doc/MergeMult.md#mergemult-4)
- [Merge2x4](doc/MergeMult.md#merge2x4)
- [Tiny Logic Comparator-2](doc/Compare.md#tiny-logic-comparator-2)
- [Logic Comparator-2](doc/Compare.md#logic-comparator-2)
- [Logic Comparator-6x2](doc/Compare.md#logic-comparator-6x2)
- [Value Comparator-1](doc/Compare.md#value-comparator-1)
- [Value Comparator-2 Mk I](doc/Compare.md#value-comparator-2-mk-i)
- [Value Comparator-2 Mk II](doc/Compare.md#value-comparator-2-mk-ii)
- [S&H/T&H-2](doc/Shth.md#shth-2)
- [S&H/T&H-2x4](doc/Shth.md#shth-2x4)
- [Sample and Update](doc/Shth.md#sample-and-update)
- [Poly-Merge](doc/PolyTools.md#poly-merge)
- [Poly-Split](doc/PolyTools.md#poly-split)
- [Poly-Stereo](doc/PolyTools.md#poly-stereo)
- [Poly-Quad](doc/PolyTools.md#poly-quad)
- [Poly-Shuffle](doc/PolyTools.md#poly-shuffle)
- [Poly-Tweak Mk I](doc/PolyTools.md#poly-tweak-mk-i)
- [Poly-Tweak Mk II](doc/PolyTools.md#poly-tweak-mk-ii)
- [Poly-Logical Compare](doc/PolyTools.md#poly-logical-compare)
- [Poly-Value Compare](doc/PolyTools.md#poly-value-compare)
- [Poly-Offset](doc/PolyTools.md#poly-offset)
- [Poly-Scale](doc/PolyTools.md#poly-scale)
- [Cross-fade switch 1to4](doc/Switch.md#cross-fade-switch-1to4)
- [Cross-fade switch 4to1](doc/Switch.md#cross-fade-switch-4to1)
- [Cross-fade 1x2](doc/CrossFade.md#cross-fade-1x2)
- [Cross-fade 4x1](doc/CrossFade.md#cross-fade-4x1)
- [Bernoulli Switch](doc/Switch.md#bernoulli-switch)
- [ON/OFF Switch](doc/Switch.md#onoff-switch)
- [Combine](doc/Switch.md#combine)
- [Sign](doc/Sign.md#sign)
- [Sign4 Mk I](doc/Sign.md#sign4-mk-i)
- [Sign4 Mk II](doc/Sign.md#sign4-mk-ii)
- [Fold](doc/Fold.md#fold)
- [Wave Shaper 2](doc/WaveShaper2.md#wave-shaper-2)
- [Ring Modulator 3](doc/RingMod3.md#ring-modulator-3)
- [Increment/Decrement Offset](doc/IncDecOffset.md#incrementdecrement-offset)
- [Delta-4](doc/Delta4.md#delta-4)
- [Flip-Flop](doc/FlipFlop.md#flip-flop)
- [Slope Detector 2](doc/SlopeDetector2.md#slope-detector-2)
- [Patch](doc/Patch.md#patch)
- [Random-4](doc/Random.md#random-4)
- [Random Curve](doc/Random.md#random-curve)
- [Arm 3 XY](doc/Arm3XY.md#arm-3-xy)
- [Turing Machine](doc/TuringMachine.md#turing-machine)
- [Bits-to-Value](doc/Bits.md#bits-to-value)
- [Value-to-Bits](doc/Bits.md#value-to-bits)
