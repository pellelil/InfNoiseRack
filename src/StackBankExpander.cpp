// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "InfNoiseStack.hpp"

struct StackBankExpanderModule : InfNoiseModule {
	enum ParamId {
		ENUMS(BANK_PARAM, 4),
		CYCLE_PARAM,
		CYCLE_MODE_PARAM,
		COUNT_PARAM,
		RESET_PARAM,
		SEND_PARAM,
		RETRIEVE_PARAM,
		PARAMS_LEN
	};
	enum InputsId {
		CYCLE_INPUT,
		RESET_INPUT,
		SEND_INPUT,
		RETRIEVE_INPUT,
		INPUTS_LEN
	};
	enum OutputsId {
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(PROCQUAL_LIGHT, 2),
		ENUMS(CLIP_RANGE_LIGHT, 2),
		ENUMS(LEFT_EXPAND_LIGHT, 2),
		ENUMS(RIGHT_EXPAND_LIGHT, 2),
		ENUMS(BANK_LIGHT, 4),
		AUTO_SEND_LIGHT,
		LIGHTS_LEN
	};

	InfNoiseStackLink link;
	InfNoiseStackData banks[4];
	int activeBank = 0;
	int pingDir = 1;
	actReqValue<bool> autoSend = actReqValue<bool>(false);

	dsp::SchmittTrigger bankTrigger[4]; // Button-only; no CV input
	infNoiseInTrigger cycleTrigger;
	infNoiseInTrigger resetTrigger;
	infNoiseInTrigger sendTrigger;
	infNoiseInTrigger retrieveTrigger;

	StackBankExpanderModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
		configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
		configLight(LEFT_EXPAND_LIGHT, "Left stack connection (green = active, red = issue)");
		configLight(RIGHT_EXPAND_LIGHT, "Right stack connection (green = active, red = issue)");
		configLight(AUTO_SEND_LIGHT, "Auto-send on cycle");

		for (int i = 0; i < 4; i++) {
			configSwitch(BANK_PARAM + i, 0.f, 1.f, 0.f, string::f("Bank %d", i + 1));
			configLight(BANK_LIGHT + i, string::f("Bank %d selected", i + 1));
		}
		configSwitch(CYCLE_PARAM, 0.f, 1.f, 0.f, "Cycle");
		configSwitch(CYCLE_MODE_PARAM, 0.f, 4.f, 0.f, "Cycle",
			{"Next", "Previous", "Ping-pong", "Random", "Random different"});
		configInput(CYCLE_INPUT, "Cycle trigger");
		configSwitch(COUNT_PARAM, 0.f, 2.f, 0.f, "Banks in cycle", {"2", "3", "4"});
		configSwitch(RESET_PARAM, 0.f, 1.f, 0.f, "Reset to bank 1");
		configInput(RESET_INPUT, "Reset trigger");
		configSwitch(SEND_PARAM, 0.f, 1.f, 0.f, "Send");
		configInput(SEND_INPUT, "Send trigger");
		configSwitch(RETRIEVE_PARAM, 0.f, 1.f, 0.f, "Retrieve");
		configInput(RETRIEVE_INPUT, "Retrieve trigger");

