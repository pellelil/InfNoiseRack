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
		ENUMS(FREQ_LIGHT, 2),
		ENUMS(SLEW_TIME_LIGHT, 3),
		LIGHTS_LEN
	};

	enum chaosSlewShape {
		css_linear, css_sCurve, css_exp, css_log
	};

	actReqValue<rateChaos> lfoRateChaos = actReqValue<rateChaos>(rc_default);
	actReqValue<fixedSlewTimes> slewTime = actReqValue<fixedSlewTimes>(fst_default);
	actReqValue<chaosSlewShape> slewShape = actReqValue<chaosSlewShape>(css_linear);
	bool maxLinkedToMin = false;
	bool distExpMode = false;

	static std::vector<std::string> getSlewShapeNames() {
		return { "Linear (default)", "S-curve", "Exponential", "Logarithmic" };
	}

	ChaosModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
		configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

		configInput(TRIG_INPUT, "Trigger (normalized to Trigger-LFO)");
		configParam<infNoiseLfoFreqQnt>(FREQ_PARAM, -8.f, 10.f, 1.f, "Trigger-LFO frequency", " Hz", 2, 1);
		configLight(FREQ_LIGHT, "LFO phase");
		configLight(SLEW_TIME_LIGHT, "Slew time (dim=0, green-to-red=0.0001-10s, blue=adaptive)");

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
		haveTrigDetect = false;
		haveTrigHighLow = false;
	}

	void onReset(const ResetEvent& e) override {
		InfNoiseModule::onReset(e);
		lfoRateChaos.setBoth(rc_default);
		slewTime.setBoth(fst_default);
		slewShape.setBoth(css_linear);
		maxLinkedToMin = false;
		distExpMode = false;
	}

	void dataFromJson(json_t* rootJ) override {
		InfNoiseModule::dataFromJson(rootJ);
		lfoRateChaos.setBoth((rateChaos)getJsonInt(rootJ, "lfoRateChaos", (int)rc_default));
		slewTime.setBoth((fixedSlewTimes)getJsonInt(rootJ, "slewTime", (int)fst_default));
		slewShape.setBoth((chaosSlewShape)getJsonInt(rootJ, "slewShape", (int)css_linear));
	}

	void dataToJson(json_t* rootJ) override {
		json_object_set_new(rootJ, "lfoRateChaos", json_integer((int)lfoRateChaos.req));
		json_object_set_new(rootJ, "slewTime", json_integer((int)slewTime.req));
		json_object_set_new(rootJ, "slewShape", json_integer((int)slewShape.req));
	}

	void processParams(const ProcessArgs& args) {
		preProcessParams(args);

		lfoRateChaos.updateActual();
		slewShape.updateActual();
		if (slewTime.needsUpdate()) {
			slewTime.updateActual();
			setFixedSlewTimesLight(this, SLEW_TIME_LIGHT, slewTime.act);
		}

		maxLinkedToMin = params[LINK_PARAM].getValue() > 0.5f;
		distExpMode = params[DIST_MODE_PARAM].getValue() > 0.5f;
		if (distExpMode)
			params[DIST_PARAM].setValue(0.f);

		postProcessParams(args);
	}

	void process(const ProcessArgs& args) override {
		bool doProcessParams = mustProcessParams ||
			((cycle256 & patternProcessParams) == patternProcessParams);
		if (doProcessParams)
			processParams(args);

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
		addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(6.91f, 65.42f), module, ChaosModule::FREQ_LIGHT));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(Vec(24.57f, 65.42f), module, ChaosModule::SLEW_TIME_LIGHT));

		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrClm, 115.95f), module, ChaosModule::CHAOS_PARAM));
		addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrClm, 145.40f), module, ChaosModule::CHAOS_CV_INPUT));

		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrClm, 180.20f), module, ChaosModule::MIN_PARAM));
		addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(Vec(5.66f, 199.80f), module, ChaosModule::LINK_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrClm, 217.08f), module, ChaosModule::MAX_PARAM));

		addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(Vec(5.66f, 243.08f), module, ChaosModule::DIST_MODE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrClm, 257.19f), module, ChaosModule::DIST_PARAM));

		addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrClm, 298.30f), module, ChaosModule::IN_INPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(cntrClm, 333.77f), module, ChaosModule::OUT_OUTPUT));

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

		appendInfNoiseMenuItems(menu);
	}
};

Model* modelChaos = createModel<ChaosModule, ChaosModuleWidget>("Chaos");
