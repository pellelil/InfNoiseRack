// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inUtil.hpp"

struct StereoBalancePanModule : InfNoiseModule {
	enum ParamId {
		BP_MODE_PARAM,
		BP_PARAM,
		BP_TRIM_PARAM,
		GAIN_PARAM,
		GAIN_TRIM_PARAM,
		PARAMS_LEN
	};
	enum InputsId {
		BP_INPUT,
		GAIN_INPUT,
		L_INPUT,
		R_INPUT,
		INPUTS_LEN
	};
	enum OutputsId {
		L_OUTPUT,
		R_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(PROCQUAL_LIGHT, 2),
		ENUMS(CLIP_RANGE_LIGHT, 2),
		ENUMS(L_LEVEL_LIGHT, 3),
		ENUMS(R_LEVEL_LIGHT, 3),
		LIGHTS_LEN
	};

	bool panMode = false; // true = Pan, false = Balance; set in processParams
	float bpKnob = 0.f;
	float bpTrim = 0.f;
	float gainKnob = 1.f;
	float gainTrim = 0.f;
	int channels = 1;
	bool lInConn = false;
	bool rInConn = false;
	bool lOutConn = false;
	bool rOutConn = false;
	float maxAbsL = 0.f; // peak |L out| since last processParams (which updates lights)
	float maxAbsR = 0.f; // peak |R out| since last processParams (which updates lights)

	StereoBalancePanModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
		configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

		configSwitch(BP_MODE_PARAM, 0.f, 1.f, 0.f, "Balance/Pan mode", { "Balance", "Pan" });
		configParam(BP_PARAM, -1.f, 1.f, 0.f, "Balance/Pan L-R", " %", 0, 100);
		configParam(BP_TRIM_PARAM, -1.f, 1.f, 0.f, "Balance/Pan CV trim", " %", 0, 100);
		configParam(GAIN_PARAM, 0.f, 2.f, 1.f, "Gain (0% to 200%)", " %", 0, 100);
		configParam(GAIN_TRIM_PARAM, -1.f, 1.f, 0.f, "Gain CV trim", " %", 0, 100);

		configInput(BP_INPUT, "Balance/Pan CV");
		configInput(GAIN_INPUT, "Gain CV");
		configInput(L_INPUT, "Left");
		configInput(R_INPUT, "Right (normalized to Left)");

		configOutput(L_OUTPUT, "Left");
		configOutput(R_OUTPUT, "Right");

		configLight(L_LEVEL_LIGHT, "Left output level");
		configLight(R_LEVEL_LIGHT, "Right output level");

		configBypass(L_INPUT, L_OUTPUT);
		configBypass(R_INPUT, R_OUTPUT);