		haveProcQuality = true;
		haveAutoProcQuality = false;
		haveOutQuantize = false;
		haveOutClipRange = false;
		haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = true;
		haveTrigHighLow = false;
	}

	int cycleBankCount() {
		int n = (int)(params[COUNT_PARAM].getValue() + 0.5f) + 2;
		if (n < 2)
			n = 2;
		if (n > 4)
			n = 4;
		return n;
	}

	void updateBankLights() {
		if (activeBank < 0 || activeBank > 3)
			activeBank = 0;
		for (int i = 0; i < 4; i++)
			lights[BANK_LIGHT + i].setBrightness(i == activeBank ? 1.f : 0.f);
		lights[AUTO_SEND_LIGHT].setBrightness(autoSend.act ? 1.f : 0.f);
	}

	void sendActive(InfNoiseStackModule* host) {
		if (!host)
			return;
		if (activeBank < 0 || activeBank > 3)
			activeBank = 0;
		host->stack.replaceAll(banks[activeBank], host->sizeKnob);
	}

	void retrieveActive(InfNoiseStackModule* host) {
		if (!host)
			return;
		if (activeBank < 0 || activeBank > 3)
			activeBank = 0;
		host->stack.copyAccessibleTo(banks[activeBank]);
	}

	void selectCycledBank() {
		int n = cycleBankCount();
		int mode = (int)(params[CYCLE_MODE_PARAM].getValue() + 0.5f);
		bool inside = activeBank >= 0 && activeBank < n;
		activeBank = clamp(infNoiseCycleIndex(activeBank, n, mode, pingDir, inside), 0, 3);
	}

	void scanButtons(InfNoiseStackModule* host, int cycles) {
		autoSend.updateActual();
		float low = trueDetectValues[trigDetLow.act];
		float high = trueDetectValues[trigDetHigh.act];

		for (int i = 0; i < 4; i++) {
			if (bankTrigger[i].process(params[BANK_PARAM + i].getValue(), 0.1f, 0.5f))
				activeBank = i;
		}

		bool doReset = resetTrigger.processMomentary(params[RESET_PARAM], inputs[RESET_INPUT], low, high, cycles);
		bool doCycle = cycleTrigger.processMomentary(params[CYCLE_PARAM], inputs[CYCLE_INPUT], low, high, cycles);
		bool doSend = sendTrigger.processMomentary(params[SEND_PARAM], inputs[SEND_INPUT], low, high, cycles);
		bool doRetrieve = retrieveTrigger.processMomentary(params[RETRIEVE_PARAM], inputs[RETRIEVE_INPUT], low, high, cycles);

		if (doReset) {
			activeBank = 0;
			pingDir = 1;
			if (autoSend.act)
				sendActive(host);
		}
		else if (doCycle) {
			selectCycledBank();
			if (autoSend.act)
				sendActive(host);
		}

		if (doRetrieve)
			retrieveActive(host);
		if (doSend)
			sendActive(host);
	}

	void exchange(InfNoiseStackModule* host) {
		if (!link.accepts(this, host))
			return;
		scanButtons(host, host->procCycles);
	}

	void onInTrigTimeChanged() override {
		cycleTrigger.setDelayCycles(trigInDelayCycles);
		resetTrigger.setDelayCycles(trigInDelayCycles);
		sendTrigger.setDelayCycles(trigInDelayCycles);
		retrieveTrigger.setDelayCycles(trigInDelayCycles);
	}

	void onExpanderChange(const ExpanderChangeEvent& e) override {
		mustProcessParams = true;
	}

	void onReset(const ResetEvent& e) override {
		InfNoiseModule::onReset(e);
		link.mode.setBoth(InfNoiseStackLink::Auto);
		autoSend.setBoth(false);
		activeBank = 0;
		pingDir = 1;
		for (int i = 0; i < 4; i++) {
			banks[i].clear();
			banks[i].appliedSize = InfNoiseStackData::kSlots;
			bankTrigger[i].reset();
		}
		cycleTrigger.reset();
		resetTrigger.reset();
		sendTrigger.reset();
		retrieveTrigger.reset();
	}

	void dataFromJson(json_t* rootJ) override {
		InfNoiseModule::dataFromJson(rootJ);
		link.mode.setBoth(getJsonInt(rootJ, "expanderMode", (int)InfNoiseStackLink::Auto));
		autoSend.setBoth(getJsonBool(rootJ, "autoSend", false));
		pingDir = getJsonInt(rootJ, "pingDir", 1) < 0 ? -1 : 1;
		activeBank = clamp(getJsonInt(rootJ, "activeBank", 0), 0, 3);
		for (int i = 0; i < 4; i++) {
			std::string countKey = string::f("bankCount%d", i);
			std::string channelsKey = string::f("bankChannels%d", i);
			std::string valuesKey = string::f("bankValues%d", i);
			banks[i].readJson(rootJ, countKey.c_str(), channelsKey.c_str(), valuesKey.c_str(), InfNoiseStackData::kSlots);
		}
	}

	void dataToJson(json_t* rootJ) override {
		json_object_set_new(rootJ, "expanderMode", json_integer(link.mode.req));
		json_object_set_new(rootJ, "autoSend", json_boolean(autoSend.req));
		json_object_set_new(rootJ, "pingDir", json_integer(pingDir));
		json_object_set_new(rootJ, "activeBank", json_integer(activeBank));
		for (int i = 0; i < 4; i++) {
			std::string countKey = string::f("bankCount%d", i);
			std::string channelsKey = string::f("bankChannels%d", i);
			std::string valuesKey = string::f("bankValues%d", i);
			banks[i].writeJson(rootJ, countKey.c_str(), channelsKey.c_str(), valuesKey.c_str());
		}
	}

	void processParams(const ProcessArgs& args) {
		preProcessParams(args);
		//--------------------

		link.mode.updateActual();
		autoSend.updateActual();
		link.update(this);
		link.updateLights(this, LEFT_EXPAND_LIGHT, RIGHT_EXPAND_LIGHT);
		updateBankLights();

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
			if (link.connectedSide < 0)
				scanButtons(nullptr, procCycles);
		}

		cycle256++;
	}
};

