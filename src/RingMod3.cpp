// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct RingMod3Module : InfNoiseModule {
    enum ParamId {
        //SOME_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        A1_INPUT,
        A2_INPUT,
        A3_INPUT,
        B1_INPUT,
        B2_INPUT,
        B3_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        AB1_OUTPUT,
        AB2_OUTPUT,
        AB3_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT, 2),
        ENUMS(CLIP_RANGE_LIGHT, 2),
        ENUMS(A1_SCALE_LIGHT, 2),
        ENUMS(A2_SCALE_LIGHT, 2),
        ENUMS(A3_SCALE_LIGHT, 2),
        ENUMS(B1_SCALE_LIGHT, 2),
        ENUMS(B2_SCALE_LIGHT, 2),
        ENUMS(B3_SCALE_LIGHT, 2),
        ENUMS(A1_NEG_LIGHT, 3),
        ENUMS(A2_NEG_LIGHT, 3),
        ENUMS(A3_NEG_LIGHT, 3),
        ENUMS(B1_NEG_LIGHT, 3),
        ENUMS(B2_NEG_LIGHT, 3),
        ENUMS(B3_NEG_LIGHT, 3),
        LIGHTS_LEN
    };

    bool haveOutputs = false;
    int firstIdx = -1;
    int lastIdx = -1;
    int channels[3] = { 1, 1, 1 }; // Number of output-channels (AB1, AB2, AB3)
    enum scaleModeType { sm_1x, sm_d5, sm_d10 };
    const float scaleModeFactors[3] = { 1.f, 0.2f, 0.1f };
    float scaleModeRed[3] = { 0.f, 1.f, 0.f };
    float scaleModeGreen[3] = { 0.f, 1.f, 1.f };
    float negModeRed[3] = { 0.f, 0.f, 1.f };
    float negModeGreen[3] = { 0.f, 0.f, 0.f };
    float negModeBlue[3] = { 0.f, 1.f, 0.f };
    actReqValue<scaleModeType> aScaleMode[3] = { 
        actReqValue<scaleModeType>(sm_1x), 
        actReqValue<scaleModeType>(sm_1x), 
        actReqValue<scaleModeType>(sm_1x) 
    };
    actReqValue<scaleModeType> bScaleMode[3] = { 
        actReqValue<scaleModeType>(sm_d10), 
        actReqValue<scaleModeType>(sm_d10), 
        actReqValue<scaleModeType>(sm_d10) 
    };
    enum negModeType { nm_sign, nm_abs, nm_cut };
    actReqValue<negModeType> aNegMode[3] = { 
        actReqValue<negModeType>(nm_sign), 
        actReqValue<negModeType>(nm_sign), 
        actReqValue<negModeType>(nm_sign) 
    };
    actReqValue<negModeType> bNegMode[3] = { 
        actReqValue<negModeType>(nm_sign), 
        actReqValue<negModeType>(nm_sign), 
        actReqValue<negModeType>(nm_sign) 
    };

    
	RingMod3Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
        
        for (int i = 0; i < 3; i++) {
            std::string aNorm = (i > 0) ? " (normalized to to prev AB-output)" : " (normalized to 0V)";
            configInput(A1_INPUT + i, string::f("A%d"+aNorm, i + 1));
            std::string bNorm = (i > 0) ? " (normalized to prev B-input)" : " (normalized to A1)";
            configInput(B1_INPUT + i, string::f("B%d"+bNorm, i + 1));
            std::string abNorm  = (i < 2) ? " (next A-input will normalized to this AB-output)" : "";
            configOutput(AB1_OUTPUT + i, string::f("AB%d"+abNorm, i + 1));
            
            int scaleLightIdx = i * 2;
            configLight(A1_SCALE_LIGHT + scaleLightIdx, string::f("A%d Scale (unlit=1x, yellow=/5, green=/10)", i + 1));
            configLight(B1_SCALE_LIGHT + scaleLightIdx, string::f("B%d Scale (unlit=1x, yellow=/5, green=/10)", i + 1));

            int negLightIdx = i * 3;
            configLight(A1_NEG_LIGHT + negLightIdx, string::f("A%d Neg (unlit=sign, blue=abs, red=cut)", i + 1));
            configLight(B1_NEG_LIGHT + negLightIdx, string::f("B%d Neg (unlit=sign, blue=abs, red=cut)", i + 1));
        }

        configBypass(A1_INPUT, AB1_OUTPUT);
        configBypass(A2_INPUT, AB2_OUTPUT);
        configBypass(A3_INPUT, AB3_OUTPUT);

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

        for (int i = 0; i < 3; i++) {
            aScaleMode[i].setBoth(sm_1x);
            bScaleMode[i].setBoth(sm_d10);
            aNegMode[i].setBoth(nm_sign);
            bNegMode[i].setBoth(nm_sign);
        }
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        

        int aScaleTmp[3];
        int bScaleTmp[3];
        int aNegTmp[3];
        int bNegTmp[3];
        getJsonIntArray(rootJ, "aScaleMode", aScaleTmp, 3, (int)sm_1x);
        getJsonIntArray(rootJ, "bScaleMode", bScaleTmp, 3, (int)sm_d10);
        getJsonIntArray(rootJ, "aNegMode", aNegTmp, 3, (int)nm_sign);
        getJsonIntArray(rootJ, "bNegMode", bNegTmp, 3, (int)nm_sign);
        for (int i = 0; i < 3; i++) {
            aScaleMode[i].setBoth((scaleModeType)aScaleTmp[i]);
            bScaleMode[i].setBoth((scaleModeType)bScaleTmp[i]);
            aNegMode[i].setBoth((negModeType)aNegTmp[i]);
            bNegMode[i].setBoth((negModeType)bNegTmp[i]);
        }
    }

    void dataToJson(json_t* rootJ) override {
        int aScaleTmp[3];
        int bScaleTmp[3];
        int aNegTmp[3];
        int bNegTmp[3];
        for (int i = 0; i < 3; i++) {
            aScaleTmp[i] = (int)aScaleMode[i].req;
            bScaleTmp[i] = (int)bScaleMode[i].req;
            aNegTmp[i] = (int)aNegMode[i].req;
            bNegTmp[i] = (int)bNegMode[i].req;
        }
        setJsonIntArray(rootJ, "aScaleMode", aScaleTmp, 3);
        setJsonIntArray(rootJ, "bScaleMode", bScaleTmp, 3);
        setJsonIntArray(rootJ, "aNegMode", aNegTmp, 3);
        setJsonIntArray(rootJ, "bNegMode", bNegTmp, 3);
    }

    inline void setScaleModeLight(int lightId, scaleModeType mode) {
        lights[lightId].setBrightness(scaleModeGreen[(int)mode]);
        lights[lightId + 1].setBrightness(scaleModeRed[(int)mode]);
    }

    inline void setNegModeLight(int lightId, negModeType mode) {
        lights[lightId].setBrightness(negModeRed[(int)mode]);
        lights[lightId + 1].setBrightness(negModeGreen[(int)mode]);
        lights[lightId + 2].setBrightness(negModeBlue[(int)mode]);
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        firstIdx = -1;
        lastIdx = -1;
        int outputCount = 0; // Number of sections with outputs connected
        int prevOutChannels = 1; // A-input normalized to prev AB-output
        int prevInChannels = 1; // B-input normalized to prev B-input
        for (int i = 0; i < 3; i++) {
            // Detect sections in use
            bool aIn = inputs[A1_INPUT + i].isConnected();
            bool bIn = inputs[B1_INPUT + i].isConnected();
            bool abOut = outputs[AB1_OUTPUT + i].isConnected();
            if (aIn || bIn || abOut) {
                if (firstIdx < 0)
                    firstIdx = i;
                lastIdx = i;

                if (abOut) outputCount++;
            }

            // Find max channels- in each section
            int aChannels = aIn 
                ? std::max(inputs[A1_INPUT + i].getChannels(), 1) 
                : prevOutChannels;
            int bChannels = bIn 
                ? std::max(inputs[B1_INPUT + i].getChannels(), 1) 
                : prevInChannels;
            prevInChannels = bChannels;

            // Set output channels for each section
            channels[i] = std::max(aChannels, bChannels);
            outputs[AB1_OUTPUT + i].setChannels(channels[i]);
            prevOutChannels = channels[i];

            // Update scale/neg-mode lights (if necessary)
            int scaleLightIdx = i * 2;
            if (aScaleMode[i].needsUpdate()) {
                aScaleMode[i].updateActual();
                setScaleModeLight(A1_SCALE_LIGHT + scaleLightIdx, aScaleMode[i].act);
            }
            if (bScaleMode[i].needsUpdate()) {
                bScaleMode[i].updateActual();
                setScaleModeLight(B1_SCALE_LIGHT + scaleLightIdx, bScaleMode[i].act);
            }

            int negLightIdx = i * 3;
            if (aNegMode[i].needsUpdate()) {
                aNegMode[i].updateActual();
                setNegModeLight(A1_NEG_LIGHT + negLightIdx, aNegMode[i].act);
            }
            if (bNegMode[i].needsUpdate()) {
                bNegMode[i].updateActual();
                setNegModeLight(B1_NEG_LIGHT + negLightIdx, bNegMode[i].act);
            }
        }

        haveOutputs = outputCount > 0;

        //--------------------
        postProcessParams(args);
    }

    inline float adjustInputToModes(float input, scaleModeType scaleMode, negModeType negMode) {
        // Apply scale mode
        input *= scaleModeFactors[(int)scaleMode];

        // Apply neg mode
        if (input < 0.f) {
            if (negMode == nm_abs)
                input = fabsf(input);
            else if (negMode == nm_cut)
                input = 0.f;
        }

        return input;
    }

    void process(const ProcessArgs& args) override {
        bool doProcessParams = mustProcessParams || 
            ((cycle256 & patternProcessParams) == patternProcessParams);
        if (doProcessParams)
            processParams(args);

        bool doProcess = (doProcessParams ||
            ((cycle256 & processQualityPatterns[procQuality.act]) == processQualityPatterns[procQuality.act]));

        if (doProcess && haveOutputs) {
            float normA[PORT_MAX_CHANNELS] = { 0.f }; // Normalized A-input (AB-output from prev. section)
            float normB[PORT_MAX_CHANNELS] = { 0.f }; // Normalized B-input (B-input from prev. section)

            int prevNormAChannels = 1; // To make nomalization "work like" getPolyVoltage
            int prevNormBChannels = 1; // To make nomalization "work like" getPolyVoltage
            for (int i = firstIdx; i <= lastIdx; i++) {
                bool aIn = inputs[A1_INPUT + i].isConnected();
                bool bIn = inputs[B1_INPUT + i].isConnected();
                int bChannels = bIn 
                    ? std::max(inputs[B1_INPUT + i].getChannels(), 1) 
                    : prevNormBChannels;

                for (int c = 0; c < channels[i]; c++) {
                    // A-input
                    float aRawVolt = aIn 
                        ? inputs[A1_INPUT + i].getPolyVoltage(c) 
                        : (prevNormAChannels == 1) ? normA[0] : normA[c];
                    float aVolt = adjustInputToModes(aRawVolt, aScaleMode[i].act, aNegMode[i].act);

                    // B-input
                    float bVolt = bIn
                        ? inputs[B1_INPUT + i].getPolyVoltage(c) 
                        : (i == 0)
                            ? aRawVolt                            
                            : (bChannels == 1) ? normB[0] : normB[c];
                    normB[c] = bVolt; // Store raw B-input for next section
                    bVolt = adjustInputToModes(bVolt, bScaleMode[i].act, bNegMode[i].act);

                    // AB-output
                    float abVolt = aVolt * bVolt;
                    abVolt = quantizeToMode(abVolt, outQuantize.act);
                    abVolt = clipToVoltRange(abVolt, outClipRange.act);
                    outputs[AB1_OUTPUT + i].setVoltage(abVolt, c);
                    normA[c] = abVolt; // Store AB-output for next section's normalized A-input
                }

                prevNormAChannels = channels[i];
                prevNormBChannels = bChannels;
            }
        }

        cycle256++;
    }
};