		haveProcQuality = true;
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
		maxAbsL = 0.f;
		maxAbsR = 0.f;
	}

	void dataFromJson(json_t* rootJ) override {
		InfNoiseModule::dataFromJson(rootJ);
		maxAbsL = 0.f;
		maxAbsR = 0.f;
	}

	void dataToJson(json_t* rootJ) override {
		InfNoiseModule::dataToJson(rootJ);
	}

	void processParams(const ProcessArgs& args) {
		preProcessParams(args);
		//--------------------

		panMode = params[BP_MODE_PARAM].getValue() > 0.5f;
		bpKnob = params[BP_PARAM].getValue();
		bpTrim = params[BP_TRIM_PARAM].getValue();
		gainKnob = params[GAIN_PARAM].getValue();
		gainTrim = params[GAIN_TRIM_PARAM].getValue();

		lInConn = inputs[L_INPUT].isConnected();
		rInConn = inputs[R_INPUT].isConnected();
		lOutConn = outputs[L_OUTPUT].isConnected();
		rOutConn = outputs[R_OUTPUT].isConnected();

		channels = 1;
		if (lInConn)
			channels = std::max(inputs[L_INPUT].getChannels(), 1);
		if (rInConn)
			channels = std::max(channels, inputs[R_INPUT].getChannels());
		outputs[L_OUTPUT].setChannels(channels);
		outputs[R_OUTPUT].setChannels(channels);

		setAbsLevelMeterLight(this, L_LEVEL_LIGHT, maxAbsL);
		setAbsLevelMeterLight(this, R_LEVEL_LIGHT, maxAbsR);
		maxAbsL = 0.f;
		maxAbsR = 0.f;

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
			float lr = bpKnob;
			if (inputs[BP_INPUT].isConnected()) {
				lr += inputs[BP_INPUT].getVoltage() / 5.f * bpTrim;
				lr = clamp(lr, -1.f, 1.f);
			}
			float t = (lr + 1.f) / 2.f;

			float gain = gainKnob;
			if (inputs[GAIN_INPUT].isConnected()) {
				gain += inputs[GAIN_INPUT].getVoltage() / 5.f * gainTrim;
				gain = clamp(gain, 0.f, 2.f);
			}

			float lGain, rGain;
			if (!panMode) {
				if (t <= 0.5f) {
					lGain = 1.f;
					rGain = t * 2.f;
				}
				else {
					lGain = (1.f - t) * 2.f;
					rGain = 1.f;
				}
			}
			else {
				lGain = std::sqrt(1.f - t);
				rGain = std::sqrt(t);
			}

			for (int c = 0; c < channels; c++) {
				float inL = lInConn ? inputs[L_INPUT].getPolyVoltage(c) : 0.f;
				float inR = rInConn ? inputs[R_INPUT].getPolyVoltage(c) : inL;
				float outL = clipToVoltRange(inL * lGain * gain, outClipRange.act);
				float outR = clipToVoltRange(inR * rGain * gain, outClipRange.act);
 				outputs[L_OUTPUT].setVoltage(outL, c);
				outputs[R_OUTPUT].setVoltage(outR, c);
				maxAbsL = std::max(maxAbsL, std::fabs(outL));
				maxAbsR = std::max(maxAbsR, std::fabs(outR));
			}
		}

		cycle256++;
	}
};

struct StereoBalancePanModuleWidget : InfNoiseModuleWidget {
	StereoBalancePanModuleWidget(StereoBalancePanModule* module) {
		initializeWidget(module, "res/StereoBalancePan");

		const float centerCol = 15.f;
		addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_green>>(
			Vec(4.289f, 38.762f), module, StereoBalancePanModule::BP_MODE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, 53.809f), module, StereoBalancePanModule::BP_PARAM));
		addParam(createParamCentered<Trimpot>(Vec(centerCol, 78.185f), module, StereoBalancePanModule::BP_TRIM_PARAM));
		addInput(createInputCentered<ThemedPJ301MPort>(Vec(centerCol, 102.910f), module, StereoBalancePanModule::BP_INPUT));

		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(Vec(4.235f, 130.533f), module, StereoBalancePanModule::L_LEVEL_LIGHT));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(Vec(25.765f, 130.533f), module, StereoBalancePanModule::R_LEVEL_LIGHT));
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, 140.687f), module, StereoBalancePanModule::GAIN_PARAM));
		addParam(createParamCentered<Trimpot>(Vec(centerCol, 165.062f), module, StereoBalancePanModule::GAIN_TRIM_PARAM));
		addInput(createInputCentered<ThemedPJ301MPort>(Vec(centerCol, 189.788f), module, StereoBalancePanModule::GAIN_INPUT));

		addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 226.973f), module, StereoBalancePanModule::L_INPUT));
		addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 262.047f), module, StereoBalancePanModule::R_INPUT));
		addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 297.120f), module, StereoBalancePanModule::L_OUTPUT));
		addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 332.194f), module, StereoBalancePanModule::R_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		InfNoiseModuleWidget::appendContextMenu(menu);
		appendInfNoiseMenuItems(menu);
	}
};

Model* modelStereoBalancePan =
	createModel<StereoBalancePanModule, StereoBalancePanModuleWidget>("StereoBalancePan");
