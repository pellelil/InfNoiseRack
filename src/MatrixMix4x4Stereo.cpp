// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inUtil.hpp"

struct MatrixMix4x4StereoModule : InfNoiseModule {
	enum ParamId {
		ENUMS(NORM_R_PARAM, 4),
		ENUMS(BP_MODE_PARAM, 4),
		ENUMS(BP_PARAM, 4),
		ENUMS(GAIN_PARAM, 4),
		ENUMS(MIX_PARAM, 16),
		ENUMS(LINK_PARAM, 12),
		ENUMS(MIX_MODE_PARAM, 4),
		PARAMS_LEN
	};
	enum InputsId {
		ENUMS(L_INPUT, 4),
		ENUMS(R_INPUT, 4),
		INPUTS_LEN
	};
	enum OutputsId {
		ENUMS(MIX_L_OUTPUT, 4),
		ENUMS(MIX_R_OUTPUT, 4),
		AVG_L_OUTPUT,
		AVG_R_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(PROCQUAL_LIGHT, 2),
		ENUMS(CLIP_RANGE_LIGHT, 2),
		ENUMS(ROW_L_LEVEL_LIGHT, 4 * 3),
		ENUMS(ROW_R_LEVEL_LIGHT, 4 * 3),
		ENUMS(COL_L_LEVEL_LIGHT, 4 * 3),
		ENUMS(COL_R_LEVEL_LIGHT, 4 * 3),
		LIGHTS_LEN
	};

	static int mixId(int row, int col) {
		return MIX_PARAM + row * 4 + col;
	}
	static int linkId(int col, int between) {
		return LINK_PARAM + col * 3 + between;
	}

	static void stereoBpGains(bool panMode, float bp, float& lGain, float& rGain) {
		float t = (bp + 1.f) * 0.5f;
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
	}

	bool lInConn[4] = {};
	bool rInConn[4] = {};
	bool mixLOutConn[4] = {};
	bool mixROutConn[4] = {};
	bool avgLOutConn = false;
	bool avgROutConn = false;
	bool haveOutputs = false;
	bool normR[4] = { true, true, true, true };
	float bpL[4] = { 1.f, 1.f, 1.f, 1.f };
	float bpR[4] = { 1.f, 1.f, 1.f, 1.f };
	float gain[4] = { 1.f, 1.f, 1.f, 1.f };
	float sendPos[16] = {};
	float maxAbsRowL[4] = {};
	float maxAbsRowR[4] = {};
	float maxAbsColL[4] = {};
	float maxAbsColR[4] = {};
	bool linkOn[4][3] = {}; // processParams; widget overlays on follower knobs
	bool activeInputs[4] = {};
	float mixScale[4] = { 1.f, 1.f, 1.f, 1.f };
	enum mixModeLockType { mml_Unlocked, mml_Locked, mml_len };
	enum linkLockModeType { llm_Unlocked, llm_Locked, llm_len };
	actReqValue<mixModeLockType> mixModeLock = actReqValue<mixModeLockType>(mml_Unlocked);
	actReqValue<linkLockModeType> linkLockMode = actReqValue<linkLockModeType>(llm_Unlocked);
	bool mixModeButtonsLocked = false; // processParams; widget overlay on MIX_MODE_PARAM
	bool linkButtonsLocked = false; // processParams; widget overlay on LINK_PARAM

	MatrixMix4x4StereoModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
		configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

