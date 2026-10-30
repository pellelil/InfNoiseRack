// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "InfNoiseStack.hpp"

struct StackModule : InfNoiseStackModule {
	enum ParamId {
		PUSH_PARAM,
		PUSH_MODE_PARAM,
		POP_PARAM,
		POP_MODE_PARAM,
		RESET_PARAM,
		RESET_MODE_PARAM,
		SIZE_PARAM,
		ORDER_PARAM,
		PARAMS_LEN
	};
	enum InputsId {
		PUSH_INPUT,
		POP_INPUT,
		RESET_INPUT,
		CV_INPUT,
		INPUTS_LEN
	};
	enum OutputsId {
		CV_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(PROCQUAL_LIGHT, 2),
		ENUMS(CLIP_RANGE_LIGHT, 2),
		ENUMS(PUSH_LIGHT, 2),
		ENUMS(POP_LIGHT, 2),
		LIGHTS_LEN
	};

	infNoiseInTrigger pushTrigger;
	infNoiseInTrigger popTrigger;
	infNoiseInTrigger resetTrigger;
	bool haveInput = false;
	int inChannels = 1; // Cached from CV input in processParams
	int heldOutChannels = 1; // Channel count last written to the stack output

	StackModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
		configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

		configSwitch(PUSH_PARAM, 0.f, 1.f, 0.f, "Push");
		configSwitch(PUSH_MODE_PARAM, 0.f, 1.f, 0.f, "Push when full", {"Ignore", "Rolling"});
		configInput(PUSH_INPUT, "Push trigger");
		configLight(PUSH_LIGHT, "Stack filling (green empty, amber filling, red full)");

		configSwitch(POP_PARAM, 0.f, 1.f, 0.f, "Pop");
		configSwitch(POP_MODE_PARAM, 0.f, 1.f, 0.f, "Pop when empty", {"Zero", "Hold"});
		configInput(POP_INPUT, "Pop trigger");
		configLight(POP_LIGHT, "Stack emptying (green full, amber emptying, red empty)");

		configSwitch(RESET_PARAM, 0.f, 1.f, 0.f, "Reset");
		configSwitch(RESET_MODE_PARAM, 0.f, 1.f, 0.f, "Reset output", {"Zero", "Hold"});
		configInput(RESET_INPUT, "Reset trigger");

		std::vector<std::string> sizeLabels;
		for (int i = 1; i <= InfNoiseStackData::kSlots; i++)
			sizeLabels.push_back(string::f("%d", i));
		configSwitch(SIZE_PARAM, 0.f, 15.f, 15.f, "Size", sizeLabels);
		configSwitch(ORDER_PARAM, 0.f, 1.f, 0.f, "Pop order", {"FIFO", "LIFO"});

		configInput(CV_INPUT, "Stack");
		configOutput(CV_OUTPUT, "Stack");
		outputs[CV_OUTPUT].setChannels(1);
		outputs[CV_OUTPUT].setVoltage(0.f);

