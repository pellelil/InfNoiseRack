// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "InfNoiseEnvelope.hpp"

struct EnvelopePhaseExpanderModule : InfNoiseModule {
	enum ParamId {
		PARAMS_LEN
	};
	enum InputsId {
		INPUTS_LEN
	};
	enum OutputsId {
		ATTACK_OUTPUT,
		DECAY_OUTPUT,
		SUSTAIN_OUTPUT,
		DELAY_OUTPUT,
		RELEASE_OUTPUT,
		IDLE_OUTPUT,
		RISE_OUTPUT,
		STDY_OUTPUT,
		FALL_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(PROCQUAL_LIGHT, 2),
		ENUMS(CLIP_RANGE_LIGHT, 2),
		ENUMS(LEFT_EXPAND_LIGHT, 2),
		ENUMS(RIGHT_EXPAND_LIGHT, 2),
		LIGHTS_LEN
	};

	enum expanderModeType {
		emode_auto,
		emode_left,
		emode_right
	};
	actReqValue<expanderModeType> expanderMode = actReqValue<expanderModeType>(emode_auto);

	/// -1 = none, 0 = left host, 1 = right host
	int connectedSide = -1;

	EnvelopePhaseExpanderModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
		configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
		configLight(LEFT_EXPAND_LIGHT, "Left envelope connection (Green=active, Red=issue)");
		configLight(RIGHT_EXPAND_LIGHT, "Right envelope connection (Green=active, Red=issue)");

		configOutput(ATTACK_OUTPUT, "Attack-phase gate");
		configOutput(DECAY_OUTPUT, "Decay-phase gate");
		configOutput(SUSTAIN_OUTPUT, "Sustain-phase gate");
		configOutput(DELAY_OUTPUT, "Delay-phase gate");
		configOutput(RELEASE_OUTPUT, "Release-phase gate");
		configOutput(IDLE_OUTPUT, "Idle-phase gate");

		configOutput(RISE_OUTPUT, "Rise gate (envelope increasing)");
		configOutput(STDY_OUTPUT, "Steady gate (envelope unchanged)");
		configOutput(FALL_OUTPUT, "Fall gate (envelope decreasing)");

		haveProcQuality = false;
		haveAutoProcQuality = false;
		haveOutQuantize = false;
		haveOutClipRange = false;
		haveGateDetect = false;
		haveGateHighLow = true;
		haveTrigDetect = false;
		haveTrigHighLow = false;
	}

	static bool isEnvelopeHost(Module* m) {
		return m && dynamic_cast<InfNoiseEnvelopeModule*>(m);
	}

	void clearOutputs() {
		float lo = voltValues[gateOutLow.act];
		for (int i = 0; i < OUTPUTS_LEN; i++)
			outputs[i].setVoltage(lo);
	}

	void updateConnectionState() {
		bool leftEnv = isEnvelopeHost(getLeftExpander().module);
		bool rightEnv = isEnvelopeHost(getRightExpander().module);

		connectedSide = -1;
		if (expanderMode.act == emode_auto) {
			if (leftEnv && !rightEnv)
				connectedSide = 0;
			else if (rightEnv && !leftEnv)
				connectedSide = 1;
		}
		else if (expanderMode.act == emode_left) {
			if (leftEnv)
				connectedSide = 0;
		}
		else { // emode_right
			if (rightEnv)
				connectedSide = 1;
		}
	}

	void updateExpandLights() {
		bool leftEnv = isEnvelopeHost(getLeftExpander().module);
		bool rightEnv = isEnvelopeHost(getRightExpander().module);

		lights[LEFT_EXPAND_LIGHT].setBrightness(0.f);
		lights[LEFT_EXPAND_LIGHT + 1].setBrightness(0.f);
		lights[RIGHT_EXPAND_LIGHT].setBrightness(0.f);
		lights[RIGHT_EXPAND_LIGHT + 1].setBrightness(0.f);

		if (expanderMode.act == emode_auto) {
			if (leftEnv && rightEnv) {
				lights[LEFT_EXPAND_LIGHT + 1].setBrightness(1.f);
				lights[RIGHT_EXPAND_LIGHT + 1].setBrightness(1.f);
			}
			else if (leftEnv) {
				lights[LEFT_EXPAND_LIGHT].setBrightness(1.f);
			}
			else if (rightEnv) {
				lights[RIGHT_EXPAND_LIGHT].setBrightness(1.f);
			}
			else {
				lights[LEFT_EXPAND_LIGHT + 1].setBrightness(1.f);
				lights[RIGHT_EXPAND_LIGHT + 1].setBrightness(1.f);
			}
		}
		else if (expanderMode.act == emode_left) {
			lights[LEFT_EXPAND_LIGHT + (leftEnv ? 0 : 1)].setBrightness(1.f);
		}
		else { // emode_right
			lights[RIGHT_EXPAND_LIGHT + (rightEnv ? 0 : 1)].setBrightness(1.f);
		}
	}

	bool acceptsHost(Module* from) const {
		if (!from || connectedSide < 0)
			return false;
		if (connectedSide == 0)
			return leftExpander.module == from;
		return rightExpander.module == from;
	}

	void receiveHostState(Module* from, InfNoiseEnvelopeModule::envPhase phase,
		InfNoiseEnvelopeModule::envMotionType motion)
	{
		if (!acceptsHost(from))
			return;

		float hi = voltValues[gateOutHigh.act];
		float lo = voltValues[gateOutLow.act];
		for (int i = 0; i < 6; i++)
			outputs[ATTACK_OUTPUT + i].setVoltage((int)phase == i ? hi : lo);
		outputs[RISE_OUTPUT].setVoltage(motion == InfNoiseEnvelopeModule::em_rise ? hi : lo);
		outputs[FALL_OUTPUT].setVoltage(motion == InfNoiseEnvelopeModule::em_fall ? hi : lo);
		outputs[STDY_OUTPUT].setVoltage(motion == InfNoiseEnvelopeModule::em_steady ? hi : lo);
	}

	void onExpanderChange(const ExpanderChangeEvent& e) override {
		updateConnectionState();
		updateExpandLights();
	}

	void onReset(const ResetEvent& e) override {
		InfNoiseModule::onReset(e);
		expanderMode.setBoth(emode_auto);
		updateConnectionState();
		updateExpandLights();
	}

	void dataFromJson(json_t* rootJ) override {
		InfNoiseModule::dataFromJson(rootJ);
		expanderMode.setBoth((expanderModeType)clamp(
			getJsonInt(rootJ, "expanderMode", (int)emode_auto),
			(int)emode_auto, (int)emode_right));
		updateConnectionState();
		updateExpandLights();
	}

	void dataToJson(json_t* rootJ) override {
		json_object_set_new(rootJ, "expanderMode", json_integer((int)expanderMode.req));
	}

	void processParams(const ProcessArgs& args) {
		preProcessParams(args);
		//--------------------

		expanderMode.updateActual();	
		updateConnectionState();
		updateExpandLights();
		
		if (connectedSide < 0)
			clearOutputs();

		//--------------------
		postProcessParams(args);
	}

	void process(const ProcessArgs& args) override {
		bool doProcessParams = mustProcessParams ||
			((cycle256 & patternProcessParams) == patternProcessParams);
		if (doProcessParams)
			processParams(args);

		// Outputs generated in receiveHostState (called from pushToExpanders, called from EnvelopeModule)

		cycle256++;
	}
};

