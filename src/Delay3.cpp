// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inUtil.hpp"

struct Delay3Module : InfNoiseModule {
	enum ParamId {
		A_DELAY_PARAM,
		B_DELAY_PARAM,
		C_DELAY_PARAM,
		PARAMS_LEN
	};
	enum InputsId {
		A_INPUT,
		B_INPUT,
		C_INPUT,
		INPUTS_LEN
	};
	enum OutputsId {
		A_OUTPUT,
		B_OUTPUT,
		C_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(PROCQUAL_LIGHT, 2),
		ENUMS(CLIP_RANGE_LIGHT, 2),
		LIGHTS_LEN
	};

	static const int delayDepth = 16;

	static std::vector<std::string> getDelayLabels() {
		std::vector<std::string> names;
		names.push_back("1 cycle");
		for (int d = 2; d <= delayDepth; d++)
			names.push_back(string::f("%d cycles", d));
		return names;
	}

	bool haveOutputs = false;
	int firstIdx = -1;
	int lastIdx = -1;
	int channels[3] = { 1, 1, 1 };
	bool inConn[3] = { false, false, false };
	int lag[3] = { 0, 0, 0 }; // knob 1–16 → 0–15 samples
	int writeIdx[3] = { 0, 0, 0 };
	float buf[3][delayDepth][PORT_MAX_CHANNELS];
	float lastOut[3][PORT_MAX_CHANNELS];
	int lastOutChannels[3] = { 1, 1, 1 };

	Delay3Module() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
		configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

		std::vector<std::string> labels = getDelayLabels();
		configSwitch(A_DELAY_PARAM, 1.f, 16.f, 1.f, "A delay", labels);
		configSwitch(B_DELAY_PARAM, 1.f, 16.f, 1.f, "B delay", labels);
		configSwitch(C_DELAY_PARAM, 1.f, 16.f, 1.f, "C delay", labels);

		configInput(A_INPUT, "A (normalized to 0V)");
		configInput(B_INPUT, "B (normalized to A-output)");
		configInput(C_INPUT, "C (normalized to B-output)");
		configOutput(A_OUTPUT, "A (B-input will normalize to this output)");
		configOutput(B_OUTPUT, "B (C-input will normalize to this output)");
		configOutput(C_OUTPUT, "C");

		configBypass(A_INPUT, A_OUTPUT);
		configBypass(B_INPUT, B_OUTPUT);
		configBypass(C_INPUT, C_OUTPUT);

		haveProcQuality = true;
		haveAutoProcQuality = false;
		haveOutQuantize = false;
		haveOutClipRange = true;
		haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = false;
		haveTrigHighLow = false;

