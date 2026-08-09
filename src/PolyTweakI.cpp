// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct PolyTweakIModule : InfNoiseModule {
    enum ParamId {
        INV1_PARAM,
        INV2_PARAM,
        INV3_PARAM,
        INV4_PARAM,
        INV5_PARAM,
        INV6_PARAM,
        INV7_PARAM,
        INV8_PARAM,
        INV9_PARAM,
        INV10_PARAM,
        INV11_PARAM,
        INV12_PARAM,
        INV13_PARAM,
        INV14_PARAM,
        INV15_PARAM,
        INV16_PARAM,
        INV_ALL_PARAM,
        INV_MODE_PARAM,
        DISABLE1_PARAM,
        DISABLE2_PARAM,
        DISABLE3_PARAM,
        DISABLE4_PARAM,
        DISABLE5_PARAM,
        DISABLE6_PARAM,
        DISABLE7_PARAM,
        DISABLE8_PARAM,
        DISABLE9_PARAM,
        DISABLE10_PARAM,
        DISABLE11_PARAM,
        DISABLE12_PARAM,
        DISABLE13_PARAM,
        DISABLE14_PARAM,
        DISABLE15_PARAM,
        DISABLE16_PARAM,
        DISABLE_ALL_PARAM,
        DISABLE_MODE_PARAM,
        DISABLE_VALUE_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        POLY_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        POLY_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        FIXED_CHANNELS_LIGHT,
        LIGHTS_LEN
    };
   
    bool haveOutput = false;
    int inputChannels = 0;
    int maxChannels = 16;
    int inUseChannels = 0;
    float invOffset = 0.f;
    actReqValue<polyphonyMode> polyphony = actReqValue<polyphonyMode>(poly_auto);
    bool doBiUnipolar = true; // true = bipolar, false = unipolar
    bool doSetVal = false; // true = set to disable-value, false = exclude from output
    bool doSetAsGate = false; // true = set as gate, false = set as voltage
    float disableValue = 0.f; // value to set to when doSetVal is true
    bool invertChannels[16] = { false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false };
    bool disableChannels[16] = { false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false };
    enum invertAllModeType { iam_off, iam_on, iam_toggle };
    actReqValue<invertAllModeType> invertAllMode = actReqValue<invertAllModeType>(iam_toggle);
    enum disableAllModeType { dam_off, dam_on, dam_toggle };
    actReqValue<disableAllModeType> disableAllMode = actReqValue<disableAllModeType>(dam_toggle);
    enum gateNonInvModeType { gnm_passthrough, gnm_gate };
    actReqValue<gateNonInvModeType> gateNonInvMode = actReqValue<gateNonInvModeType>(gnm_gate);
    bool doInvertAll = false;
    bool doDisableAll = false;
    dsp::SchmittTrigger invAllTrigger = dsp::SchmittTrigger();
    dsp::SchmittTrigger disableAllTrigger = dsp::SchmittTrigger();
    const std::string invAllTooltip[3] = {
        "Set all channels to normal",
        "Set all channels to inverted",
        "Toggle all normal/inverted channels"
    };
    const std::string disableAllTooltip[3] = {
        "Set all channels to enabled",
        "Set all channels to disabled",
        "Toggle all enabled/disabled channels"
    };

    PolyTweakIModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
        
        configLight(FIXED_CHANNELS_LIGHT, "Fixed polyphonic count if lit");
        configInput(POLY_INPUT, "Original");

        for (int i = 0; i < 16; i++) {
            configSwitch(INV1_PARAM + i, 0.f, 1.f, 0.f, string::f("Invert channel-%d", i + 1), { "Normal", "Inverted" });
            configSwitch(DISABLE1_PARAM + i, 0.f, 1.f, 0.f, string::f("Disable channel-%d", i + 1), { "Enabled", "Disabled" });
		}
        configSwitch(INV_ALL_PARAM, 0.f, 1.f, 0.f, invAllTooltip[(int)iam_toggle]);
        configSwitch(DISABLE_ALL_PARAM, 0.f, 1.f, 0.f, disableAllTooltip[(int)dam_toggle]);

        configSwitch(INV_MODE_PARAM, 0.f, 2.f, 0.f, "Invert-mode", { "Bipolar (-5V to +5V)", "Unipolar (0V to 10V)", "Gate (high/low-gate)" });
        configSwitch(DISABLE_MODE_PARAM, 0.f, 2.f, 0.f, "Disable-mode", { "Set to disable-value", "Exclude from output" });
        configParam(DISABLE_VALUE_PARAM, -10.0f, 10.0f, 0.0f, "Disable-value (-10V to +10V)", " V");

        configOutput(POLY_OUTPUT, "Tweaked");

        configBypass(POLY_INPUT, POLY_OUTPUT);

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
		haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = true;  
		haveGateDetect = true;
		haveGateHighLow = true;
		haveTrigDetect = false;
		haveTrigHighLow = false;
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        
        polyphony.setBoth(poly_auto);
        invertAllMode.setBoth(iam_toggle);
        disableAllMode.setBoth(dam_toggle);
        gateNonInvMode.setBoth(gnm_gate);
        doInvertAll = false;
        doDisableAll = false;
        invAllTrigger.reset();
        disableAllTrigger.reset();
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        polyphony.setBoth((polyphonyMode)getJsonInt(rootJ, "polyphony", (int)poly_auto));
        invertAllMode.setBoth((invertAllModeType)getJsonInt(rootJ, "invertAllMode", (int)iam_toggle));
        disableAllMode.setBoth((disableAllModeType)getJsonInt(rootJ, "disableAllMode", (int)dam_toggle));
        gateNonInvMode.setBoth((gateNonInvModeType)getJsonInt(rootJ, "gateNonInvMode", (int)gnm_gate));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "polyphony", json_integer((int)polyphony.req));
        json_object_set_new(rootJ, "invertAllMode", json_integer((int)invertAllMode.req));
        json_object_set_new(rootJ, "disableAllMode", json_integer((int)disableAllMode.req));
        json_object_set_new(rootJ, "gateNonInvMode", json_integer((int)gateNonInvMode.req));
    }

    void setAllInvertButtons(bool invert) {
        for (int i = 0; i < 16; i++) {
			params[INV1_PARAM + i].setValue(invert ? 1.f : 0.f);
		}
	}

    void setAllDisableButtons(bool disable) {
        for (int i = 0; i < 16; i++) {
            params[DISABLE1_PARAM + i].setValue(disable ? 1.f : 0.f);
        }
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        if (invertAllMode.needsUpdate()) {
            invertAllMode.updateActual();
            paramQuantities[INV_ALL_PARAM]->name = invAllTooltip[(int)invertAllMode.act];
        }
        if (disableAllMode.needsUpdate()) {
            disableAllMode.updateActual();
            paramQuantities[DISABLE_ALL_PARAM]->name = disableAllTooltip[(int)disableAllMode.act];
        }
        
        gateNonInvMode.updateActual();
        invOffset = params[INV_MODE_PARAM].getValue() < 0.5f ? 0.f : 10.f;       
        doBiUnipolar = params[INV_MODE_PARAM].getValue() < 1.5f; // false = gate
        doSetVal = params[DISABLE_MODE_PARAM].getValue() < 0.5f;
        disableValue = params[DISABLE_VALUE_PARAM].getValue();
        doSetAsGate = gateNonInvMode.act == gnm_gate;

        if (doInvertAll) {
            doInvertAll = false;
            if (invertAllMode.act == iam_toggle) {
                for (int i = 0; i < 16; i++) {
                    bool setValue = params[INV1_PARAM + i].getValue() > 0.5f
                        ? 0.f
                        : 1.f;
                    params[INV1_PARAM + i].setValue(setValue);
                }
            }
            else {
                float setValue = invertAllMode.act == iam_on ? 1.f : 0.f;
                for (int i = 0; i < 16; i++) {
                    params[INV1_PARAM + i].setValue(setValue);
                }
            }
        }
        if (doDisableAll) {
            doDisableAll = false;
            if (disableAllMode.act == dam_toggle) {
                for (int i = 0; i < 16; i++) {
                    bool setValue = params[DISABLE1_PARAM + i].getValue() > 0.5f
                        ? 0.f
                        : 1.f;
                    params[DISABLE1_PARAM + i].setValue(setValue);
                }
            }
            else {
                float setValue = disableAllMode.act == dam_on ? 1.f : 0.f;
                for (int i = 0; i < 16; i++) {
                    params[DISABLE1_PARAM + i].setValue(setValue);
                }
            }
        }

        for (int i = 0; i < 16; i++) {
            invertChannels[i] = params[INV1_PARAM + i].getValue() > 0.5f;
            disableChannels[i] = params[DISABLE1_PARAM + i].getValue() > 0.5f;
        }

        haveOutput = outputs[POLY_OUTPUT].isConnected();
        inUseChannels = 0;
        inputChannels = inputs[POLY_INPUT].isConnected() 
            ? inputs[POLY_INPUT].getChannels() 
            : 0;
        if (polyphony.needsUpdate()) {
            polyphony.updateActual();
            lights[FIXED_CHANNELS_LIGHT].setBrightness(polyphony.act != poly_auto ? 1.f : 0.f);
        }
        maxChannels = polyphony.act != poly_auto 
            ? polyphonyModeChannels[polyphony.act] 
            : inputChannels;
        if (maxChannels > 0) {
            if (doSetVal)
                inUseChannels = maxChannels;
            else {
                for (int c = 0; c < maxChannels; c++) {
                    if (!disableChannels[c])
                        inUseChannels++;
                }
            }
        }

        outputs[POLY_OUTPUT].setChannels(std::max(1, inUseChannels));
        if (inUseChannels == 0)
            outputs[POLY_OUTPUT].setVoltage(0.f);

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
            // These 2 booleans are handled in next processParams() call
            if (invAllTrigger.process(params[INV_ALL_PARAM].getValue(), 0.1f, 0.9f))
                doInvertAll = true;
            if (disableAllTrigger.process(params[DISABLE_ALL_PARAM].getValue(), 0.1f, 0.9f))
                doDisableAll = true;

            if (haveOutput && inUseChannels > 0) {
                int outChnl = 0; // used by setVoltage()
                for (int c = 0; c < maxChannels; c++) {
                    float voltage = 0.f;
                    if (doSetVal || !disableChannels[c]) {
                        if (disableChannels[c])
                            voltage = disableValue;
                        else {
                            if ((c < inputChannels) && inputs[POLY_INPUT].isConnected())
                                voltage = inputs[POLY_INPUT].getVoltage(c);
                            if (!doBiUnipolar && doSetAsGate) {
                                voltage = voltage >= trueDetectValues[gateDetHigh.act]
                                    ? voltValues[gateOutHigh.act]
                                    : voltValues[gateOutLow.act];
                            }
                        }

                        if (invertChannels[c] && !disableChannels[c])  // Invert
                        {
                            if (doBiUnipolar) // Bipolar or unipolar
                            {
                                voltage = invOffset - voltage;
							}
                            else // Gate
                            {
								voltage = voltage >= trueDetectValues[gateDetHigh.act]
                                    ? voltValues[gateOutLow.act] 
                                    : voltValues[gateOutHigh.act];
							}
                        }

                        voltage = clipToVoltRange(voltage, outClipRange.act);
						outputs[POLY_OUTPUT].setVoltage(voltage, outChnl++);
					}
				}   
            }
        }

        cycle256++;
    }
};