struct RingMod3ModuleWidget : InfNoiseModuleWidget {
    RingMod3ModuleWidget(RingMod3Module *module) {
        initializeWidget(module, "res/RingMod3");
        
        const float centerCol = 15.f;
        const float lightCol = 5.060f;
        const float light1Offset = 10.318f;
        const float light2Offset = light1Offset + 3.967f;
        const float bOffset = 35.258f; // Offset of B-input from A-input
        const float abOffset = 70.516f; // Offset of AB-output from A-input
        const float sectionSpacing = 105.774f;
        float baseRow = 51.383f;
        for (int i = 0; i < 3; i++) {
            int scaleLightIdx = i * 2;
            int negLightIdx = i * 3;
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, baseRow), module, RingMod3Module::A1_INPUT + i));
            addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(lightCol, baseRow + light1Offset), module, RingMod3Module::A1_SCALE_LIGHT + scaleLightIdx));
            addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(Vec(lightCol, baseRow + light2Offset), module, RingMod3Module::A1_NEG_LIGHT + negLightIdx));
            
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, baseRow + bOffset), module, RingMod3Module::B1_INPUT + i));
            addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(lightCol, baseRow + bOffset + light1Offset), module, RingMod3Module::B1_SCALE_LIGHT + scaleLightIdx));
            addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(Vec(lightCol, baseRow + bOffset + light2Offset), module, RingMod3Module::B1_NEG_LIGHT + negLightIdx));
            
            addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, baseRow + abOffset), module, RingMod3Module::AB1_OUTPUT + i));

            baseRow += sectionSpacing;
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        RingMod3Module* module = dynamic_cast<RingMod3Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        // Per-input menus for A1/B1, A2/B2, A3/B3
        std::vector<std::string> scaleModeNames = {"1x (no scaling)", "Divide by 5 (/5)", "Divide by 10 (/10)"};
        std::vector<std::string> negModeNames = {"Keep sign", "Absolute (|x|)", "Cut negatives (0V)"};
        for (int i = 0; i < 3; i++) {
            // A-input i+1
            menu->addChild(createSubmenuItem(string::f("A%d input", i + 1), "",
                [=](Menu* submenu) {
                    submenu->addChild(createIndexPtrSubmenuItem("Scale-mode", scaleModeNames,
                        &module->aScaleMode[i].req));
                    submenu->addChild(createIndexPtrSubmenuItem("Negative-mode", negModeNames,
                        &module->aNegMode[i].req));
                }
            ));

            // B-input i+1
            menu->addChild(createSubmenuItem(string::f("B%d input", i + 1), "",
                [=](Menu* submenu) {
                    submenu->addChild(createIndexPtrSubmenuItem("Scale-mode", scaleModeNames,
                        &module->bScaleMode[i].req));
                    submenu->addChild(createIndexPtrSubmenuItem("Negative-mode", negModeNames,
                        &module->bNegMode[i].req));
                }
            ));
        }

        // All A inputs
        menu->addChild(new MenuSeparator);
        menu->addChild(createSubmenuItem("Set all A inputs", "",
            [=](Menu* allAMenu) {
                allAMenu->addChild(createSubmenuItem("Scale-mode", "",
                    [=](Menu* submenu) {
                        for (int mode = 0; mode < 3; mode++) {
                            submenu->addChild(createMenuItem(scaleModeNames[mode], "",
                                [=]() {
                                    for (int i = 0; i < 3; i++)
                                        module->aScaleMode[i].req = (RingMod3Module::scaleModeType)mode;
                                }
                            ));
                        }
                    }
                ));

                allAMenu->addChild(createSubmenuItem("Negative-mode", "",
                    [=](Menu* submenu) {
                        for (int mode = 0; mode < 3; mode++) {
                            submenu->addChild(createMenuItem(negModeNames[mode], "",
                                [=]() {
                                    for (int i = 0; i < 3; i++)
                                        module->aNegMode[i].req = (RingMod3Module::negModeType)mode;
                                }
                            ));
                        }
                    }
                ));
            }
        ));

        // All B inputs
        menu->addChild(createSubmenuItem("Set all B inputs", "",
            [=](Menu* allBMenu) {
                allBMenu->addChild(createSubmenuItem("Scale-mode", "",
                    [=](Menu* submenu) {
                        for (int mode = 0; mode < 3; mode++) {
                            submenu->addChild(createMenuItem(scaleModeNames[mode], "",
                                [=]() {
                                    for (int i = 0; i < 3; i++)
                                        module->bScaleMode[i].req = (RingMod3Module::scaleModeType)mode;
                                }
                            ));
                        }
                    }
                ));

                allBMenu->addChild(createSubmenuItem("Negative-mode", "",
                    [=](Menu* submenu) {
                        for (int mode = 0; mode < 3; mode++) {
                            submenu->addChild(createMenuItem(negModeNames[mode], "",
                                [=]() {
                                    for (int i = 0; i < 3; i++)
                                        module->bNegMode[i].req = (RingMod3Module::negModeType)mode;
                                }
                            ));
                        }
                    }
                ));
            }
        ));

        // All A/B inputs
        menu->addChild(createSubmenuItem("Set all A/B inputs", "",
            [=](Menu* allABMenu) {
                allABMenu->addChild(createSubmenuItem("Scale-mode", "",
                    [=](Menu* submenu) {
                        for (int mode = 0; mode < 3; mode++) {
                            submenu->addChild(createMenuItem(scaleModeNames[mode], "",
                                [=]() {
                                    for (int i = 0; i < 3; i++) {
                                        module->aScaleMode[i].req = (RingMod3Module::scaleModeType)mode;
                                        module->bScaleMode[i].req = (RingMod3Module::scaleModeType)mode;
                                    }
                                }
                            ));
                        }
                    }
                ));

                allABMenu->addChild(createSubmenuItem("Negative-mode", "",
                    [=](Menu* submenu) {
                        for (int mode = 0; mode < 3; mode++) {
                            submenu->addChild(createMenuItem(negModeNames[mode], "",
                                [=]() {
                                    for (int i = 0; i < 3; i++) {
                                        module->aNegMode[i].req = (RingMod3Module::negModeType)mode;
                                        module->bNegMode[i].req = (RingMod3Module::negModeType)mode;
                                    }
                                }
                            ));
                        }
                    }
                ));
            }
        ));
    
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelRingMod3 = createModel<RingMod3Module, RingMod3ModuleWidget>("RingMod3");