		haveProcQuality = true;
		haveAutoProcQuality = false;
		haveOutQuantize = true;
		haveOutClipRange = true;
		haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = true;
		haveTrigHighLow = false;
	}

	void writeZero() {
		heldOutChannels = inChannels;
		writePort(outputs[CV_OUTPUT], nullptr, 0, heldOutChannels);
	}

	void writeFrame(const InfNoiseStackData::Frame& frame) {
		heldOutChannels = haveInput ? inChannels : frame.channels;
		if (heldOutChannels < 1)
			heldOutChannels = 1;
		writePort(outputs[CV_OUTPUT], frame.v, frame.channels, heldOutChannels);
	}

	void updateFillLights() {
		int size = sizeKnob < 1 ? 1 : sizeKnob;
		int acc = clamp(stack.accessible(), 0, size);
		bool full = acc >= size;
		bool empty = acc <= 0;
		// Fade green→amber over the steps before full/empty, so the last of those is amber and red is a distinct jump.
		float steps = (float)(size - 1);
		float pushAmber = (!full && steps > 0.f) ? (float)acc / steps : 0.f;
		float popAmber = (!empty && steps > 0.f) ? (float)(size - acc) / steps : 0.f;
		lights[PUSH_LIGHT].setBrightness(full ? 0.f : 1.f);
		lights[PUSH_LIGHT + 1].setBrightness(full ? 1.f : pushAmber);
		lights[POP_LIGHT].setBrightness(empty ? 0.f : 1.f);
		lights[POP_LIGHT + 1].setBrightness(empty ? 1.f : popAmber);
	}

	void syncOutputChannels() {
		int ch = haveInput ? inChannels : heldOutChannels;
		int prev = outputs[CV_OUTPUT].getChannels();
		// 0 means no cable yet. setChannels() would ignore that, and a later cable starts at 1 channel.
		if (prev == 0 || ch == prev)
			return;
		outputs[CV_OUTPUT].setChannels(ch);
		if (haveInput) {
			for (int c = prev; c < ch; c++)
				outputs[CV_OUTPUT].setVoltage(0.f, c);
		}
	}

	void onInTrigTimeChanged() override {
		pushTrigger.setDelayCycles(trigInDelayCycles);
		popTrigger.setDelayCycles(trigInDelayCycles);
		resetTrigger.setDelayCycles(trigInDelayCycles);
	}

	void onReset(const ResetEvent& e) override {
		InfNoiseModule::onReset(e);
		stack.clear();
		sizeKnob = (int)(params[SIZE_PARAM].getValue() + 0.5f) + 1;
		stack.appliedSize = sizeKnob;
		stack.hidden = 0;
		pushTrigger.reset();
		popTrigger.reset();
		resetTrigger.reset();
		pushOverflow = false;
		popUnderflow = false;
		writeZero();
	}

	void dataFromJson(json_t* rootJ) override {
		InfNoiseModule::dataFromJson(rootJ);
		sizeKnob = (int)(params[SIZE_PARAM].getValue() + 0.5f) + 1;
		stack.readJson(rootJ, "stackCount", "stackChannels", "stackValues", sizeKnob);
	}

	void dataToJson(json_t* rootJ) override {
		stack.writeJson(rootJ, "stackCount", "stackChannels", "stackValues");
	}

	void processParams(const ProcessArgs& args) {
		preProcessParams(args);
		//--------------------

		sizeKnob = (int)(params[SIZE_PARAM].getValue() + 0.5f) + 1;
		stack.applySize(sizeKnob);
		haveInput = inputs[CV_INPUT].isConnected();
		inChannels = haveInput ? inputs[CV_INPUT].getChannels() : 1;
		updateFillLights();

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
			pushOverflow = false;
			popUnderflow = false;

			float low = trueDetectValues[trigDetLow.act];
			float high = trueDetectValues[trigDetHigh.act];
			bool doReset = resetTrigger.processMomentary(params[RESET_PARAM], inputs[RESET_INPUT], low, high, procCycles);
			bool doPop = popTrigger.processMomentary(params[POP_PARAM], inputs[POP_INPUT], low, high, procCycles);
			bool doPush = pushTrigger.processMomentary(params[PUSH_PARAM], inputs[PUSH_INPUT], low, high, procCycles);

			if (doReset) {
				stack.clear();
				if (params[RESET_MODE_PARAM].getValue() < 0.5f)
					writeZero();
			}

			if (doPop) {
				InfNoiseStackData::Frame frame;
				bool fifo = params[ORDER_PARAM].getValue() < 0.5f;
				if (stack.pop(fifo, frame))
					writeFrame(frame);
				else {
					popUnderflow = true;
					if (params[POP_MODE_PARAM].getValue() < 0.5f)
						writeZero();
				}
			}

			if (doPush) {
				InfNoiseStackData::Frame frame;
				frame.channels = inChannels;
				frame.id = 0;
				for (int c = 0; c < PORT_MAX_CHANNELS; c++)
					frame.v[c] = (haveInput && c < inChannels) ? inputs[CV_INPUT].getVoltage(c) : 0.f;
				bool wasFull = false;
				bool rolling = params[PUSH_MODE_PARAM].getValue() >= 0.5f;
				stack.tryPush(sizeKnob, rolling, frame, wasFull);
				if (wasFull)
					pushOverflow = true;
			}

			syncOutputChannels();
			pushToExpanders();
		}

		cycle256++;
	}
};

// Update left/right expanders
void InfNoiseStackModule::pushToExpanders() {  
	Module* left = getLeftExpander().module;
	infNoiseStackExpander_exchange(left, this);
	infNoiseStackBank_exchange(left, this);

	Module* right = getRightExpander().module;
	infNoiseStackExpander_exchange(right, this);
	infNoiseStackBank_exchange(right, this);
}

struct StackModuleWidget : InfNoiseModuleWidget {
	StackModuleWidget(StackModule* module) {
		initializeWidget(module, "res/Stack");

		addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(3.395f, 34.807f), module, StackModule::PUSH_LIGHT));
		addParam(createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(14.996f, 50.308f), module, StackModule::PUSH_PARAM));
		addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_red>>(Vec(24.969f, 62.924f), module, StackModule::PUSH_MODE_PARAM));
		addInput(createInputCentered<infNoiseThemedMonoPort>(Vec(15.f, 75.904f), module, StackModule::PUSH_INPUT));

		addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(3.395f, 93.981f), module, StackModule::POP_LIGHT));
		addParam(createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(14.996f, 110.894f), module, StackModule::POP_PARAM));
		addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_red>>(Vec(24.969f, 123.510f), module, StackModule::POP_MODE_PARAM));
		addInput(createInputCentered<infNoiseThemedMonoPort>(Vec(15.f, 136.490f), module, StackModule::POP_INPUT));

		addParam(createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(14.996f, 169.871f), module, StackModule::RESET_PARAM));
		addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_red>>(Vec(24.969f, 182.487f), module, StackModule::RESET_MODE_PARAM));
		addInput(createInputCentered<infNoiseThemedMonoPort>(Vec(15.f, 195.467f), module, StackModule::RESET_INPUT));

		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(15.f, 229.154f), module, StackModule::SIZE_PARAM));
		addParam(createParamCentered<CKSS>(Vec(9.601f, 265.877f), module, StackModule::ORDER_PARAM));

		addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(15.f, 298.120f), module, StackModule::CV_INPUT));
		addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(15.f, 332.826f), module, StackModule::CV_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		InfNoiseModuleWidget::appendContextMenu(menu);
		appendInfNoiseMenuItems(menu);
	}
};

Model* modelStack = createModel<StackModule, StackModuleWidget>("Stack");