struct PolyTweakIModuleWidget : InfNoiseModuleWidget {
    PolyTweakIModuleWidget(PolyTweakIModule *module) {
        initializeWidget(module, "res/PolyTweakI");

        // Fixed polyphonic count light
        addChild(createLightCentered<TinyLight<RedLight>>(Vec(4.364f, 35.408f), module, PolyTweakIModule::FIXED_CHANNELS_LIGHT));

        // Poly-input
        const float cntrClm = 15.f;
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 51.428f), module, PolyTweakIModule::POLY_INPUT));

        // Invert-buttons (1-16 and All)
        const float lftBtnClm = 10.897f;
        const float rgtBtnClm = 18.709f;
        const float chnlRowSpacing = 7.6992f;
        float chnlRow = 74.876;
        for (int i = 0; i < 8; i++)
        {
            // Left invert-button
            addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(lftBtnClm, chnlRow), module, PolyTweakIModule::INV1_PARAM + i));

            // Right invert-button
            addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(rgtBtnClm, chnlRow), module, PolyTweakIModule::INV9_PARAM + i));

            chnlRow+= chnlRowSpacing;
        }

        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, true>>(Vec(cntrClm, chnlRow), module, PolyTweakIModule::INV_ALL_PARAM));

        // Invert-mode
        float switchCol = 9.522f;
        addParam(createParamCentered<CKSSThree>(Vec(switchCol, 156.374f), module, PolyTweakIModule::INV_MODE_PARAM));

        // Enable-buttons (1-16 and All)
        chnlRow = 185.918f;
        for (int i = 0; i < 8; i++)
        {
            // Left enable-button
            addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(lftBtnClm, chnlRow), module, PolyTweakIModule::DISABLE1_PARAM + i));

            // Right enable-button
            addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(rgtBtnClm, chnlRow), module, PolyTweakIModule::DISABLE9_PARAM + i));

            chnlRow += chnlRowSpacing;
        }

        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, true>>(Vec(cntrClm, chnlRow), module, PolyTweakIModule::DISABLE_ALL_PARAM));

        // Enable-mode/value
        addParam(createParamCentered<CKSS>(Vec(switchCol, 263.563f), module, PolyTweakIModule::DISABLE_MODE_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrClm, 292.937f), module, PolyTweakIModule::DISABLE_VALUE_PARAM));

        // Poly-output
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 332.694f), module, PolyTweakIModule::POLY_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        PolyTweakIModule* module = dynamic_cast<PolyTweakIModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> polyNames = getPolyphonyModeNames(true);
        menu->addChild(createIndexPtrSubmenuItem("Polyphony", polyNames,
            &module->polyphony.req));

        std::vector<std::string> invertAllModeNames = {
            "Set all to normal",
            "Set all to inverted",
            "Toggle all"
        };
        menu->addChild(createIndexPtrSubmenuItem("Invert all-button mode", invertAllModeNames,
            &module->invertAllMode.req));

        std::vector<std::string> disableAllModeNames = {
            "Set all to enabled",
            "Set all to disabled",
            "Toggle all"
        };
        menu->addChild(createIndexPtrSubmenuItem("Disable all-button mode", disableAllModeNames,
            &module->disableAllMode.req));
        
        std::vector<std::string> gateNonInvModeNames = {
            "Pass through",
            "High/low-gate"
        };
        menu->addChild(createIndexPtrSubmenuItem("Gate mode: non-inverted channels", gateNonInvModeNames,
            &module->gateNonInvMode.req));

        menu->addChild(new MenuSeparator);

        menu->addChild(createSubmenuItem("Set all invert-buttons", "",
            [=](Menu* menu) {
            	menu->addChild(createMenuItem("To Normal", "", [=]() {
                    module->setAllInvertButtons(false);
                    }));
            	menu->addChild(createMenuItem("To Invert", "", [=]()  {
                    module->setAllInvertButtons(true);
                    }));
            }
        ));

        menu->addChild(createSubmenuItem("Set all Disable-buttons", "",
            [=](Menu* menu) {
                menu->addChild(createMenuItem("To Enabled", "", [=]() {
                    module->setAllDisableButtons(false);
                    }));
                menu->addChild(createMenuItem("To Disabled", "", [=]() {
                    module->setAllDisableButtons(true);
                    }));
            }
        ));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelPolyTweakI = createModel<PolyTweakIModule, PolyTweakIModuleWidget>("PolyTweakI");