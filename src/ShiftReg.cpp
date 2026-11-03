// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct ShiftRegModule : InfNoiseModule {
	enum ParamId {
		FREQ_PARAM,
		MODE_PARAM,
		SHIFT_LOOP_MODE_PARAM,
		SHIFT_PARAM,
		SHIFT_TRIM_PARAM,
		LOOP_PARAM,
		LOOP_TRIM_PARAM,
		DATA_MODE_PARAM,
		ORDER_MODE_PARAM,
		PARAMS_LEN
	};
	enum InputsId {
		A_CLK_INPUT,
		B_CLK_INPUT,
		SHIFT_INPUT,
		LOOP_INPUT,
		A_INPUT,
		B_INPUT,
		INPUTS_LEN
	};
	enum OutputsId {
		A1_OUTPUT,
		A2_OUTPUT,
		A3_OUTPUT,
		A4_OUTPUT,
		B1_OUTPUT,
		B2_OUTPUT,
		B3_OUTPUT,
		B4_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(PROCQUAL_LIGHT, 2),
		ENUMS(CLIP_RANGE_LIGHT, 2),
		ENUMS(FREQ_LIGHT, 2),
		LIGHTS_LEN
	};
	enum shiftLoopModeType {
		slm_loopOnShift, // dim
		slm_loopPriority, // green
		slm_independent // red
	};
	enum noiseGateHighType { // unpatched Gate-mode: chance of storing 10V
		ngh_5, ngh_10, ngh_15, ngh_20, ngh_25, ngh_30, ngh_35, ngh_40, ngh_45,
		ngh_50, ngh_55, ngh_60, ngh_65, ngh_70, ngh_75, ngh_80, ngh_85, ngh_90,
		ngh_95, ngh_len
	};

	float freqPitch = 1.f;
	float phaseStep = 0.f; // internal LFO, used when A-clock is unpatched
	float phase = 0.f;
	float shiftKnob = 1.f;
	float shiftTrim = 0.f;
	float loopKnob = 0.f;
	float loopTrim = 0.f;
	bool combinedMode = true; // true = A+B, false = A/B
	bool dataCv = true; // true = CV, false = Gate
	bool nextOrder = true; // true = Next, false = Prev
	shiftLoopModeType shiftLoopMode = slm_loopOnShift;
	int channels[2] = { 1, 1 }; // [0] = A input, [1] = B input (0 in A+B mode)
	bool inputConnected[INPUTS_LEN] = {};
	bool outputConnected[OUTPUTS_LEN] = {};
	bool haveOutputs = false;
	float regValue[8][PORT_MAX_CHANNELS] = {}; // slots 0..3 are A, 4..7 are B; not rotated
	int firstSlot[2] = { 0, 4 }; // slot currently shown as A1 / B1. A+B uses [0] across 0..7
	bool prevCombinedMode = true; // so a mode change can put firstSlot back at A1/B1
	infNoiseInTrigger clockTrigger[2]; // [0] = A clock, [1] = B clock
	actReqValue<voltRange> noiseRange = actReqValue<voltRange>(vr_Bipolar);
	float noiseScale = 10.f; // maps randomNorm 0..1 onto the menu noise range
	float noiseOffset = -5.f;
	actReqValue<noiseGateHighType> noiseGateHigh = actReqValue<noiseGateHighType>(ngh_50);
	float noiseGateHighProb = 0.5f; // processParams; chance of storing 10V
	actReqValue<rateChaos> lfoRateChaos = actReqValue<rateChaos>(rc_default);
	float chaosAmount = 0.f;
	float chaosFactor = 1.f;

	ShiftRegModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
		configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

		configParam<infNoiseLfoFreqQnt>(FREQ_PARAM, -8.f, 10.f, 1.f, "Clock frequency (when no A-clock input)", " Hz", 2, 1);
		configLight(FREQ_LIGHT, "LFO phase");
		configSwitch(MODE_PARAM, 0.f, 1.f, 0.f, "Mode", { "A+B (8-sections)", "A/B (2x4-sections)" });

		configInput(A_CLK_INPUT, "A-clock");
		configInput(B_CLK_INPUT, "B-clock (normalized to A-clock)");

		configSwitch(SHIFT_LOOP_MODE_PARAM, 0.f, 2.f, 0.f, "Shift/Loop mode",
			{ "Only loop on shift", "Loop takes priority", "Independent loop/shift" });
		configParam(SHIFT_PARAM, 0.f, 1.f, 1.f, "Shift probability", " %", 0, 100);
		configParam(SHIFT_TRIM_PARAM, -1.f, 1.f, 0.f, "Shift probability CV trim", " %", 0, 100);
		configParam(LOOP_PARAM, 0.f, 1.f, 0.f, "Loop probability", " %", 0, 100);
		configParam(LOOP_TRIM_PARAM, -1.f, 1.f, 0.f, "Loop probability CV trim", " %", 0, 100);
		configInput(SHIFT_INPUT, "Shift probability CV");
		configInput(LOOP_INPUT, "Loop probability CV");

		configSwitch(DATA_MODE_PARAM, 0.f, 1.f, 0.f, "Data mode", { "CV", "Gate" });
		configSwitch(ORDER_MODE_PARAM, 0.f, 1.f, 0.f, "Order mode", { "Next", "Prev" });

		configInput(A_INPUT, "A (normalized to noise)");
		configInput(B_INPUT, "B (normalized to noise)");

		const char* outName[8] = { "A1", "A2", "A3", "A4", "B1", "B2", "B3", "B4" };
		for (int i = 0; i < 8; i++)
			configOutput(A1_OUTPUT + i, outName[i]);

		// Set InfNoise features (e.g. menu-items)
		haveProcQuality = true;
		haveAutoProcQuality = true;
		haveOutQuantize = true;
		haveOutClipRange = true;
		haveGateDetect = true;
		haveGateHighLow = true;
		haveTrigDetect = true;
		haveTrigHighLow = false;
		autoProcQuality.setBoth(true);
	}

	void onInTrigTimeChanged() override {
		for (int i = 0; i < 2; i++)
			clockTrigger[i].setDelayCycles(trigInDelayCycles);
	}

	void onReset(const ResetEvent& e) override {
		InfNoiseModule::onReset(e);
		for (int s = 0; s < 8; s++) {
			for (int c = 0; c < PORT_MAX_CHANNELS; c++)
				regValue[s][c] = 0.f;
		}
		firstSlot[0] = 0;
		firstSlot[1] = 4;
		prevCombinedMode = true;
		phase = 0.f;
		for (int i = 0; i < 2; i++)
			clockTrigger[i].reset();
		noiseRange.setBoth(vr_Bipolar);
		noiseGateHigh.setBoth(ngh_50);
		noiseGateHighProb = 0.5f;
		lfoRateChaos.setBoth(rc_default);
		chaosAmount = 0.f;
		chaosFactor = 1.f;
	}

	void dataFromJson(json_t* rootJ) override {
		InfNoiseModule::dataFromJson(rootJ);
		getJsonFloatArray(rootJ, "regValue", &regValue[0][0], 8 * PORT_MAX_CHANNELS, 0.f);
		const int firstSlotDefault[2] = { 0, 4 };
		getJsonIntArray(rootJ, "firstSlot", firstSlot, 2, firstSlotDefault, 0, 7); // index into regValue
		bool modeNow = params[MODE_PARAM].getValue() > 0.5f;
		prevCombinedMode = getJsonBool(rootJ, "prevCombinedMode", modeNow); // missing key: load is not a mode change
		noiseRange.setBoth((voltRange)getJsonInt(rootJ, "noiseRange", (int)vr_Bipolar, (int)vr_len - 1));
		noiseGateHigh.setBoth((noiseGateHighType)getJsonInt(rootJ, "noiseGateHigh", (int)ngh_50, (int)ngh_len - 1));
		lfoRateChaos.setBoth((rateChaos)getJsonInt(rootJ, "lfoRateChaos", (int)rc_default, (int)rc_len - 1));
	}

	void dataToJson(json_t* rootJ) override {
		InfNoiseModule::dataToJson(rootJ);
		setJsonFloatArray(rootJ, "regValue", &regValue[0][0], 8 * PORT_MAX_CHANNELS);
		setJsonIntArray(rootJ, "firstSlot", firstSlot, 2);
		json_object_set_new(rootJ, "prevCombinedMode", json_boolean(prevCombinedMode));
		json_object_set_new(rootJ, "noiseRange", json_integer((int)noiseRange.req));
		json_object_set_new(rootJ, "noiseGateHigh", json_integer((int)noiseGateHigh.req));
		json_object_set_new(rootJ, "lfoRateChaos", json_integer((int)lfoRateChaos.req));
	}

	// One Shift toss and one Loop toss, then apply Shift/Loop mode. A+B calls this once; A/B calls it per section.
	inline void coinToss(float shift, float loop, bool& doShift, bool& doLoop) {
		doShift = (random::uniform() < shift || shift >= 0.99999f) && (shift > 0.00001f);
		doLoop = (random::uniform() < loop || loop >= 0.99999f) && (loop > 0.00001f);
		switch (shiftLoopMode) {
			case slm_loopOnShift:
				doLoop = doLoop && doShift; // Only loop if shift
				doShift = doShift && !doLoop; // Only shift if loop is not active
				break;
			case slm_loopPriority:
				doShift = doShift && !doLoop; // Only shift if loop is not active
				break;
			default:
				break; // slm_independent: both flags stay
		}
	}

	// first is the slot shown as output 1. base/len is 0/8 (A+B), 0/4 (A), or 4/4 (B).
	inline void applyShiftLoop(int& first, int base, int len, bool doShift, bool doLoop, int inputId, int chans) {
		if (doLoop) {
			if (nextOrder) {
				if (--first < base)
					first = base + len - 1;
			}
			else {
				if (++first >= base + len)
					first = base;
			}
		}
		if (doShift) {
			if (nextOrder) {
				if (--first < base)
					first = base + len - 1;
			}
			int slot = first;
			if (inputConnected[inputId]) {
				for (int c = 0; c < chans; c++)
					regValue[slot][c] = inputs[inputId].getVoltage(c);
			}
			else {
				for (int c = 0; c < chans; c++) {
					regValue[slot][c] = dataCv
						? (randomNorm() * noiseScale + noiseOffset)
						: ((randomNorm() < noiseGateHighProb) ? 10.f : 0.f);
				}
			}
			if (!nextOrder) {
				if (++first >= base + len)
					first = base;
			}
		}
	}

	void processParams(const ProcessArgs& args) {
		preProcessParams(args);
		//--------------------

		combinedMode = params[MODE_PARAM].getValue() <= 0.5f;
		dataCv = params[DATA_MODE_PARAM].getValue() <= 0.5f;
		nextOrder = params[ORDER_MODE_PARAM].getValue() <= 0.5f;
		if (combinedMode != prevCombinedMode) {
			firstSlot[0] = 0;
			firstSlot[1] = 4;
			prevCombinedMode = combinedMode;
		}

		shiftLoopMode = (shiftLoopModeType)clamp((int)params[SHIFT_LOOP_MODE_PARAM].getValue(),
			0, (int)slm_independent);

		for (int i = 0; i < INPUTS_LEN; i++)
			inputConnected[i] = inputs[i].isConnected();

		shiftKnob = params[SHIFT_PARAM].getValue();
		shiftTrim = params[SHIFT_TRIM_PARAM].getValue();
		loopKnob = params[LOOP_PARAM].getValue();
		loopTrim = params[LOOP_TRIM_PARAM].getValue();

		noiseRange.updateActual();
		noiseScale = voltRangeNormScale[noiseRange.act];
		noiseOffset = voltRangeNormOffset[noiseRange.act];
		noiseGateHigh.updateActual();
		noiseGateHighProb = ((int)noiseGateHigh.act + 1) * 0.05f;
		lfoRateChaos.updateActual();
		chaosAmount = rateChaosValues[lfoRateChaos.act];

		haveOutputs = false;
		for (int i = 0; i < OUTPUTS_LEN; i++) {
			outputConnected[i] = outputs[i].isConnected();
			haveOutputs |= outputConnected[i];
		}

		// Set channels
		channels[0] = inputConnected[A_INPUT] ? inputs[A_INPUT].getChannels() : 1;
		if (combinedMode) {
			channels[1] = 0;
			for (int i = 0; i < 8; i++) 
				outputs[A1_OUTPUT + i].setChannels(channels[0]);
		}
		else {
			channels[1] = inputConnected[B_INPUT] ? inputs[B_INPUT].getChannels() : 1;
			for (int i = 0; i < 4; i++) {
				outputs[A1_OUTPUT + i].setChannels(channels[0]);
				outputs[B1_OUTPUT + i].setChannels(channels[1]);
			}
		}

		// Internal LFO when A-clock is unpatched. B-clock is ignored in A+B mode.
		bool useInternalClock = !inputConnected[A_CLK_INPUT];
		bool extClock = inputConnected[A_CLK_INPUT] ||
			(!combinedMode && inputConnected[B_CLK_INPUT]);
		float clockFreq = 0.f;
		if (useInternalClock) {
			freqPitch = params[FREQ_PARAM].getValue();
			clockFreq = dsp::exp2_taylor5(freqPitch); // 2 Hz at pitch 1
		}

		if (autoProcQuality.act) {
			if (!haveOutputs)
				procQuality.setBoth(pq_veryLowRate, false);
			else if (extClock)
				procQuality.setBoth(pq_audioRate, false);
			else {
				float sampleRate = (float)procCycles / procSampleTime; // engine rate cached by preProcessParams
				procQuality.setBoth(getEstimatedLfoProcessQuality(sampleRate,
					clockFreq * rateChaosMaxFactor[lfoRateChaos.act]), false);
			}
		}

		if (useInternalClock) {
			float sampleRate = (float)procCycles / procSampleTime;
			phaseStep = clockFreq * processQualityCycles[procQuality.act] / sampleRate;
			lights[FREQ_LIGHT].setBrightness(getFreqPhaseBrightness(clockFreq, phase));
			lights[FREQ_LIGHT + 1].setBrightness(0.f);
		}
		else {
			lights[FREQ_LIGHT].setBrightness(0.f);
			lights[FREQ_LIGHT + 1].setBrightness(0.f);
		}

		//--------------------
		postProcessParams(args);
	}

	void process(const ProcessArgs& args) override {
		bool doProcessParams = mustProcessParams ||
			((cycle256 & patternProcessParams) == patternProcessParams);
		if (doProcessParams)
			processParams(args);

		bool doProcess = (doProcessParams ||
			((cycle256 & processQualityPatterns[procQuality.act]) == processQualityPatterns[procQuality.act]));
		if (doProcess) {
			// Detect A-clock (from input or internal LFO)
			float aVoltage;
			if (inputConnected[A_CLK_INPUT])
				aVoltage = inputs[A_CLK_INPUT].getVoltage();
			else { // internal clock
				aVoltage = phase < 0.5f ? 10.f : 0.f;
				phase += phaseStep * chaosFactor;
				if (phase >= 1.f) {
					phase -= std::truncf(phase);
					chaosFactor = rateChaosFactor(chaosAmount);
				}
			}
			bool aClock = clockTrigger[0].process(aVoltage,
				trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act], procCycles);

			// Detect B-clock 
			bool bClock = aClock;
			if (!combinedMode && inputConnected[B_CLK_INPUT]) {
				float bVoltage = inputs[B_CLK_INPUT].getVoltage();
				bClock = clockTrigger[1].process(bVoltage,
					trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act], procCycles);
			}

			// Shared Shift/Loop probabilities. A+B tosses once; A/B tosses once per clocking section.
			bool aShift = false;
			bool aLoop = false;
			bool bShift = false;
			bool bLoop = false;
			if (aClock || bClock) {
				float shift = shiftKnob;
				if (inputConnected[SHIFT_INPUT])
					shift = clamp(shift + inputs[SHIFT_INPUT].getVoltage() / 10.f * shiftTrim, 0.f, 1.f);
				float loop = loopKnob;
				if (inputConnected[LOOP_INPUT])
					loop = clamp(loop + inputs[LOOP_INPUT].getVoltage() / 10.f * loopTrim, 0.f, 1.f);

				if (aClock) {
					coinToss(shift, loop, aShift, aLoop);
					if (aShift || aLoop)
						applyShiftLoop(firstSlot[0], 0, combinedMode ? 8 : 4, aShift, aLoop, A_INPUT, channels[0]);
				}
	
				if (!combinedMode && bClock) {
					coinToss(shift, loop, bShift, bLoop);
					if (bShift || bLoop)
						applyShiftLoop(firstSlot[1], 4, 4, bShift, bLoop, B_INPUT, channels[1]);
				}
			}


			// Set all outputs in logical order from firstSlot
			if (haveOutputs && (aShift || aLoop || bShift || bLoop || doProcessParams)) {
				if (combinedMode) {
					for (int i = 0; i < 8; i++) {
						int s = (firstSlot[0] + i) % 8;
						for (int c = 0; c < channels[0]; c++) {
							float v = regValue[s][c];
							float out = dataCv
								? clipToVoltRange(quantizeToMode(v, outQuantize.act), outClipRange.act)
								: (v >= trueDetectValues[gateDetHigh.act]
									? voltValues[gateOutHigh.act]
									: voltValues[gateOutLow.act]);
							outputs[A1_OUTPUT + i].setVoltage(out, c);
						}
					}
				}
				else {
					for (int i = 0; i < 4; i++) {
						int sa = (firstSlot[0] + i) % 4;
						int sb = 4 + (firstSlot[1] - 4 + i) % 4;
						for (int c = 0; c < channels[0]; c++) {
							float v = regValue[sa][c];
							float out = dataCv
								? clipToVoltRange(quantizeToMode(v, outQuantize.act), outClipRange.act)
								: (v >= trueDetectValues[gateDetHigh.act]
									? voltValues[gateOutHigh.act]
									: voltValues[gateOutLow.act]);
							outputs[A1_OUTPUT + i].setVoltage(out, c);
						}
						for (int c = 0; c < channels[1]; c++) {
							float v = regValue[sb][c];
							float out = dataCv
								? clipToVoltRange(quantizeToMode(v, outQuantize.act), outClipRange.act)
								: (v >= trueDetectValues[gateDetHigh.act]
									? voltValues[gateOutHigh.act]
									: voltValues[gateOutLow.act]);
							outputs[B1_OUTPUT + i].setVoltage(out, c);
						}
					}
				}
			}
		}

		cycle256++;
	}
};

