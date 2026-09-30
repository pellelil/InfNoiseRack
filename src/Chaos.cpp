// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct ChaosModule : InfNoiseModule {
	enum ParamId {
		FREQ_PARAM,
		CHAOS_PARAM,
		MIN_PARAM,
		MAX_PARAM,
		LINK_PARAM,
		DIST_PARAM,
		DIST_MODE_PARAM,
		PARAMS_LEN
	};
	enum InputsId {
		TRIG_INPUT,
		CHAOS_CV_INPUT,
		IN_INPUT,
		INPUTS_LEN
	};
	enum OutputsId {
		OUT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(PROCQUAL_LIGHT, 2),
		ENUMS(CLIP_RANGE_LIGHT, 2),
		FREQ_LIGHT,
		ENUMS(SLEW_TIME_LIGHT, 3),
		LIGHTS_LEN
	};

	enum chaosSlewShape {
		css_linear, css_sCurve, css_exp, css_log,
		css_len
	};

	enum chaosSlewTarget {
		cst_chaos, cst_inputChaos,
		cst_len
	};

	actReqValue<rateChaos> lfoRateChaos = actReqValue<rateChaos>(rc_default);
	actReqValue<fixedSlewTimes> slewTime = actReqValue<fixedSlewTimes>(fst_default);
	actReqValue<chaosSlewShape> slewShape = actReqValue<chaosSlewShape>(css_linear);
	actReqValue<chaosSlewTarget> slewTarget = actReqValue<chaosSlewTarget>(cst_chaos);
	bool maxLinkedToMin = false;
	bool distExpMode = false;
	float slewTimeSecs = 1.f;
	bool useAdaptiveSlewTime = false;
	infNoiseSlew slew[PORT_MAX_CHANNELS];
	float rngMin = 0.f;
	float rngMax = 0.f;
	float rngCntr = 0.f;
	float rngSpan = 0.f;
	float rngSpan_2 = 0.f;
	dsp::SchmittTrigger trigger;
	infNoiseEventTracker trigEvents;
	infNoisePeriodTracker periodTracker;
	bool inConn = false;
	bool outConn = false;
	int channels = 1;
	bool useLFO = false;
	float lfoFreq = 2.f;
	float lfoPhase = 0.f;
	float lfoPhaseInc = 0.f;
	float chaosAmount = 0.f; // Chaos rate set via context-menu
	float chaosFactor = 1.f; // Current rate-chaos factor (new each cycle)
	float chaosKnob = 0.f; // Cached rate-chaos amount (0-1)
	float distKnob = 0.f; // Cached distribution (-1..1)
	float distAnchor = 0.f; // Distribution center within Min..Max
	float distSpanAbove = 0.f; // distAnchor → rngMax
	float distSpanBelow = 0.f; // distAnchor → rngMin
	float chaosValue = 0.f; // Current chaos value (applied to all channels)

	static std::vector<std::string> getSlewShapeNames() {
		return { "Linear (default)", "S-curve", "Exponential", "Logarithmic" };
	}

	static std::vector<std::string> getSlewTargetNames() {
		return { "Chaos", "Input + chaos" };
	}

	ChaosModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
		configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

		configInput(TRIG_INPUT, "Trigger (normalized to Trigger-LFO)");
		configParam<infNoiseLfoFreqQnt>(FREQ_PARAM, -8.f, 10.f, 1.f, "Trigger-LFO frequency", " Hz", 2, 1);
		configLight(FREQ_LIGHT, "LFO phase");
		configLight(SLEW_TIME_LIGHT, "Slew time: " + getFixedSlewTimesName(fst_default));

		configParam(CHAOS_PARAM, 0.f, 1.f, 0.5f, "Chaos", " %", 0, 100);
		configInput(CHAOS_CV_INPUT, "Chaos CV (0-10V added to Chaos)");

		configParam(MIN_PARAM, -10.f, 10.f, -5.f, "Min", " V");
		configParam(MAX_PARAM, -10.f, 10.f, 5.f, "Max", " V");
		configSwitch(LINK_PARAM, 0.f, 1.f, 0.f, "Link max to min (mirrored)", { "Off", "On" });

		configParam(DIST_PARAM, -1.f, 1.f, 0.f, "Distribution", "");
		configSwitch(DIST_MODE_PARAM, 0.f, 1.f, 0.f, "Distribution mode", { "Knob", "Exponential" });

		configInput(IN_INPUT, "Input (normalized to 0V)");
		configOutput(OUT_OUTPUT, "Output");
		configBypass(IN_INPUT, OUT_OUTPUT);

		haveProcQuality = false;
		haveAutoProcQuality = false;
		haveOutQuantize = false;
		haveOutClipRange = true;
		haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = true;
		haveTrigHighLow = false;
	}

	void onReset(const ResetEvent& e) override {
		InfNoiseModule::onReset(e);
		lfoRateChaos.setBoth(rc_default);
		slewTime.setBoth(fst_default);
		slewShape.setBoth(css_linear);
		slewTarget.setBoth(cst_chaos);
		maxLinkedToMin = false;
		distExpMode = false;
		for(int i=0; i<PORT_MAX_CHANNELS; i++) {
			slew[i].reset();
			slew[i].setMode(sm_constantTime);
		}
		trigger.reset();
		trigEvents.reset();
		periodTracker.reset();
		chaosValue = 0.f;
		chaosFactor = 1.f;
	}

	void applySlewTimes() {
		for (int i = 0; i < PORT_MAX_CHANNELS; i++) {
			slew[i].setTimes(slewTimeSecs, slewTimeSecs);
			slew[i].setMode(sm_constantTime);
		}
	}

	void dataFromJson(json_t* rootJ) override {
		InfNoiseModule::dataFromJson(rootJ);
		lfoRateChaos.setBoth((rateChaos)getJsonInt(rootJ, "lfoRateChaos", (int)rc_default));
		slewTime.setBoth((fixedSlewTimes)getJsonInt(rootJ, "slewTime", (int)fst_default));
		slewShape.setBoth((chaosSlewShape)getJsonInt(rootJ, "slewShape", (int)css_linear, (int)css_len - 1));
		slewTarget.setBoth((chaosSlewTarget)getJsonInt(rootJ, "slewTarget", (int)cst_chaos, (int)cst_len - 1));
	}

	void dataToJson(json_t* rootJ) override {
		json_object_set_new(rootJ, "lfoRateChaos", json_integer((int)lfoRateChaos.req));
		json_object_set_new(rootJ, "slewTime", json_integer((int)slewTime.req));
		json_object_set_new(rootJ, "slewShape", json_integer((int)slewShape.req));
		json_object_set_new(rootJ, "slewTarget", json_integer((int)slewTarget.req));
	}

	void processParams(const ProcessArgs& args) {
		preProcessParams(args);

		lfoRateChaos.updateActual();
		slewTarget.updateActual();
		chaosAmount = rateChaosValues[lfoRateChaos.act];
		chaosKnob = params[CHAOS_PARAM].getValue();
		
		if (slewTime.needsUpdate()) {
			slewTime.updateActual();
			useAdaptiveSlewTime = slewTime.act == fst_adaptive;
			if (useAdaptiveSlewTime)
				trigEvents.reset();
			else {
				slewTimeSecs = fixedSlewTimesValues[(int)slewTime.act];
				applySlewTimes();
			}
			setFixedSlewTimesLight(this, SLEW_TIME_LIGHT, slewTime.act);
			if ((int)lightInfos.size() > SLEW_TIME_LIGHT && lightInfos[SLEW_TIME_LIGHT])
				lightInfos[SLEW_TIME_LIGHT]->name = "Slew time: " + getFixedSlewTimesName(slewTime.act);
		}

		if (slewShape.needsUpdate()) {
			slewShape.updateActual();
			float shape = 0.f;
			infNoiseLinearMode linearMode = lm_linear;
			switch (slewShape.act) {
				case css_linear:
					shape = 0.f;
					linearMode = lm_linear;
					break;
				case css_sCurve:
					shape = 0.f;
					linearMode = lm_sCurve;
					break;
				case css_exp:
					shape = -1.f;
					linearMode = lm_linear;
					break;
				case css_log:
					shape = 1.f;
					linearMode = lm_linear;
					break;
				default:
					break;
			}
			for(int i=0; i<PORT_MAX_CHANNELS; i++) {
				slew[i].setShapes(shape, shape);
				slew[i].setLinearMode(linearMode);
			}
		}

		maxLinkedToMin = params[LINK_PARAM].getValue() > 0.5f;
		if (maxLinkedToMin) {
			params[MAX_PARAM].setValue(-params[MIN_PARAM].getValue());
		}

		rngMin = std::min(params[MIN_PARAM].getValue(), params[MAX_PARAM].getValue());
		rngMax = std::max(params[MIN_PARAM].getValue(), params[MAX_PARAM].getValue());
		rngSpan = (rngMax - rngMin);
		rngSpan_2 = rngSpan / 2.f;
		rngCntr = rngMin + rngSpan_2;

		distExpMode = params[DIST_MODE_PARAM].getValue() > 0.5f;
		if (distExpMode)
			params[DIST_PARAM].setValue(0.f);
		distKnob = params[DIST_PARAM].getValue();
		distAnchor = rngCntr + distKnob * rngSpan_2;
		distSpanAbove = rngSpan_2 * (1.f - distKnob);
		distSpanBelow = rngSpan_2 * (1.f + distKnob);

		inConn = inputs[IN_INPUT].isConnected();
		channels = inConn ? std::max(inputs[IN_INPUT].getChannels(), 1) : 1;
		outputs[OUT_OUTPUT].setChannels(channels);
		outConn = outputs[OUT_OUTPUT].isConnected();

		bool wasLfo = useLFO;
		useLFO = !inputs[TRIG_INPUT].isConnected();
		if (wasLfo != useLFO)
			trigEvents.reset();
		float sampleRate = safeSampleRate(args.sampleRate);
		if (useLFO) {
			lfoFreq = 2.f; // 2 Hz at pitch 1
			float pitch = params[FREQ_PARAM].getValue();
			lfoFreq = lfoFreq / 2.f * dsp::exp2_taylor5(pitch);
			float cycleStep = processQualityCycles[procQuality.act];
			lfoPhaseInc = (lfoFreq * cycleStep) / sampleRate;
		}

		if (!(inConn || outConn) || !useLFO) {
			lights[FREQ_LIGHT].setBrightness(0.f);
		}
		else {
			lights[FREQ_LIGHT].setBrightness(getFreqPhaseBrightness(lfoFreq, lfoPhase));
		}

		postProcessParams(args);
	}

	float calcChaosValue() {
		float chaos = chaosKnob;
		if (inputs[CHAOS_CV_INPUT].isConnected()) {
			float cv = inputs[CHAOS_CV_INPUT].getVoltage();
			chaos = rack::math::clamp(chaosKnob + cv * 0.1f, 0.f, 1.f);
		}

		if (distExpMode) {
			if (chaos <= 0.f)
				return 0.f; // no pitch offset
			float u1 = randomNorm();
			float u2 = randomNorm();
			float x = u1 * (1.f + chaos) + u2 * (1.f - chaos) - 1.f; // [-1, 1], mean 0
			return chaos * x * 3.321928f; // log2(10): ±3.32 V at Chaos 1 (×10 / ÷10 at V/oct)
		}

		if (chaos <= 0.f || rngSpan <= 0.f)
			return distAnchor; // no random draw: sit on the distribution center

		float mag = std::pow(randomNorm(), 1.f / chaos); // 0 at the anchor, 1 at a rail
		float pAbove = distSpanAbove / rngSpan;
		float pUp = chaos * pAbove + (1.f - chaos) * (1.f - pAbove); // chaos 1 follows the span; lower chaos prefers the short side
		float distSpan = (randomNorm() < pUp) ? distSpanAbove : -distSpanBelow;
		return distAnchor + distSpan * mag;
	}

	void process(const ProcessArgs& args) override {
		bool doProcessParams = mustProcessParams ||
			((cycle256 & patternProcessParams) == patternProcessParams);
		if (doProcessParams)
			processParams(args);

		bool doProcess = (doProcessParams ||
				((cycle256 & processQualityPatterns[procQuality.act]) == processQualityPatterns[procQuality.act]));
		if (doProcess && (inConn || outConn)) {
			// Handle LFO or trigger (detect new chaos value)
			bool calcChaos = false;
			if (useLFO) {
				lfoPhase += lfoPhaseInc * chaosFactor;
				if (lfoPhase >= 1.f) {
					lfoPhase -= std::truncf(lfoPhase); // robust if a fast cycle overshoots past 1.0
					chaosFactor = rateChaosFactor(chaosAmount);
					calcChaos = true;
				}
			}
			else {
				trigEvents.process(procSampleTime);
				if (trigger.process(inputs[TRIG_INPUT].getVoltage(0),
					trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act])) {
						calcChaos = true;
				}
			}

			// Generate new chaos value and calculate adaptive slew time (if applicable)
			if (calcChaos) {
				chaosValue = calcChaosValue();
				if (useAdaptiveSlewTime) {
					if (useLFO) {
						float stepHz = lfoFreq * chaosFactor;
						if (stepHz > 0.f) {
							periodTracker.snapTo((1.f - lfoPhase) / stepHz);
							slewTimeSecs = periodTracker.get();
						}
					}
					else if (trigEvents.onEvent()) {
						periodTracker.blendToward(trigEvents.period);
						slewTimeSecs = rack::math::clamp(periodTracker.get(), 0.00001f, 10.f);
					}
				}
			}

			// Process chaos value and output
			for(int i=0; i<channels; i++) {
				float slewChaos = chaosValue;
				float input = inConn ? inputs[IN_INPUT].getVoltage(i) : 0.f;
				float output = input + slewChaos;

				if (calcChaos && useAdaptiveSlewTime)
					slew[i].setTimes(slewTimeSecs, slewTimeSecs);

				if (slewTimeSecs > 0.f) {
					if (slewTarget.act == cst_chaos) {
						slewChaos = slew[i].next(slewChaos, procSampleTime);
						output = input + slewChaos;
					} else {
						output = slew[i].next(input + slewChaos, procSampleTime);
					}
				} 

				outputs[OUT_OUTPUT].setVoltage(clipToVoltRange(output, outClipRange.act), i);				
			}
		}

		cycle256++;
	}
};

