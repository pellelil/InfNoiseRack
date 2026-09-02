// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inUtil.hpp"

struct ManMute8Module : InfNoiseModule {
    enum ParamId {
        MUTED_LEVEL_PARAM,
        MUTE_ALL_PARAM,
        MUTE_ALL_LATCH_PARAM,
        MUTE_1_PARAM,
        MUTE_2_PARAM,
        MUTE_3_PARAM,
        MUTE_4_PARAM,
        MUTE_5_PARAM,
        MUTE_6_PARAM,
        MUTE_7_PARAM,
        MUTE_8_PARAM,
        LATCH1_PARAM,
        LATCH2_PARAM,
        LATCH3_PARAM,
        LATCH4_PARAM,
        LATCH5_PARAM,
        LATCH6_PARAM,
        LATCH7_PARAM,
        LATCH8_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        MUTED_LEVEL_INPUT,
        CV1_INPUT,
        CV2_INPUT,
        CV3_INPUT,
        CV4_INPUT,
        CV5_INPUT,
        CV6_INPUT,
        CV7_INPUT,
        CV8_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        CV1_OUTPUT,
        CV2_OUTPUT,
        CV3_OUTPUT,
        CV4_OUTPUT,
        CV5_OUTPUT,
        CV6_OUTPUT,
        CV7_OUTPUT,
        CV8_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        LIGHTS_LEN
    };
    
    enum allButtonMode { abm_Off, abm_On, abm_Toggle };
    actReqValue<allButtonMode> allMode = actReqValue<allButtonMode>(abm_Toggle); 
    infNoiseButtonTrigger btAll = infNoiseButtonTrigger();
    bool outputsInUse = false;
    int firstIdx = -1;
    int lastIdx = -1;
    int muteChannels = 1;
    int channels[8] = { 1 };

    void applyLoadedOutputs() {
        if (!outputsInUse)
            return;
        float normVolt[PORT_MAX_CHANNELS] = { 0.f };
        for (int i = firstIdx; i <= lastIdx; i++) {
            if (inputs[CV1_INPUT].isConnected())
                inputs[CV1_INPUT + i].readVoltages(normVolt);

            if (outputs[CV1_OUTPUT + i].isConnected()) {
                float muteLevelKnob = params[MUTED_LEVEL_PARAM].getValue();
                for (int c = 0; c < channels[i]; c++) {
                    float cv = (params[MUTE_1_PARAM + i].getValue() > 0.5f)
                        ? (inputs[MUTED_LEVEL_INPUT].isConnected())
                            ? inputs[MUTED_LEVEL_INPUT].getVoltage(c) + muteLevelKnob
                            : muteLevelKnob
                        : normVolt[c];
                    cv = quantizeToMode(cv, outQuantize.act);
                    cv = clipToVoltRange(cv, outClipRange.act);
                    outputs[CV1_OUTPUT + i].setVoltage(cv, c);
                }
            }
        }
    }

    ManMute8Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configParam(MUTED_LEVEL_PARAM, -10.0f, 10.0f, 0.0f, "Muted output level (-10V to +10V)", " V");
        configInput(MUTED_LEVEL_INPUT, "Muted output signal (use knob-selected value if not connected)");
        configSwitch(MUTE_ALL_PARAM, 0.0f, 1.0f, 0.0f, "All", { "Unmuted", "Muted" });
        configSwitch(MUTE_ALL_LATCH_PARAM, 0.0f, 1.0f, 0.0f, "Latch Mute-button", { "Unlatched", "Latched" });

        for (int i = 0; i < 8; i++) {
            configInput(CV1_INPUT + i, string::f("CV-%d", i + 1));
            configSwitch(MUTE_1_PARAM + i, 0.0f, 1.0f, 0.0f, string::f("Mute-button %d", i + 1), { "Unmuted", "Muted" });
            configSwitch(LATCH1_PARAM + i, 0.0f, 1.0f, 0.0f, string::f("Latch Mute-button %d", i + 1), { "Unlatched", "Latched" });
            configOutput(CV1_OUTPUT + i, string::f("CV-%d", i + 1));
        }

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
        
