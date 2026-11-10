// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "InfNoiseStack.hpp"

struct StackExpanderModule : InfNoiseModule {
	enum ParamId {
		CYCLE_PARAM,
		EMPTY_MODE_PARAM,
		RESET_PARAM,
		PARAMS_LEN
	};
	enum InputsId {
		CYCLE_INPUT,
		RESET_INPUT,
		INPUTS_LEN
	};
	enum OutputsId {
		VALUE_OUTPUT,
		OVERFLOW_OUTPUT,
		UNDERFLOW_OUTPUT,
		FULL_OUTPUT,
		EMPTY_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(PROCQUAL_LIGHT, 2),
		ENUMS(CLIP_RANGE_LIGHT, 2),
		ENUMS(LEFT_EXPAND_LIGHT, 2),
		ENUMS(RIGHT_EXPAND_LIGHT, 2),
		ENUMS(DIR_LIGHT, 3),
		ENUMS(SLOT_LIGHT, 16 * 3),
		LIGHTS_LEN
	};

	enum CycleOrder {
		co_next = 0,
		co_prev,
		co_ping,
		co_random,
		co_randomDiff,
		co_len
	};

	InfNoiseStackLink link;
	actReqValue<int> cycleOrder = actReqValue<int>(co_next);
	int pingDir = 1;
	uint32_t selectedId = 0;
	float cycleValues[PORT_MAX_CHANNELS];
	int cycleChannels = 1;
	quantizeMode writtenQuantize = qm_off; // Quantize mode last applied to Value

	infNoiseInTrigger cycleTrigger;
	infNoiseInTrigger resetTrigger;
	infNoiseOutTrigger overflowTrigger;
	infNoiseOutTrigger underflowTrigger;

	StackExpanderModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
		configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
		configLight(LEFT_EXPAND_LIGHT, "Left stack connection (green = active, red = issue)");
		configLight(RIGHT_EXPAND_LIGHT, "Right stack connection (green = active, red = issue)");
		configLight(DIR_LIGHT, "Cycle direction");

		configSwitch(CYCLE_PARAM, 0.f, 1.f, 0.f, "Cycle");
		configSwitch(EMPTY_MODE_PARAM, 0.f, 1.f, 0.f, "Cycle when empty", {"Zero", "Hold"});
		configInput(CYCLE_INPUT, "Cycle trigger");
		configSwitch(RESET_PARAM, 0.f, 1.f, 0.f, "Cycle reset");
		configInput(RESET_INPUT, "Cycle reset trigger");

		configOutput(VALUE_OUTPUT, "Value");
		outputs[VALUE_OUTPUT].setChannels(1);
		outputs[VALUE_OUTPUT].setVoltage(0.f);
		configOutput(OVERFLOW_OUTPUT, "Overflow trigger");
		configOutput(UNDERFLOW_OUTPUT, "Underflow trigger");
		configOutput(FULL_OUTPUT, "Full gate");
		configOutput(EMPTY_OUTPUT, "Empty gate");

		for (int i = 0; i < 16; i++)
			configLight(SLOT_LIGHT + i * 3, string::f("Slot %d (newest at top-left)", i + 1));

		for (int c = 0; c < PORT_MAX_CHANNELS; c++)
			cycleValues[c] = 0.f;

