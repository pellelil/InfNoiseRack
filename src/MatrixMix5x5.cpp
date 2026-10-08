// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inUtil.hpp"

struct MatrixMix5x5Module : InfNoiseModule {
	enum ParamId {
		ENUMS(GAIN_PARAM, 5),
		ENUMS(INV_PARAM, 5),
		NORM5_PARAM,
		ENUMS(MIX_PARAM, 25),
		ENUMS(LINK_PARAM, 20),
		ENUMS(RANGE_PARAM, 5),
		ENUMS(LEVEL_PARAM, 5),
		ENUMS(TRIM_PARAM, 5),
		ENUMS(MIX_MODE_PARAM, 5),
		PARAMS_LEN
	};
	enum InputsId {
		ENUMS(IN_INPUT, 5),
		ENUMS(LEVEL_CV_INPUT, 5),
		INPUTS_LEN
	};
	enum OutputsId {
		ENUMS(MIX_OUTPUT, 5),
		MIN_OUTPUT,
		MAX_OUTPUT,
		AVG_OUTPUT,
		RNG_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(PROCQUAL_LIGHT, 2),
		ENUMS(CLIP_RANGE_LIGHT, 2),
		ENUMS(ROW_LEVEL_LIGHT, 5 * 3),
		ENUMS(COL_LEVEL_LIGHT, 5 * 3),
		LIGHTS_LEN
	};

	struct SendQuantity : ParamQuantity {
		int column = 0;
		bool isBipolar() const {
			if (!module)
				return false;
			return module->params[RANGE_PARAM + column].getValue() > 0.5f;
		}
		float getDisplayValue() override {
			float pos = getValue();
			if (isBipolar())
				return (pos * 2.f - 1.f) * 100.f;
			return pos * 100.f;
		}
		void setDisplayValue(float v) override {
			v /= 100.f;
			if (isBipolar())
				setValue((clamp(v, -1.f, 1.f) + 1.f) * 0.5f);
			else
				setValue(clamp(v, 0.f, 1.f));
		}
	};

	static int mixId(int row, int col) {
		return MIX_PARAM + row * 5 + col;
	}
	static int linkId(int col, int between) {
		return LINK_PARAM + col * 4 + between;
	}

	bool inConn[5] = {};
	bool cvConn[5] = {};
	bool mixOutConn[5] = {};
	bool minOutConn = false;
	bool maxOutConn = false;
	bool avgOutConn = false;
	bool rngOutConn = false;
	bool haveOutputs = false;
	bool invert[5] = {};
	bool bipolar[5] = {};
	int norm5 = 0;
	float gain[5] = { 1.f, 1.f, 1.f, 1.f, 1.f };
	float sendPos[25] = {};
	float levelKnob[5] = { 1.f, 1.f, 1.f, 1.f, 1.f };
	float trimKnob[5] = {};
	float maxAbsRow[5] = {};
	float maxAbsCol[5] = {};
	bool linkOn[5][4] = {}; // processParams; widget overlays on follower knobs
	bool activeInputs[5] = {};
	float mixScale[5] = { 1.f, 1.f, 1.f, 1.f, 1.f };
	enum mixModeLockType { mml_Unlocked, mml_Locked, mml_len };
	enum linkLockModeType { llm_Unlocked, llm_Locked, llm_len };
	actReqValue<mixModeLockType> mixModeLock = actReqValue<mixModeLockType>(mml_Unlocked);
	actReqValue<linkLockModeType> linkLockMode = actReqValue<linkLockModeType>(llm_Unlocked);
	bool mixModeButtonsLocked = false; // processParams; widget overlay on MIX_MODE_PARAM
	bool linkButtonsLocked = false; // processParams; widget overlay on LINK_PARAM

	MatrixMix5x5Module() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
		configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

		const char* rowNames[] = { "1", "2", "3", "4", "5" };
		const char* colNames[] = { "A", "B", "C", "D", "E" };
		for (int r = 0; r < 5; r++) {
			configParam(GAIN_PARAM + r, 0.f, 2.f, 1.f, string::f("Row %s gain (0%% to 200%%)", rowNames[r]), " %", 0, 100);
			configSwitch(INV_PARAM + r, 0.f, 1.f, 0.f, string::f("Row %s invert", rowNames[r]), { "Off", "Inverted" });
			configInput(IN_INPUT + r, string::f("Row %s", rowNames[r]));
			configLight(ROW_LEVEL_LIGHT + r * 3, string::f("Row %s level (<=5V green, <=10V amber, >10V red)", rowNames[r]));
		}
		configSwitch(NORM5_PARAM, 0.f, 2.f, 0.f, "Row 5 normalize (unpatched)", { "0V", "+5V", "+10V" });

