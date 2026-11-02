// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include <cstring>

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct Flux4Module : InfNoiseModule {
	enum ParamId {
		POS_A_PARAM,
		POS_B_PARAM,
		POS_C_PARAM,
		POS_D_PARAM,
		POS_A_TRIM_PARAM,
		POS_B_TRIM_PARAM,
		POS_C_TRIM_PARAM,
		POS_D_TRIM_PARAM,
		RANGE_A_PARAM,
		RANGE_B_PARAM,
		RANGE_C_PARAM,
		RANGE_D_PARAM,
		RANGE_A_TRIM_PARAM,
		RANGE_B_TRIM_PARAM,
		RANGE_C_TRIM_PARAM,
		RANGE_D_TRIM_PARAM,
		RANGE_LINK_AB_PARAM,
		RANGE_LINK_BC_PARAM,
		RANGE_LINK_CD_PARAM,
		SWEEP_PARAM,
		SWEEP_TRIM_PARAM,
		NORM_510_PARAM,
		LEVEL_A_PARAM,
		LEVEL_B_PARAM,
		LEVEL_C_PARAM,
		LEVEL_D_PARAM,
		SIG_MODE_A_PARAM,
		SIG_MODE_B_PARAM,
		SIG_MODE_C_PARAM,
		SIG_MODE_D_PARAM,
		LVL_MODE_A_PARAM,
		LVL_MODE_B_PARAM,
		LVL_MODE_C_PARAM,
		LVL_MODE_D_PARAM,
		AB_MIX_PARAM,
		CD_MIX_PARAM,
		PARAMS_LEN
	};
	enum InputsId {
		POS_A_INPUT,
		POS_B_INPUT,
		POS_C_INPUT,
		POS_D_INPUT,
		RANGE_A_INPUT,
		RANGE_B_INPUT,
		RANGE_C_INPUT,
		RANGE_D_INPUT,
		LEVEL_A_INPUT,
		LEVEL_B_INPUT,
		LEVEL_C_INPUT,
		LEVEL_D_INPUT,
		SIG_A_INPUT,
		SIG_B_INPUT,
		SIG_C_INPUT,
		SIG_D_INPUT,
		SWEEP_INPUT,
		INPUTS_LEN
	};
	enum OutputsId {
		SIG_A_OUTPUT,
		SIG_B_OUTPUT,
		SIG_C_OUTPUT,
		SIG_D_OUTPUT,
		AB_OUTPUT,
		CD_OUTPUT,
		MIN_OUTPUT,
		MAX_OUTPUT,
		AVG_OUTPUT,
		SUM_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(PROCQUAL_LIGHT, 2),
		ENUMS(CLIP_RANGE_LIGHT, 2),
		ENUMS(Y_A_LIGHT, 2), // brightness tracks Y; green = using Y, red = Level input patched
		ENUMS(Y_B_LIGHT, 2),
		ENUMS(Y_C_LIGHT, 2),
		ENUMS(Y_D_LIGHT, 2),
		ENUMS(NORM_LIGHT, 2), // green = 5V, red = 10V, beside the A input
		NORM_10_LIGHT, // upper light beside the 5/10 switch
		NORM_5_LIGHT, // lower light beside the 5/10 switch
		LIGHTS_LEN
	};

	int channels[4] = {1, 1, 1, 1};
	bool posConnected[4] = {};
	float posKnob[4] = { 2.f, 4.f, 6.f, 8.f };
	float posTrim[4] = {};
	bool usePosCv[4] = {};  // true: Pos depends on CV
	int posSrc[4] = { -1, -1, -1, -1 }; // Pos source (normalized to this column)
	bool rangeConnected[4] = {};
	float rngKnob[4] = { 2.f, 2.f, 2.f, 2.f };
	float rngTrim[4] = {};
	bool useRangeCv[4] = {};  // true: Range depends on CV
	int rangeSrc[4] = { -1, -1, -1, -1 }; // Range source (normalized to this column)
	bool levelConnected[4] = {}; // Levels A-D are connected
	float lvlKnob[4] = { 1.f, 1.f, 1.f, 1.f };
	bool sigConnected[4] = {};  // Inputs A-D are connected
	bool sweepConnected = false;
	float sweepKnob = 0.f;
	float sweepTrim = 0.f;
	bool useSweepCv = false;  // true: Sweep depends on CV
	int maxChannels = 1;  // maximum number of channels (for cumulative outputs)
	float yValue[4] = {0.f, 0.f, 0.f, 0.f};  // AY, BY, CY, DY
	bool calcY[4] = {false, false, false, false}; // true: Y depends on CV, recalculated in process()
	bool haveOutputs = false; // true when: A, B, C, D, A+B, C+D, Min, Max, Average or Sum are connected
	bool haveCummulativeOutputs = false; // true when: A+B, C+D, Min, Max, Average or Sum are connected
	bool haveABOutputs = false; // A+B or C+D connected
	bool haveMinMaxOutputs = false; // Min or Max connected
	bool haveSumAvgOutputs = false; // Sum or Average connected
	float outputAD[4][PORT_MAX_CHANNELS] = {}; // column outputs for the cumulative ports
	float prevInput[PORT_MAX_CHANNELS] = {}; // previous column's raw input
	float abMixScale = 1.f; // 1 = unity sum, 0.5 = average
	float cdMixScale = 1.f; // 1 = unity sum, 0.5 = average
	float normInput = 5.f; // 5V or 10V

	enum inputMode {
		im_unchanged,
		im_inverted,
		im_absolute,
		im_cutNegative,
		im_len
	};
	inputMode sigMode[4] = {};
	inputMode lvlMode[4] = {};

	Flux4Module() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
		configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

		const std::string col[4] = {"A", "B", "C", "D"};
		const float posDefault[4] = {2.f, 4.f, 6.f, 8.f};
		const std::vector<std::string> sigModes = {"Unchanged", "Inverted", "Absolute", "Cut negative"};
		for (int i = 0; i < 4; i++) {
			configParam(POS_A_PARAM + i, 0.f, 10.f, posDefault[i], col[i] + "-Pos");
			configParam(POS_A_TRIM_PARAM + i, -1.f, 1.f, 0.f, col[i] + "-Pos trim", "%", 0, 100);
			configParam(RANGE_A_PARAM + i, 0.f, 10.f, 2.f, col[i] + "-Range");
			configParam(RANGE_A_TRIM_PARAM + i, -1.f, 1.f, 0.f, col[i] + "-Range trim", "%", 0, 100);
			configParam(LEVEL_A_PARAM + i, -1.f, 1.f, 1.f, col[i] + "-Level", "%", 0, 100);
			configSwitch(SIG_MODE_A_PARAM + i, 0.f, 3.f, 0.f, col[i] + "-Input mode", sigModes);
			configSwitch(LVL_MODE_A_PARAM + i, 0.f, 3.f, 0.f, col[i] + "-Level mode", sigModes);
			configLight(Y_A_LIGHT + i * 2, col[i] + "-Y");
			configBypass(SIG_A_INPUT + i, SIG_A_OUTPUT + i);
		}

		configParam(SWEEP_PARAM, 0.f, 10.f, 0.f, "Sweep");
		configParam(SWEEP_TRIM_PARAM, -1.f, 1.f, 0.f, "Sweep trim", "%", 0, 100);
		configSwitch(NORM_510_PARAM, 0.f, 1.f, 0.f, "A normalization", {"5V", "10V"});
		configSwitch(RANGE_LINK_AB_PARAM, 0.f, 1.f, 0.f, "Link B-Range to A", {"Individual", "Linked to A"});
		configSwitch(RANGE_LINK_BC_PARAM, 0.f, 1.f, 0.f, "Link C-Range to B", {"Individual", "Linked to B"});
		configSwitch(RANGE_LINK_CD_PARAM, 0.f, 1.f, 0.f, "Link D-Range to C", {"Individual", "Linked to C"});
		configSwitch(AB_MIX_PARAM, 0.f, 1.f, 0.f, "A+B mix", {"Unity", "Average"});
		configSwitch(CD_MIX_PARAM, 0.f, 1.f, 0.f, "C+D mix", {"Unity", "Average"});

		configInput(POS_A_INPUT, "A-Pos ");
		configInput(POS_B_INPUT, "B-Pos (normalized to A-Pos)");
		configInput(POS_C_INPUT, "C-Pos (normalized to B-Pos)");
		configInput(POS_D_INPUT, "D-Pos (normalized to C-Pos)");
		configInput(RANGE_A_INPUT, "A-Range");
		configInput(RANGE_B_INPUT, "B-Range (normalized to A-Range)");
		configInput(RANGE_C_INPUT, "C-Range (normalized to B-Range)");
		configInput(RANGE_D_INPUT, "D-Range (normalized to C-Range)");
		configInput(LEVEL_A_INPUT, "A-Level (normalized to AY)");
		configInput(LEVEL_B_INPUT, "B-Level (normalized to BY)");
		configInput(LEVEL_C_INPUT, "C-Level (normalized to CY)");
		configInput(LEVEL_D_INPUT, "D-Level (normalized to DY)");
		configInput(SIG_A_INPUT, "A (normalized to 5V or 10V)");
		configInput(SIG_B_INPUT, "B (normalized to A)");
		configInput(SIG_C_INPUT, "C (normalized to B)");
		configInput(SIG_D_INPUT, "D (normalized to C)");
		configInput(SWEEP_INPUT, "Sweep");

		configOutput(SIG_A_OUTPUT, "A");
		configOutput(SIG_B_OUTPUT, "B");
		configOutput(SIG_C_OUTPUT, "C");
		configOutput(SIG_D_OUTPUT, "D");
		configOutput(AB_OUTPUT, "A+B");
		configOutput(CD_OUTPUT, "C+D");
		configOutput(MIN_OUTPUT, "Min");
		configOutput(MAX_OUTPUT, "Max");
		configOutput(AVG_OUTPUT, "Average");
		configOutput(SUM_OUTPUT, "Sum");

		configLight(NORM_LIGHT, "A normalization (green = 5V, red = 10V)");
		configLight(NORM_10_LIGHT, "10V");
		configLight(NORM_5_LIGHT, "5V");

		// Set InfNoise features (e.g. menu-items)
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
	}

	void dataFromJson(json_t* rootJ) override {
		InfNoiseModule::dataFromJson(rootJ);
	}

	void dataToJson(json_t* rootJ) override {
		InfNoiseModule::dataToJson(rootJ);
	}

	inline float applyInputMode(float value, inputMode mode) {
		if (mode == im_unchanged)
			return value;
		if (mode == im_inverted)
			return -value;
		if (mode == im_absolute)
			return fabsf(value);
		return (value < 0.f) ? 0.f : value; // im_cutNegative
	}

	void processParams(const ProcessArgs& args) {
		preProcessParams(args);
		//--------------------

		sweepConnected = inputs[SWEEP_INPUT].isConnected();
		sweepKnob = params[SWEEP_PARAM].getValue();
		sweepTrim = params[SWEEP_TRIM_PARAM].getValue();
		useSweepCv = sweepConnected && sweepTrim != 0.f;

		maxChannels = 1;
		int posSrcCol = -1; // Pos source (normalized to this column)
		int rangeSrcCol = -1; // Range source (normalized to this column)
		for (int i = 0; i < 4; i++) {
			// Handle range linking
			if (i > 0 && params[RANGE_LINK_AB_PARAM + i - 1].getValue() > 0.5f) {
				params[RANGE_A_PARAM + i].setValue(params[RANGE_A_PARAM + i - 1].getValue());
				params[RANGE_A_TRIM_PARAM + i].setValue(params[RANGE_A_TRIM_PARAM + i - 1].getValue());
			}

			// Detect input modes
			sigMode[i] = (inputMode)(int)(params[SIG_MODE_A_PARAM + i].getValue() + 0.5f);
			lvlMode[i] = (inputMode)(int)(params[LVL_MODE_A_PARAM + i].getValue() + 0.5f);

			// Cache active input connections
			posConnected[i] = inputs[POS_A_INPUT + i].isConnected();
			rangeConnected[i] = inputs[RANGE_A_INPUT + i].isConnected();
			levelConnected[i] = inputs[LEVEL_A_INPUT + i].isConnected();
			sigConnected[i] = inputs[SIG_A_INPUT + i].isConnected();

			// Cache knobs and trims; CV is applied only when a trim has a source port
			posKnob[i] = params[POS_A_PARAM + i].getValue();
			posTrim[i] = params[POS_A_TRIM_PARAM + i].getValue();
			if (posConnected[i])
				posSrcCol = i;
			posSrc[i] = posSrcCol;
			usePosCv[i] = posSrcCol >= 0 && posTrim[i] != 0.f;

			rngKnob[i] = params[RANGE_A_PARAM + i].getValue();
			rngTrim[i] = params[RANGE_A_TRIM_PARAM + i].getValue();
			if (rangeConnected[i])
				rangeSrcCol = i;
			rangeSrc[i] = rangeSrcCol;
			useRangeCv[i] = rangeSrcCol >= 0 && rngTrim[i] != 0.f;

			lvlKnob[i] = params[LEVEL_A_PARAM + i].getValue();
			
			// Cache channel counts and set A-D output channels
			if (sigConnected[i])
				channels[i] = std::max(inputs[SIG_A_INPUT + i].getChannels(), 1);
			else if (i == 0)
				channels[i] = 1; // unpatched A is a constant 5V/10V
			else
				channels[i] = channels[i - 1]; // B/C/D normalize to the previous column
			outputs[SIG_A_OUTPUT + i].setChannels(channels[i]);
			if (channels[i] > maxChannels)
				maxChannels = channels[i];

			// Knob-only Y is cached here; a live CV is recalculated in process()
			calcY[i] = useSweepCv || usePosCv[i] || useRangeCv[i];
			if (!calcY[i]) {
				yValue[i] = 0.f;
				if (rngKnob[i] > 0.f &&
					sweepKnob >= posKnob[i] - rngKnob[i] &&
					sweepKnob <= posKnob[i] + rngKnob[i])
					yValue[i] = 5.f * (1.f - fabsf(sweepKnob - posKnob[i]) / rngKnob[i]);
			}

			// Set AY, BY, CY, DY lights (brightness tracks Y 0–5V; red if Level input is patched)
			int yLight = Y_A_LIGHT + i * 2;
			float yBright = yValue[i] / 5.f;
			lights[yLight].setBrightness(levelConnected[i] ? 0.f : yBright);
			lights[yLight + 1].setBrightness(levelConnected[i] ? yBright : 0.f);
		}

		// Cache (have) cumulative outputs (A+B, C+D, Min, Max, Average, Sum), and set channels
		for (int i = AB_OUTPUT; i <= SUM_OUTPUT; i++)
			outputs[i].setChannels(maxChannels);
		haveABOutputs = outputs[AB_OUTPUT].isConnected() || outputs[CD_OUTPUT].isConnected();
		haveMinMaxOutputs = outputs[MIN_OUTPUT].isConnected() || outputs[MAX_OUTPUT].isConnected();
		haveSumAvgOutputs = outputs[SUM_OUTPUT].isConnected() || outputs[AVG_OUTPUT].isConnected();
		haveCummulativeOutputs = haveABOutputs || haveMinMaxOutputs || haveSumAvgOutputs;
		if (haveCummulativeOutputs)
			std::memset(outputAD, 0, sizeof(outputAD));

		// Handle A+B and C+D mix-modes
		abMixScale = params[AB_MIX_PARAM].getValue() > 0.5f ? 0.5f : 1.f;
		cdMixScale = params[CD_MIX_PARAM].getValue() > 0.5f ? 0.5f : 1.f;

		// Handle 5V/10V switch (and set green/red lights)
		bool norm10 = params[NORM_510_PARAM].getValue() > 0.5f;
		normInput = norm10 ? 10.f : 5.f;  // 5V or 10V
		float greenNorm = norm10 ? 0.f : 1.f;
		float redNorm = norm10 ? 1.f : 0.f;
		lights[NORM_5_LIGHT].setBrightness(greenNorm);
		lights[NORM_10_LIGHT].setBrightness(redNorm);
		lights[NORM_LIGHT].setBrightness(greenNorm);
		lights[NORM_LIGHT + 1].setBrightness(redNorm);

		haveOutputs = haveCummulativeOutputs ||
			outputs[SIG_A_OUTPUT].isConnected() || outputs[SIG_B_OUTPUT].isConnected() || 
			outputs[SIG_C_OUTPUT].isConnected() || outputs[SIG_D_OUTPUT].isConnected();

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
			// Get current sweep
			float sweep = useSweepCv
				? clamp(sweepKnob + sweepTrim * inputs[SWEEP_INPUT].getVoltage(), 0.f, 10.f)
				: sweepKnob;

			// Process each column
			for (int i = 0; i < 4; i++) {
				// Calculate Y (when not 100% knob based)
				if (calcY[i]) {
					yValue[i] = 0.f;
					float range = useRangeCv[i]
						? clamp(rngKnob[i] + rngTrim[i] * inputs[RANGE_A_INPUT + rangeSrc[i]].getVoltage(), 0.f, 10.f)
						: rngKnob[i];
					if (range > 0.f) {  // "Empty" range ignored (zero volts)
						float pos = usePosCv[i]
							? clamp(posKnob[i] + posTrim[i] * inputs[POS_A_INPUT + posSrc[i]].getVoltage(), 0.f, 10.f)
							: posKnob[i];
						if (sweep >= pos - range && sweep <= pos + range)
							yValue[i] = 5.f * (1.f - fabsf(sweep - pos) / range);
					}
				}

				if (haveOutputs) {
					// Set level (input or Y)
					float level = levelConnected[i]
						? inputs[LEVEL_A_INPUT + i].getVoltage()
						: yValue[i];
					level = applyInputMode(level, lvlMode[i]);
					level = clamp((level * lvlKnob[i]) / 5.f, -1.f, 1.f);

					// loop over this column's own channels
					int n = channels[i];
					for (int c = 0; c < n; c++) {
						if (sigConnected[i])
							prevInput[c] = inputs[SIG_A_INPUT + i].getVoltage(c);
						else if (i == 0)
							prevInput[c] = normInput;

						// Set output									
						outputAD[i][c] = applyInputMode(prevInput[c], sigMode[i]) * level;
						outputs[SIG_A_OUTPUT + i].setVoltage(clipToVoltRange(outputAD[i][c], outClipRange.act), c);
					}

					// Special case for monophonic inputs (one channel "repeated" across all channels)
					if (haveCummulativeOutputs && n == 1 && maxChannels > 1) {
						float v = outputAD[i][0];
						for (int c = 1; c < maxChannels; c++)
							outputAD[i][c] = v;
					}
				}
			}

			// Update cumulative outputs (if in use)
			if (haveCummulativeOutputs) {
				for (int c = 0; c < maxChannels; c++) {
					float abSum = (outputAD[0][c] + outputAD[1][c]);  // Used for A+B and sum/avg
					float cdSum = (outputAD[2][c] + outputAD[3][c]);  // Used for C+D and sum/avg
					if (haveABOutputs) 
					{
						outputs[AB_OUTPUT].setVoltage(clipToVoltRange(abSum * abMixScale, outClipRange.act), c);
						outputs[CD_OUTPUT].setVoltage(clipToVoltRange(cdSum * cdMixScale, outClipRange.act), c);
					}

					if (haveMinMaxOutputs)
					{
						float min = outputAD[0][c]; // initialize to first column's value
						float max = outputAD[0][c]; // initialize to first column's value
						for (int i = 1; i < 4; i++) { // compare to other columns
							if (outputAD[i][c] < min)
								min = outputAD[i][c];
							if (outputAD[i][c] > max)
								max = outputAD[i][c];
						}
						outputs[MIN_OUTPUT].setVoltage(clipToVoltRange(min, outClipRange.act), c);
						outputs[MAX_OUTPUT].setVoltage(clipToVoltRange(max, outClipRange.act), c);
					}

					if (haveSumAvgOutputs)
					{
						float sum = (abSum + cdSum);
						float avg = sum * 0.25f;  // avg = sum / 4
						outputs[SUM_OUTPUT].setVoltage(clipToVoltRange(sum, outClipRange.act), c);
						outputs[AVG_OUTPUT].setVoltage(clipToVoltRange(avg, outClipRange.act), c);
					}
				}
			}
		}

		cycle256++;
	}
};

