// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct TinyLCMP2Module : InfNoiseModule {
    enum ParamId {
        CMP1_MODE_PARAM,
        CMP2_MODE_PARAM,
        CMP1_INV_PARAM,
        CMP2_INV_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        CMP11_INPUT,
        CMP12_INPUT,    
        CMP13_INPUT,
        CMP14_INPUT,
        CMP21_INPUT,
        CMP22_INPUT,
        CMP23_INPUT,
        CMP24_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        CMP1_OUTPUT,
        CMP2_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        XOR1_COUNT_LIGHT,
        XOR2_COUNT_LIGHT,
        INVA1_LIGHT,
        INVA2_LIGHT,
        INVA3_LIGHT,
        INVA4_LIGHT,
        INVB1_LIGHT,
        INVB2_LIGHT,
        INVB3_LIGHT,
        INVB4_LIGHT,
        LIGHTS_LEN
    };

    enum xorCountValueType { xcvt_one, xcvt_two, xcvt_three, xcvt_four };
    actReqValue<xorCountValueType> xorCountValue[2] = {
        actReqValue<xorCountValueType>(xcvt_one),
        actReqValue<xorCountValueType>(xcvt_one)
    };
    int inputsInUse[2] = { 0, 0 };
    int channels[2] = { 0, 0 };
    int lightCycle = 0;
    bool haveOutputs = false;
    actReqValue<bool> inpInv[8] = {
        actReqValue<bool>(false),
        actReqValue<bool>(false),
        actReqValue<bool>(false),
        actReqValue<bool>(false),
        actReqValue<bool>(false),
        actReqValue<bool>(false),
        actReqValue<bool>(false),
        actReqValue<bool>(false)
    };
    
	TinyLCMP2Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configSwitch(CMP1_MODE_PARAM, 0.f, 2.f, 0.f, "Operation-mode A", { "AND (NAND)", "OR (NOR)", "XOR (XNOR)" });
        configLight(XOR1_COUNT_LIGHT, "A-OR/XOR-count not 1 when lit");
        configSwitch(CMP1_INV_PARAM, 0.f, 1.f, 0.f, "Operation-logic A", { "Normal (AND/OR/XOR)", "Inverted (NAND/NOR/XNOR)" });
        configInput(CMP11_INPUT, "Compare A1");
        configInput(CMP12_INPUT, "Compare A2");
        configInput(CMP13_INPUT, "Compare A3");
        configInput(CMP14_INPUT, "Compare A4");
        configOutput(CMP1_OUTPUT, "Compare A");
        configLight(INVA1_LIGHT, "Compare A1 input inverted if lit");
        configLight(INVA2_LIGHT, "Compare A2 input inverted if lit");
        configLight(INVA3_LIGHT, "Compare A3 input inverted if lit");
        configLight(INVA4_LIGHT, "Compare A4 input inverted if lit");

        configSwitch(CMP2_MODE_PARAM, 0.f, 2.f, 0.f, "Operation-mode B", { "AND (NAND)", "OR (NOR)", "XOR (XNOR)" });
        configLight(XOR2_COUNT_LIGHT, "B-OR/XOR-count not 1 when lit");
        configSwitch(CMP2_INV_PARAM, 0.f, 1.f, 0.f, "Operation-logic B", { "Normal (AND/OR/XOR)", "Inverted (NAND/NOR/XNOR)" });
        configInput(CMP21_INPUT, "Compare B1");
        configInput(CMP22_INPUT, "Compare B2");
        configInput(CMP23_INPUT, "Compare B3");
        configInput(CMP24_INPUT, "Compare B4");
        configOutput(CMP2_OUTPUT, "Compare B");
        configLight(INVB1_LIGHT, "Compare B1 input inverted if lit");
        configLight(INVB2_LIGHT, "Compare B2 input inverted if lit");
        configLight(INVB3_LIGHT, "Compare B3 input inverted if lit");
        configLight(INVB4_LIGHT, "Compare B4 input inverted if lit");

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
		haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = false;
		haveGateDetect = true;
		haveGateHighLow = true;
		haveTrigDetect = false;
		haveTrigHighLow = false;
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        xorCountValue[0].setBoth(xcvt_one);
        xorCountValue[1].setBoth(xcvt_one);

        for (int i = 0; i < 8; i++) {
			inpInv[i].setBoth(false);
		}
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        int xorCountTmp[2];
        getJsonIntArray(rootJ, "xorCount", xorCountTmp, 2, (int)xorCountValueType::xcvt_one);
        for (int i = 0; i < 2; i++)
            xorCountValue[i].setBoth((xorCountValueType)xorCountTmp[i]);

        int inpInvTmp[8];
        getJsonIntArray(rootJ, "inpInv", inpInvTmp, 8, 0);
        for (int i = 0; i < 8; i++)
            inpInv[i].setBoth(inpInvTmp[i] == 1);
    }

    void dataToJson(json_t* rootJ) override {
        int xorCountTmp[2];
        for (int i = 0; i < 2; i++)
            xorCountTmp[i] = (int)xorCountValue[i].req;
        setJsonIntArray(rootJ, "xorCount", xorCountTmp, 2);

        int inpInvTmp[8];
        for (int i = 0; i < 8; i++)
            inpInvTmp[i] = inpInv[i].req ? 1 : 0;
        setJsonIntArray(rootJ, "inpInv", inpInvTmp, 8);
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        // Force periodical light updates: every 32768 (128*256) cycles
        bool forceLightUpdate = (lightCycle & 0x80) == 0x80; // 
        lightCycle++;
        lightCycle &= 0xFFFF;

        haveOutputs = false;
        for (int i = 0; i < 2; i++) {
            if (outputs[CMP1_OUTPUT + i].isConnected()) {
				haveOutputs = true;
			}

            if (xorCountValue[i].needsUpdate() || forceLightUpdate) {
                xorCountValue[i].updateActual();
                float brightness = (xorCountValue[i].act != xcvt_one && params[CMP1_MODE_PARAM + i].getValue() > 0.5)
                    ? 1.f 
                    : 0.f;
                lights[XOR1_COUNT_LIGHT + i].setBrightness(brightness); // Update light
            }

            inputsInUse[i] = 0;
            channels[i] = 1;
            for (int j = 0; j < 4; j++) {
                int inpIdx = CMP11_INPUT + 4 * i + j;
                if (inputs[inpIdx].isConnected()) {
                    channels[i] = std::max(channels[i], inputs[inpIdx].getChannels());
					inputsInUse[i]++;
				}

                int lgtIdx = i * 4 + j;
                if (inpInv[lgtIdx].needsUpdate() || forceLightUpdate) {
                    inpInv[lgtIdx].updateActual();
                    float brightness = inpInv[lgtIdx].act 
                        ? (inputs[inpIdx].isConnected())
                            ? 1.f // Lit full, as it is in effect
                            : 0.3f  // Lit dimmed, as it has no effect (no input)
                        : 0.f;
                    lights[INVA1_LIGHT + lgtIdx].setBrightness(brightness);
                }
			}

            outputs[CMP1_OUTPUT + i].setChannels(channels[i]);
        }

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
            for (int i=0; i<2; i++) {
                if (outputs[CMP1_OUTPUT + i].isConnected())
                {
                    if (inputsInUse[i] > 0) {
                        for (int c = 0; c < channels[i]; c++) {
                            int trueCount = 0;
                            for (int j = 0; j < 4; j++) {
                                int inpIdx = CMP11_INPUT + 4 * i + j;
                                if (inputs[inpIdx].isConnected()) {
                                    bool isHigh = (inputs[inpIdx].getPolyVoltage(c) >= trueDetectValues[gateDetHigh.act]);
                                    if (inpInv[i * 4 + j].act)
										isHigh = !isHigh;
                                    if (isHigh)
										trueCount++;
                                }
							}

                            bool evaluation = false;
                            switch ((int)params[CMP1_MODE_PARAM + i].getValue()) {
								case 0: evaluation = (trueCount == inputsInUse[i]); break; // AND
								case 1: evaluation = (trueCount >= (int)xorCountValue[i].act + 1); break; // OR
								case 2: evaluation = (trueCount == (int)xorCountValue[i].act + 1); break;  // XOR
							}
                            if (params[CMP1_INV_PARAM + i].getValue() > 0.5f)
								evaluation = !evaluation;

                            float voltage = evaluation 
                                ? voltValues[gateOutHigh.act] 
                                : voltValues[gateOutLow.act];
							outputs[CMP1_OUTPUT + i].setVoltage(voltage, c);
						}
					}
					else {
                        float voltage = (params[CMP1_INV_PARAM + i].getValue() > 0.5f) 
                            ? voltValues[gateOutHigh.act] 
                            : voltValues[gateOutLow.act];
                        outputs[CMP1_OUTPUT + i].setVoltage(voltage);
                    }
				}
			}
		}

		cycle256++;
    }
};

