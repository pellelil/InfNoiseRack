// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct PolyScaleModule : InfNoiseModule {
    enum ParamId {
        MODE_PARAM,
        ALL_SCALE_PARAM,
        ALL_SCALE_TRIM_PARAM,
        SCALE_MODE_PARAM,
        SCALE1_PARAM,
        SCALE2_PARAM,
        SCALE3_PARAM,
        SCALE4_PARAM,
        SCALE5_PARAM,
        SCALE6_PARAM,
        SCALE7_PARAM,
        SCALE8_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        POLY_INPUT,
        ALL_SCALE_INPUT,
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
        SCALE_1X_LIGHT,
        SCALE_2X_LIGHT,
        SCALE_5X_LIGHT,
        SCALE_10X_LIGHT,
        MULTIPLE_SCALE_LIGHT,
        LIGHTS_LEN
    };

    int channels = 1;
    actReqValue<polyphonyMode> polyphony = actReqValue<polyphonyMode>(poly_auto);
    bool haveOutput = false;
    bool primaryMode = true;
    bool haveAllScale = false;
    bool havePriSecScale = false;
    float scales[8] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
    float allTrim = 0.f;
    dsp::SchmittTrigger scaleModeTrigger; 
    actReqValue<infNoiseAttRngQnt::attRange> attRng =
        actReqValue<infNoiseAttRngQnt::attRange>(infNoiseAttRngQnt::attRange::ar_1x);
    bool doCycleAttRng = false;
    float attRngFactor = 1.f;
    
    const std::string rangeName[8]{ "1 / Odd scale", "2 / Even scale", "3 / 1-4 scale", "4 / 5-8 scale",
        "5 / 9-12 scale", "6 / 13-16 scale", "7 / 1-8 scale", "8 / 9-16 scale" };

	PolyScaleModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
        
        configInput(POLY_INPUT, "Polyphonic");
        configSwitch(MODE_PARAM, 0.0, 1.0, 0.0, "Mode", { "Primary", "Secondary" });

        configParam<infNoiseAttRngQnt>(ALL_SCALE_PARAM, -1.f, 1.f, 1.f, "All scale (-1x to +1x)", " x", 0, 1);
        configInput(ALL_SCALE_INPUT, "All scale");
        configParam(ALL_SCALE_TRIM_PARAM, -1.f, 1.f, 0.f, "All scale CV-trim", "%", 0, 100);

        configSwitch(SCALE_MODE_PARAM, 0.0f, 1.0f, 0.0f, "Scale-mode cycle");

        configParam<infNoiseAttRngQnt>(SCALE1_PARAM, -1.f, 1.f, 1.f, rangeName[0], " x", 0, 1);
        configParam<infNoiseAttRngQnt>(SCALE2_PARAM, -1.f, 1.f, 1.f, rangeName[1], " x", 0, 1);
        configParam<infNoiseAttRngQnt>(SCALE3_PARAM, -1.f, 1.f, 1.f, rangeName[2], " x", 0, 1);
        configParam<infNoiseAttRngQnt>(SCALE4_PARAM, -1.f, 1.f, 1.f, rangeName[3], " x", 0, 1);
        configParam<infNoiseAttRngQnt>(SCALE5_PARAM, -1.f, 1.f, 1.f, rangeName[4], " x", 0, 1);
        configParam<infNoiseAttRngQnt>(SCALE6_PARAM, -1.f, 1.f, 1.f, rangeName[5], " x", 0, 1);
        configParam<infNoiseAttRngQnt>(SCALE7_PARAM, -1.f, 1.f, 1.f, rangeName[6], " x", 0, 1);
        configParam<infNoiseAttRngQnt>(SCALE8_PARAM, -1.f, 1.f, 1.f, rangeName[7], " x", 0, 1);
        
        configOutput(POLY_OUTPUT, "Polyphonic");

        configLight(FIXED_CHANNEL_LIGHT, "Fixed polyphony if lit");
        configLight(MULTIPLE_SCALE_LIGHT, "Multiple scales warning if lit");

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
        
        doCycleAttRng = false;
        attRng.setBoth(infNoiseAttRngQnt::attRange::ar_1x);
        polyphony.setBoth(poly_auto);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        attRng.setBoth((infNoiseAttRngQnt::attRange)getJsonInt(rootJ, "attRng", (int)infNoiseAttRngQnt::attRange::ar_1x));
        polyphony.setBoth((polyphonyMode)getJsonInt(rootJ, "polyphony", (int)poly_auto));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "attRng", json_integer((int)attRng.req));
        json_object_set_new(rootJ, "polyphony", json_integer((int)polyphony.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        // Handle cycle of scale-factor range if applicable
        if (doCycleAttRng) {
            if (attRng.req < infNoiseAttRngQnt::attRange::ar_10x)
                attRng.setBoth((infNoiseAttRngQnt::attRange)((int)attRng.req + 1));
            else
                attRng.setBoth(infNoiseAttRngQnt::attRange::ar_1x);
            doCycleAttRng = false;
        }

        // Update attRng (scale-factor range) lights
        if (attRng.needsUpdate()) {
            attRng.updateActual();

            const float rangeFactor[4] = { 1.f, 2.f, 5.f, 10.f };
            attRngFactor = rangeFactor[(int)attRng.act];

            infNoiseAttRngQnt* attQty = dynamic_cast<infNoiseAttRngQnt*>(paramQuantities[ALL_SCALE_PARAM]);
            attQty->setRange(attRng.act, "All-scale");

            for (int i = 0; i < 8; i++) {
                infNoiseAttRngQnt* attQty = dynamic_cast<infNoiseAttRngQnt*>(paramQuantities[SCALE1_PARAM + i]);
                attQty->setRange(attRng.act, rangeName[i]);
            }

            lights[SCALE_1X_LIGHT].setBrightness(attRng.act == infNoiseAttRngQnt::attRange::ar_1x ? 1.f : 0.f);
            lights[SCALE_2X_LIGHT].setBrightness(attRng.act == infNoiseAttRngQnt::attRange::ar_2x ? 1.f : 0.f);
            lights[SCALE_5X_LIGHT].setBrightness(attRng.act == infNoiseAttRngQnt::attRange::ar_5x ? 1.f : 0.f);
            lights[SCALE_10X_LIGHT].setBrightness(attRng.act == infNoiseAttRngQnt::attRange::ar_10x ? 1.f : 0.f);
        }

        allTrim = params[ALL_SCALE_TRIM_PARAM].getValue();

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

        havePriSecScale = false;     
        bool usePriSecScale[8] = { false, false, false, false, false, false, false, false };
        for (int i = 0; i < 8; i++) {
            scales[i] = params[SCALE1_PARAM + i].getValue();
            usePriSecScale[i] = scales[i] != 1.f;
            if (usePriSecScale[i])
                havePriSecScale = true;
        }
        haveAllScale = params[ALL_SCALE_PARAM].getValue() != 1.f || 
            (inputs[ALL_SCALE_INPUT].isConnected() && allTrim != 0.f);  

        // Check for multiple overlapping scales (every 64th processParams = every 16384 cycle @48kHz)
        if ((proParCalls256 & 0x3F) == 0x00) { 
            const int groupOdd = 0;
            const int groupEven = 1;
            const int group14 = 2;
            const int group58 = 3;
            const int group912 = 4;
            const int group1316 = 5;
            const int group18 = 6;
            const int group916 = 7;
            bool haveMultipleScale = haveAllScale && havePriSecScale;
            if (!haveMultipleScale && !primaryMode && !haveMultipleScale && havePriSecScale) {
                usePriSecScale[groupOdd] = usePriSecScale[groupOdd] && channels >= 1;
                usePriSecScale[groupEven] = usePriSecScale[groupEven] && channels >= 2;
                usePriSecScale[group14] = usePriSecScale[group14] && channels >= 1;
                usePriSecScale[group58] = usePriSecScale[group58] && channels >= 5;
                usePriSecScale[group912] = usePriSecScale[group912] && channels >= 9;
                usePriSecScale[group1316] = usePriSecScale[group1316] && channels >= 13;
                usePriSecScale[group18] = usePriSecScale[group18] && channels >= 1;
                usePriSecScale[group916] = usePriSecScale[group916] && channels >= 9;
                if (usePriSecScale[group18] && (usePriSecScale[group14] || usePriSecScale[group58])) {
                    haveMultipleScale = true;
                }
                else if (usePriSecScale[group916] && (usePriSecScale[group912] || usePriSecScale[group1316])) {
                    haveMultipleScale = true;
                }
                else if (channels > 2) {
                    if ((usePriSecScale[groupOdd] || usePriSecScale[groupEven]) &&
                        ((usePriSecScale[group14] || usePriSecScale[group58]) ||
                        (usePriSecScale[group912] || usePriSecScale[group1316]) ||
                        (usePriSecScale[group18] || usePriSecScale[group916]))) {
                        haveMultipleScale = true;
                    }
                }
            }
            lights[MULTIPLE_SCALE_LIGHT].setBrightness((haveMultipleScale) ? 1.f : 0.f);
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

        if (doProcess) {           
            // Detect scale-mode button press (handled by processParams)
            if (scaleModeTrigger.process(params[SCALE_MODE_PARAM].getValue(), 0.1f, 0.9f))
                doCycleAttRng = true;

            if (haveOutput) {
                float allScaleParam = params[ALL_SCALE_PARAM].getValue();
                for (int c = 0; c < channels; c++) {
                    // All SCALE (supports polyphonic input)
                    float scale = allScaleParam;
                    if (inputs[ALL_SCALE_INPUT].isConnected()) {
                        scale += allTrim *
                            (inputs[ALL_SCALE_INPUT].getPolyVoltage(c) / 5.f);
                            scale = clamp(scale, -1.f, 1.f);
                    }

                    if (havePriSecScale) { // Only enter if there are PRI/SEC SCALEs
                        if (primaryMode) {
                            if (c < 4) {
                                if (c == 0)
                                    scale *= scales[0];
                                else if (c == 1)
                                    scale *= scales[1];
                                else if (c == 2)
                                    scale *= scales[2];
                                else if (c == 3)
                                    scale *= scales[3];
                            } else if (c < 8) {
                                if (c == 4)
                                    scale *= scales[4];
                                else if (c == 5)
                                    scale *= scales[5];
                                else if (c == 6)
                                    scale *= scales[6];
                                else if (c == 7)
                                    scale *= scales[7];
                            }
                        } else {  // Secondary mode
                            if (c % 2 == 0) // Odd (1st channel: c == 0)
                                scale *= scales[0];
                            else // Even (2nd channel: c == 1)
                                scale *= scales[1];
                            if (c < 4) // 1-4
                                scale *= scales[2];
                            else if (c < 8) // 5-8
                                scale *= scales[3];
                            else if (c < 12) // 9-12
                                scale *= scales[4];
                            else // 13-16
                                scale *= scales[5];
                            if (c < 8) // 1-8
                                scale *= scales[6];
                            else // 9-16
                                scale *= scales[7];
                        }
                    }

                    float voltage = (inputs[POLY_INPUT].isConnected())
                        ? inputs[POLY_INPUT].getPolyVoltage(c) * scale * attRngFactor
                        : 0.f;
                    voltage = quantizeToMode(voltage, outQuantize.act);
                    voltage = clipToVoltRange(voltage, outClipRange.act);
                    outputs[POLY_OUTPUT].setVoltage(voltage, c);
                }
            }
        }

        cycle256++;
    }
};

struct PolyScaleModuleWidget : InfNoiseModuleWidget {
    PolyScaleModuleWidget(PolyScaleModule *module) {
        initializeWidget(module, "res/PolyScale");

        const float clm1 = 14.810f;
        const float clm2 = 44.146f;
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(clm1, 51.428f), module, PolyScaleModule::POLY_INPUT));
        addChild(createLightCentered<TinyLight<RedLight>>(Vec(25.129f, 41.909f), module, PolyScaleModule::FIXED_CHANNEL_LIGHT));
        addParam(createParamCentered<CKSS>(Vec(37.303f, 51.428f), module, PolyScaleModule::MODE_PARAM));

        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(clm1, 89.330f), module, PolyScaleModule::ALL_SCALE_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(clm1, 112.858f), module, PolyScaleModule::ALL_SCALE_TRIM_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(clm1, 136.735f), module, PolyScaleModule::ALL_SCALE_INPUT));

        addParam(createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(clm2, 89.330f), module, PolyScaleModule::SCALE_MODE_PARAM));
        const float scaleModeLightCol = 38.984f;
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(scaleModeLightCol, 105.424f), module, PolyScaleModule::SCALE_1X_LIGHT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(scaleModeLightCol, 110.660f), module, PolyScaleModule::SCALE_2X_LIGHT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(scaleModeLightCol, 115.896f), module, PolyScaleModule::SCALE_5X_LIGHT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(scaleModeLightCol, 121.132f), module, PolyScaleModule::SCALE_10X_LIGHT));

        float row = 173.279f;
        float rowSpacing = 38.879f;
        for (int i = 0; i < 4; i++) {
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(clm1, row), module, PolyScaleModule::SCALE1_PARAM + i));
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(clm2, row), module, PolyScaleModule::SCALE5_PARAM + i));
            row += rowSpacing;
        }

        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(29.500f, 332.694f), module, PolyScaleModule::POLY_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        PolyScaleModule* module = dynamic_cast<PolyScaleModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> polyNames = getPolyphonyModeNames(true);
        menu->addChild(createIndexPtrSubmenuItem("Polyphony", polyNames,
            &module->polyphony.req));

        menu->addChild(createMenuItem("Set SCALE1-8 to 0", "", [=]() {
            for (int i = 0; i < 8; i++)
                module->params[PolyScaleModule::SCALE1_PARAM + i].setValue(0.f);
            }));
        menu->addChild(createMenuItem("Set SCALE1-8 to 1", "", [=]() {
            for (int i = 0; i < 8; i++)
                module->params[PolyScaleModule::SCALE1_PARAM + i].setValue(1.f / module->attRngFactor);
            }));
    
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelPolyScale = createModel<PolyScaleModule, PolyScaleModuleWidget>("PolyScale");