struct Flux4ModuleWidget : InfNoiseModuleWidget {
	InfNoiseDisableOverlayGroup* linkBOverlayGroup = nullptr;
	InfNoiseDisableOverlayGroup* linkCOverlayGroup = nullptr;
	InfNoiseDisableOverlayGroup* linkDOverlayGroup = nullptr;
	bool linkBToA = false;
	bool linkCToB = false;
	bool linkDToC = false;

	Flux4ModuleWidget(Flux4Module* module) {
		initializeWidget(module, "res/Flux4");

		const float col[4] = {14.68f, 44.84f, 75.00f, 105.16f};
		const float sweepCol = 135.32f;
		const float modeX[4] = {24.53f, 54.70f, 84.87f, 115.03f};
		const float linkX[3] = {29.76f, 59.92f, 90.08f};

		for (int i = 0; i < 4; i++) {
			addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(col[i], 26.12f), module, Flux4Module::Y_A_LIGHT + i * 2));
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(col[i], 49.78f), module, Flux4Module::POS_A_PARAM + i));
			addParam(createParamCentered<Trimpot>(Vec(col[i], 74.16f), module, Flux4Module::POS_A_TRIM_PARAM + i));
			addInput(createInputCentered<infNoiseThemedMonoPort>(Vec(col[i], 98.88f), module, Flux4Module::POS_A_INPUT + i));

			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(col[i], 141.56f), module, Flux4Module::RANGE_A_PARAM + i));
			addParam(createParamCentered<Trimpot>(Vec(col[i], 165.94f), module, Flux4Module::RANGE_A_TRIM_PARAM + i));
			addInput(createInputCentered<infNoiseThemedMonoPort>(Vec(col[i], 190.66f), module, Flux4Module::RANGE_A_INPUT + i));

			addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_red, bc_green, bc_blue>>(
				Vec(modeX[i], 219.51f), module, Flux4Module::SIG_MODE_A_PARAM + i));
			addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(col[i], 232.00f), module, Flux4Module::SIG_A_INPUT + i));

			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(col[i], 260.57f), module, Flux4Module::LEVEL_A_PARAM + i));
			addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_red, bc_green, bc_blue>>(
				Vec(modeX[i], 277.03f), module, Flux4Module::LVL_MODE_A_PARAM + i));
			addInput(createInputCentered<infNoiseThemedMonoPort>(Vec(col[i], 289.51f), module, Flux4Module::LEVEL_A_INPUT + i));

			addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(col[i], 332.04f), module, Flux4Module::SIG_A_OUTPUT + i));
		}

		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(sweepCol, 49.78f), module, Flux4Module::SWEEP_PARAM));
		addParam(createParamCentered<Trimpot>(Vec(sweepCol, 74.16f), module, Flux4Module::SWEEP_TRIM_PARAM));
		addInput(createInputCentered<infNoiseThemedMonoPort>(Vec(sweepCol, 98.88f), module, Flux4Module::SWEEP_INPUT));
		addParam(createParamCentered<CKSS>(Vec(129.96f, 122.77f), module, Flux4Module::NORM_510_PARAM));
		addChild(createLightCentered<TinyLight<RedLight>>(Vec(145.91f, 118.68f), module, Flux4Module::NORM_10_LIGHT));
		addChild(createLightCentered<TinyLight<GreenLight>>(Vec(145.91f, 127.17f), module, Flux4Module::NORM_5_LIGHT));
		addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(4.37f, 215.51f), module, Flux4Module::NORM_LIGHT));

		for (int i = 0; i < 3; i++) {
			addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(Vec(linkX[i], 124.78f), module, Flux4Module::RANGE_LINK_AB_PARAM + i));
		}

		addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(Vec(126.52f, 140.88f), module, Flux4Module::AB_MIX_PARAM));
		addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(Vec(126.52f, 175.95f), module, Flux4Module::CD_MIX_PARAM));
		addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(sweepCol, 156.67f), module, Flux4Module::AB_OUTPUT));
		addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(sweepCol, 191.75f), module, Flux4Module::CD_OUTPUT));
		addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(sweepCol, 226.82f), module, Flux4Module::MIN_OUTPUT));
		addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(sweepCol, 261.89f), module, Flux4Module::MAX_OUTPUT));
		addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(sweepCol, 296.97f), module, Flux4Module::AVG_OUTPUT));
		addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(sweepCol, 332.04f), module, Flux4Module::SUM_OUTPUT));

		InfNoiseDisableOverlayManager& overlayManager = getDisableOverlayManager();
		linkBOverlayGroup = overlayManager.addGroup("B range linked to A");
		linkBOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
			Flux4Module::RANGE_B_PARAM,
			Flux4Module::RANGE_B_TRIM_PARAM
		});
		linkCOverlayGroup = overlayManager.addGroup("C range linked to B");
		linkCOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
			Flux4Module::RANGE_C_PARAM,
			Flux4Module::RANGE_C_TRIM_PARAM
		});
		linkDOverlayGroup = overlayManager.addGroup("D range linked to C");
		linkDOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
			Flux4Module::RANGE_D_PARAM,
			Flux4Module::RANGE_D_TRIM_PARAM
		});
	}

	void step() override {
		InfNoiseModuleWidget::step();
		if (!module)
			return;

		auto* m = static_cast<Flux4Module*>(module);
		bool linkAB = m->params[Flux4Module::RANGE_LINK_AB_PARAM].getValue() > 0.5f;
		bool linkBC = m->params[Flux4Module::RANGE_LINK_BC_PARAM].getValue() > 0.5f;
		bool linkCD = m->params[Flux4Module::RANGE_LINK_CD_PARAM].getValue() > 0.5f;
		if (linkBOverlayGroup && linkAB != linkBToA) {
			linkBToA = linkAB;
			linkBOverlayGroup->setActive(linkBToA);
		}
		if (linkCOverlayGroup && linkBC != linkCToB) {
			linkCToB = linkBC;
			linkCOverlayGroup->setActive(linkCToB);
		}
		if (linkDOverlayGroup && linkCD != linkDToC) {
			linkDToC = linkCD;
			linkDOverlayGroup->setActive(linkDToC);
		}
	}

	void setPosRange(Flux4Module* m, const float pos[4], float range) {
		for (int i = 0; i < 4; i++) {
			m->params[Flux4Module::POS_A_PARAM + i].setValue(pos[i]);
			m->params[Flux4Module::RANGE_A_PARAM + i].setValue(range);
		}
	}

	void appendContextMenu(Menu* menu) override {
		InfNoiseModuleWidget::appendContextMenu(menu);
		Flux4Module* module = dynamic_cast<Flux4Module*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator);
		menu->addChild(createSubmenuItem("Setup Pos/Range for", "",
			[=](Menu* submenu) {
				submenu->addChild(createMenuItem("4-section attenuator (Sweep=level)", "", [=]() {
					const float pos[4] = {10.f, 10.f, 10.f, 10.f};
					setPosRange(module, pos, 10.f);
				}));
				submenu->addChild(createMenuItem("Mixer (Pos=level, Set Sweep=10)", "", [=]() {
					const float pos[4] = {0.f, 0.f, 0.f, 0.f};
					setPosRange(module, pos, 10.f);
				}));
				submenu->addChild(createMenuItem("Stereo cross-fade (A/B=Left, C/D=Right)", "", [=]() {
					const float pos[4] = {0.f, 10.f, 0.f, 10.f};
					setPosRange(module, pos, 10.f);
				}));
				submenu->addChild(createMenuItem("4-way cross-fade (A-B-C-D)", "", [=]() {
					const float third = 10.f / 3.f;
					const float pos[4] = {0.f, third, third * 2.f, 10.f};
					setPosRange(module, pos, third);
				}));
				submenu->addChild(createMenuItem("3-way cross-fade (A-B-C)", "", [=]() {
					const float pos[4] = {0.f, 5.f, 10.f, 0.f};
					setPosRange(module, pos, 5.f);
					module->params[Flux4Module::RANGE_D_PARAM].setValue(0.f);
					module->params[Flux4Module::RANGE_LINK_CD_PARAM].setValue(0.f); // keep D at 0
				}));
				submenu->addChild(createMenuItem("All muted (Pos/Range=0)", "", [=]() {
					const float pos[4] = {0.f, 0.f, 0.f, 0.f};
					setPosRange(module, pos, 0.f);
				}));
				submenu->addChild(createMenuItem("Initialized (Pos=2/4/6/8, Range=2)", "", [=]() {
					const float pos[4] = {2.f, 4.f, 6.f, 8.f};
					setPosRange(module, pos, 2.f);
				}));
			}
		));

		appendInfNoiseMenuItems(menu);
	}
};

Model* modelFlux4 = createModel<Flux4Module, Flux4ModuleWidget>("Flux4");