struct ShiftRegModuleWidget : InfNoiseModuleWidget {
	InfNoiseDisableOverlayGroup* bIgnoredOverlayGroup = nullptr;
	bool combinedApplied = false;

	ShiftRegModuleWidget(ShiftRegModule* module) {
		initializeWidget(module, "res/ShiftReg");

		const float leftCol = 15.f;
		const float rightCol = 45.f;

		addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(3.955f, 33.236f), module, ShiftRegModule::FREQ_LIGHT));
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(leftCol, 48.955f), module, ShiftRegModule::FREQ_PARAM));
		addParam(createParamCentered<CKSS>(Vec(38.858f, 48.955f), module, ShiftRegModule::MODE_PARAM));

		addInput(createInputCentered<infNoiseThemedMonoPort>(Vec(leftCol, 81.566f), module, ShiftRegModule::A_CLK_INPUT));
		addInput(createInputCentered<infNoiseThemedMonoPort>(Vec(rightCol, 81.566f), module, ShiftRegModule::B_CLK_INPUT));

		addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_green, bc_red>>(
			Vec(30.f, 99.694f), module, ShiftRegModule::SHIFT_LOOP_MODE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(leftCol, 115.189f), module, ShiftRegModule::SHIFT_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(rightCol, 115.189f), module, ShiftRegModule::LOOP_PARAM));
		addParam(createParamCentered<Trimpot>(Vec(leftCol, 139.565f), module, ShiftRegModule::SHIFT_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(Vec(rightCol, 139.565f), module, ShiftRegModule::LOOP_TRIM_PARAM));
		addInput(createInputCentered<infNoiseThemedMonoPort>(Vec(leftCol, 164.290f), module, ShiftRegModule::SHIFT_INPUT));
		addInput(createInputCentered<infNoiseThemedMonoPort>(Vec(rightCol, 164.290f), module, ShiftRegModule::LOOP_INPUT));

		addParam(createParamCentered<CKSS>(Vec(8.689f, 197.753f), module, ShiftRegModule::DATA_MODE_PARAM));
		addParam(createParamCentered<CKSS>(Vec(38.858f, 197.753f), module, ShiftRegModule::ORDER_MODE_PARAM));

		addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftCol, 229.849f), module, ShiftRegModule::A_INPUT));
		addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(rightCol, 229.849f), module, ShiftRegModule::B_INPUT));

		const float outY[4] = { 259.297f, 283.929f, 308.561f, 333.194f };
		for (int i = 0; i < 4; i++) {
			addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftCol, outY[i]), module, ShiftRegModule::A1_OUTPUT + i));
			addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightCol, outY[i]), module, ShiftRegModule::B1_OUTPUT + i));
		}

		InfNoiseDisableOverlayManager& overlayManager = getDisableOverlayManager();
		bIgnoredOverlayGroup = overlayManager.addGroup("Ignored in A+B mode");
		bIgnoredOverlayGroup->addTargets(InfNoiseOverlayTargetType::input, {
			ShiftRegModule::B_CLK_INPUT,
			ShiftRegModule::B_INPUT
		});
	}

	void step() override {
		InfNoiseModuleWidget::step();
		if (!module || !bIgnoredOverlayGroup)
			return;

		ShiftRegModule* sr = static_cast<ShiftRegModule*>(module);
		if (sr->combinedMode != combinedApplied) {
			combinedApplied = sr->combinedMode;
			bIgnoredOverlayGroup->setActive(combinedApplied);
		}
	}

	void appendContextMenu(Menu* menu) override {
		InfNoiseModuleWidget::appendContextMenu(menu);
		ShiftRegModule* module = dynamic_cast<ShiftRegModule*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator);
		std::vector<std::string> noiseRangeNames = getVoltRangesNames(false);
		const int vrOffOffset = 1; // vr[0] ("off") excluded from noise-range menu
		menu->addChild(createIndexSubmenuItem("Noise range", noiseRangeNames,
			[=]() {
				return (int)(module->noiseRange.req - vrOffOffset);
			},
			[=](int range) {
				module->noiseRange.req = (voltRange)(range + vrOffOffset);
			}
		));
		std::vector<std::string> noiseGateHighNames;
		for (int i = 0; i < (int)ShiftRegModule::ngh_len; i++) {
			int pct = (i + 1) * 5;
			noiseGateHighNames.push_back(pct == 50
				? "50% (default)"
				: string::f("%d%%", pct));
		}
		menu->addChild(createIndexPtrSubmenuItem("Noise high-gate chance", noiseGateHighNames,
			&module->noiseGateHigh.req));
		menu->addChild(createIndexPtrSubmenuItem("Rate chaos", getRateChaosNames(),
			&module->lfoRateChaos.req));

		appendInfNoiseMenuItems(menu);
	}
};

Model* modelShiftReg = createModel<ShiftRegModule, ShiftRegModuleWidget>("ShiftReg");
