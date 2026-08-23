# Status
This status list is primarily for my own benefit, but if you notice anything not behaving as expected (or described in the manual), feel free to check if there are any "known issues" mentioned below. A "known issue" is not necessarily a "bug" or something that "needs fixing" or things that "will" change. It might simply be a "note-to-self" regarding something I plan to look into or features I am considering adding or changing at a later time. If you do run into any issues be sure to also check **[The changelog](CHANGELOG.md)** as the issue might already be fixed in a development version that is currently not available from the VCV Library.

## General
+ Random: As of now, the Random feature have only been specifically defined a single module. In all other modules the default VCV randomization is performed (i.e. all knobs/switch-settings are randomized, however context-menu settings are not randomized).

## Module-status/known issues
Below is a list of all modules, grouped by category. A line containing only a dash (**-**) indicates that no special notes or known issues have been recorded for that module. I aim to ensure that all source code committed to GitHub is in a working and stable state. However, if a module is marked as **In Development** or **Needs Testing**, it means that the module is not yet finalized. This may be because it is a newly added module, a recently updated module with new features that have not yet been fully tested, or a module currently being evaluated by external beta testers. If you encounter a module with either of these labels, please use it with caution.

### LFO's
+ SLFO4ss: -
+ SLFO4st: -
+ TLFO: -
+ LFO1: -
+ PhaseDrivenLFO: -

### Tweak (attenuate/amplify, offset and mix)
+ Tweak2I: -
+ Tweak2II: -
+ Tweak4I: -
+ Tweak4II: -
+ Tweak8: -
+ VCA2: -
+ VCA4I: -
+ VCA4II: -
+ Clamp4: -
+ AutoScale4: -

### Controllers/converters
+ ManTrGtCv: -
+ ManPush2: -
+ ManTrigger8: -
+ ManGate8: -
+ ManCV8I: -
+ ManCV8II: -
+ ManMix4I: -
+ ManMix4II: -
+ ManMix4st: -
+ Mute2: -
+ ManMute8: -
+ CvToggle8: -
+ CvToGt: -
+ CvToGtTr8: -

### Merge/Mult
+ Merge2x4: -
+ MergeMult4: -
+ Mult2x4: -

### Logic/Value-compare
+ TinyLCMP2: -
+ LCMP2: -
+ LCMP6x2: -
+ VCMP1: -
+ VCMP2I: -
+ VCMP2II: -

### S&H, T&H, H&T
+ SHTH2: -
+ SHTH2x4: -
+ SampleAndUpdate: -

### Polyphonic-tools
+ PolyMerge: -
+ PolySplit: -
+ PolyStereo: -
+ PolyQuad: -
+ PolyShuffle: -
+ PolyTweakI: -
+ PolyTweakII: -
+ PolyOffset: -
+ PolyScale: -
+ PolyLCMP: -
+ PolyVCMP: -

### Switches
+ CrossFadeSwitch1to4: -
+ CrossFadeSwitch4to1: -
+ CxFade1x2: -
+ CxFade4x1: -
+ BernoulliSwitch: -
+ OnOffSwitch: -
+ Combine: -

### Random
+ Random4: -
+ RandomCurve: -
+ Arm3XY: -

### Envelope
+ ADREnvelope: -
+ ADSDREnvelope: New module **Needs testing**

### Misc
+ Sign: -
+ Sign4I: -
+ Sign4II: -
+ Fold: -
+ WaveShaper2: -
+ RingMod3: -
+ IncDecOffset: -
+ Delta4: -
+ FlipFlop: Made polyphonic **Needs testing**
+ SlopeDetector2: -
+ Patch: -
+ TuringMachine: -
+ BitsToValue: -
+ ValueToBits: -
