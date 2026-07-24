// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"


struct PolyOffsetModule : InfNoiseModule {
    enum ParamId {
        MODE_PARAM,
        ALL_OFFSET_PARAM,
        ALL_OFFSET_TRIM_PARAM,
        INC_OFFSET_PARAM,
        INC_OFFSET_TRIM_PARAM,
        OFFSET1_PARAM,
        OFFSET2_PARAM,
        OFFSET3_PARAM,
        OFFSET4_PARAM,
        OFFSET5_PARAM,
        OFFSET6_PARAM,
        OFFSET7_PARAM,
        OFFSET8_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        POLY_INPUT,
        ALL_OFFSET_INPUT,
        INC_OFFSET_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        POLY_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT, 2),
        ENUMS(CLIP_RANGE_LIGHT, 2),
        FIXED_CHANNEL_LIGHT,
        LIGHTS_LEN
    };

    int channels = 1;
    actReqValue<polyphonyMode> polyphony = actReqValue<polyphonyMode>(poly_auto);
    bool haveOutput = false;
    bool primaryMode = true;
    bool havePriSecOffset = false;
    float offsets[8] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
    float allTrim = 0.f;
    float incTrim = 0.f;
    
	PolyOffsetModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
        
        configInput(POLY_INPUT, "Polyphonic");
        configSwitch(MODE_PARAM, 0.0, 1.0, 0.0, "Mode", { "Primary", "Secondary" });

        configParam(ALL_OFFSET_PARAM, -10.0f, 10.0f, 0.0f, "All offset", " V");
        configInput(ALL_OFFSET_INPUT, "All offset");
        configParam(ALL_OFFSET_TRIM_PARAM, -1.f, 1.f, 0.f, "All offset CV-trim", "%", 0, 100);

        configParam(INC_OFFSET_PARAM, -10.0f, 10.0f, 0.0f, "Incremental offset", " V");
        configParam(INC_OFFSET_TRIM_PARAM, -1.f, 1.f, 0.f, "Incremental offset CV-trim", "%", 0, 100);
        configInput(INC_OFFSET_INPUT, "Incremental offset");

        configParam(OFFSET1_PARAM, -10.0f, 10.0f, 0.0f, "1 / Odd offset", " V");
        configParam(OFFSET2_PARAM, -10.0f, 10.0f, 0.0f, "2 / Even offset", " V");
        configParam(OFFSET3_PARAM, -10.0f, 10.0f, 0.0f, "3 / 1-4 offset", " V");
        configParam(OFFSET4_PARAM, -10.0f, 10.0f, 0.0f, "4 / 5-8 offset", " V");
        configParam(OFFSET5_PARAM, -10.0f, 10.0f, 0.0f, "5 / 9-12 offset", " V");
        configParam(OFFSET6_PARAM, -10.0f, 10.0f, 0.0f, "6 / 13-16 offset", " V");
        configParam(OFFSET7_PARAM, -10.0f, 10.0f, 0.0f, "7 / 1-8 offset", " V");
        configParam(OFFSET8_PARAM, -10.0f, 10.0f, 0.0f, "8 / 9-16 offset", " V");
        
        configOutput(POLY_OUTPUT, "Polyphonic");

        configLight(FIXED_CHANNEL_LIGHT, "Fixed polyphony if lit");

        configBypass(POLY_INPUT, POLY_OUTPUT);

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
		haveAutoProcQuality = false;
        haveOutQuantize = true;
        haveOutClipRange = true;  
		haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = false;
		haveTrigHighLow = false;
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        
        polyphony.setBoth(poly_auto);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        polyphony.setBoth((polyphonyMode)getJsonInt(rootJ, "polyphony", (int)poly_auto));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "polyphony", json_integer((int)polyphony.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        allTrim = params[ALL_OFFSET_TRIM_PARAM].getValue();
        incTrim = params[INC_OFFSET_TRIM_PARAM].getValue();

        polyphony.updateActual();
        if (polyphony.act == poly_auto) {
            channels = inputs[POLY_INPUT].isConnected() 
                ? inputs[POLY_INPUT].getChannels() 
                : 1;
            lights[FIXED_CHANNEL_LIGHT].setBrightness(0.f); // Auto-polyphony
        } else {
            channels = (int)polyphony.act + 1;
            lights[FIXED_CHANNEL_LIGHT].setBrightness(1.f); // Fixed-polyphony
        }

        primaryMode = params[MODE_PARAM].getValue() < 0.5f;

        haveOutput = outputs[POLY_OUTPUT].isConnected();
        if (haveOutput)
            outputs[POLY_OUTPUT].setChannels(channels);
        else
            outputs[POLY_OUTPUT].setChannels(1);

        havePriSecOffset = false;     
        for (int i = 0; i < 8; i++) {
            offsets[i] = params[OFFSET1_PARAM + i].getValue();
            if (offsets[i] != 0.f)
                havePriSecOffset = true;
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

        if (doProcess && haveOutput) {           
            // Incremental offset (supports only mono input)
            float incVoltage = params[INC_OFFSET_PARAM].getValue();
            if (inputs[INC_OFFSET_INPUT].isConnected())
                incVoltage += inputs[INC_OFFSET_INPUT].getVoltage() * incTrim;

            for (int c = 0; c < channels; c++) {
                float voltage = (inputs[POLY_INPUT].isConnected())
                    ? inputs[POLY_INPUT].getPolyVoltage(c)
                    : 0.f;

                // All offset (supports polyphonic input)
                voltage += params[ALL_OFFSET_PARAM].getValue();
                if (inputs[ALL_OFFSET_INPUT].isConnected())
                    voltage += inputs[ALL_OFFSET_INPUT].getPolyVoltage(c) * allTrim;
                
                // Apply incremental offset
                voltage += incVoltage * c;

                if (havePriSecOffset) { // Only enter if there are PRI/SEC offsets
                    if (primaryMode) {
                        if (c < 4) {
                            if (c == 0)
                                voltage += offsets[0];
                            else if (c == 1)
                                voltage += offsets[1];
                            else if (c == 2)
                                voltage += offsets[2];
                            else if (c == 3)
                                voltage += offsets[3];
                        } else if (c < 8) {
                            if (c == 4)
                                voltage += offsets[4];
                            else if (c == 5)
                                voltage += offsets[5];
                            else if (c == 6)
                                voltage += offsets[6];
                            else if (c == 7)
                                voltage += offsets[7];
                        }
                    } else {  // Secondary mode
                        int mod2 = c % 2;
                        if (mod2 == 0) // Odd (1st channel: c == 0)
                            voltage += offsets[0];
                        else if (mod2 == 1) // Even (2nd channel: c == 1)
                            voltage += offsets[1];
                        if (c < 4) // 1-4
                            voltage += offsets[2];
                        else if (c >= 4 && c < 8) // 5-8
                            voltage += offsets[3];
                        else if (c >= 8 && c < 12) // 9-12
                            voltage += offsets[4];
                        else // 13-16
                            voltage += offsets[5];
                        if (c < 8) // 1-8
                            voltage += offsets[6];
                        else // 9-16
                            voltage += offsets[7];
                    }
                }

                voltage = quantizeToMode(voltage, outQuantize.act);
                voltage = clipToVoltRange(voltage, outClipRange.act);
                outputs[POLY_OUTPUT].setVoltage(voltage, c);
            }
        }

        cycle256++;
    }
};

