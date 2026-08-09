// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct PatchModule : InfNoiseModule {
    enum ParamId {
        NORM_PARAM,
        MUTE1_PARAM,
        MUTE2_PARAM,
        MUTE3_PARAM,
        MUTE4_PARAM,
        MUTE5_PARAM,
        MUTE6_PARAM,
        MUTE7_PARAM,
        MUTE8_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        PORT1_INPUT,
        PORT2_INPUT,
        PORT3_INPUT,
        PORT4_INPUT,
        PORT5_INPUT,
        PORT6_INPUT,
        PORT7_INPUT,
        PORT8_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        PORT1_OUTPUT,
        PORT2_OUTPUT,
        PORT3_OUTPUT,
        PORT4_OUTPUT,
        PORT5_OUTPUT,
        PORT6_OUTPUT,
        PORT7_OUTPUT,
        PORT8_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT, 2),
        LIGHTS_LEN
    };

    bool haveOutputs = false;
    int firstIdx = -1;
    int lastIdx = -1;
    bool isMuted[8] = { false, false, false, false, false, false, false, false };
    bool inputInUse[8] = { false, false, false, false, false, false, false, false };
    bool outputInUse[8] = { false, false, false, false, false, false, false, false };
    actReqValue<voltValue> muteVoltage = actReqValue<voltValue>(v_zero);
    float muteVolt[PORT_MAX_CHANNELS] = { 0.f };

    PatchModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configSwitch(NORM_PARAM, 0.0f, 2.0f, 0.0f, "Normalization mode", {"ON", "Mute (OFF when muted, else ON)", "OFF"});
        for (int i = 0; i < 8; i++) {
            configInput(PORT1_INPUT + i, string::f("Port-%d", i + 1));
            configOutput(PORT1_OUTPUT + i, string::f("Port-%d", i + 1));
            configSwitch(MUTE1_PARAM + i, 0.0f, 1.0f, 0.0f, string::f("Mute input-%d", i + 1), { "Unmuted", "Muted" });
        }

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
        muteVoltage.setBoth(v_zero);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        muteVoltage.setBoth((voltValue)getJsonInt(rootJ, "muteVoltage", (int)v_zero));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "muteVoltage", json_integer((int)muteVoltage.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        haveOutputs = false;
        firstIdx = -1;
        lastIdx = -1;
        for (int i = 0; i < 8; i++) {
            isMuted[i] = params[MUTE1_PARAM + i].value > 0.5f;
            inputInUse[i] = inputs[PORT1_INPUT + i].isConnected();
            outputInUse[i] = outputs[PORT1_OUTPUT + i].isConnected();
            
            if (outputInUse[i])
                haveOutputs = true;
            
            if (inputInUse[i] || outputInUse[i]) {
                if (firstIdx < 0) firstIdx = i;
                lastIdx = i;
            }
        }

        if (muteVoltage.needsUpdate()) {
            muteVoltage.updateActual();
            for (int i = 0; i < PORT_MAX_CHANNELS; i++) {
                muteVolt[i] = voltValues[muteVoltage.act];
            }
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
            float normVolt[PORT_MAX_CHANNELS] = { 0.f };
            float normMode = params[NORM_PARAM].value;
            int channels = 1;
            for (int i = firstIdx; i <= lastIdx; i++) {
                if (inputInUse[i] || outputInUse[i]) {
                    if (inputInUse[i]) {
                        channels = std::max(inputs[PORT1_INPUT + i].getChannels(), 1);
                        inputs[PORT1_INPUT + i].readVoltages(normVolt);
                    }

                    if (outputInUse[i]) {
                        outputs[PORT1_OUTPUT + i].setChannels(channels);
                        if (isMuted[i]) {
                            outputs[PORT1_OUTPUT + i].writeVoltages(muteVolt);
                        }
                        else { // Not muted
                            for (int c = 0; c < channels; c++) {
                                outputs[PORT1_OUTPUT + i].setVoltage(clipToVoltRange(normVolt[c], outClipRange.act), c);
                            }
                        }
                    }
                }

                // Check if normalization should be cleared
                bool clearNorm = (normMode > 1.5f) ||
                    (normMode > 0.5f && normMode < 1.5f && isMuted[i]);
                if (clearNorm) {
                    channels = 1;
                    normVolt[0] = 0.f; // no need to clear all channels
                }
            }
        }

        cycle256++;
    }
};

struct PatchModuleWidget : InfNoiseModuleWidget {
    PatchModuleWidget(PatchModule *module) {
        initializeWidget(module, "res/Patch");

        const float normRow = 44.555f;
        addParam(createParamCentered<CKSSThree>(Vec(24.791f, normRow), module, PatchModule::NORM_PARAM));

        float portRow = 87.179f;
        float butRow = 72.167f;
        const float rowSpacing = 35.0878f;
        for (int i = 0; i < 8; i++) {
            // 1 to 8
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(14.810f, portRow), module, PatchModule::PORT1_INPUT + i));
            addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(44.389f, portRow), module, PatchModule::PORT1_OUTPUT + i));

            addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(23.419f, butRow), module, PatchModule::MUTE1_PARAM + i));

            portRow += rowSpacing;
            butRow += rowSpacing;
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        PatchModule* module = dynamic_cast<PatchModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> voltNames = getVoltValuesNames();
        menu->addChild(createIndexPtrSubmenuItem("Mute voltage", voltNames,
            &module->muteVoltage.req));

        menu->addChild(createSubmenuItem("Set mute", "", [=](Menu* menu) {
            menu->addChild(createMenuItem("ON", "", [=]() {
                for (int i=0; i<8; i++)
                    module->params[PatchModule::MUTE1_PARAM + i].setValue(1.f);
                }));
            menu->addChild(createMenuItem("OFF", "", [=]() {
                for (int i=0; i<8; i++)
                    module->params[PatchModule::MUTE1_PARAM + i].setValue(0.f);
                }));
        }));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelPatch = createModel<PatchModule, PatchModuleWidget>("Patch");