void InfNoiseEnvelopeModule::pushToExpanders() {  // Called from ADR/ADSDR Envelope
	Module* left = getLeftExpander().module;
	if (left && left->model == modelEnvelopePhaseExpander) {
		static_cast<EnvelopePhaseExpanderModule*>(left)->receiveHostState(this, phase, envMotion);
	}
	Module* right = getRightExpander().module;
	if (right && right->model == modelEnvelopePhaseExpander) {
		static_cast<EnvelopePhaseExpanderModule*>(right)->receiveHostState(this, phase, envMotion);
	}
}

struct EnvelopePhaseExpanderModuleWidget : InfNoiseModuleWidget {
	EnvelopePhaseExpanderModuleWidget(EnvelopePhaseExpanderModule* module) {
		initializeWidget(module, "res/EnvelopePhaseExpander");

		const float clm = 15.0f;
		addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(3.857f, 29.269f), module,
			EnvelopePhaseExpanderModule::LEFT_EXPAND_LIGHT));
		addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(26.301f, 29.269f), module,
			EnvelopePhaseExpanderModule::RIGHT_EXPAND_LIGHT));

		addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(clm, 53.235f), module, EnvelopePhaseExpanderModule::ATTACK_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(clm, 88.545f), module, EnvelopePhaseExpanderModule::DECAY_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(clm, 123.855f), module, EnvelopePhaseExpanderModule::SUSTAIN_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(clm, 159.165f), module, EnvelopePhaseExpanderModule::DELAY_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(clm, 194.475f), module, EnvelopePhaseExpanderModule::RELEASE_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(clm, 229.785f), module, EnvelopePhaseExpanderModule::IDLE_OUTPUT));

		addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(clm, 265.095f), module, EnvelopePhaseExpanderModule::RISE_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(clm, 300.405f), module, EnvelopePhaseExpanderModule::STDY_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(clm, 335.715f), module, EnvelopePhaseExpanderModule::FALL_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		InfNoiseModuleWidget::appendContextMenu(menu);
		EnvelopePhaseExpanderModule* module = dynamic_cast<EnvelopePhaseExpanderModule*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createIndexPtrSubmenuItem("Expander-mode", {"Auto", "Forced Left", "Forced Right"},
			&module->expanderMode.req));
			
		appendInfNoiseMenuItems(menu);
	}
};

Model* modelEnvelopePhaseExpander = createModel<EnvelopePhaseExpanderModule, EnvelopePhaseExpanderModuleWidget>("EnvelopePhaseExpander");