struct PolyOffsetModuleWidget : InfNoiseModuleWidget {
    PolyOffsetModuleWidget(PolyOffsetModule *module) {
        initializeWidget(module, "res/PolyOffset");

        const float clm1 = 14.810f;
        const float clm2 = 44.146f;
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(clm1, 51.428f), module, PolyOffsetModule::POLY_INPUT));
        addChild(createLightCentered<TinyLight<RedLight>>(Vec(25.129f, 41.909f), module, PolyOffsetModule::FIXED_CHANNEL_LIGHT));
        addParam(createParamCentered<CKSS>(Vec(37.303f, 51.428f), module, PolyOffsetModule::MODE_PARAM));

        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(clm1, 89.330f), module, PolyOffsetModule::ALL_OFFSET_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(clm1, 112.858f), module, PolyOffsetModule::ALL_OFFSET_TRIM_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(clm1, 136.735f), module, PolyOffsetModule::ALL_OFFSET_INPUT));

        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(clm2, 89.330f), module, PolyOffsetModule::INC_OFFSET_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(clm2, 112.858f), module, PolyOffsetModule::INC_OFFSET_TRIM_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(clm2, 136.735f), module, PolyOffsetModule::INC_OFFSET_INPUT));

        float row = 173.279f;
        float rowSpacing = 38.879f;
        for (int i = 0; i < 4; i++) {
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(clm1, row), module, PolyOffsetModule::OFFSET1_PARAM + i));
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(clm2, row), module, PolyOffsetModule::OFFSET5_PARAM + i));
            row += rowSpacing;
        }

        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(29.500f, 332.694f), module, PolyOffsetModule::POLY_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        PolyOffsetModule* module = dynamic_cast<PolyOffsetModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> polyNames = getPolyphonyModeNames(true);
        menu->addChild(createIndexPtrSubmenuItem("Polyphony", polyNames,
            &module->polyphony.req));

        std::vector<std::string> intervalNames = getVoltIntervalValuesNames();
        std::vector<std::string> voltNames = getVoltValuesNames();
        menu->addChild(createSubmenuItem("Set: All offset", "", [=](Menu* setAllMenu) {
            setAllMenu->addChild(createSubmenuItem("Interval", "", [=](Menu* submenu) {
                for (int i = 0; i < voltIntervalValueCount; i++) {
                    submenu->addChild(createMenuItem(intervalNames[i], "", [=]() {
                        module->setParamKnobToVoltInterval(module->ALL_OFFSET_PARAM, (voltIntervalValue)i);
                    }));
                }
            }));
            setAllMenu->addChild(createSubmenuItem("Volt level", "", [=](Menu* submenu) {
                for (int i = 0; i < voltValueCount; i++) {
                    submenu->addChild(createMenuItem(voltNames[i], "", [=]() {
                        module->setParamKnobToVolt(module->ALL_OFFSET_PARAM, (voltValue)i);
                    }));
                }
            }));
        }));

        menu->addChild(createSubmenuItem("Set: Incremental offset", "", [=](Menu* setIncMenu) {
            setIncMenu->addChild(createSubmenuItem("Interval", "", [=](Menu* submenu) {
                for (int i = 0; i < voltIntervalValueCount; i++) {
                    submenu->addChild(createMenuItem(intervalNames[i], "", [=]() {
                        module->setParamKnobToVoltInterval(module->INC_OFFSET_PARAM, (voltIntervalValue)i);
                    }));
                }
            }));
            setIncMenu->addChild(createSubmenuItem("Volt level", "", [=](Menu* submenu) {
                for (int i = 0; i < voltValueCount; i++) {
                    submenu->addChild(createMenuItem(voltNames[i], "", [=]() {
                        module->setParamKnobToVolt(module->INC_OFFSET_PARAM, (voltValue)i);
                    }));
                }
            }));
        }));

        menu->addChild(createSubmenuItem("Set: Offsets 1-8", "", [=](Menu* setOffsetsMenu) {
            appendScaleChordSetMenuItems(setOffsetsMenu, module, PolyOffsetModule::OFFSET1_PARAM, 8);
        }));

        for (int k = 0; k < 8; k++) {
            menu->addChild(createSubmenuItem(string::f("Set: Offset %d", k + 1), "", [=](Menu* setKMenu) {
                setKMenu->addChild(createSubmenuItem("Interval", "", [=](Menu* submenu) {
                    for (int i = 0; i < voltIntervalValueCount; i++) {
                        submenu->addChild(createMenuItem(intervalNames[i], "", [=]() {
                            module->setParamKnobToVoltInterval(module->OFFSET1_PARAM + k, (voltIntervalValue)i);
                        }));
                    }
                }));
                setKMenu->addChild(createSubmenuItem("Volt level", "", [=](Menu* submenu) {
                    for (int i = 0; i < voltValueCount; i++) {
                        submenu->addChild(createMenuItem(voltNames[i], "", [=]() {
                            module->setParamKnobToVolt(module->OFFSET1_PARAM + k, (voltValue)i);
                        }));
                    }
                }));
            }));
        }

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelPolyOffset = createModel<PolyOffsetModule, PolyOffsetModuleWidget>("PolyOffset");