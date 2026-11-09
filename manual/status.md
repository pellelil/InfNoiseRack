# Status
This status list is primarily for my own benefit, but if you notice anything not behaving as expected (or described in the manual), feel free to check if there are any "known issues" mentioned below. A "known issue" is not necessarily a "bug" or something that "needs fixing" or things that "will" change. It might simply be a "note-to-self" regarding something I plan to look into or features I am considering adding or changing at a later time. 

## Manual and changelog
I tend to push changes to GitHub incrementally while working on a new development version. I aim to ensure that all source code committed to GitHub is in a working and stable state. However if you have the VCV Library version of this plug-in installed, the manual and/or changelog might describe new features that is only avaliable in the development version (still not included in the VCV Library version). Likewise when I actually push these changes to be included into the VCV Library as a version update, their workflow typical takes a week or so, before it will be available in the library. If you do run into any issues be sure to check **[The changelog](changelog.md)** as the issue might already be fixed in a version that is currently not available from the VCV Library.

## General
+ Random: As of now, the Random feature have only been specifically defined a single module. In all other modules the default VCV randomization is performed (i.e. all knobs/switch-settings are randomized, however context-menu settings are not randomized).

## Module-status/known issues
Below is a list of all modules, grouped by category. A line containing only a dash (**-**) indicates that no special notes or known issues have been recorded for that module. However, if a module is marked as **In Development** or **Needs Testing**, it means that the module is not yet finalized. This may be because it is a newly added module, a recently updated module with new features that have not yet been fully tested, or a module currently being evaluated by external beta testers. If you encounter a module with either of these labels, please use it with caution.

### LFO's
+ [SLFO4ss](SLFO4ss.md): -
+ [SLFO4st](SLFO4st.md): -
+ [TLFO](TLFO.md): -
+ [LFO1](LFO1.md): -
+ [PhaseDrivenLFO](PhaseDrivenLFO.md): -

### Tweak (attenuate/amplify, offset and mix)
+ [Tweak2I](Tweak2I.md): -
+ [Tweak2II](Tweak2II.md): -
+ [Tweak4I](Tweak4I.md): -
+ [Tweak4II](Tweak4II.md): -
+ [Tweak8](Tweak8.md): -
+ [VCA2](VCA2.md): -
+ [VCA4I](VCA4I.md): -
+ [VCA4II](VCA4II.md): -
+ [Flux4](Flux4.md#flux-4): New module **Needs testing**
+ [Clamp4](Clamp4.md): -
+ [AutoScale4](AutoScale4.md): -

### Controllers/converters
+ [ManTrGtCv](ManTrGtCv.md): -
+ [ManPush2](ManPush2.md): -
+ [ManTrigger8](ManTrigger8.md): -
+ [ManGate8](ManGate8.md): -
+ [ManCV8I](ManCV8I.md): -
+ [ManCV8II](ManCV8II.md): -
+ [Mute2](Mute2.md): -
+ [ManMute8](ManMute8.md): -
+ [CvToggle8](CvToggle8.md): -
+ [CvToGt](CvToGt.md#cv-to-gate): -
+ [CvToGtTr8](CvToGtTr8.md#cv-to-gatetrigger-8): -

### Mixer
+ [ManMix4I](ManMix4I.md): -
+ [ManMix4II](ManMix4II.md): -
+ [ManMix4st](ManMix4st.md): -
+ [MatrixMix5x5](MatrixMix5x5.md): Added NfZ and FfZ outputs **Needs Testing**
+ [MatrixMix4x4Stereo](MatrixMix4x4Stereo.md): -
+ [StereoBalancePan](StereoBalancePan.md#stereo-balance-pan): -

### Merge/Mult
+ [Mult2x4](Mult2x4.md): -
+ [Merge2x4](Merge2x4.md): -
+ [MergeMult4](MergeMult4.md): -

### Logic/Value-compare
+ [TinyLCMP2](TinyLCMP2.md): -
+ [LCMP2](LCMP2.md): -
+ [LCMP6x2](LCMP6x2.md): -
+ [VCMP1](VCMP1.md): -
+ [VCMP2I](VCMP2I.md): -
+ [VCMP2II](VCMP2II.md): -

### S&H, T&H, H&T
+ [SHTH2](SHTH2.md): -
+ [SHTH2x4](SHTH2x4.md): -
+ [SampleAndUpdate](SampleAndUpdate.md): -
+ [Stack](Stack.md): New module **Needs testing**
+ [StackExpander](StackExpander.md): New module **Needs testing**
+ [StackBankExpander](StackBankExpander.md): New module **Needs testing**

### Polyphonic-tools
+ [PolyMerge](PolyMerge.md): -
+ [PolySplit](PolySplit.md): -
+ [PolyStereo](PolyStereo.md): -
+ [PolyQuad](PolyQuad.md): -
+ [PolyShuffle](PolyShuffle.md): -
+ [PolyTweakI](PolyTweakI.md): -
+ [PolyTweakII](PolyTweakII.md): -
+ [PolyOffset](PolyOffset.md): -
+ [PolyScale](PolyScale.md): -
+ [PolyLCMP](PolyLCMP.md): -
+ [PolyVCMP](PolyVCMP.md): -

### Switches
+ [CxFade1x2](CxFade1x2.md): -
+ [CxFade4x1](CxFade4x1.md): -
+ [CrossFadeSwitch1to4](CrossFadeSwitch1to4.md): -
+ [CrossFadeSwitch4to1](CrossFadeSwitch4to1.md): -
+ [BernoulliSwitch](BernoulliSwitch.md): -
+ [OnOffSwitch](OnOffSwitch.md): -
+ [Combine](Combine.md): -

### Random
+ [Random4](Random4.md): -
+ [RandomCurve](RandomCurve.md): -
+ [Chaos](Chaos.md): -
+ [Arm3XY](Arm3XY.md#arm-3-xy): -

### Envelope
+ [ADREnvelope](ADREnvelope.md): -
+ [ADSDREnvelope](ADSDREnvelope.md): -
+ [EnvelopePhaseExpander](EnvelopePhaseExpander.md): -

### Slew
+ [Slew2](Slew2.md): -
+ [Slew4](Slew4.md): -

### Misc
+ [Delay3](Delay3.md#delay-3): -
+ [Sign](Sign.md): -
+ [Sign4I](Sign4I.md): -
+ [Sign4II](Sign4II.md): -
+ [Fold](Fold.md#fold): -
+ [WaveShaper2](WaveShaper2.md#wave-shaper-2): -
+ [RingMod3](RingMod3.md#ring-modulator-3): -
+ [IncDecOffset](IncDecOffset.md#incrementdecrement-offset): -
+ [Delta4](Delta4.md#delta-4): -
+ [FlipFlop](FlipFlop.md#flip-flop): -
+ [SlopeDetector2](SlopeDetector2.md#slope-detector-2): -
+ [ShiftReg](ShiftReg.md#shiftreg): New module **In Development**
+ [Patch](Patch.md#patch): -
+ [TuringMachine](TuringMachine.md#turing-machine): -
+ [BitsToValue](BitsToValue.md): -
+ [ValueToBits](ValueToBits.md): -