		const char* rowNames[] = { "1", "2", "3", "4" };
		const char* colNames[] = { "A", "B", "C", "D" };
		for (int r = 0; r < 4; r++) {
			configSwitch(NORM_R_PARAM + r, 0.f, 1.f, 1.f,
				string::f("Row %s right normalized to left", rowNames[r]), { "Disabled", "Enabled" });
			configSwitch(BP_MODE_PARAM + r, 0.f, 1.f, 0.f,
				string::f("Row %s balance/pan mode", rowNames[r]), { "Balance", "Pan" });
			configParam(BP_PARAM + r, -1.f, 1.f, 0.f, string::f("Row %s balance/pan L-R", rowNames[r]), " %", 0, 100);
			configParam(GAIN_PARAM + r, 0.f, 2.f, 1.f, string::f("Row %s gain (0%% to 200%%)", rowNames[r]), " %", 0, 100);
			configInput(L_INPUT + r, string::f("Row %s left", rowNames[r]));
			configInput(R_INPUT + r, string::f("Row %s right (normalized to Left)", rowNames[r]));
			configLight(ROW_L_LEVEL_LIGHT + r * 3, string::f("Row %s left level (<=5V green, <=10V amber, >10V red)", rowNames[r]));
			configLight(ROW_R_LEVEL_LIGHT + r * 3, string::f("Row %s right level (<=5V green, <=10V amber, >10V red)", rowNames[r]));
			for (int c = 0; c < 4; c++) {
				configParam(mixId(r, c), 0.f, 1.f, 0.f,
					string::f("Row %s to column %s", rowNames[r], colNames[c]), " %", 0, 100);
			}
		}
		for (int c = 0; c < 4; c++) {
			for (int k = 0; k < 3; k++) {
				configSwitch(linkId(c, k), 0.f, 2.f, 0.f,
					string::f("Column %s link %d-%d", colNames[c], k + 1, k + 2),
					{ "Off", "Follow", "Inverse" });
			}
			configSwitch(MIX_MODE_PARAM + c, 0.f, 1.f, 0.f, string::f("Column %s mix mode", colNames[c]),
				{ "Unity", "Averaging" });
			configOutput(MIX_L_OUTPUT + c, string::f("Column %s left mix", colNames[c]));
			configOutput(MIX_R_OUTPUT + c, string::f("Column %s right mix", colNames[c]));
			configLight(COL_L_LEVEL_LIGHT + c * 3, string::f("Column %s left level (<=5V green, <=10V amber, >10V red)", colNames[c]));
			configLight(COL_R_LEVEL_LIGHT + c * 3, string::f("Column %s right level (<=5V green, <=10V amber, >10V red)", colNames[c]));
		}

		configOutput(AVG_L_OUTPUT, "Average left (connected rows after gain)");
		configOutput(AVG_R_OUTPUT, "Average right (connected rows after gain)");

		configBypass(L_INPUT, MIX_L_OUTPUT);
		configBypass(R_INPUT, MIX_R_OUTPUT);