struct ChaosModuleWidget : InfNoiseModuleWidget {
	InfNoiseDisableOverlayGroup* linkMaxOverlayGroup = nullptr;
	InfNoiseDisableOverlayGroup* distExpOverlayGroup = nullptr;
	bool maxLinkedToMin = false;
	bool distExpMode = false;

	ChaosModuleWidget(ChaosModule* module) {
		initializeWidget(module, "res/Chaos");

		const float cntrClm = 15.f;
		addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrClm, 50.27f), module, ChaosModule::TRIG_INPUT));
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrClm, 79.72f), module, ChaosModule::FREQ_PARAM));
		addChild(createLightCentered<SmallLight<GreenLight>>(Vec(6.91f, 65.42f), module, ChaosModule::FREQ_LIGHT));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(Vec(24.57f, 65.42f), module, ChaosModule::SLEW_TIME_LIGHT));

		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrClm, 115.95f), module, ChaosModule::CHAOS_PARAM));
		addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrClm, 145.40f), module, ChaosModule::CHAOS_CV_INPUT));

		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrClm, 180.20f), module, ChaosModule::MIN_PARAM));
		addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(Vec(5.66f, 199.80f), module, ChaosModule::LINK_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrClm, 217.08f), module, ChaosModule::MAX_PARAM));

		addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(Vec(5.66f, 243.08f), module, ChaosModule::DIST_MODE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrClm, 257.19f), module, ChaosModule::DIST_PARAM));

		addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 298.30f), module, ChaosModule::IN_INPUT));
		addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 333.77f), module, ChaosModule::OUT_OUTPUT));

		InfNoiseDisableOverlayManager& overlayManager = getDisableOverlayManager();
		linkMaxOverlayGroup = overlayManager.addGroup("Max linked to min (mirrored)");
		linkMaxOverlayGroup->addTarget(InfNoiseOverlayTargetType::param, ChaosModule::MAX_PARAM);
		distExpOverlayGroup = overlayManager.addGroup("Distribution locked in Exponential mode");
		distExpOverlayGroup->addTarget(InfNoiseOverlayTargetType::param, ChaosModule::DIST_PARAM);
	}

	void step() override {
		InfNoiseModuleWidget::step();
		if (!module)
			return;

		auto* m = static_cast<ChaosModule*>(module);
		if (linkMaxOverlayGroup && m->maxLinkedToMin != maxLinkedToMin) {
			maxLinkedToMin = m->maxLinkedToMin;
			linkMaxOverlayGroup->setActive(maxLinkedToMin);
		}
		if (distExpOverlayGroup && m->distExpMode != distExpMode) {
			distExpMode = m->distExpMode;
			distExpOverlayGroup->setActive(distExpMode);
		}
	}

	void appendContextMenu(Menu* menu) override {
		InfNoiseModuleWidget::appendContextMenu(menu);
		ChaosModule* module = dynamic_cast<ChaosModule*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexPtrSubmenuItem("LFO rate chaos", getRateChaosNames(),
			&module->lfoRateChaos.req));
		menu->addChild(createIndexPtrSubmenuItem("Slew time", getFixedSlewTimesNames(true),
			&module->slewTime.req));
		menu->addChild(createIndexPtrSubmenuItem("Slew shape", ChaosModule::getSlewShapeNames(),
			&module->slewShape.req));
		menu->addChild(createIndexPtrSubmenuItem("Slew target", ChaosModule::getSlewTargetNames(),
			&module->slewTarget.req));

		appendInfNoiseMenuItems(menu);
	}
};

Model* modelChaos = createModel<ChaosModule, ChaosModuleWidget>("Chaos");
