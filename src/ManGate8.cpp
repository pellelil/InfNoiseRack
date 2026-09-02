// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inUtil.hpp"

struct ManGate8Module : InfNoiseModule {
    enum ParamId {
        GATE_ALL_PARAM,
        GATE1_PARAM,
        GATE2_PARAM,
        GATE3_PARAM,
        GATE4_PARAM,
        GATE5_PARAM,
        GATE6_PARAM,
        GATE7_PARAM,
        GATE8_PARAM,
        GATE_ALL_LATCH_PARAM,
        GATE1_LATCH_PARAM,
        GATE2_LATCH_PARAM,
        GATE3_LATCH_PARAM,
        GATE4_LATCH_PARAM,
        GATE5_LATCH_PARAM,
        GATE6_LATCH_PARAM,
        GATE7_LATCH_PARAM,
        GATE8_LATCH_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        //SOME_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        GATE_POLY_OUTPUT,
        GATE1_OUTPUT,
        GATE2_OUTPUT,
        GATE3_OUTPUT,
        GATE4_OUTPUT,
        GATE5_OUTPUT,
        GATE6_OUTPUT,
        GATE7_OUTPUT,
        GATE8_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        POLY_HINT_LIGHT,
        LIGHTS_LEN
    };

    enum allButtonMode { abm_Off, abm_On, abm_Toggle };
    actReqValue<allButtonMode> allMode = actReqValue<allButtonMode>(abm_Toggle);
    bool haveOutputs = false;
    actReqValue<polyphonyMode> polyOutput = actReqValue<polyphonyMode>(poly_8);
    int polyChannels = 8;
    infNoiseButtonTrigger btAll = infNoiseButtonTrigger();
    
    ManGate8Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
        configLight(POLY_HINT_LIGHT, "Poly output: lit when channel count is not 8");

        configSwitch(GATE_ALL_PARAM, 0.0f, 1.0f, 0.0f, "Gate-All", { "Off", "On" });
        configSwitch(GATE_ALL_LATCH_PARAM, 0.0f, 1.0f, 0.0f, "Latch Gate-All", { "Not latched", "Latched" });
        configOutput(GATE_POLY_OUTPUT, "Polyphonic gates");
        for (int i = 0; i < 8; i++) {
            configSwitch(GATE1_PARAM + i, 0.0f, 1.0f, 0.0f, string::f("Gate-button %d", i + 1), { "Low", "High" });
            configSwitch(GATE1_LATCH_PARAM + i, 0.0f, 1.0f, 0.0f, string::f("Latch Gate-button %d", i + 1), { "Unlatched", "Latched" });
            configOutput(GATE1_OUTPUT + i, string::f("Gate %d", i + 1));
        }

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
        haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = false;
        haveGateDetect = false;
		haveGateHighLow = true;
		haveTrigDetect = false;
		haveTrigHighLow = false;
        outClipRange.setBoth(vr_off);  // By default on, but output can't exceed -+12V
        procQuality.setBoth(pq_balancedRate); // Ballanced rate (every 16th cycle)
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        
        outClipRange.setBoth(vr_off);  // By default on, but output can't exceed -+12V
        procQuality.setBoth(pq_balancedRate); // Ballanced rate (every 16th cycle)