		haveProcQuality = true;
		haveAutoProcQuality = false;
		haveOutQuantize = true;
		haveOutClipRange = false;
		haveGateDetect = false;
		haveGateHighLow = true;
		haveTrigDetect = true;
		haveTrigHighLow = true;
	}

	void onInTrigTimeChanged() override {
		cycleTrigger.setDelayCycles(trigInDelayCycles);
		resetTrigger.setDelayCycles(trigInDelayCycles);
	}

	void onOutTrigTimeChanged() override {
		overflowTrigger.setCycles(trigOnCycles, trigOffCycles);
		overflowTrigger.setDelayCycles(trigOutDelayCycles);
		underflowTrigger.setCycles(trigOnCycles, trigOffCycles);
		underflowTrigger.setDelayCycles(trigOutDelayCycles);
	}

	void clearCycleValue() {
		cycleChannels = 1;
		selectedId = 0;
		for (int c = 0; c < PORT_MAX_CHANNELS; c++)
			cycleValues[c] = 0.f;
	}

	void refreshValueChannels() {
		int ch = clamp(cycleChannels, 1, PORT_MAX_CHANNELS);
		int prev = outputs[VALUE_OUTPUT].getChannels();
		if (prev > 0 && prev != ch)
			outputs[VALUE_OUTPUT].setChannels(ch);
	}

	void writeCycle(InfNoiseStackModule* host) {
		int ch = clamp(cycleChannels, 1, PORT_MAX_CHANNELS);
		outputs[VALUE_OUTPUT].setChannels(ch);
		for (int c = 0; c < ch; c++) {
			float v = (c < cycleChannels) ? cycleValues[c] : 0.f;
			v = quantizeToMode(v, outQuantize.act);
			if (host)
				v = clipToVoltRange(v, host->outClipRange.act, host->outClipMode.act);
			outputs[VALUE_OUTPUT].setVoltage(v, c);
		}
		writtenQuantize = outQuantize.act;
	}

	void holdOrZero(InfNoiseStackModule* host) {
		if (params[EMPTY_MODE_PARAM].getValue() >= 0.5f)
			return;
		clearCycleValue();
		writeCycle(host);
	}

	void copyFrame(const InfNoiseStackData::Frame& frame, InfNoiseStackModule* host) {
		cycleChannels = clamp(frame.channels, 1, PORT_MAX_CHANNELS);
		for (int c = 0; c < PORT_MAX_CHANNELS; c++)
			cycleValues[c] = (c < frame.channels) ? frame.v[c] : 0.f;
		selectedId = frame.id;
		writeCycle(host);
	}

	void cycleStack(InfNoiseStackModule* host, bool resetToOldest) {
		if (!host) {
			holdOrZero(nullptr);
			return;
		}
		int acc = host->stack.accessible();
		if (acc <= 0) {
			holdOrZero(host);
			return;
		}
		int idx;
		if (resetToOldest) {
			idx = 0;
		}
		else {
			int current = host->stack.findAccessible(selectedId);
			idx = infNoiseCycleIndex(current, acc, cycleOrder.act, pingDir, current >= 0);
		}
		const InfNoiseStackData::Frame* frame = host->stack.accessibleAt(idx);
		if (!frame) {
			holdOrZero(host);
			return;
		}
		copyFrame(*frame, host);
	}

	void updateDirectionLight() {
		static const float dirR[co_len] = {0.f, 1.f, 1.f, 1.f, 0.f};
		static const float dirG[co_len] = {1.f, 0.f, 1.f, 0.5f, 0.f};
		static const float dirB[co_len] = {0.f, 0.f, 0.f, 0.f, 1.f};
		int mode = cycleOrder.act;
		if (mode < 0 || mode >= co_len)
			mode = co_next;
		lights[DIR_LIGHT].setBrightness(dirR[mode]);
		lights[DIR_LIGHT + 1].setBrightness(dirG[mode]);
		lights[DIR_LIGHT + 2].setBrightness(dirB[mode]);
	}

	void updateSlotLights(InfNoiseStackModule* host) {
		int size = clamp(host ? host->sizeKnob : 0, 0, 16);
		int acc = host ? host->stack.accessible() : 0;
		for (int i = 0; i < 16; i++) {
			float r = 0.f;
			float g = 0.f;
			float b = 0.f;
			if (host && i < size) {
				if (i < acc) {
					const InfNoiseStackData::Frame* frame = host->stack.accessibleAt(acc - 1 - i);
					if (frame && selectedId != 0 && frame->id == selectedId)
						b = 1.f;
					else
						g = 1.f;
				}
				else {
					r = 1.f;
				}
			}
			lights[SLOT_LIGHT + i * 3].setBrightness(r);
			lights[SLOT_LIGHT + i * 3 + 1].setBrightness(g);
			lights[SLOT_LIGHT + i * 3 + 2].setBrightness(b);
		}
	}

	void writeTrigger(infNoiseOutTrigger& trig, int outputId) {
		float v = trig.isHigh() ? voltValues[trigOutHigh.act] : voltValues[trigOutLow.act];
		outputs[outputId].setVoltage(v);
	}

	void writeGates(InfNoiseStackModule* host) {
		float hi = voltValues[gateOutHigh.act];
		float lo = voltValues[gateOutLow.act];
		bool full = false;
		bool empty = false;
		if (host) {
			int acc = host->stack.accessible();
			full = acc >= host->sizeKnob;
			empty = acc <= 0;
		}
		outputs[FULL_OUTPUT].setVoltage(full ? hi : lo);
		outputs[EMPTY_OUTPUT].setVoltage(empty ? hi : lo);
	}

	void scanButtons(InfNoiseStackModule* host, int cycles) {
		float low = trueDetectValues[trigDetLow.act];
		float high = trueDetectValues[trigDetHigh.act];
		bool doReset = resetTrigger.processMomentary(params[RESET_PARAM], inputs[RESET_INPUT], low, high, cycles);
		bool doCycle = cycleTrigger.processMomentary(params[CYCLE_PARAM], inputs[CYCLE_INPUT], low, high, cycles);
		if (doReset)
			cycleStack(host, true);
		else if (doCycle)
			cycleStack(host, false);
	}

	void exchange(InfNoiseStackModule* host) {
		if (!link.accepts(this, host))
			return;
		int cycles = host->procCycles;
		scanButtons(host, cycles);
		overflowTrigger.process(cycles);
		if (host->pushOverflow)
			overflowTrigger.trigger();
		underflowTrigger.process(cycles);
		if (host->popUnderflow)
			underflowTrigger.trigger();
		writeTrigger(overflowTrigger, OVERFLOW_OUTPUT);
		writeTrigger(underflowTrigger, UNDERFLOW_OUTPUT);
		writeGates(host);
	}

	void runUnconnected() {
		scanButtons(nullptr, procCycles);
		overflowTrigger.process(procCycles);
		underflowTrigger.process(procCycles);
		writeTrigger(overflowTrigger, OVERFLOW_OUTPUT);
		writeTrigger(underflowTrigger, UNDERFLOW_OUTPUT);
		writeGates(nullptr);
	}

	void onExpanderChange(const ExpanderChangeEvent& e) override {
		mustProcessParams = true;
	}

	void onReset(const ResetEvent& e) override {
		InfNoiseModule::onReset(e);
		link.mode.setBoth(InfNoiseStackLink::Auto);
		cycleOrder.setBoth(co_next);
		pingDir = 1;
		clearCycleValue();
		cycleTrigger.reset();
		resetTrigger.reset();
		overflowTrigger.reset();
		underflowTrigger.reset();
		writeCycle(nullptr);
	}

	void dataFromJson(json_t* rootJ) override {
		InfNoiseModule::dataFromJson(rootJ);
		link.mode.setBoth(getJsonInt(rootJ, "expanderMode", (int)InfNoiseStackLink::Auto));
		cycleOrder.setBoth(getJsonInt(rootJ, "cycleOrder", (int)co_next));
		pingDir = getJsonInt(rootJ, "pingDir", 1) < 0 ? -1 : 1;
		cycleChannels = getJsonInt(rootJ, "cycleChannels", 1);
		cycleChannels = clamp(cycleChannels, 1, PORT_MAX_CHANNELS);
		getJsonFloatArray(rootJ, "cycleValues", cycleValues, PORT_MAX_CHANNELS, 0.f);
		selectedId = 0;
	}

	void dataToJson(json_t* rootJ) override {
		json_object_set_new(rootJ, "expanderMode", json_integer(link.mode.req));
		json_object_set_new(rootJ, "cycleOrder", json_integer(cycleOrder.req));
		json_object_set_new(rootJ, "pingDir", json_integer(pingDir));
		json_object_set_new(rootJ, "cycleChannels", json_integer(cycleChannels));
		setJsonFloatArray(rootJ, "cycleValues", cycleValues, PORT_MAX_CHANNELS);
	}

	void processParams(const ProcessArgs& args) {
		preProcessParams(args);
		//--------------------

		link.mode.updateActual();
		cycleOrder.updateActual();
		link.update(this);
		link.updateLights(this, LEFT_EXPAND_LIGHT, RIGHT_EXPAND_LIGHT);
		updateDirectionLight();
		
		InfNoiseStackModule* host = link.host(this);
		if (wasJustLoaded || writtenQuantize != outQuantize.act)
			writeCycle(host);
		updateSlotLights(host);

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
				runUnconnected();
			refreshValueChannels();
		}

		cycle256++;
	}
};

