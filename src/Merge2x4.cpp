// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct Merge2x4Module : InfNoiseModule {
    enum ParamId {
        MERGEMODEA_PARAM,
        MERGEMODEB_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        MERGEA1_INPUT,
        MERGEA2_INPUT,
        MERGEA3_INPUT,
        MERGEA4_INPUT,
        MERGEB1_INPUT,
        MERGEB2_INPUT,
        MERGEB3_INPUT,
        MERGEB4_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        MERGEA_OUTPUT,
        MERGEB_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT, 2),
        ENUMS(NEGMODEA_LIGHT, 2),
        ENUMS(NEGMODEB_LIGHT, 2),
        LIGHTS_LEN
    };

    bool haveInputs[8] = { false, false, false, false, false, false, false, false };
    bool haveOutputs[2] = { false, false };
    bool mixMode[2] = { false, false };
    int channels[2] = { 1, 1 };
    int inPortCount[2] = { 1, 1 };
    enum negModeType { nm_neg, nm_zero, nm_absIn, nm_absOut };
    actReqValue<negModeType> negMode[2] = { 
        actReqValue<negModeType>(nm_neg), 
        actReqValue<negModeType>(nm_neg) 
    };
    float negModeGreen[4] = { 0.f, 1.f, 1.f, 0.f };
    float negModeRed[4] = { 0.f, 0.f, 1.f, 1.f };
    
    Merge2x4Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configSwitch(MERGEMODEA_PARAM, 0.0f, 1.0f, 0.0f,"Mode-A", { "Merge/Sum", "Mix/Average" });
        configInput(MERGEA1_INPUT, "Merge-A1");
        configInput(MERGEA2_INPUT, "Merge-A2");
        configInput(MERGEA3_INPUT, "Merge-A3");
        configInput(MERGEA4_INPUT, "Merge-A4");
        configOutput(MERGEA_OUTPUT, "Merge-A (Merge/Sum or Mix/Average)");
        configLight(NEGMODEA_LIGHT, "Negative-A: Unlit=Signed, Green=Zero, Yellow=Abs-in, Red=Abs-out");

        configSwitch(MERGEMODEB_PARAM, 0.0f, 1.0f, 0.0f,"Mode-B", { "Merge/Sum", "Mix/Average" });
        configInput(MERGEB1_INPUT, "Merge-B1");
        configInput(MERGEB2_INPUT, "Merge-B2");
        configInput(MERGEB3_INPUT, "Merge-B3");
        configInput(MERGEB4_INPUT, "Merge-B4");
        configOutput(MERGEB_OUTPUT, "Merge-B (Merge/Sum or Mix/Average)");
        configLight(NEGMODEB_LIGHT, "Negative-B: Unlit=Signed, Green=Zero, Yellow=Abs-in, Red=Abs-out");

        configBypass(MERGEA1_INPUT, MERGEA_OUTPUT);
        configBypass(MERGEB1_INPUT, MERGEB_OUTPUT);

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
        negMode[0].setBoth(nm_neg);
        negMode[1].setBoth(nm_neg);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);

        negMode[0].setBoth((negModeType)getJsonInt(rootJ, "negMode0", nm_neg));
        negMode[1].setBoth((negModeType)getJsonInt(rootJ, "negMode1", nm_neg));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "negMode0", json_integer((int)negMode[0].req));
        json_object_set_new(rootJ, "negMode1", json_integer((int)negMode[1].req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        for (int i = 0; i < 2; i++) {
            // Negative-mode/lights
            if (negMode[i].needsUpdate()) {
                negMode[i].updateActual();
                int lgtIdx = i * 2;
                lights[NEGMODEA_LIGHT + lgtIdx].setBrightness(negModeGreen[negMode[i].act]);
                lights[NEGMODEA_LIGHT + lgtIdx + 1].setBrightness(negModeRed[negMode[i].act]);
            }

            // Input related
            mixMode[i] = params[MERGEMODEA_PARAM + i].getValue() > 0.5f;
            channels[i] = 1;
            inPortCount[i] = 0;
            for (int j = 0; j < 4; j++) {
                int inPortIdx = i * 4 + j;
                haveInputs[inPortIdx] = inputs[MERGEA1_INPUT + inPortIdx].isConnected();
                if (haveInputs[inPortIdx]) {
                    inPortCount[i]++;
                    channels[i] = std::max(channels[i], inputs[MERGEA1_INPUT + inPortIdx].getChannels());
                }
            }

            // Output related
            haveOutputs[i] = outputs[MERGEA_OUTPUT + i].isConnected();
            if (!haveOutputs[i]) {
                outputs[MERGEA_OUTPUT + i].setVoltage(0.f);
            }
            outputs[MERGEA_OUTPUT + i].setChannels(channels[i]);
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

        if (doProcess && (haveOutputs[0] || haveOutputs[1])) {
            for (int i = 0; i < 2; i++) {
                if (haveOutputs[i]) {
                    for (int c = 0; c < channels[i]; c++) {
                        float mergeSum = 0.f;
                        for (int j = 0; j < 4; j++) {
                            int inPortIdx = i * 4 + j;
                            if (haveInputs[inPortIdx])
                            {
                                float voltage = inputs[MERGEA1_INPUT + inPortIdx].getPolyVoltage(c);
                                if (voltage < 0.f) {
                                    if (negMode[i].act == nm_zero)
                                        voltage = 0.f;
                                    else if (negMode[i].act == nm_absIn)
                                        voltage = -voltage;
                                }                           
                                mergeSum += voltage;
                            }
                        }

                        if (negMode[i].act == nm_absOut)
                            mergeSum = std::abs(mergeSum);
                        if (mixMode[i] && inPortCount[i] > 0)
                            mergeSum /= inPortCount[i];
                        mergeSum = quantizeToMode(mergeSum, outQuantize.act);
                        mergeSum = clipToVoltRange(mergeSum, outClipRange.act);
                        outputs[MERGEA_OUTPUT + i].setVoltage(mergeSum, c);
                    }
                }
            }
        }

        cycle256++;
    }
};

struct Merge2x4ModuleWidget : InfNoiseModuleWidget {
    Merge2x4ModuleWidget(Merge2x4Module *module) {
        initializeWidget(module, "res/Merge2x4");

        const float centerCol = 15.f;
        const float rowSpacing = 29.9425f;
        const float sectionSpacing = 41.025f;
        const float lgtOfs = -9.518f;
        const float modeColOfs = 9.742f;
        const float modeRowOfs = -13.222f;

        // Mult-A/B inputs/outputs
        float row = 52.106f;
        for (int i = 0; i < 2; i++) {
            addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(Vec(centerCol + modeColOfs, row + modeRowOfs), module, Merge2x4Module::MERGEMODEA_PARAM + i));
                    
            for (int j = 0; j < 4; j++) {
                int inIdx = i * 4 + j;
                addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, row), module, Merge2x4Module::MERGEA1_INPUT + inIdx));
                row += rowSpacing;
            }

            addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, row), module, Merge2x4Module::MERGEA_OUTPUT + i));
            int lgtIdx = i * 2;
            addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(centerCol + lgtOfs, row + lgtOfs), module, Merge2x4Module::NEGMODEA_LIGHT + lgtIdx));

            row += sectionSpacing;
        }
    }

    void appendContextMenu(Menu* menu) override {
        Merge2x4Module* module = dynamic_cast<Merge2x4Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> negModeNames = { "Signed (default)", "Zero", "Absolute (input)", "Absolute (output)" };
        menu->addChild(createIndexPtrSubmenuItem("A-Negative mode", negModeNames,
            &module->negMode[0].req));
        menu->addChild(createIndexPtrSubmenuItem("B-Negative mode", negModeNames,
            &module->negMode[1].req));   

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelMerge2x4 = createModel<Merge2x4Module, Merge2x4ModuleWidget>("Merge2x4");