		haveProcQuality = true;
		haveAutoProcQuality = false;
		haveOutQuantize = false;
		haveOutClipRange = true;
		haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = false;
		haveTrigHighLow = false;
	}

	void setMatrixSends(int col, float p) {
		int c0 = (col < 0) ? 0 : col;
		int c1 = (col < 0) ? 4 : col + 1;
		for (int c = c0; c < c1; c++) {
			for (int r = 0; r < 4; r++)
				params[mixId(r, c)].setValue(p);
		}
	}

	void setDiagonalSends(float p) {
		for (int r = 0; r < 4; r++)
			params[mixId(r, r)].setValue(p);
	}

	void clearPeakMeters() {
		for (int i = 0; i < 4; i++) {
			maxAbsRowL[i] = 0.f;
			maxAbsRowR[i] = 0.f;
			maxAbsColL[i] = 0.f;
			maxAbsColR[i] = 0.f;
		}
	}

	void onReset(const ResetEvent& e) override {
		InfNoiseModule::onReset(e);
		mixModeLock.setBoth(mml_Unlocked);
		linkLockMode.setBoth(llm_Unlocked);
		clearPeakMeters();
	}

	void dataFromJson(json_t* rootJ) override {
		InfNoiseModule::dataFromJson(rootJ);
		mixModeLock.setBoth((mixModeLockType)getJsonInt(rootJ, "mixModeLock", (int)mml_Unlocked, (int)mml_len - 1));
		linkLockMode.setBoth((linkLockModeType)getJsonInt(rootJ, "linkLockMode", (int)llm_Unlocked, (int)llm_len - 1));
		clearPeakMeters();
	}

	void dataToJson(json_t* rootJ) override {
		InfNoiseModule::dataToJson(rootJ);
		json_object_set_new(rootJ, "mixModeLock", json_integer((int)mixModeLock.req));
		json_object_set_new(rootJ, "linkLockMode", json_integer((int)linkLockMode.req));
	}

	void applyLinks() {
		for (int c = 0; c < 4; c++) {
			for (int k = 0; k < 3; k++) {
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
		for (int i = 0; i < 16; i++)
			sendPos[i] = params[MIX_PARAM + i].getValue();

		haveOutputs = false;
		for (int i = 0; i < 4; i++) {
			normR[i] = params[NORM_R_PARAM + i].getValue() > 0.5f;
			bool panMode = params[BP_MODE_PARAM + i].getValue() > 0.5f;
			stereoBpGains(panMode, params[BP_PARAM + i].getValue(), bpL[i], bpR[i]);
			gain[i] = params[GAIN_PARAM + i].getValue();
			lInConn[i] = inputs[L_INPUT + i].isConnected();
			rInConn[i] = inputs[R_INPUT + i].isConnected();
			mixLOutConn[i] = outputs[MIX_L_OUTPUT + i].isConnected();
			mixROutConn[i] = outputs[MIX_R_OUTPUT + i].isConnected();
			haveOutputs = haveOutputs || mixLOutConn[i] || mixROutConn[i];
			outputs[MIX_L_OUTPUT + i].setChannels(1);
			outputs[MIX_R_OUTPUT + i].setChannels(1);
			setAbsLevelMeterLight(this, ROW_L_LEVEL_LIGHT + i * 3, maxAbsRowL[i]);
			setAbsLevelMeterLight(this, ROW_R_LEVEL_LIGHT + i * 3, maxAbsRowR[i]);
			setAbsLevelMeterLight(this, COL_L_LEVEL_LIGHT + i * 3, maxAbsColL[i]);
			setAbsLevelMeterLight(this, COL_R_LEVEL_LIGHT + i * 3, maxAbsColR[i]);
			maxAbsRowL[i] = 0.f;
			maxAbsRowR[i] = 0.f;
			maxAbsColL[i] = 0.f;
			maxAbsColR[i] = 0.f;
			activeInputs[i] = (lInConn[i] || rInConn[i]) && gain[i] != 0.f;
		}
		for (int c = 0; c < 4; c++) {
			mixScale[c] = 1.f;
			if (params[MIX_MODE_PARAM + c].getValue() >= 0.5f) {
				int n = 0;
				for (int r = 0; r < 4; r++) {
					if (activeInputs[r] && sendPos[r * 4 + c] != 0.f)
						n++;
				}
				if (n > 0)
					mixScale[c] = 1.f / (float) n;
			}
		}
		avgLOutConn = outputs[AVG_L_OUTPUT].isConnected();
		avgROutConn = outputs[AVG_R_OUTPUT].isConnected();
		haveOutputs = haveOutputs || avgLOutConn || avgROutConn;
		outputs[AVG_L_OUTPUT].setChannels(1);
		outputs[AVG_R_OUTPUT].setChannels(1);

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
			float rowL[4];
			float rowR[4];
			int cabled = 0;
			float sumL = 0.f;
			float sumR = 0.f;
			for (int r = 0; r < 4; r++) {
				float inL = lInConn[r] ? inputs[L_INPUT + r].getVoltage() : 0.f;
				float inR = rInConn[r] ? inputs[R_INPUT + r].getVoltage()
					: ((normR[r] && lInConn[r]) ? inL : 0.f);
				float vL = inL * bpL[r] * gain[r];
				float vR = inR * bpR[r] * gain[r];
				rowL[r] = vL;
				rowR[r] = vR;
				maxAbsRowL[r] = std::max(maxAbsRowL[r], std::fabs(vL));
				maxAbsRowR[r] = std::max(maxAbsRowR[r], std::fabs(vR));
				if (lInConn[r] || rInConn[r]) {
					sumL += vL;
					sumR += vR;
					cabled++;
				}
			}

			if (avgLOutConn)
				outputs[AVG_L_OUTPUT].setVoltage(cabled > 0 ? clipToVoltRange(sumL / (float) cabled, outClipRange.act) : 0.f);
			if (avgROutConn)
				outputs[AVG_R_OUTPUT].setVoltage(cabled > 0 ? clipToVoltRange(sumR / (float) cabled, outClipRange.act) : 0.f);

			for (int c = 0; c < 4; c++) {
				if (!mixLOutConn[c] && !mixROutConn[c])
					continue;
				float mixL = 0.f;
				float mixR = 0.f;
				for (int r = 0; r < 4; r++) {
					float send = sendPos[r * 4 + c];
					mixL += rowL[r] * send;
					mixR += rowR[r] * send;
				}
				mixL *= mixScale[c];
				mixR *= mixScale[c];
				mixL = clipToVoltRange(mixL, outClipRange.act);
				mixR = clipToVoltRange(mixR, outClipRange.act);
				if (mixLOutConn[c])
					outputs[MIX_L_OUTPUT + c].setVoltage(mixL);
				if (mixROutConn[c])
					outputs[MIX_R_OUTPUT + c].setVoltage(mixR);
				maxAbsColL[c] = std::max(maxAbsColL[c], std::fabs(mixL));
				maxAbsColR[c] = std::max(maxAbsColR[c], std::fabs(mixR));
			}
		}

		cycle256++;
	}
};