void infNoiseStackExpander_exchange(Module* neighbor, InfNoiseStackModule* host) {
	if (!neighbor || !host || neighbor->model != modelStackExpander)
		return;
	static_cast<StackExpanderModule*>(neighbor)->exchange(host);
}

struct StackExpanderModuleWidget : InfNoiseModuleWidget {
	StackExpanderModuleWidget(StackExpanderModule* module) {
		initializeWidget(module, "res/StackExpander");

		addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(3.344f, 28.769f), module, StackExpanderModule::LEFT_EXPAND_LIGHT));
		addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(26.656f, 28.769f), module, StackExpanderModule::RIGHT_EXPAND_LIGHT));

		const float slotX[4] = {6.117f, 12.039f, 17.961f, 23.883f};
		const float slotY[4] = {43.516f, 49.438f, 55.360f, 61.282f};
		for (int i = 0; i < 16; i++) {
			addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(
				Vec(slotX[i % 4], slotY[i / 4]), module, StackExpanderModule::SLOT_LIGHT + i * 3));
		}

		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(Vec(2.996f, 74.316f), module, StackExpanderModule::DIR_LIGHT));
		addParam(createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(15.156f, 90.372f), module, StackExpanderModule::CYCLE_PARAM));
		addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_red>>(Vec(24.969f, 103.003f), module, StackExpanderModule::EMPTY_MODE_PARAM));
		addInput(createInputCentered<infNoiseThemedMonoPort>(Vec(15.156f, 115.983f), module, StackExpanderModule::CYCLE_INPUT));

		addParam(createParamCentered<infNoiseLtSmallButton<bc_green, true>>(Vec(5.031f, 137.709f), module, StackExpanderModule::RESET_PARAM));
		addInput(createInputCentered<infNoiseThemedMonoPort>(Vec(15.156f, 150.689f), module, StackExpanderModule::RESET_INPUT));

		addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(15.156f, 186.862f), module, StackExpanderModule::VALUE_OUTPUT));
		addOutput(createOutputCentered<infNoiseThemedMonoPort>(Vec(15.156f, 228.708f), module, StackExpanderModule::OVERFLOW_OUTPUT));
		addOutput(createOutputCentered<infNoiseThemedMonoPort>(Vec(15.156f, 263.414f), module, StackExpanderModule::UNDERFLOW_OUTPUT));
		addOutput(createOutputCentered<infNoiseThemedMonoPort>(Vec(15.156f, 298.120f), module, StackExpanderModule::FULL_OUTPUT));
		addOutput(createOutputCentered<infNoiseThemedMonoPort>(Vec(15.156f, 332.826f), module, StackExpanderModule::EMPTY_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		InfNoiseModuleWidget::appendContextMenu(menu);
		StackExpanderModule* module = dynamic_cast<StackExpanderModule*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexPtrSubmenuItem("Expander-mode",
			{"Auto", "Forced Left", "Forced Right"}, &module->link.mode.req));
		menu->addChild(createIndexPtrSubmenuItem("Cycle direction",
			{"Next", "Previous", "Ping-pong", "Random", "Random different"}, &module->cycleOrder.req));
		appendInfNoiseMenuItems(menu);
	}
};

Model* modelStackExpander = createModel<StackExpanderModule, StackExpanderModuleWidget>("StackExpander");
