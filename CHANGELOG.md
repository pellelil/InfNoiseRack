# 2.0.3 (In development)
Ensured all module descriptions in manifest are identical to module descriptions in the manual. Also updated the manifest tags for several modules, e.g. LMCP2, VCMP1 and VCMP2 Mk I, are all able to work as 2-to-1 switches thanks to their "True"/"False" inputs in the top of the modules, but were not tagged as "Switch" in the maninfest. Various minor updates to the manual (e.g. added badges to highlight module features). Added support for multi-stage (color) push-buttons (needed one for the ADR Envelope).

## New modules
- [ADR Envelope](doc/ADREnvelope.md#adr-envelope): Attack-Delay-Release Envelope with attack/release time and shape (technically it is an Attack-Sustain-Delay-Release Envelope).

## Updated modules
- [Manuel Push 2](doc/ManCV.md#manuel-push-2p): In previous release this module was not able to react to trigger-inputs as the manual otherwise stated. There is now a gate/trigger-mode switch next to the input that lets you select whether the input should be detected as a gate or a trigger (defaults to gate as previous version).
- [Cross-fade switch 4to1](doc/Switch.md#cross-fade-switch-4to1p): Using the context menu you can now specify which volt-level the input should normalize to when not connected (e.g. if you need to switch/fade a fixed volt level to up to 4 different outputs). It defaults to 0V as previous version.

## Fixed modules
- [Simple LFO4-ss](doc/LFO.md#simple-lfo4-ss): Phase light for LFO-4 and sync lights were not set correctly.
- [Turing Machine](doc/TuringMachine.md#turing-machineq): Min/Center/Max toggle-button appeared pressed (green) when adding module to rack. Min/Center/Max mode-lights changed to tiny-lights.
- [LFO1](doc/LFO.md#lfo1): Sync-output remained being monophonic even when the frequency-input was polyphonic.
- [Fold](doc/Fold.md#foldpoq): Output port appeared to be monophonic ("red circle" was missing), but would output polyphonic signals.

# 2.0.2 
Fixed various warnings based on cppcheck report from VCV. Going forward I now have a local release-check script I can run, which monitors the various issues reported from VCV. So future releases should be less painfull.


# 2.0.1 (not included in VCV Library due various issues)
All panel SVG-files were simplified and reduced in file-size, but appears the same as before (e.g. a hidden layer inside the files were removed).

## Updated Modules
- [Poly-Merge](doc/PolyTools.md#poly-mergep): Ligths (next to each input port) indicating channel count are now color-coded, whether input is availble (green) or whether it will simply output 0V (red). Ports that are not included in the output still shows as dimmed light. *Might have fixed a bug from 2.0.0, otherwise it was a bug caused by the new code that was fixed. Anyway no known bugs.*
- [Poly-Split](doc/PolyTools.md#poly-splitp): Output ports that have available input still shows with a green light (whether cable is connected or not). However lights for output ports without available input will either show as dimmed (when no cable is connected), or red (if a cable is connected). This works both in mono- and poly-mode. E.g. if you in Poly-mode input a 6-channel polyphonic cable, and only insert a cable into output port 8, it will still output a 8 channel signal. The lights for port 1-6 will be green (as input is available), however the lights for ports 7 and 8 will now show as red (as no input is available, and ports 7 and 8 will simply output 0V).


# 2.0.0 (First release - not included in VCV Library due to manifest issues)

## New Modules
As everying in this very-first release is new, it only consists of "New Modules" (nothing changed/fixed in this version). Here below is a list of all the modules included in this (1st) version.

- [Simple LFO4-ss](doc/LFO.md#simple-lfo4-ss)
- [Simple LFO4-st](doc/LFO.md#simple-lfo4-st)
- [Tiny LFO](doc/LFO.md#tiny-lfo)
- [LFO1](doc/LFO.md#lfo1)
- [Phase-Driven LFO](doc/LFO.md#phase-driven-lfo)
- [Tweak-2 Mk I](doc/Tweak.md#tweak-2-mk-ipq)
- [Tweak-2 Mk II](doc/Tweak.md#tweak-2-mk-iipq)
- [Tweak-4 Mk I](doc/Tweak.md#tweak-4-mk-ipq)
- [Tweak-4 Mk II](doc/Tweak.md#tweak-4-mk-iiq)
- [Tweak-8](doc/Tweak.md#tweak-8q)
- [VCA-2](doc/Tweak.md#vca-2p)
- [VCA-4 Mk I](doc/Tweak.md#vca-4-mk-ip)
- [VCA-4 Mk II](doc/Tweak.md#vca-4-mk-iip)
- [Clamp 4](doc/Tweak.md#clamp-4po)
- [Auto-Scale 4](doc/Tweak.md#auto-scale-4p)
- [Manuel Trigger, Gate and CV](doc/ManCV.md#manuel-trigger-gate-and-cvpq)
- [Manuel Push 2](doc/ManCV.md#manuel-push-2p)
- [Manuel Trigger 8](doc/ManCV.md#manuel-trigger-8p)
- [Manuel Gate 8](doc/ManCV.md#manuel-gate-8p)
- [Manuel CV 8 Mk I](doc/ManCV.md#manuel-cv-8-mk-ipq)
- [Manuel CV 8 Mk II](doc/ManCV.md#manuel-cv-8-mk-iipq)
- [ManMix4 Mk I](doc/ManCV.md#manuel-mix-4-mk-ip)
- [ManMix4 Mk II](doc/ManCV.md#manuel-mix-4-mk-iip)
- [Manuel Mix 4 Stereo](doc/ManCV.md#manuel-mix-4-stereop)
- [Mute 2](doc/ManCV.md#mute-2p)
- [Manuel Mute 8](doc/ManCV.md#manuel-mute-8pq)
- [CV-Toggle 8](doc/ManCV.md#cv-toggle-8pq)
- [CV to Gate](doc/CvToGt.md#cv-to-gate)
- [CV to Gate/Trigger 8](doc/CvToGtTr8.md#cv-to-gatetrigger-8)
- [Mult2x4](doc/MergeMult.md#mult2x4p)
- [Merge/Mult-4](doc/MergeMult.md#mergemult-4pq)
- [Merge2x4](doc/MergeMult.md#merge2x4pq)
- [Tiny Logic Comparator-2](doc/Compare.md#tiny-logic-comparator-2p)
- [Logic Comparator-2](doc/Compare.md#logic-comparator-2p)
- [Logic Comparator-6x2](doc/Compare.md#logic-comparator-6x2p)
- [Value Comparator-1](doc/Compare.md#value-comparator-1p)
- [Value Comparator-2 Mk I](doc/Compare.md#value-comparator-2-mk-ip)
- [Value Comparator-2 Mk II](doc/Compare.md#value-comparator-2-mk-iip)
- [S&H/T&H-2](doc/Shth.md#shth-2paq)
- [S&H/T&H-2x4](doc/Shth.md#shth-2x4paq)
- [Sample and Update](doc/Shth.md#sample-and-updatepq)
- [Poly-Merge](doc/PolyTools.md#poly-mergep)
- [Poly-Split](doc/PolyTools.md#poly-splitp)
- [Poly-Stereo](doc/PolyTools.md#poly-stereop)
- [Poly-Quad](doc/PolyTools.md#poly-quadp)
- [Poly-Shuffle](doc/PolyTools.md#poly-shufflep)
- [Poly-Tweak Mk I](doc/PolyTools.md#poly-tweak-mk-ip)
- [Poly-Tweak Mk II](doc/PolyTools.md#poly-tweak-mk-iip)
- [Poly-Logical Compare](doc/PolyTools.md#poly-logical-comparep)
- [Poly-Value Compare](doc/PolyTools.md#poly-value-comparep)
- [Poly-Offset](doc/PolyTools.md#poly-offsetp)
- [Poly-Scale](doc/PolyTools.md#poly-scalep)
- [Cross-fade switch 1to4](doc/Switch.md#cross-fade-switch-1to4p)
- [Cross-fade switch 4to1](doc/Switch.md#cross-fade-switch-4to1p)
- [Cross-fade 1x2](doc/CrossFade.md#cross-fade-1x2p)
- [Cross-fade 4x1](doc/CrossFade.md#cross-fade-4x1p)
- [Bernoulli Switch](doc/Switch.md#bernoulli-switchp)
- [ON/OFF Switch](doc/Switch.md#onoff-switchp)
- [Combine](doc/Switch.md#combinep)
- [Sign](doc/Sign.md#signpq)
- [Sign4 Mk I](doc/Sign.md#sign4-mk-ip)
- [Sign4 Mk II](doc/Sign.md#sign4-mk-iip)
- [Fold](doc/Fold.md#foldpoq)
- [Wave Shaper 2](doc/WaveShaper2.md#wave-shaper-2poq)
- [Ring Modulator 3](doc/RingMod3.md#ring-modulator-3pq)
- [Increment/Decrement Offset](doc/IncDecOffset.md#incrementdecrement-offset)
- [Delta-4](doc/Delta4.md#delta-4-p)
- [Flip-Flop](doc/FlipFlop.md#flip-flop)
- [Slope Detector 2](doc/SlopeDetector2.md#slope-detector-2)
- [Patch](doc/Patch.md#patchp)
- [Random-4](doc/Random.md#random-4paq)
- [Random Curve](doc/Random.md#random-curveaq)
- [Arm 3 XY](doc/Arm3XY.md#arm-3-xya)
- [Turing Machine](doc/TuringMachine.md#turing-machineq)
- [Bits-to-Value](doc/Bits.md#bits-to-value)
- [Value-to-Bits](doc/Bits.md#value-to-bits)