void infNoiseStackBank_exchange(Module* neighbor, InfNoiseStackModule* host) {
	if (!neighbor || !host || neighbor->model != modelStackBankExpander)
		return;
	static_cast<StackBankExpanderModule*>(neighbor)->exchange(host);
}

struct StackBankExpanderModuleWidget : InfNoiseModuleWidget {
	StackBankExpanderModuleWidget(StackBankExpanderModule* module) {
		initializeWidget(module, "res/StackBankExpander");

		addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(2.837f, 28.573f), module, StackBankExpanderModule::LEFT_EXPAND_LIGHT));
		addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(27.321f, 28.573f), module, StackBankExpanderModule::RIGHT_EXPAND_LIGHT));

		const float bankX[4] = {4.329f, 11.496f, 18.662f, 25.829f};
		for (int i = 0; i < 4; i++) {
			addChild(createLightCentered<TinyLight<GreenLight>>(Vec(bankX[i], 42.331f), module, StackBankExpanderModule::BANK_LIGHT + i));
			addParam(createParamCentered<infNoiseLtSmallButton<bc_green, true>>(Vec(bankX[i], 47.988f), module, StackBankExpanderModule::BANK_PARAM + i));
		}

		addChild(createLightCentered<TinyLight<RedLight>>(Vec(2.998f, 63.693f), module, StackBankExpanderModule::AUTO_SEND_LIGHT));
		addParam(createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(15.075f, 79.796f), module, StackBankExpanderModule::CYCLE_PARAM));
		addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_green, bc_red, bc_yellow, bc_orange, bc_blue>>(
			Vec(25.397f, 92.412f), module, StackBankExpanderModule::CYCLE_MODE_PARAM));
		addInput(createInputCentered<infNoiseThemedMonoPort>(Vec(15.079f, 105.392f), module, StackBankExpanderModule::CYCLE_INPUT));

		addParam(createParamCentered<CKSSThree>(Vec(12.038f, 134.159f), module, StackBankExpanderModule::COUNT_PARAM));

		addParam(createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(15.075f, 176.336f), module, StackBankExpanderModule::RESET_PARAM));
		addInput(createInputCentered<infNoiseThemedMonoPort>(Vec(15.079f, 201.932f), module, StackBankExpanderModule::RESET_INPUT));

		addParam(createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(15.075f, 241.943f), module, StackBankExpanderModule::SEND_PARAM));
		addInput(createInputCentered<infNoiseThemedMonoPort>(Vec(15.079f, 267.539f), module, StackBankExpanderModule::SEND_INPUT));

		addParam(createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(15.075f, 307.230f), module, StackBankExpanderModule::RETRIEVE_PARAM));
		addInput(createInputCentered<infNoiseThemedMonoPort>(Vec(15.079f, 332.826f), module, StackBankExpanderModule::RETRIEVE_INPUT));
	}

	void appendContextMenu(Menu* menu) override {
		InfNoiseModuleWidget::appendContextMenu(menu);
		StackBankExpanderModule* module = dynamic_cast<StackBankExpanderModule*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexPtrSubmenuItem("Expander-mode",
			{"Auto", "Forced Left", "Forced Right"}, &module->link.mode.req));
		menu->addChild(createBoolPtrMenuItem("Auto-send on cycle", "", &module->autoSend.req));
		appendInfNoiseMenuItems(menu);
	}
};

Model* modelStackBankExpander = createModel<StackBankExpanderModule, StackBankExpanderModuleWidget>("StackBankExpander");