        allMode.setBoth(abm_Toggle);
        btAll.reset();
        polyOutput.setBoth(poly_8);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        allMode.setBoth((allButtonMode)getJsonInt(rootJ, "allButtonMode", (int)allButtonMode::abm_Toggle));
        polyOutput.setBoth((polyphonyMode)getJsonInt(rootJ, "polyOutput",
            getJsonInt(rootJ, "anyPoly", (int)polyphonyMode::poly_8)));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "allButtonMode", json_integer((int)allMode.req));
        json_object_set_new(rootJ, "polyOutput", json_integer((int)polyOutput.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        // Handle All-button
        allMode.updateActual();
        if (btAll.process(params[GATE_ALL_PARAM].getValue() > 0.5f)) {
            // only once when it fires
            if (btAll.isPressed()) {
                if (allMode.act == abm_Toggle) {
                    for (int i = 0; i < 8; i++) {
                        float buttonValue = (params[GATE1_PARAM + i].getValue() > 0.5f) ? 0.0f : 1.0f;
                        params[GATE1_PARAM + i].setValue(buttonValue);
                    }
                }
            }
            else { // release non-latched buttons
                for (int i = 0; i < 8; i++) {
                    if (params[GATE1_LATCH_PARAM + i].getValue() < 0.5f)
                        params[GATE1_PARAM + i].setValue(0.f);
                }
            }
        }
        // Always
        if (btAll.isPressed()) {
            if (allMode.act == abm_On) {
                for (int i = 0; i < 8; i++) {
                    params[GATE1_PARAM + i].setValue(1.0f);
                }
            }
            else if (allMode.act == abm_Off) {
                for (int i = 0; i < 8; i++) {
                    params[GATE1_PARAM + i].setValue(0.f);
                }
            }
        }

        haveOutputs = outputs[GATE_POLY_OUTPUT].isConnected();
        if (polyOutput.needsUpdate()) {
            polyOutput.updateActual();
            lights[POLY_HINT_LIGHT].setBrightness(polyOutput.req != poly_8 ? 1.f : 0.f);
            if (outputInfos.size() > (unsigned)GATE_POLY_OUTPUT && outputInfos[GATE_POLY_OUTPUT]) {
                outputInfos[GATE_POLY_OUTPUT]->name = polyPortPrefix() + "Poly output: " + getPolyphonyModeName(polyOutput.act);
            }
        }
        polyChannels = polyphonyModeChannels[polyOutput.act];
        outputs[GATE_POLY_OUTPUT].setChannels(polyChannels);
        for (int i = 0; i < 8; i++) {
            if (outputs[GATE1_OUTPUT + i].isConnected())
				haveOutputs = true;
            outputs[GATE1_OUTPUT + i].setChannels(1);
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
            for (int i = 0; i < 8; i++) {
                float voltage = (params[GATE1_PARAM + i].getValue() > 0.5f) 
                    ? voltValues[gateOutHigh.act]
                    : voltValues[gateOutLow.act];
                outputs[GATE1_OUTPUT + i].setVoltage(voltage);
                if (i < polyChannels)
                    outputs[GATE_POLY_OUTPUT].setVoltage(voltage, i);
            }
        }

        cycle256++;
    }
};

struct ManGate8ModuleWidget : InfNoiseModuleWidget {
    infNoiseSmallButton<bc_green, true>* allBtn;
    infNoiseSmallButton<bc_green, true>* gateBtn[8];

    ManGate8ModuleWidget(ManGate8Module *module) {
        initializeWidget(module, "res/ManGate8");

        const float butClm = 14.806f;
        const float outClm = 43.545f;
        const float allRow = 52.120f;
        const float lgtOfs = 10.021f;
        const float latchColOffset = 12.46f;
        const float latchRowOffset = 8.955f;
        allBtn = createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(butClm, allRow), module, ManGate8Module::GATE_ALL_PARAM);
        addParam(allBtn);
        addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(butClm + latchColOffset, allRow + latchRowOffset), module, ManGate8Module::GATE_ALL_LATCH_PARAM));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(outClm, allRow), module, ManGate8Module::GATE_POLY_OUTPUT));
        addChild(createLightCentered<TinyLight<RedLight>>(Vec(outClm - lgtOfs, allRow - lgtOfs), module, ManGate8Module::POLY_HINT_LIGHT));

        const float rowSpacing = 35.0734f;
        float row = 87.194f;
        for (int i = 0; i < 8; i++) {
            gateBtn[i] = createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(butClm, row), module, ManGate8Module::GATE1_PARAM + i);
            addParam(gateBtn[i]);
            addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(butClm + latchColOffset, row + latchRowOffset), module, ManGate8Module::GATE1_LATCH_PARAM + i));
            addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(outClm, row), module, ManGate8Module::GATE1_OUTPUT + i));

            row += rowSpacing;
        }
    }

    void step() override {
        if (module) {
            applyButtonMomentary(allBtn, module->params[ManGate8Module::GATE_ALL_LATCH_PARAM].getValue() < 0.5f);
            for (int i = 0; i < 8; i++) {
                applyButtonMomentary(gateBtn[i], module->params[ManGate8Module::GATE1_LATCH_PARAM + i].getValue() < 0.5f);
            }
        }

        InfNoiseModuleWidget::step();
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        ManGate8Module* module = dynamic_cast<ManGate8Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexPtrSubmenuItem("All-button mode",
            { "All OFF", "All ON", "Toggle ALL" },
            &module->allMode.req));

        menu->addChild(createSubmenuItem("Set gate-buttons 1-8", "",
            [=](Menu* menu) {
                menu->addChild(createMenuItem("Latched", "", [=]() {
                    for (int i = 0; i < 8; i++)
                        module->params[ManGate8Module::GATE1_LATCH_PARAM + i].setValue(1.0f);
                    }));
                menu->addChild(createMenuItem("Unlatched", "", [=]() {
                    for (int i = 0; i < 8; i++)
                        module->params[ManGate8Module::GATE1_LATCH_PARAM + i].setValue(0.0f);
                    }));
            }
        ));

        std::vector<std::string> polyNames = getPolyphonyModeNames(false);
        polyNames.resize(8);
        menu->addChild(createIndexPtrSubmenuItem("Poly output channels", polyNames,
            &module->polyOutput.req));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelManGate8 = createModel<ManGate8Module, ManGate8ModuleWidget>("ManGate8");