		for (int r = 0; r < 5; r++) {
			for (int c = 0; c < 5; c++) {
				SendQuantity* q = configParam<SendQuantity>(mixId(r, c), 0.f, 1.f, 0.f,
					string::f("Row %s to column %s", rowNames[r], colNames[c]), " %");
				q->column = c;
			}
		}
		for (int c = 0; c < 5; c++) {
			for (int k = 0; k < 4; k++) {
				configSwitch(linkId(c, k), 0.f, 2.f, 0.f,
					string::f("Column %s link %d-%d", colNames[c], k + 1, k + 2),
					{ "Off", "Follow", "Inverse" });
			}
			configSwitch(MIX_MODE_PARAM + c, 0.f, 1.f, 0.f, string::f("Column %s mix mode", colNames[c]),
				{ "Unity", "Averaging" });
			configSwitch(RANGE_PARAM + c, 0.f, 1.f, 0.f, string::f("Column %s range", colNames[c]),
				{ "0x to 1x", "-1x to 1x" });
			configParam(LEVEL_PARAM + c, 0.f, 1.f, 1.f, string::f("Column %s level (0%% to 100%%)", colNames[c]), " %", 0, 100);
			configParam(TRIM_PARAM + c, -1.f, 1.f, 0.f, string::f("Column %s CV trim", colNames[c]), " %", 0, 100);
			if (c == 0)
				configInput(LEVEL_CV_INPUT + c, "Column A level CV (normalized to +10V)");
			else
				configInput(LEVEL_CV_INPUT + c, string::f("Column %s level CV (normalized to %s)", colNames[c], colNames[c - 1]));
			configOutput(MIX_OUTPUT + c, string::f("Column %s mix", colNames[c]));
			configLight(COL_LEVEL_LIGHT + c * 3, string::f("Column %s level (<=5V green, <=10V amber, >10V red)", colNames[c]));
		}

		configOutput(MIN_OUTPUT, "Min (connected rows after gain)");
		configOutput(MAX_OUTPUT, "Max (connected rows after gain)");
		configOutput(AVG_OUTPUT, "Average (connected rows after gain)");
		configOutput(RNG_OUTPUT, "Range Max-Min (connected rows after gain)");

		configBypass(IN_INPUT, MIX_OUTPUT);