struct TinyLCMP2ModuleWidget : InfNoiseModuleWidget {
    TinyLCMP2ModuleWidget(TinyLCMP2Module *module) {
        initializeWidget(module, "res/TinyLCMP2");

        const float tglCol = 9.178f;
        const float cntrCol = 15.f;

        // Top-section
        addParam(createParamCentered<CKSSThree>(Vec(tglCol, 41.927f), module, TinyLCMP2Module::CMP1_MODE_PARAM));
        infNoiseLtSmallButton* inv1Btn = createParamCentered<infNoiseLtSmallButton>(Vec(25.010f, 57.050f), module, TinyLCMP2Module::CMP1_INV_PARAM);
        inv1Btn->setup(bc_red, false);
        addParam(inv1Btn);

        addChild(createLightCentered<TinyLight<RedLight>>(Vec(22.515f, 28.942f), module, TinyLCMP2Module::XOR1_COUNT_LIGHT));

        const float invLgtColOffs = +10.421f;
        const float invLgtRowOffs = 10.521f;
        const float inpSpacing = 24.6323f;
        float row = 69.370f;
        for (int i = 0; i < 4; i++) {
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, row), module, TinyLCMP2Module::CMP11_INPUT + i));
            addChild(createLightCentered<TinyLight<RedLight>>(Vec(cntrCol + invLgtColOffs, row + invLgtRowOffs), module, TinyLCMP2Module::INVA1_LIGHT + i));
            row += inpSpacing;
        }
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 169.694f), module, TinyLCMP2Module::CMP1_OUTPUT));

        // Bottom-section
        addParam(createParamCentered<CKSSThree>(Vec(tglCol, 204.927f), module, TinyLCMP2Module::CMP2_MODE_PARAM));
        infNoiseLtSmallButton* inv2Btn = createParamCentered<infNoiseLtSmallButton>(Vec(25.010f, 220.050f), module, TinyLCMP2Module::CMP2_INV_PARAM);
        inv2Btn->setup(bc_red, false);
        addParam(inv2Btn);

        addChild(createLightCentered<TinyLight<RedLight>>(Vec(22.515f, 191.942f), module, TinyLCMP2Module::XOR2_COUNT_LIGHT));

        row = 232.370f;
        for (int i = 0; i < 4; i++) {
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, row), module, TinyLCMP2Module::CMP21_INPUT + i));
            addChild(createLightCentered<TinyLight<RedLight>>(Vec(cntrCol + invLgtColOffs, row + invLgtRowOffs), module, TinyLCMP2Module::INVB1_LIGHT + i));
            row += inpSpacing;
        }
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 332.694f), module, TinyLCMP2Module::CMP2_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        TinyLCMP2Module* module = dynamic_cast<TinyLCMP2Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> xorCountNames = { "1 (default)", "2", "3", "4" };
        menu->addChild(createIndexPtrSubmenuItem("A: OR min count/XOR exact count",
            xorCountNames, &module->xorCountValue[0].req));
        menu->addChild(createIndexPtrSubmenuItem("Input A1", { "Normal", "Inverted" }, &module->inpInv[0].req));
        menu->addChild(createIndexPtrSubmenuItem("Input A2", { "Normal", "Inverted" }, &module->inpInv[1].req));
        menu->addChild(createIndexPtrSubmenuItem("Input A3", { "Normal", "Inverted" }, &module->inpInv[2].req));
        menu->addChild(createIndexPtrSubmenuItem("Input A4", { "Normal", "Inverted" }, &module->inpInv[3].req));

        menu->addChild(new MenuSeparator);
        menu->addChild(createIndexPtrSubmenuItem("B: OR min count/XOR exact count",
            xorCountNames, &module->xorCountValue[1].req));
        menu->addChild(createIndexPtrSubmenuItem("Input B1", { "Normal", "Inverted" }, &module->inpInv[4].req));
        menu->addChild(createIndexPtrSubmenuItem("Input B2", { "Normal", "Inverted" }, &module->inpInv[5].req));
        menu->addChild(createIndexPtrSubmenuItem("Input B3", { "Normal", "Inverted" }, &module->inpInv[6].req));
        menu->addChild(createIndexPtrSubmenuItem("Input B4", { "Normal", "Inverted" }, &module->inpInv[7].req));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelTinyLCMP2 = createModel<TinyLCMP2Module, TinyLCMP2ModuleWidget>("TinyLCMP2");