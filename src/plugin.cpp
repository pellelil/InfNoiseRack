// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inMath.hpp"

Plugin *pluginInstance;

void init(Plugin *p) {
	pluginInstance = p;
	ensureSoftClipLut(); // shared by every module that can soft-clip

	// Add modules here
	p->addModel(modelADREnvelope);
	p->addModel(modelADSDREnvelope);
	p->addModel(modelArm3XY);
	p->addModel(modelAutoScale4);
	p->addModel(modelBernoulliSwitch);
	p->addModel(modelBitsToValue);
	p->addModel(modelChaos);
	p->addModel(modelClamp4);
	p->addModel(modelCombine);
	p->addModel(modelCrossFadeSwitch1to4);
	p->addModel(modelCrossFadeSwitch4to1);
	p->addModel(modelCvToggle8);
	p->addModel(modelCvToGt);
	p->addModel(modelCvToGtTr8);
	p->addModel(modelCxFade1x2);
	p->addModel(modelCxFade4x1);
	p->addModel(modelDelay3);
	p->addModel(modelDelta4);
	p->addModel(modelEnvelopePhaseExpander);
	p->addModel(modelFlipFlop);
	p->addModel(modelFlux4);
	p->addModel(modelFold);
	p->addModel(modelIncDecOffset);
	p->addModel(modelLCMP2);
	p->addModel(modelLCMP6x2);
	p->addModel(modelLFO1);
	p->addModel(modelManCV8I);
	p->addModel(modelManCV8II);
	p->addModel(modelManGate8);
	p->addModel(modelManMix4I);
	p->addModel(modelManMix4II);
	p->addModel(modelManMix4st);
	p->addModel(modelManMute8);
	p->addModel(modelManPush2);
	p->addModel(modelManTrGtCv);
	p->addModel(modelManTrigger8);
	p->addModel(modelMatrixMix4x4Stereo);
	p->addModel(modelMatrixMix5x5);
	p->addModel(modelMerge2x4);
	p->addModel(modelMergeMult4);
	p->addModel(modelMult2x4);
	p->addModel(modelMute2);
	p->addModel(modelOnOffSwitch);
	p->addModel(modelPatch);
	p->addModel(modelPhaseDrivenLFO);
	p->addModel(modelPolyLCMP);
	p->addModel(modelPolyMerge);
	p->addModel(modelPolyOffset);
	p->addModel(modelPolyQuad);
	p->addModel(modelPolyScale);
	p->addModel(modelPolyShuffle);
	p->addModel(modelPolySplit);
	p->addModel(modelPolyStereo);
	p->addModel(modelPolyTweakI);
	p->addModel(modelPolyTweakII);
	p->addModel(modelPolyVCMP);
	p->addModel(modelRandom4);
	p->addModel(modelRandomCurve);
	p->addModel(modelRingMod3);
	p->addModel(modelSampleAndUpdate);
	p->addModel(modelSHTH2);
	p->addModel(modelSHTH2x4);
	p->addModel(modelShiftReg);
	p->addModel(modelSign);
	p->addModel(modelSign4I);
	p->addModel(modelSign4II);
	p->addModel(modelSlew2);
	p->addModel(modelSlew4);
	p->addModel(modelSLFO4ss);
	p->addModel(modelSLFO4st);
	p->addModel(modelSlopeDetector2);
	p->addModel(modelStack);
	p->addModel(modelStackBankExpander);
	p->addModel(modelStackExpander);
	p->addModel(modelStereoBalancePan);
	p->addModel(modelTinyLCMP2);
	p->addModel(modelTLFO);
	p->addModel(modelTuringMachine);
	p->addModel(modelTweak2I);
	p->addModel(modelTweak2II);
	p->addModel(modelTweak4I);
	p->addModel(modelTweak4II);
	p->addModel(modelTweak8);
	p->addModel(modelValueToBits);
	p->addModel(modelVCA2);
	p->addModel(modelVCA4I);
	p->addModel(modelVCA4II);
	p->addModel(modelVCMP1);
	p->addModel(modelVCMP2I);
	p->addModel(modelVCMP2II);
	p->addModel(modelWaveShaper2);

	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
	inLoadSettings();  
}
