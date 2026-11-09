# 2.0.6 (in development)
- Input and output port-graphics now match the active panel theme, regardless if VCV is configured to use dark theme or not (before they were "wrong" if theme was forced to dark but VCV rack did not prefer dark or vice versa). The port-graphics is now light or dark together with the panel, whether the theme is set to Auto or forced to light or dark. Also **for new users the default theme will be Dark**, whereas it was previously Auto.
- Modules that accept or generate triggers now have optional **Trigger input delay** and/or **Trigger output delay** items in the Trigger context menu (defaults to none). You can delay detection and/or generation of triggers by a number of cycles (1–16, 20, 24, 28, 32, 64, 128 or 256 cycles). This can be useful when you need two modules to detect the same trigger signal **at the same time**, or one **before** the other: each rack cable introduces a 1 cycle delay, so a short extra delay on an input or output can line events up — or deliberately stagger them. *This new feature is described here [Trigger delays](commonFeatures.md#trigger-delays) along with an example of when you might need it.*
- This new trigger-delay feature (as the name implies) only applies to triggers (not gates) and is naturally only available in Infinite-Noise modules. The new Delay 3 module (see **New modules**) does the same thing (delays a signal by a few cycles), but can also be used with gate signals and/or external modules (e.g. to delay signals sent to them or received from their outputs).
- Generally tweaked the quantize feature, and fixed issues with quantize settings greater the 1V.
- Various minor panel tweaks (e.g. updated a few tool-tips, removed/corrected a few wrong labels - e.g. the dark themed Slew 2 panel had a "GT/TR" label that should not be there).
- Improved stability of reading floats from json.
- VCV is now offering to host user manuals for plugins that are included in the VCV Library. Going forward, this will allow me to add/update manual pages (for new/changed modules) for an "in development version" on GitHub, without affecting the manual for a current version available from the VCV library. So all documentation files where split (1 file per module) and moved into the new manual folder. *Sometimes viewing manual pages in GitHub it would not correctly scroll to the selected module, however now they are split (1 file per module) going forward this will no longer be an issue.*

## New modules
- **[Delay 3](Delay3.md)**: 2HP 3-section polyphonic delay (1–16 cycles per section). B/C inputs normalize to the previous section's output (1-cycle hop, same as a cable), allowing for a 48 cycle delay of a single input in the A-section.
- **[Stack](Stack.md)**: 2HP polyphonic stack. Push stores up to 16 polyphonic values, Pop takes the oldest (FIFO) or newest (LIFO), and Size chooses how many of the newest items are accessible without deleting the rest.
- **[Stack Expander](StackExpander.md)**: 2HP expander for Stack: Cycle the accessible stack-items to a Value output, with triger-overflow, underflow outputs, and gate-full, and empty outputs.
- **[Stack Bank Expander](StackBankExpander.md)**: 2HP expander for Stack: Four banks (stacks). Cycle among them, send a bank into Stack, or retrieve a bank from Stack.
- **[Flux 4](Flux4.md)**: 10HP quad morphing VCA: Pos, Range and Sweep shape four signals, with optional linked ranges and A+B, C+D, min, max, average and sum outputs.
- **[ShiftReg](ShiftReg.md)**: 4HP combined 8-stage or dual 4-stage polyphonic shift register. Shift/loop probability, internal clock, CV or gate outputs.

## Updated modules
- **[Matrix Mix 5x5](MatrixMix5x5.md)**: Added NtZ (Nearest to Zero) and FfZ (Farthest from Zero) outputs to the lower-left output cluster. Level lights hold peaks a bit longer so brief amber/red overloads are easier to notice.
- **[Matrix Mix 4x4 Stereo](MatrixMix4x4Stereo.md)**: Level lights hold peaks a bit longer so brief amber/red overloads are easier to notice.
- **[Stereo Balance/Pan](StereoBalancePan.md#stereo-balance-pan)**: Level lights hold peaks a bit longer so brief amber/red overloads are easier to notice.
- **ADR Envelope**, **ADSDR Envelope**, **Auto-Scale 4**, **Bernoulli Switch**, **Chaos**, **Cross-fade 1x2**, **Cross-fade 4x1**, **Cross-fade switch 1to4**, **Cross-fade switch 4to1**, **CV to Gate/Trigger 8**, **CV-Toggle 8**, **Delta-4**, **Flip-Flop**, **Increment/Decrement Offset**, **LFO1**, **Manuel CV 8 Mk II**, **Manuel Push 2**, **Manuel Trigger 8**, **Manuel Trigger, Gate and CV**, **Mute 2**, **ON/OFF Switch**, **Poly-Shuffle**, **Poly-Value Compare**, **Random-4**, **Random Curve**, **S&H/T&H-2**, **S&H/T&H-2x4**, **Sample and Update**, **Slew 2**, **Slope Detector 2**, **Turing Machine**, **Value Comparator-1**, **Wave Shaper 2**: Enables input- and/or output-triggers with new optional delay (set via context-menu).

## Fixed modules
- **[Chaos](Chaos.md)**: Did not implement Process Quality selection via context menu. Failed to ensure the LUT it used were initialized, which would affect exp/log slew-shapes.
- **[Phase-Driven LFO](PhaseDrivenLFO.md)**: Failed to ensure the LUT used by modulation was initialized, so exp/log MOD shapes were not applied.
- **[Envelope Phase Expander](EnvelopePhaseExpander.md)**: Did not implement Process Quality selection via context menu.
- **[CV-Toggle 8](CvToggle8.md)**: Attenuvert input was reading wrong channel input.

# 2.0.5
- Added new knob-scaling to the time knobs utilized in ADR-/ADSDR-Envelope. Loading a patch saved in a previous version, the knobs will automatic rescale on load to ensure the knobs keep the same time as prevoius (please see **Breaking changes** below). Added context-menu Time scale (0.1× / 1× / 10×) with indicator lights on the time knobs. The 0.1x scaling makes is more easy to dial in times in the 0-1 second range, whereas the 10x scaling allows you to dial in times up to 100 seconds. *The scaling will default to 1x, hence by default the knobs allows you to set times in the 0 to 10 seconds range as in prevoius versions.*
- Added internal (fixed time) cross fade to several switch and mute modules (see **Updated modules** below). By default these modules will still perform an instant switch/mute, however using the context menu you are now able select between different fixed time cross fades - in the range from 0.0001 to 10 seconds. Likewise using the context menu you can select either a Linear (default) or S-Curve cross fade (starts slow -> fast in the middle -> slow at the end). *This build in cross-fade can achive results, an external tool cannot do (as it only affects the actual switch, hence not affect the signal before/after the switch).*
- In all of the Tweak-modules the Scale-range/Offset-mode lights were replaced by multi-stage/color push buttons (see **Updated Modules** below). This means Scale-range/Offset-mode can be changed directly from the panel, whereas these had to be changed using the context-menu in previous versions (old context menu items were removed). *When loading a patch made with a previous version, the old context-menu selections are automatically "converted" into stages for the new buttons*.
- Added separate Mixer section to the manual.
- In previous versions, Manuel Mix 4 Mk I, Manuel Mix 4 Mk II and Manuel Mix 4 Stereo defaulted to perform an Avering mix. These modules will now in stead default to perform a Unity mix. Likewise in previous versions you had to use the context menu to switch mix-mode. However now there is a small toggle button near the "Mix" label in the top to toggle between Unity- or Avering mix-mode. *Old patches will still load with the correct mix-mode.*
- While testing some of the new modules, I sadly discovered a couple of simple/embarrassing bugs in a few other modules. So, I've decided to go through all of the modules, to test them all once again. *It was not half as fun as it might sound ... knowing it doesn't sound like fun at all :-)* Anyway, while I can't fully test each and every combination of how a module might be used, I'm at least happy to report that what I found was fixed, and there are no known bugs.

## Braking changes
- As mentioned above, the time knobs on the the ADR-/ADSDR-envelope will automatic adjust to the new knob scaling when loading an old patch. However I suspect if you use external modules to set knob positions (such as PatchMaster by MindMeld) that you would have to tweak those knob settings to get the same time-settings.
- MOD CV-input for LFO1 and Phase Driven LFO now have double the effect it had in previous versions (see **Fixed modules**).
- In Cross-fade 1x2 if B1 was left un-patched it normalized to 0V, but now it will normalize to A1 (see **Updated modules**).

## New modules
- **[Slew 2](Slew2.md)**: 4HP dual polyphonic constant rate/time slew with knobs and (polyphonic) CV/trim for rise and fall time and shape, Snap (gate/trigger), B-Catch (gate) output, shared Scale/Offset of the input, and envelope follower. Via context-menu the (center) linear shape, can be replaced by an S-Curve shape
- **[Slew 4](Slew4.md)**: 2HP quad polyphoinc constant rate/time slew with knobs based rise/fall times/shape settings. Via context-menu the (center) linear shape, can be replaced by an S-Curve shape.
- **[Chaos](Chaos.md)**: 2HP adds random chaos to an input (or outputs chaos). Range, distribution, slew, Rate Chaos.
- **[Stereo Balance/Pan](StereoBalancePan.md#stereo-balance-pan)**: 2HP stereo balance/pan with post gain (0–200%), CV/trim, and L/R level lights. Polyphonic audio; mono B/P and Gain CV. Right input normalizes from Left.
- **[Matrix Mix 5x5](MatrixMix5x5.md)**: 14HP mono 5x5 matrix mixer with per-row invert/gain, column send links, per-column unity or averaging mix, 0-1x or +/-1x range, level CV/trim, and Min/Max/Avg/Rng.
- **[Matrix Mix 4x4 Stereo](MatrixMix4x4Stereo.md)**: 22HP stereo 4x4 matrix mixer with per-row L/R normalization, balance/pan, gain, linked unipolar sends, per-column unity or averaging mix, L/R level lights, and Avg outputs.

## Updated modules
- **[ADR Envelope](ADREnvelope.md)**: Changed scaling of the time-knobs. Added context-menu Time scale (0.1× / 1× / 10×). 
- **[ADSDR Envelope](ADSDREnvelope.md)**: Changed scaling of time-knobs. Added context-menu Time scale (0.1× / 1× / 10×).
- **[Random-4](Random4.md)**: Added fixed/adaptive time slew between generated random values (set via context menu).
- **[Bernoulli Switch](BernoulliSwitch.md)**: Added independent fixed-time A/B fades (context menu, default 0 / instant) and optional S-curve fade.
- **[Combine](Combine.md)**: Added per-channel fixed-time A/B fade (context menu, default 0 / instant) and optional S-curve fade. Gate output remains instant.
- **[Cross-fade Switch 4to1](CrossFadeSwitch4to1.md)**: Added fixed-time fade in switch mode (context menu, default 0 / instant) and optional S-curve fade.
- **[Cross-fade Switch 1to4](CrossFadeSwitch1to4.md)**: Added fixed-time fade in switch mode (context menu, default 0 / instant) and optional S-curve fade.
- **[ON/OFF Switch](OnOffSwitch.md)**: Added fixed-time fade between ON and OFF (context menu, default 0 / instant) and optional S-curve fade.
- **[Mute-2](Mute2.md)**: Added fixed-time fade between live and mute (context menu, default 0 / instant) and optional S-curve fade. Changed lights to tiny lights.
- **[Manuel Mute 8](ManMute8.md)**: Added fixed-time fade between live and mute (context menu, default 0 / instant) and optional S-curve fade.
- **[CV-Toggle 8](CvToggle8.md)**: Added fixed-time fade between ON and OFF (context menu, default 0 / instant) and optional S-curve fade.
- **[Tweak-2 Mk I](Tweak2I.md)**: Replaced Scale-range/Offset-mode lights with Scale-range/Offset-mode buttons.
- **[Tweak-2 Mk II](Tweak2II.md)**: Replaced Scale-range/Offset-mode lights with Scale-range/Offset-mode buttons. Previous version had a single scale-range/offset-mode used for both sections (A and B). Now both sections have their own/individual buttons, hence they can be configured individually.
- **[Tweak-4 Mk I](Tweak4I.md)**: Replaced Scale-range/Offset-mode lights with Scale-range/Offset-mode buttons.
- **[Tweak-4 Mk II](Tweak4II.md)**: Replaced Scale-range/Offset-mode lights with Scale-range/Offset-mode buttons.
- **[Tweak-8](Tweak8.md)**: Replaced Scale-range/Offset-mode lights with Scale-range/Offset-mode buttons.
- **[Delta-4](Delta4.md#delta-4)**: Tooltip for "Toggle output mode" was blocking lights showing active mode (button is now below lights).
- **[Cross-fade 1x2](CxFade1x2.md)**: Added Cross-fade / Balance / Pan modes (Cf / Bl / Pn), so the module can be used as a stereo balance or panning module. Unpatched B1 now also normalizes to A1, so a mono signal into A1 can be balanced/panned across the stereo outputs (A2 still normalizes to B1, B2 still to A1). Added fixed-time fade on the trigger input in trigger/switch mode (context menu, default 0 / instant) and optional S-curve fade. Manual A/B-toggle remains instant.
- **[Cross-fade 4x1](CxFade4x1.md)**: Added fixed-time fade on the trigger input in trigger/switch mode (context menu, default 0 / instant) and optional S-curve fade, shared by master and all sections (each control ramps independently). Manual A/B-toggle remains instant.
- **[CV to Gate](CvToGt.md#cv-to-gate)**: Changed lights to tiny-lights.
- **[Manuel Mix 4 Mk I](ManMix4I.md)**: Now default to unity mix. New button can toggle between unity- and avering mix-mode.
- **[Manuel Mix 4 Mk II](ManMix4II.md)**: Now default to unity mix. New button can toggle between unity- and avering mix-mode.
- **[Manuel Mix 4 Stereo](ManMix4st.md)**: Now default to unity mix. New button can toggle between unity- and avering mix-mode.

## Fixed modules
- **[ADR Envelope](ADREnvelope.md)**: Fixed trigger outputs (were high for 2 ms - high for both high and low phases).
- **[ADSDR Envelope](ADSDREnvelope.md)**: Fixed trigger outputs (were high for 2 ms - high for both high and low phases).
- **[Manuel Mute 8](ManMute8.md)**: Fixed issues with inputs in sections 2-8 not being read.
- **[CV-Toggle 8](CvToggle8.md)**: Fixed issues with normalized inputs in sections 2-8.
- **[LFO1](LFO1.md)**: Fixed scaling of MOD CV-input (input had half the effect it should have).
- **[Phase-Driven LFO](PhaseDrivenLFO.md)**: Fixed scaling of MOD CV-input (input had half the effect it should have).
- **[Tweak-4 Mk II](Tweak4II.md)**: Fixed issues with mixing when input were not connected but scale/offset was, and trim-knob setting were "carried" to the next section when scale/offset CV-inputs were normalized.
- **[Tweak-8](Tweak8.md)**: Fixed issues with mixing when input were not connected but scale/offset was, and trim-knob setting were "carried" to the next section when scale/offset CV-inputs were normalized.
- **[Poly-Scale](PolyScale.md)**: The multiple-scale warning light was not added to panel (next to "Poly" output).


# 2.0.4
- Modules with trigger output can now specify output trigger high- and low-length/duration independently (using their context menu). By default an output trigger is forced to be **high for 1 ms, then low for 1 ms** (hence each trigger output can max generate 500 triggers per second). However modules (with trigger outputs) can now specify **Trigger-high length** and **Trigger-low length** as **number of ms** (1, 2, 5, 10, 50, 100, 200, 500, or 1000) or **number of cycles** (1, 2, 4, 8, 16, 32, 64, 128 or 256). High and low can differ (e.g. 10 ms high then 1 ms low - before the next trigger is allowed to fire). *Choosing a low cycle count you will be able to generate more than 500 triggers per second (if needed), however be sure to not pick a too low value as some modules might not look for/process triggers every cycle. Likewise you can instead choose a high ms count, where output ports that can otherwise only generate triggers, can be used as fixed length gates (longer than 1 ms) - directly without needing another utility module.*
- Added a bit more (elaborate) description for each module both in the manifest (shown in the VCV library) and in the module overview of the manual.
- Changed the order of the modules as they appear in the browser and manual for a few modules, as there is now a "Random" and "Envelope" section in the manual, containing some of the modules that were previously located in the "Misc" section. A few other modules were re-ordered as well for a more "logical" order. *Going forward, order should not change unless for new modules.*
- Various changes/fixes to the documentation (e.g. added a few illustrations to the documentation of the Envelope modules).
- In multiple modules, overlay(s) were added on top of controls (e.g. knobs/ports) when these are rendered "not in use" due to certain configurations set via switches (see **Updated modules** below).
- In multiple modules, latchable/pressed buttons remained pressed when set to momentary (see **Fixed modules** below).

## New modules
- **[ADSDR Envelope](ADSDREnvelope.md)**: Attack-Decay-Sustain-Delay-Release Envelope with attack/decay/release time and shape, and levels for Attack, Sustain and Release. Times can be affected by Rate Chaos (set via context-menu).
- **[Envelope Phase Expander](EnvelopePhaseExpander.md)**: Expander for ADR/ADSDR Envelope with gate outputs for each phase (Attack, Decay, Sustain, Delay, Release and Idle) plus gate outputs for envelope movement (Rise, Steady and Fall). The Idle output can optionally output phase position (0V to 10V) instead of an idle-phase gate. 

## Updated modules
- **[ADR Envelope](ADREnvelope.md)**: Added inverted Envelope-output. Added a Delay time link button (D.time can follow A.time). Fixed issue where the high/low outputs of BOA, EOA, BOR and EOR would use gate-threshold values instead of trigger-threshold values (only an issue if you changed output levels via the context menu). Also removed context menu-items to set gate-output levels, as the module doesn't have gate-outputs.
- **[Flip-Flop](FlipFlop.md#flip-flop)**: All input/output (except enable/disable input) were made polyphonic, so there is basically an individual flip-flop for each channel of a polyphonic signal. You can mix polyphonic and monophonic signals (e.g. have a polyphonic Set-input and a monophonic Reset-input), however if using multiple polyphonic inputs they should ideally have the same number of channels.
- **[Random-4](Random4.md)**: Trig-input now accepts a polyphonic signal, so new random values can be triggered independently per channel. Polyphony defaults to **Auto** (same number of channels as the Trig-input, or 1 if no input is connected); a fixed count can still be selected regardless of the Trig-input (a red light next to the Trig-input is lit when a fixed count is used).
- **[Random Curve](RandomCurve.md)**: Added step-mode context menu, so steps can occur either mid-phase (as in previous versions) or at the start of the phase (at the same time as the trigger output fires a trigger). Fixed issue where trigger output would not fire unless one of the six curve outputs was in use (e.g., if you simply used this module with **LFO rate chaos** to generate a series of "chaotic triggers"). Added overlay on top of the FP (Forced Polarity) button when distribution is Min/Max (as Forced Polarity is only used in Center/Edge mode).
- **[Cross-fade 1x2](CxFade1x2.md)**: Added overlay on top of trim-knob when CV-Mode is in Trigger mode (as the trim is then ignored).
- **[Cross-fade 4x1](CxFade4x1.md)**: Added overlay on top of trim-knob when CV-Mode is in Trigger mode (as the trim is then ignored). Each of the five sections (Master plus 1–4) has its own overlay.
- **[Cross-fade switch 4to1](CrossFadeSwitch4to1.md)**: Added overlay on top of trim-knob when CV-Mode is in Trigger mode (as the trim is then ignored). Likewise an overlay was added on top of Reset-input when not in Trigger mode.
- **[Cross-fade switch 1to4](CrossFadeSwitch1to4.md)**: Added overlay on top of trim-knob when CV-Mode is in Trigger mode (as the trim is then ignored). Likewise an overlay was added on top of Reset-input when not in Trigger mode.
- **[Combine](Combine.md)**: Added overlay on top of the Range switch when not in Upper/Lower mode (as Range is then ignored).
- **[Wave Shaper 2](WaveShaper2.md#wave-shaper-2)**: Added overlay on top of Reset-input when Value-range is not Automatic (as Reset is then ignored).
- **[Bits-to-Value](BitsToValue.md)**: Min/Center/Max mode-lights changed to tiny-lights.
- **[Value-to-Bits](ValueToBits.md)**: Min/Center/Max mode-lights changed to tiny-lights.

## Fixed modules
- **[LFO1](LFO1.md)**: Fixed issue storing sync-out mode in json and issue with polyphonic Sync out triggers in n-shot mode.
- **[Logic Comparator-2](LCMP2.md)**: Fixed issues: detecting connected outputs, using normalized A/B input when no A/B outputs were connected and outputting polyphonic C/D results.
- **[Value Comparator-1](VCMP1.md)**: Fixed issue where the A<=B output stayed monophonic while other outputs could be polyphonic.
- **[Value Comparator-2 Mk I](VCMP2I.md)**: Fixed issues detecting connected outputs.
- **[Value Comparator-2 Mk II](VCMP2II.md)**: Fixed issues detecting connected outputs.
- **Manuel Push 2**, **Manuel Trigger, Gate and CV**, **ON/OFF Switch**, **Manuel CV 8 Mk I**, **CV-Toggle 8**, **Turing Machine**, **Mute 2**, **Manuel Mute 8**, **Manuel Gate 8**, **Manuel CV 8 Mk II**, **Sample and Update** and **Delta-4**: Fixed issue with latchable buttons remaining "pressed" when changed to momentary mode.


# 2.0.3
Ensured all module descriptions in manifest are identical to module descriptions in the manual. Also updated the manifest tags for several modules, e.g. LCMP2, VCMP1 and VCMP2 Mk I, are all able to work as 2-to-1 switches thanks to their "True"/"False" inputs in the top of the modules, but were not tagged as "Switch" in the manifest. Various minor updates to the manual (e.g. added badges to highlight module features, and changed all module headers). Added support for multi-stage (color) push-buttons (needed one for the ADR Envelope).

## New modules
- **[ADR Envelope](ADREnvelope.md)**: Attack-Delay-Release Envelope with attack/release time and shape (technically it is an Attack-Sustain-Delay-Release Envelope).

## Updated modules
- **[Manuel Push 2](ManPush2.md)**: In previous release this module was not able to react to trigger-inputs as the manual otherwise stated. There is now a gate/trigger-mode switch next to the input that lets you select whether the input should be detected as a gate or a trigger (defaults to gate as previous version).
- **[Cross-fade switch 4to1](CrossFadeSwitch4to1.md)**: Using the context menu you can now specify which volt-level the input should normalize to when not connected (e.g. if you need to switch/fade a fixed volt level to up to 4 different outputs). It defaults to 0V as previous version.

## Fixed modules
- **[Simple LFO4-ss](SLFO4ss.md)**: Phase light for LFO-4 and sync lights were not set correctly.
- **[Turing Machine](TuringMachine.md#turing-machine)**: Min/Center/Max toggle-button appeared pressed (green) when adding module to rack. Min/Center/Max mode-lights changed to tiny-lights.
- **[LFO1](LFO1.md)**: Sync-output remained being monophonic even when the frequency-input was polyphonic.
- **[Fold](Fold.md#fold)**: Output port appeared to be monophonic ("red circle" was missing), but would output polyphonic signals.


# 2.0.2 
Fixed various warnings based on cppcheck report from VCV. Going forward I now have a local release-check script I can run, which monitors the various issues reported from VCV. So future releases should be less painful.


# 2.0.1 (not included in VCV Library due to various issues)
All panel SVG-files were simplified and reduced in file-size, but appears the same as before (e.g. a hidden layer inside the files were removed).

## Updated Modules
- **[Poly-Merge](PolyMerge.md)**: Lights (next to each input port) indicating channel count are now color-coded, whether input is available (green) or whether it will simply output 0V (red). Ports that are not included in the output still shows as dimmed light. *Might have fixed a bug from 2.0.0, otherwise it was a bug caused by the new code that was fixed. Anyway no known bugs.*
- **[Poly-Split](PolySplit.md)**: Output ports that have available input still shows with a green light (whether cable is connected or not). However lights for output ports without available input will either show as dimmed (when no cable is connected), or red (if a cable is connected). This works both in mono- and poly-mode. E.g. if you in Poly-mode input a 6-channel polyphonic cable, and only insert a cable into output port 8, it will still output a 8 channel signal. The lights for port 1-6 will be green (as input is available), however the lights for ports 7 and 8 will now show as red (as no input is available, and ports 7 and 8 will simply output 0V).


# 2.0.0 (First release - not included in VCV Library due to manifest issues)

## New Modules
As everything in this very-first release is new, it only consists of "New Modules" (nothing changed/fixed in this version). Here below is a list of all the modules included in this (1st) version.

- [Simple LFO4-ss](SLFO4ss.md)
- [Simple LFO4-st](SLFO4st.md)
- [Tiny LFO](TLFO.md)
- [LFO1](LFO1.md)
- [Phase-Driven LFO](PhaseDrivenLFO.md)
- [Tweak-2 Mk I](Tweak2I.md)
- [Tweak-2 Mk II](Tweak2II.md)
- [Tweak-4 Mk I](Tweak4I.md)
- [Tweak-4 Mk II](Tweak4II.md)
- [Tweak-8](Tweak8.md)
- [VCA-2](VCA2.md)
- [VCA-4 Mk I](VCA4I.md)
- [VCA-4 Mk II](VCA4II.md)
- [Clamp 4](Clamp4.md)
- [Auto-Scale 4](AutoScale4.md)
- [Manuel Trigger, Gate and CV](ManTrGtCv.md)
- [Manuel Push 2](ManPush2.md)
- [Manuel Trigger 8](ManTrigger8.md)
- [Manuel Gate 8](ManGate8.md)
- [Manuel CV 8 Mk I](ManCV8I.md)
- [Manuel CV 8 Mk II](ManCV8II.md)
- [Manuel Mix 4 Mk I](ManMix4I.md)
- [Manuel Mix 4 Mk II](ManMix4II.md)
- [Manuel Mix 4 Stereo](ManMix4st.md)
- [Mute 2](Mute2.md)
- [Manuel Mute 8](ManMute8.md)
- [CV-Toggle 8](CvToggle8.md)
- [CV to Gate](CvToGt.md#cv-to-gate)
- [CV to Gate/Trigger 8](CvToGtTr8.md#cv-to-gatetrigger-8)
- [Mult2x4](Mult2x4.md)
- [Merge/Mult-4](MergeMult4.md)
- [Merge2x4](Merge2x4.md)
- [Tiny Logic Comparator-2](TinyLCMP2.md)
- [Logic Comparator-2](LCMP2.md)
- [Logic Comparator-6x2](LCMP6x2.md)
- [Value Comparator-1](VCMP1.md)
- [Value Comparator-2 Mk I](VCMP2I.md)
- [Value Comparator-2 Mk II](VCMP2II.md)
- [S&H/T&H-2](SHTH2.md)
- [S&H/T&H-2x4](SHTH2x4.md)
- [Sample and Update](SampleAndUpdate.md)
- [Poly-Merge](PolyMerge.md)
- [Poly-Split](PolySplit.md)
- [Poly-Stereo](PolyStereo.md)
- [Poly-Quad](PolyQuad.md)
- [Poly-Shuffle](PolyShuffle.md)
- [Poly-Tweak Mk I](PolyTweakI.md)
- [Poly-Tweak Mk II](PolyTweakII.md)
- [Poly-Logical Compare](PolyLCMP.md)
- [Poly-Value Compare](PolyVCMP.md)
- [Poly-Offset](PolyOffset.md)
- [Poly-Scale](PolyScale.md)
- [Cross-fade 1x2](CxFade1x2.md)
- [Cross-fade 4x1](CxFade4x1.md)
- [Cross-fade switch 1to4](CrossFadeSwitch1to4.md)
- [Cross-fade switch 4to1](CrossFadeSwitch4to1.md)
- [Bernoulli Switch](BernoulliSwitch.md)
- [ON/OFF Switch](OnOffSwitch.md)
- [Combine](Combine.md)
- [Sign](Sign.md)
- [Sign4 Mk I](Sign4I.md)
- [Sign4 Mk II](Sign4II.md)
- [Fold](Fold.md#fold)
- [Wave Shaper 2](WaveShaper2.md#wave-shaper-2)
- [Ring Modulator 3](RingMod3.md#ring-modulator-3)
- [Increment/Decrement Offset](IncDecOffset.md#incrementdecrement-offset)
- [Delta-4](Delta4.md#delta-4)
- [Flip-Flop](FlipFlop.md#flip-flop)
- [Slope Detector 2](SlopeDetector2.md#slope-detector-2)
- [Patch](Patch.md#patch)
- [Random-4](Random4.md)
- [Random Curve](RandomCurve.md)
- [Arm 3 XY](Arm3XY.md#arm-3-xy)
- [Turing Machine](TuringMachine.md#turing-machine)
- [Bits-to-Value](BitsToValue.md)
- [Value-to-Bits](ValueToBits.md)
