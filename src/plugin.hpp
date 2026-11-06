// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#pragma once
#include <rack.hpp>
#include <settings.hpp>
using namespace ::rack;
#include "inUtil.hpp"

// Declare each Model, defined in each module source file

extern Model* modelADREnvelope;
extern Model* modelADSDREnvelope;
extern Model* modelArm3XY;
extern Model* modelAutoScale4;
extern Model* modelBernoulliSwitch;
extern Model* modelBitsToValue;
extern Model* modelChaos;
extern Model* modelClamp4;
extern Model* modelCombine;
extern Model* modelCrossFadeSwitch1to4;
extern Model* modelCrossFadeSwitch4to1;
extern Model* modelCvToggle8;
extern Model* modelCvToGt;
extern Model* modelCvToGtTr8;
extern Model* modelCxFade1x2;
extern Model* modelCxFade4x1;
extern Model* modelDelay3;
extern Model* modelDelta4;
extern Model* modelEnvelopePhaseExpander;
extern Model* modelFlipFlop;
extern Model* modelFlux4;
extern Model* modelFold;
extern Model* modelIncDecOffset;
extern Model* modelLCMP2;
extern Model* modelLCMP6x2;
extern Model* modelLFO1;
extern Model* modelManCV8I;
extern Model* modelManCV8II;
extern Model* modelManGate8;
extern Model* modelManMix4I;
extern Model* modelManMix4II;
extern Model* modelManMix4st;
extern Model* modelManMute8;
extern Model* modelManPush2;
extern Model* modelManTrGtCv;
extern Model* modelManTrigger8;
extern Model* modelMatrixMix4x4Stereo;
extern Model* modelMatrixMix5x5;
extern Model* modelMerge2x4;
extern Model* modelMergeMult4;
extern Model* modelMult2x4;
extern Model* modelMute2;
extern Model* modelOnOffSwitch;
extern Model* modelPatch;
extern Model* modelPhaseDrivenLFO;
extern Model* modelPolyLCMP;
extern Model* modelPolyMerge;
extern Model* modelPolyOffset;
extern Model* modelPolyQuad;
extern Model* modelPolyScale;
extern Model* modelPolyShuffle;
extern Model* modelPolySplit;
extern Model* modelPolyStereo;
extern Model* modelPolyTweakI;
extern Model* modelPolyTweakII;
extern Model* modelPolyVCMP;
extern Model* modelRandom4;
extern Model* modelRandomCurve;
extern Model* modelRingMod3;
extern Model* modelSampleAndUpdate;
extern Model* modelSHTH2;
extern Model* modelSHTH2x4;
extern Model* modelShiftReg;
extern Model* modelSign;
extern Model* modelSign4I;
extern Model* modelSign4II;
extern Model* modelSlew2;
extern Model* modelSlew4;
extern Model* modelSLFO4ss;
extern Model* modelSLFO4st;
extern Model* modelSlopeDetector2;
extern Model* modelStack;
extern Model* modelStackBankExpander;
extern Model* modelStackExpander;
extern Model* modelStereoBalancePan;
extern Model* modelTinyLCMP2;
extern Model* modelTLFO;
extern Model* modelTuringMachine;
extern Model* modelTweak2I;
extern Model* modelTweak2II;
extern Model* modelTweak4I;
extern Model* modelTweak4II;
extern Model* modelTweak8;
extern Model* modelValueToBits;
extern Model* modelVCA2;
extern Model* modelVCA4I;
extern Model* modelVCA4II;
extern Model* modelVCMP1;
extern Model* modelVCMP2I;
extern Model* modelVCMP2II;
extern Model* modelWaveShaper2;

#include "inModule.hpp"