struct MatrixMix4x4StereoModuleWidget : InfNoiseModuleWidget {
	InfNoiseDisableOverlayGroup* linkOverlayGroup[4][3] = {};
	bool linkOn[4][3] = {};
	InfNoiseDisableOverlayGroup* mixModeLockOverlayGroup = nullptr;
	InfNoiseDisableOverlayGroup* linkLockOverlayGroup = nullptr;
	bool mixModeButtonsLocked = false;
	bool linkButtonsLocked = false;

	MatrixMix4x4StereoModuleWidget(MatrixMix4x4StereoModule* module) {
		initializeWidget(module, "res/MatrixMix4x4Stereo");

		const float inX = 16.171f;
		const float inLY[4] = { 51.606f, 121.753f, 191.900f, 262.047f };
		const float inRY[4] = { 86.679f, 156.826f, 226.973f, 297.120f };
		const float rowY[4] = { 69.141f, 139.290f, 209.436f, 279.583f };
		const float colX[4] = { 118.420f, 178.223f, 238.026f, 297.830f };
		const float normX = 24.375f;
		const float normY[4] = { 65.865f, 136.027f, 206.174f, 276.321f };
		const float bpX = 49.476f;
		const float bpModeY[4] = { 46.229f, 116.378f, 186.525f, 256.672f };
		const float gainX = 76.525f;
		const float rowLgtLX = 66.956f;
		const float rowLgtRX = 86.095f;
		const float rowLgtY[4] = { 58.384f, 128.533f, 198.680f, 268.827f };
		const float linkX[4] = { 104.508f, 164.311f, 224.115f, 283.918f };
		const float linkY[3] = { 104.215f, 174.363f, 244.510f };
		const float mixLX[4] = { 102.994f, 162.797f, 222.600f, 282.403f };
		const float mixRX[4] = { 133.152f, 192.955f, 252.758f, 312.561f };
		const float colLgtLX[4] = { 103.593f, 163.396f, 223.199f, 283.003f };
		const float colLgtRX[4] = { 133.327f, 193.131f, 252.934f, 312.737f };
		const float colLgtY = 306.984f;
		const float outY = 332.194f;

		for (int r = 0; r < 4; r++) {
			addInput(createInputCentered<ThemedPJ301MPort>(Vec(inX, inLY[r]), module, MatrixMix4x4StereoModule::L_INPUT + r));
			addInput(createInputCentered<ThemedPJ301MPort>(Vec(inX, inRY[r]), module, MatrixMix4x4StereoModule::R_INPUT + r));
			addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_red, bc_green>>(
				Vec(normX, normY[r]), module, MatrixMix4x4StereoModule::NORM_R_PARAM + r));
			addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_green>>(
				Vec(bpX, bpModeY[r]), module, MatrixMix4x4StereoModule::BP_MODE_PARAM + r));
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(bpX, rowY[r]), module, MatrixMix4x4StereoModule::BP_PARAM + r));
			addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(
				Vec(rowLgtLX, rowLgtY[r]), module, MatrixMix4x4StereoModule::ROW_L_LEVEL_LIGHT + r * 3));
			addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(
				Vec(rowLgtRX, rowLgtY[r]), module, MatrixMix4x4StereoModule::ROW_R_LEVEL_LIGHT + r * 3));
			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(gainX, rowY[r]), module, MatrixMix4x4StereoModule::GAIN_PARAM + r));
			for (int c = 0; c < 4; c++) {
				addParam(createParamCentered<RoundBigBlackKnob>(
					Vec(colX[c], rowY[r]), module, MatrixMix4x4StereoModule::mixId(r, c)));
			}
		}

		for (int c = 0; c < 4; c++) {
			addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_blue>>(
				Vec(linkX[c], 37.830f), module, MatrixMix4x4StereoModule::MIX_MODE_PARAM + c));
			for (int k = 0; k < 3; k++) {
				addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_green, bc_red>>(
					Vec(linkX[c], linkY[k]), module, MatrixMix4x4StereoModule::linkId(c, k)));
			}
			addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(
				Vec(colLgtLX[c], colLgtY), module, MatrixMix4x4StereoModule::COL_L_LEVEL_LIGHT + c * 3));
			addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(
				Vec(colLgtRX[c], colLgtY), module, MatrixMix4x4StereoModule::COL_R_LEVEL_LIGHT + c * 3));
			addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(mixLX[c], outY), module, MatrixMix4x4StereoModule::MIX_L_OUTPUT + c));
			addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(mixRX[c], outY), module, MatrixMix4x4StereoModule::MIX_R_OUTPUT + c));
		}

		addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(16.744f, outY), module, MatrixMix4x4StereoModule::AVG_L_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(46.902f, outY), module, MatrixMix4x4StereoModule::AVG_R_OUTPUT));

		InfNoiseDisableOverlayManager& overlayManager = getDisableOverlayManager();
		for (int c = 0; c < 4; c++) {
			for (int k = 0; k < 3; k++) {
				linkOverlayGroup[c][k] = overlayManager.addGroup("Linked to the knob above");
				linkOverlayGroup[c][k]->addTarget(InfNoiseOverlayTargetType::param, MatrixMix4x4StereoModule::mixId(k + 1, c));
			}
		}
		mixModeLockOverlayGroup = overlayManager.addGroup("Mix-mode buttons locked");
		linkLockOverlayGroup = overlayManager.addGroup("Link buttons locked");
		for (int c = 0; c < 4; c++) {
			mixModeLockOverlayGroup->addTarget(InfNoiseOverlayTargetType::param, MatrixMix4x4StereoModule::MIX_MODE_PARAM + c);
			for (int k = 0; k < 3; k++)
				linkLockOverlayGroup->addTarget(InfNoiseOverlayTargetType::param, MatrixMix4x4StereoModule::linkId(c, k));
		}
	}

	void step() override {
		InfNoiseModuleWidget::step();
		if (!module)
			return;
		auto* m = static_cast<MatrixMix4x4StereoModule*>(module);
		for (int c = 0; c < 4; c++) {
			for (int k = 0; k < 3; k++) {
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

	void addSendPresetMenu(Menu* parent, MatrixMix4x4StereoModule* module, const char* name, int col) {
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
		MatrixMix4x4StereoModule* module = dynamic_cast<MatrixMix4x4StereoModule*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator);
		addSendPresetMenu(menu, module, "All matrix knobs", -1);
		addSendPresetMenu(menu, module, "Diagonal knobs", -2);
		addSendPresetMenu(menu, module, "Column A", 0);
		addSendPresetMenu(menu, module, "Column B", 1);
		addSendPresetMenu(menu, module, "Column C", 2);
		addSendPresetMenu(menu, module, "Column D", 3);

		menu->addChild(createIndexPtrSubmenuItem("Mix-mode buttons",
			{ "Unlocked", "Locked" },
			&module->mixModeLock.req));
		menu->addChild(createIndexPtrSubmenuItem("Link buttons",
			{ "Unlocked", "Locked" },
			&module->linkLockMode.req));

		appendInfNoiseMenuItems(menu);
	}
};

Model* modelMatrixMix4x4Stereo =
	createModel<MatrixMix4x4StereoModule, MatrixMix4x4StereoModuleWidget>("MatrixMix4x4Stereo");