		onResetBuffers();
	}

	void onResetBuffers() {
		for (int i = 0; i < 3; i++) {
			writeIdx[i] = 0;
			lag[i] = 0;
			lastOutChannels[i] = 1;
			for (int c = 0; c < PORT_MAX_CHANNELS; c++)
				lastOut[i][c] = 0.f;
			for (int s = 0; s < delayDepth; s++) {
				for (int c = 0; c < PORT_MAX_CHANNELS; c++)
					buf[i][s][c] = 0.f;
			}
		}
	}

	void onReset(const ResetEvent& e) override {
		InfNoiseModule::onReset(e);
		onResetBuffers();
	}

	void dataFromJson(json_t* rootJ) override {
		InfNoiseModule::dataFromJson(rootJ);
	}

	void dataToJson(json_t* rootJ) override {
		InfNoiseModule::dataToJson(rootJ);
	}

	void processParams(const ProcessArgs& args) {
		preProcessParams(args);
		//--------------------

		firstIdx = -1;
		lastIdx = -1;
		int outputCount = 0;
		int prevChannels = 1;
		for (int i = 0; i < 3; i++) {
			inConn[i] = inputs[A_INPUT + i].isConnected();
			bool outConn = outputs[A_OUTPUT + i].isConnected();
			if (inConn[i] || outConn) {
				if (firstIdx < 0)
					firstIdx = i;
				lastIdx = i;
				if (outConn)
					outputCount++;
			}

			int delay = (int)params[A_DELAY_PARAM + i].getValue();
			if (delay < 1)
				delay = 1;
			if (delay > delayDepth)
				delay = delayDepth;
			lag[i] = delay - 1;

			channels[i] = inConn[i]
				? std::max(inputs[A_INPUT + i].getChannels(), 1)
				: prevChannels;
			outputs[A_OUTPUT + i].setChannels(channels[i]);
			prevChannels = channels[i];
		}
		haveOutputs = outputCount > 0;

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
		if (doProcess && haveOutputs) {
			float curOut[3][PORT_MAX_CHANNELS];
			for (int i = firstIdx; i <= lastIdx; i++) {
				int w = writeIdx[i];
				int r = w - lag[i];
				if (r < 0)
					r += delayDepth;
				int prevCh = (i > 0) ? lastOutChannels[i - 1] : 1;
				for (int c = 0; c < channels[i]; c++) {
					float inVolt = inConn[i]
						? inputs[A_INPUT + i].getPolyVoltage(c)
						: (i == 0)
							? 0.f
							: (prevCh <= 1) ? lastOut[i - 1][0] : lastOut[i - 1][c];
					buf[i][w][c] = inVolt;
					float outVolt = clipToVoltRange(buf[i][r][c], outClipRange.act);
					outputs[A_OUTPUT + i].setVoltage(outVolt, c);
					curOut[i][c] = outVolt;
				}
				writeIdx[i] = (w + 1) & (delayDepth - 1);
			}
			for (int i = firstIdx; i <= lastIdx; i++) {
				for (int c = 0; c < channels[i]; c++)
					lastOut[i][c] = curOut[i][c];
				lastOutChannels[i] = channels[i];
			}
		}

		cycle256++;
	}
};

struct Delay3ModuleWidget : InfNoiseModuleWidget {
	Delay3ModuleWidget(Delay3Module* module) {
		initializeWidget(module, "res/Delay3");

		const float centerCol = 15.079f;
		const float inY[3] = { 51.851f, 159.144f, 266.436f };
		const float knobY[3] = { 83.756f, 191.049f, 298.341f };
		const float outY[3] = { 119.362f, 226.655f, 333.947f };
		for (int i = 0; i < 3; i++) {
			addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, inY[i]), module, Delay3Module::A_INPUT + i));
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, knobY[i]), module, Delay3Module::A_DELAY_PARAM + i));
			addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, outY[i]), module, Delay3Module::A_OUTPUT + i));
		}
	}

	void appendContextMenu(Menu* menu) override {
		InfNoiseModuleWidget::appendContextMenu(menu);
		Delay3Module* module = dynamic_cast<Delay3Module*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator);
		menu->addChild(createSubmenuItem("Set all delays", "",
			[=](Menu* submenu) {
				std::vector<std::string> labels = Delay3Module::getDelayLabels();
				int cur[3];
				for (int i = 0; i < 3; i++)
					cur[i] = (int)module->params[Delay3Module::A_DELAY_PARAM + i].getValue();
				for (int d = 1; d <= Delay3Module::delayDepth; d++) {
					bool allMatch = (cur[0] == d && cur[1] == d && cur[2] == d);
					submenu->addChild(createMenuItem(labels[d - 1],
						allMatch ? CHECKMARK_STRING : "",
						[=]() {
							for (int i = 0; i < 3; i++)
								module->params[Delay3Module::A_DELAY_PARAM + i].setValue((float)d);
						}
					));
				}
			}
		));

		appendInfNoiseMenuItems(menu);
	}
};

Model* modelDelay3 = createModel<Delay3Module, Delay3ModuleWidget>("Delay3");