		haveProcQuality = true;
		haveAutoProcQuality = false;
		haveOutQuantize = false;
		haveOutClipRange = true;
		haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = false;
		haveTrigHighLow = false;
	}

	bool columnBipolar(int col) {
		return params[RANGE_PARAM + col].getValue() > 0.5f;
	}

	float sendPosForAmount(int col, float p) {
		if (columnBipolar(col))
			return (p + 1.f) * 0.5f;
		return p;
	}

	void setMatrixSends(int col, float p) {
		int c0 = (col < 0) ? 0 : col;
		int c1 = (col < 0) ? 5 : col + 1;
		for (int c = c0; c < c1; c++) {
			float pos = sendPosForAmount(c, p);
			for (int r = 0; r < 5; r++)
				params[mixId(r, c)].setValue(pos);
		}
	}

	void setDiagonalSends(float p) {
		for (int r = 0; r < 5; r++)
			params[mixId(r, r)].setValue(sendPosForAmount(r, p));
	}

	void onReset(const ResetEvent& e) override {
		InfNoiseModule::onReset(e);
		mixModeLock.setBoth(mml_Unlocked);
		linkLockMode.setBoth(llm_Unlocked);
		for (int i = 0; i < 5; i++) {
			maxAbsRow[i] = 0.f;
			maxAbsCol[i] = 0.f;
		}
	}

	void dataFromJson(json_t* rootJ) override {
		InfNoiseModule::dataFromJson(rootJ);
		mixModeLock.setBoth((mixModeLockType)getJsonInt(rootJ, "mixModeLock", (int)mml_Unlocked, (int)mml_len - 1));
		linkLockMode.setBoth((linkLockModeType)getJsonInt(rootJ, "linkLockMode", (int)llm_Unlocked, (int)llm_len - 1));
		for (int i = 0; i < 5; i++) {
			maxAbsRow[i] = 0.f;
			maxAbsCol[i] = 0.f;
		}
	}

	void dataToJson(json_t* rootJ) override {
		InfNoiseModule::dataToJson(rootJ);
		json_object_set_new(rootJ, "mixModeLock", json_integer((int)mixModeLock.req));
		json_object_set_new(rootJ, "linkLockMode", json_integer((int)linkLockMode.req));
	}

	void applyLinks() {
		for (int c = 0; c < 5; c++) {
			for (int k = 0; k < 4; k++) {
				int mode = (int) std::round(params[linkId(c, k)].getValue());
				linkOn[c][k] = (mode >= 1);
				if (mode < 1)
					continue;
				float above = params[mixId(k, c)].getValue();
				float below = (mode == 1) ? above : (1.f - above);
				int belowId = mixId(k + 1, c);
				if (params[belowId].getValue() != below)
					params[belowId].setValue(below);
			}
		}
	}

	void processParams(const ProcessArgs& args) {
		preProcessParams(args);
		//--------------------

		mixModeLock.updateActual();
		mixModeButtonsLocked = (mixModeLock.act == mml_Locked);
		linkLockMode.updateActual();
		linkButtonsLocked = (linkLockMode.act == llm_Locked);

		applyLinks();
		for (int i = 0; i < 25; i++)
			sendPos[i] = params[MIX_PARAM + i].getValue();

		norm5 = (int) std::round(params[NORM5_PARAM].getValue() * 5.f);
		haveOutputs = false;
		for (int i = 0; i < 5; i++) {
			invert[i] = params[INV_PARAM + i].getValue() > 0.5f;
			gain[i] = params[GAIN_PARAM + i].getValue();
			bipolar[i] = columnBipolar(i);
			levelKnob[i] = params[LEVEL_PARAM + i].getValue();
			trimKnob[i] = params[TRIM_PARAM + i].getValue();
			inConn[i] = inputs[IN_INPUT + i].isConnected();
			cvConn[i] = inputs[LEVEL_CV_INPUT + i].isConnected();
			mixOutConn[i] = outputs[MIX_OUTPUT + i].isConnected();
			haveOutputs = haveOutputs || mixOutConn[i];
			outputs[MIX_OUTPUT + i].setChannels(1);
			setAbsLevelMeterLight(this, ROW_LEVEL_LIGHT + i * 3, maxAbsRow[i]);
			setAbsLevelMeterLight(this, COL_LEVEL_LIGHT + i * 3, maxAbsCol[i]);
			maxAbsRow[i] = 0.f;
			maxAbsCol[i] = 0.f;
			activeInputs[i] = inConn[i] && gain[i] != 0.f;
		}
		for (int c = 0; c < 5; c++) {
			mixScale[c] = 1.f;
			if (params[MIX_MODE_PARAM + c].getValue() >= 0.5f) {
				int n = 0;
				for (int r = 0; r < 5; r++) {
					float pos = sendPos[r * 5 + c];
					float send = bipolar[c] ? (pos * 2.f - 1.f) : pos;
					if (activeInputs[r] && send != 0.f)
						n++;
				}
				if (n > 0)
					mixScale[c] = 1.f / (float) n;
			}
		}
		minOutConn = outputs[MIN_OUTPUT].isConnected();
		maxOutConn = outputs[MAX_OUTPUT].isConnected();
		avgOutConn = outputs[AVG_OUTPUT].isConnected();
		rngOutConn = outputs[RNG_OUTPUT].isConnected();
		haveOutputs = haveOutputs || minOutConn || maxOutConn || avgOutConn || rngOutConn;
		outputs[MIN_OUTPUT].setChannels(1);
		outputs[MAX_OUTPUT].setChannels(1);
		outputs[AVG_OUTPUT].setChannels(1);
		outputs[RNG_OUTPUT].setChannels(1);

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
			float rowV[5];
			int cabled = 0;
			float sumCabled = 0.f;
			float minC = 0.f;
			float maxC = 0.f;
			for (int r = 0; r < 5; r++) {
				float v = 0.f;
				if (inConn[r])
					v = inputs[IN_INPUT + r].getVoltage();
				else if (r == 4) {
					v = norm5;
				}
				if (invert[r])
					v = -v;
				v *= gain[r];
				rowV[r] = v;
				maxAbsRow[r] = std::max(maxAbsRow[r], std::fabs(v));
				if (inConn[r]) {
					if (cabled == 0) {
						minC = v;
						maxC = v;
					}
					else {
						minC = std::min(minC, v);
						maxC = std::max(maxC, v);
					}
					sumCabled += v;
					cabled++;
				}
			}

			if (cabled > 0)	{
				outputs[MIN_OUTPUT].setVoltage(clipToVoltRange(minC, outClipRange.act));
				outputs[MAX_OUTPUT].setVoltage(clipToVoltRange(maxC, outClipRange.act));
				outputs[AVG_OUTPUT].setVoltage(clipToVoltRange(sumCabled / (float) cabled, outClipRange.act));
				outputs[RNG_OUTPUT].setVoltage(clipToVoltRange(maxC - minC, outClipRange.act));
			}
			else {
				outputs[MIN_OUTPUT].setVoltage(0.f);
				outputs[MAX_OUTPUT].setVoltage(0.f);
				outputs[AVG_OUTPUT].setVoltage(0.f);
				outputs[RNG_OUTPUT].setVoltage(0.f);
			}

			float cv = 10.f; // CV in A-column normalized to +10V
			for (int c = 0; c < 5; c++) {
				if (cvConn[c])
					cv = inputs[LEVEL_CV_INPUT + c].getVoltage();
				if (!mixOutConn[c])
					continue;
				float colGain = clamp(levelKnob[c] + trimKnob[c] * cv / 10.f, 0.f, 2.f);
				float mix = 0.f;
				for (int r = 0; r < 5; r++) {
					float pos = sendPos[r * 5 + c];
					float send = bipolar[c] ? (pos * 2.f - 1.f) : pos;
					mix += rowV[r] * send;
				}
				mix *= mixScale[c] * colGain;
				mix = clipToVoltRange(mix, outClipRange.act);
				outputs[MIX_OUTPUT + c].setVoltage(mix);
				maxAbsCol[c] = std::max(maxAbsCol[c], std::fabs(mix));
			}
		}

		cycle256++;
	}
};

