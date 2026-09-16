// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "InfNoiseEnvelope.hpp"

namespace {
const int steadyHoldCycles[] = { 1, 2, 3, 4, 6, 8, 12, 16, 32, 64, 128, 256 };
const int steadyHoldCount = 12;
const int steadyHoldDefaultIndex = 2; // 3 cycles
}

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
		IDLE_MODE_LIGHT,
		LIGHTS_LEN
	};

	enum expanderModeType {
		emode_auto,
		emode_left,
		emode_right
	};
	actReqValue<expanderModeType> expanderMode = actReqValue<expanderModeType>(emode_auto);
	actReqValue<int> steadyHoldIndex = actReqValue<int>(steadyHoldDefaultIndex);

	enum idleOutputModeType {
		iom_gate,
		iom_phasePos
	};
	actReqValue<idleOutputModeType> idleOutputMode = actReqValue<idleOutputModeType>(iom_gate);
	actReqValue<voltValue> sustainPosVolt = actReqValue<voltValue>(v_p10);
	actReqValue<voltValue> idlePosVolt = actReqValue<voltValue>(v_zero);

	/// -1 = none, 0 = left host, 1 = right host
	int connectedSide = -1;

	enum envMotionType {
		em_steady,
		em_rise,
		em_fall
	};
	envMotionType envMotion = em_steady;
	float prevEnvelope = 0.f;
	bool havePrevEnvelope = false;
	int envMotionSteadyCount = 0;

	EnvelopePhaseExpanderModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
		configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
		configLight(LEFT_EXPAND_LIGHT, "Left envelope connection (Green=active, Red=issue)");
		configLight(RIGHT_EXPAND_LIGHT, "Right envelope connection (Green=active, Red=issue)");
		configLight(IDLE_MODE_LIGHT, "Idle output is phase position if lit");

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
		if (idleOutputMode.act == iom_phasePos)
			outputs[IDLE_OUTPUT].setVoltage(0.f);
	}

	void resetMotionTracker() {
		havePrevEnvelope = false;
		prevEnvelope = 0.f;
		envMotion = em_steady;
		envMotionSteadyCount = 0;
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
		float envelope, float phasePos)
	{
		if (!acceptsHost(from))
			return;

		float hi = voltValues[gateOutHigh.act];
		float lo = voltValues[gateOutLow.act];
		int phaseGateCount = (idleOutputMode.act == iom_phasePos) ? 5 : 6;
		for (int i = 0; i < phaseGateCount; i++)
			outputs[ATTACK_OUTPUT + i].setVoltage((int)phase == i ? hi : lo);

		if (idleOutputMode.act == iom_phasePos) {
			float v;
			if (phase == InfNoiseEnvelopeModule::ep_sustain)
				v = voltValues[sustainPosVolt.act];
			else if (phase == InfNoiseEnvelopeModule::ep_idle)
				v = voltValues[idlePosVolt.act];
			else
				v = phasePos * 10.f;
			outputs[IDLE_OUTPUT].setVoltage(v);
		}

		if (!havePrevEnvelope) {
			prevEnvelope = envelope;
			havePrevEnvelope = true;
			envMotion = em_steady;
			envMotionSteadyCount = 0;
		}
		else {
			float delta = envelope - prevEnvelope;
			if (std::fabs(delta) > 1e-10f) {
				envMotionSteadyCount = 0;
				envMotion = (delta > 0.f) ? em_rise : em_fall;
			}
			else if (envMotion != em_steady) {
				envMotionSteadyCount++;
				int holdIdx = steadyHoldIndex.act;
				if (holdIdx < 0 || holdIdx >= steadyHoldCount)
					holdIdx = steadyHoldDefaultIndex;
				if (envMotionSteadyCount >= steadyHoldCycles[holdIdx])
					envMotion = em_steady;
			}
			prevEnvelope = envelope;
		}

		outputs[RISE_OUTPUT].setVoltage(envMotion == em_rise ? hi : lo);
		outputs[FALL_OUTPUT].setVoltage(envMotion == em_fall ? hi : lo);
		outputs[STDY_OUTPUT].setVoltage(envMotion == em_steady ? hi : lo);
	}

	void onExpanderChange(const ExpanderChangeEvent& e) override {
		mustProcessParams = true;  // Connection state and lights updated in processParams
	}

	void onReset(const ResetEvent& e) override {
		InfNoiseModule::onReset(e);
		expanderMode.setBoth(emode_auto);
		steadyHoldIndex.setBoth(steadyHoldDefaultIndex);
		idleOutputMode.setBoth(iom_gate);
		sustainPosVolt.setBoth(v_p10);
		idlePosVolt.setBoth(v_zero);
		resetMotionTracker();
	}

	void dataFromJson(json_t* rootJ) override {
		InfNoiseModule::dataFromJson(rootJ);
		expanderMode.setBoth((expanderModeType)getJsonInt(rootJ, "expanderMode", (int)emode_auto));
		steadyHoldIndex.setBoth(getJsonInt(rootJ, "steadyHoldIndex", steadyHoldDefaultIndex));
		idleOutputMode.setBoth((idleOutputModeType)getJsonInt(rootJ, "idleOutputMode", (int)iom_gate));
		sustainPosVolt.setBoth((voltValue)getJsonInt(rootJ, "sustainPosVolt", (int)v_p10));
		idlePosVolt.setBoth((voltValue)getJsonInt(rootJ, "idlePosVolt", (int)v_zero));
		resetMotionTracker();
	}

	void dataToJson(json_t* rootJ) override {
		json_object_set_new(rootJ, "expanderMode", json_integer((int)expanderMode.req));
		json_object_set_new(rootJ, "steadyHoldIndex", json_integer(steadyHoldIndex.req));
		json_object_set_new(rootJ, "idleOutputMode", json_integer((int)idleOutputMode.req));
		json_object_set_new(rootJ, "sustainPosVolt", json_integer((int)sustainPosVolt.req));
		json_object_set_new(rootJ, "idlePosVolt", json_integer((int)idlePosVolt.req));
	}

	void processParams(const ProcessArgs& args) {
		preProcessParams(args);
		//--------------------

		expanderMode.updateActual();
		steadyHoldIndex.updateActual();
		int prevSide = connectedSide;
		updateConnectionState();
		updateExpandLights();
		if (connectedSide != prevSide)
			resetMotionTracker();
		if (connectedSide < 0)
			clearOutputs();

		if (idleOutputMode.needsUpdate()) {
			idleOutputMode.updateActual();

			if (idleOutputMode.act == iom_phasePos)
				outputInfos[IDLE_OUTPUT]->name = monoPortPrefix() + "Phase position (0V to 10V)";
			else
				outputInfos[IDLE_OUTPUT]->name = monoPortPrefix() + "Idle-phase gate";

			lights[IDLE_MODE_LIGHT].setBrightness(idleOutputMode.act == iom_phasePos ? 1.f : 0.f);
		}
		sustainPosVolt.updateActual();
		idlePosVolt.updateActual();

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
		static_cast<EnvelopePhaseExpanderModule*>(left)->receiveHostState(this, phase, envelope, phasePos);
	}
	Module* right = getRightExpander().module;
	if (right && right->model == modelEnvelopePhaseExpander) {
		static_cast<EnvelopePhaseExpanderModule*>(right)->receiveHostState(this, phase, envelope, phasePos);
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
		addChild(createLightCentered<TinyLight<RedLight>>(Vec(5.164f, 213.083f), module,
			EnvelopePhaseExpanderModule::IDLE_MODE_LIGHT));
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
		menu->addChild(createIndexPtrSubmenuItem("Steady hold", {
			"Immediate (1 cycle)",
			"2 cycles",
			"3 cycles (default)",
			"4 cycles",
			"6 cycles",
			"8 cycles",
			"12 cycles",
			"16 cycles",
			"32 cycles",
			"64 cycles",
			"128 cycles",
			"256 cycles"
		}, &module->steadyHoldIndex.req));

		menu->addChild(createIndexPtrSubmenuItem("Idle output-mode", {
			"Gate (idle phase active)",
			"Phase position (0V to 10V)"
		}, &module->idleOutputMode.req));
		std::vector<std::string> voltNames = getVoltValuesNames();
		
		auto idleVoltDisabled = [=]() {
			return module->idleOutputMode.req != EnvelopePhaseExpanderModule::iom_phasePos;
		};

		DynamicDisabledMenuItem* sustainVoltItem = createIndexSubmenuItem<DynamicDisabledMenuItem>(
			"Sustain voltage", voltNames,
			[=]() { return (size_t)module->sustainPosVolt.req; },
			[=](size_t index) { module->sustainPosVolt.req = (voltValue)index; });
		sustainVoltItem->disabledWhen = idleVoltDisabled;
		menu->addChild(sustainVoltItem);
		DynamicDisabledMenuItem* idleVoltItem = createIndexSubmenuItem<DynamicDisabledMenuItem>(
			"Idle voltage", voltNames,
			[=]() { return (size_t)module->idlePosVolt.req; },
			[=](size_t index) { module->idlePosVolt.req = (voltValue)index; });
		idleVoltItem->disabledWhen = idleVoltDisabled;
		menu->addChild(idleVoltItem);

		appendInfNoiseMenuItems(menu);
	}
};

Model* modelEnvelopePhaseExpander = createModel<EnvelopePhaseExpanderModule, EnvelopePhaseExpanderModuleWidget>("EnvelopePhaseExpander");