        btAll.reset();
        allMode.setBoth(abm_Toggle);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        allMode.setBoth((allButtonMode)getJsonInt(rootJ, "allMode", (int)abm_Toggle));
        bool allPressed = params[MUTE_ALL_PARAM].getValue() > 0.5f;
        btAll.reset(allPressed
            ? infNoiseButtonTrigger::bt_pressed
            : infNoiseButtonTrigger::bt_released);
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "allMode", json_integer((int)allMode.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        // Handle All-button
        allMode.updateActual();
        if (!wasJustLoaded && btAll.process(params[MUTE_ALL_PARAM].getValue() > 0.5f)) {
            // only once when it fires
            if (btAll.isPressed()) {
                if (allMode.act == abm_Toggle) {
                    for (int i = 0; i < 8; i++) {
                        float buttonValue = (params[MUTE_1_PARAM + i].getValue() > 0.5f) ? 0.0f : 1.0f;
                        params[MUTE_1_PARAM + i].setValue(buttonValue);
                    }
                }
            }
            else { // release non-latched buttons
                for (int i = 0; i < 8; i++) {
                    if (params[LATCH1_PARAM + i].getValue() < 0.5f)
                        params[MUTE_1_PARAM + i].setValue(0.f);
                }
            }
        }
        // Always
        if (!wasJustLoaded && btAll.isPressed()) {
            if (allMode.act == abm_On) {
                for (int i = 0; i < 8; i++) {
                    params[MUTE_1_PARAM + i].setValue(1.0f);
                }
            }
            else if (allMode.act == abm_Off) {
                for (int i = 0; i < 8; i++) {
                    params[MUTE_1_PARAM + i].setValue(0.f);
                }
            }
        }

        muteChannels = inputs[MUTED_LEVEL_INPUT].isConnected() 
            ? std::max(inputs[MUTED_LEVEL_INPUT].getChannels(), 1) 
            : 0;

        outputsInUse = false;
        firstIdx = -1;
        lastIdx = -1;
        int channelCount = muteChannels > 0 ? muteChannels : 1;
        for (int i=0; i<8; i++) {
            if (inputs[CV1_INPUT + i].isConnected() || outputs[CV1_OUTPUT + i].isConnected()) {
                if (outputs[CV1_OUTPUT + i].isConnected())
                    outputsInUse = true;

                if (firstIdx == -1)
                    firstIdx = i;
                lastIdx = i;

                if (inputs[CV1_INPUT + i].isConnected()) {
                    channelCount = std::max(inputs[CV1_INPUT + i].getChannels(), 1);
                    if (muteChannels > 0)
						channelCount = std::min(channelCount, muteChannels);
                }
            }

            channels[i] = channelCount;
            outputs[CV1_OUTPUT + i].setChannels(channels[i]);
        }

        if (wasJustLoaded && outputsInUse)
            applyLoadedOutputs();

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

        if (doProcess && outputsInUse) {
            float normVolt[PORT_MAX_CHANNELS] = { 0.f };
            for (int i = firstIdx; i <= lastIdx; i++) {
                if (inputs[CV1_INPUT].isConnected()) {
                    inputs[CV1_INPUT + i].readVoltages(normVolt);
                }

                if (outputs[CV1_OUTPUT + i].isConnected()) {
                    float muteLevelKnob = params[MUTED_LEVEL_PARAM].getValue();
                    for (int c = 0; c < channels[i]; c++) {
						float cv = (params[MUTE_1_PARAM + i].getValue() > 0.5f)
                            ? (inputs[MUTED_LEVEL_INPUT].isConnected())
							    ? inputs[MUTED_LEVEL_INPUT].getVoltage(c) + muteLevelKnob
							    : muteLevelKnob
                            : normVolt[c];
                        cv = quantizeToMode(cv, outQuantize.act);
                        cv = clipToVoltRange(cv, outClipRange.act);
						outputs[CV1_OUTPUT + i].setVoltage(cv, c);
					}
                }
			}
        }

        cycle256++;
    }
};

struct ManMute8ModuleWidget : InfNoiseModuleWidget {
    infNoiseSmallButton<bc_red, true>* allBtn;
    infNoiseSmallButton<bc_red>* muteBtn[8];

    ManMute8ModuleWidget(ManMute8Module *module) {
        initializeWidget(module, "res/ManMute8");

        float col1 = 14.810f;
        float col2 = 43.498f;
        float col3 = 72.229f;
        float allRow = 51.443f;
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(col1, allRow), module, ManMute8Module::MUTED_LEVEL_INPUT));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(col2, allRow), module, ManMute8Module::MUTED_LEVEL_PARAM));
        allBtn = createParamCentered<infNoiseSmallButton<bc_red, true>>(Vec(col3, allRow), module, ManMute8Module::MUTE_ALL_PARAM);
        addParam(allBtn);
        addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(82.312f, 61.497f), module, ManMute8Module::MUTE_ALL_LATCH_PARAM));

        float latchCol = 55.442f;
        float latchOffset = 9.5f;  //float latchOffset = 10.054f;
        float row = 87.194f;
        float rowSpacing = 35.0734f;
        for (int i = 0; i < 8; i++) {
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(col1, row), module, ManMute8Module::CV1_INPUT + i));
            muteBtn[i] = createParamCentered<infNoiseSmallButton<bc_red>>(Vec(col2, row), module, ManMute8Module::MUTE_1_PARAM + i);
            addParam(muteBtn[i]);
            addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(latchCol, row + latchOffset), module, ManMute8Module::LATCH1_PARAM + i));
            addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(col3, row), module, ManMute8Module::CV1_OUTPUT + i));

            row += rowSpacing;
        }
    }

    void step() override {
        if (module) {
            applyButtonMomentary(allBtn, module->params[ManMute8Module::MUTE_ALL_LATCH_PARAM].getValue() < 0.5f);
            for (int i = 0; i < 8; i++) {
                applyButtonMomentary(muteBtn[i], module->params[ManMute8Module::LATCH1_PARAM + i].getValue() < 0.5f);
            }
        }

        InfNoiseModuleWidget::step();
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        ManMute8Module* module = dynamic_cast<ManMute8Module*>(this->module);
        assert(module);
        
        menu->addChild(new MenuSeparator);
            
        menu->addChild(createIndexPtrSubmenuItem("All-button mode",
            { "All OFF", "All ON", "Toggle ALL" },
            &module->allMode.req));

        menu->addChild(createSubmenuItem("Set mute-buttons 1-8", "",
            [=](Menu* menu) {
                menu->addChild(createMenuItem("Latched", "", [=]() {
                    for (int i = 0; i < 8; i++)
                        module->params[ManMute8Module::LATCH1_PARAM + i].setValue(1.0f);
                    }));
                menu->addChild(createMenuItem("Unlatched", "", [=]() {
                    for (int i = 0; i < 8; i++)
                        module->params[ManMute8Module::LATCH1_PARAM + i].setValue(0.0f);
                    }));
            }
        ));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelManMute8 = createModel<ManMute8Module, ManMute8ModuleWidget>("ManMute8");