struct MatrixMix5x5ModuleWidget : InfNoiseModuleWidget {
	InfNoiseDisableOverlayGroup* linkOverlayGroup[5][4] = {};
	bool linkOn[5][4] = {};
	InfNoiseDisableOverlayGroup* mixModeLockOverlayGroup = nullptr;
	InfNoiseDisableOverlayGroup* linkLockOverlayGroup = nullptr;
	bool mixModeButtonsLocked = false;
	bool linkButtonsLocked = false;

	MatrixMix5x5ModuleWidget(MatrixMix5x5Module* module) {
		initializeWidget(module, "res/MatrixMix5x5");

		const float rowY[5] = { 51.606f, 86.679f, 121.753f, 156.826f, 191.900f };
		const float colX[5] = { 74.291f, 104.449f, 134.607f, 164.764f, 194.922f };
		const float inX = 13.873f;
		const float invX = 25.871f;
		const float rowLgtX = 34.564f;
		const float gainX = 44.133f;
		const float linkXOff = 7.854f;
		const float rangeXOff = 6.106f;
		const float colLgtXOff = 9.098f;

		for (int r = 0; r < 5; r++) {
			addInput(createInputCentered<ThemedPJ301MPort>(Vec(inX, rowY[r]), module, MatrixMix5x5Module::IN_INPUT + r));
			addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(
				Vec(invX, rowY[r] - 11.050f), module, MatrixMix5x5Module::INV_PARAM + r));
			addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(
				Vec(rowLgtX, rowY[r] - 11.050f), module, MatrixMix5x5Module::ROW_LEVEL_LIGHT + r * 3));
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(gainX, rowY[r]), module, MatrixMix5x5Module::GAIN_PARAM + r));
			for (int c = 0; c < 5; c++) {
				addParam(createParamCentered<RoundSmallBlackKnob>(
					Vec(colX[c], rowY[r]), module, MatrixMix5x5Module::mixId(r, c)));
			}
		}
		addParam(createParamCentered<CKSSThree>(Vec(13.766f, 223.531f), module, MatrixMix5x5Module::NORM5_PARAM));

		for (int c = 0; c < 5; c++) {
			addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_blue>>(
				Vec(colX[c] - linkXOff, 36.288f), module, MatrixMix5x5Module::MIX_MODE_PARAM + c));
			for (int k = 0; k < 4; k++) {
				float y = (rowY[k] + rowY[k + 1]) * 0.5f;
				addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_green, bc_red>>(
					Vec(colX[c] - linkXOff, y), module, MatrixMix5x5Module::linkId(c, k)));
			}
			addParam(createParamCentered<CKSS>(Vec(colX[c] - rangeXOff, 216.652f), module, MatrixMix5x5Module::RANGE_PARAM + c));
			addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(
				Vec(colX[c] - colLgtXOff, 232.992f), module, MatrixMix5x5Module::COL_LEVEL_LIGHT + c * 3));
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(colX[c], 248.158f), module, MatrixMix5x5Module::LEVEL_PARAM + c));
			addParam(createParamCentered<Trimpot>(Vec(colX[c], 272.534f), module, MatrixMix5x5Module::TRIM_PARAM + c));
			addInput(createInputCentered<ThemedPJ301MPort>(Vec(colX[c], 297.259f), module, MatrixMix5x5Module::LEVEL_CV_INPUT + c));
			addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(colX[c], 332.194f), module, MatrixMix5x5Module::MIX_OUTPUT + c));
		}

		addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(15.078f, 297.259f), module, MatrixMix5x5Module::MIN_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(44.923f, 297.259f), module, MatrixMix5x5Module::MAX_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(15.078f, 332.194f), module, MatrixMix5x5Module::AVG_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(44.923f, 332.194f), module, MatrixMix5x5Module::RNG_OUTPUT));

		InfNoiseDisableOverlayManager& overlayManager = getDisableOverlayManager();
		for (int c = 0; c < 5; c++) {
			for (int k = 0; k < 4; k++) {
				linkOverlayGroup[c][k] = overlayManager.addGroup("Linked to the knob above");
				linkOverlayGroup[c][k]->addTarget(InfNoiseOverlayTargetType::param, MatrixMix5x5Module::mixId(k + 1, c));
			}
		}
		mixModeLockOverlayGroup = overlayManager.addGroup("Mix-mode buttons locked");
		linkLockOverlayGroup = overlayManager.addGroup("Link buttons locked");
		for (int c = 0; c < 5; c++) {
			mixModeLockOverlayGroup->addTarget(InfNoiseOverlayTargetType::param, MatrixMix5x5Module::MIX_MODE_PARAM + c);
			for (int k = 0; k < 4; k++)
				linkLockOverlayGroup->addTarget(InfNoiseOverlayTargetType::param, MatrixMix5x5Module::linkId(c, k));
		}
	}

	void step() override {
		InfNoiseModuleWidget::step();
		if (!module)
			return;
		auto* m = static_cast<MatrixMix5x5Module*>(module);
		for (int c = 0; c < 5; c++) {
			for (int k = 0; k < 4; k++) {
				if (!linkOverlayGroup[c][k])
					continue;
				if (m->linkOn[c][k] != linkOn[c][k]) {
					linkOn[c][k] = m->linkOn[c][k];
					linkOverlayGroup[c][k]->setActive(linkOn[c][k]);
				}
			}
		}
		if (mixModeLockOverlayGroup && m->mixModeButtonsLocked != mixModeButtonsLocked) {
			mixModeButtonsLocked = m->mixModeButtonsLocked;
			mixModeLockOverlayGroup->setActive(mixModeButtonsLocked);
		}
		if (linkLockOverlayGroup && m->linkButtonsLocked != linkButtonsLocked) {
			linkButtonsLocked = m->linkButtonsLocked;
			linkLockOverlayGroup->setActive(linkButtonsLocked);
		}
	}

	void addSendPresetMenu(Menu* parent, MatrixMix5x5Module* module, const char* name, int col) {
		parent->addChild(createSubmenuItem(name, "", [=](Menu* menu) {
			const float presets[8] = { 0.f, 0.2f, 0.25f, 1.f / 3.f, 0.5f, 2.f / 3.f, 0.75f, 1.f };
			const char* labels[8] = { "0%", "20%", "25%", "33.3%", "50%", "66.7%", "75%", "100%" };
			for (int i = 0; i < 8; i++) {
				float p = presets[i];
				menu->addChild(createMenuItem(labels[i], "", [=]() {
					if (col == -2)
						module->setDiagonalSends(p);
					else
						module->setMatrixSends(col, p);
				}));
			}
		}));
	}

	void appendContextMenu(Menu* menu) override {
		InfNoiseModuleWidget::appendContextMenu(menu);
		MatrixMix5x5Module* module = dynamic_cast<MatrixMix5x5Module*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator);
		addSendPresetMenu(menu, module, "All matrix knobs", -1);
		addSendPresetMenu(menu, module, "Diagonal knobs", -2);
		addSendPresetMenu(menu, module, "Column A", 0);
		addSendPresetMenu(menu, module, "Column B", 1);
		addSendPresetMenu(menu, module, "Column C", 2);
		addSendPresetMenu(menu, module, "Column D", 3);
		addSendPresetMenu(menu, module, "Column E", 4);

		menu->addChild(createIndexPtrSubmenuItem("Mix-mode buttons",
			{ "Unlocked", "Locked" },
			&module->mixModeLock.req));
		menu->addChild(createIndexPtrSubmenuItem("Link buttons",
			{ "Unlocked", "Locked" },
			&module->linkLockMode.req));

		appendInfNoiseMenuItems(menu);
	}
};

Model* modelMatrixMix5x5 =
	createModel<MatrixMix5x5Module, MatrixMix5x5ModuleWidget>("MatrixMix5x5");
