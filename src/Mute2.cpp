// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct Mute2Module : InfNoiseModule {
    enum ParamId {
        BOTH_MUTE_PARAM,
        A_MUTE_PARAM,
        B_MUTE_PARAM,
        BOTH_LATCH_PARAM,
        A_LATCH_PARAM,
        B_LATCH_PARAM,
        BOTH_GATE_TRIG_PARAM,
        A_GATE_TRIG_PARAM,
        B_GATE_TRIG_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        BOTH_MUTE_INPUT,
        A_MUTE_INPUT,
        B_MUTE_INPUT,
        A_INPUT,
        B_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        A_OUTPUT,
        B_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        A_MUTED_LIGHT,
        B_MUTED_LIGHT,
        LIGHTS_LEN
    };

    bool haveOutputs = false;
    int channels[2] = { 1, 1 };
    actReqValue<voltValue> muteVoltage = actReqValue<voltValue>(v_zero);
    bool bothMuted = false;
    dsp::SchmittTrigger bothTrigger;
    bool triggerMuted[2] = { false, false };  // A/B-section muted or not
    dsp::SchmittTrigger muteTrigger[2] = { dsp::SchmittTrigger(), dsp::SchmittTrigger() };
    enum gateMuteModeType { gmm_highGate, gmm_lowGate };
    actReqValue<gateMuteModeType> gateMuteMode = actReqValue<gateMuteModeType>(gmm_highGate);

	Mute2Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        // Both
        configSwitch(BOTH_MUTE_PARAM, 0.0f, 1.0f, 0.0f, "Both-mute", { "Unmuted", "Muted" });
        configSwitch(BOTH_LATCH_PARAM, 0.0f, 1.0f, 0.0f, "Latch Mute-button", { "Unlatched", "Latched" });
        configInput(BOTH_MUTE_INPUT, "Both-Gate/Trigger");
        configSwitch(BOTH_GATE_TRIG_PARAM, 0.0f, 1.0f, 1.0f,"Both-Gate/Trigger", { "Trigger when red", "Gate when green" });

        // A
        configSwitch(A_MUTE_PARAM, 0.0f, 1.0f, 0.0f, "A-mute", { "Unmuted", "Muted" });
        configSwitch(A_LATCH_PARAM, 0.0f, 1.0f, 0.0f, "Latch Mute-button", { "Unlatched", "Latched" });
        configInput(A_MUTE_INPUT, "A-Gate/Trigger");
        configSwitch(A_GATE_TRIG_PARAM, 0.0f, 1.0f, 1.0f, "A-Gate/Trigger", { "Trigger when red", "Gate when green" });
        configInput(A_INPUT, "A-CV");
        configOutput(A_OUTPUT, "A-CV");

        // B
        configSwitch(B_MUTE_PARAM, 0.0f, 1.0f, 0.0f, "B-mute", { "Unmuted", "Muted" });
        configSwitch(B_LATCH_PARAM, 0.0f, 1.0f, 0.0f, "Latch Mute-button", { "Unlatched", "Latched" });
        configInput(B_MUTE_INPUT, "B-Gate/Trigger");
        configSwitch(B_GATE_TRIG_PARAM, 0.0f, 1.0f, 1.0f, "B-Gate/Trigger", { "Trigger when red", "Gate when green" });
        configInput(B_INPUT, "B-CV");
        configOutput(B_OUTPUT, "B-CV");

        configLight(A_MUTED_LIGHT, "A-muted when lit");
        configLight(B_MUTED_LIGHT, "B-muted when lit");

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
		haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = true;  
		haveGateDetect = true;
		haveGateHighLow = false;
		haveTrigDetect = true;
		haveTrigHighLow = false;
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        
        muteVoltage.setBoth(v_zero);
        gateMuteMode.setBoth(gmm_highGate);
        
        bothMuted = false;
        bothTrigger.reset();
        for (int i = 0; i < 2; i++) {
			triggerMuted[i] = false;
            muteTrigger[i].reset();
		}
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        muteVoltage.setBoth((voltValue)getJsonInt(rootJ, "muteVoltage", (int)v_zero));
        gateMuteMode.setBoth((gateMuteModeType)getJsonInt(rootJ, "gateMuteMode", (int)gmm_highGate));
        bothMuted = getJsonBool(rootJ, "bothMuted", false);
        getJsonBoolArray(rootJ, "triggerMuted", triggerMuted, 2, false);
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "muteVoltage", json_integer((int)muteVoltage.req));
        json_object_set_new(rootJ, "gateMuteMode", json_integer((int)gateMuteMode.req));
        json_object_set_new(rootJ, "bothMuted", json_boolean(bothMuted));
        setJsonBoolArray(rootJ, "triggerMuted", triggerMuted, 2);
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        gateMuteMode.updateActual();
        muteVoltage.updateActual();

        // Check if trigger-input is not connected, or in Gate-mode
        if (!inputs[BOTH_MUTE_INPUT].isConnected() || 
            params[BOTH_GATE_TRIG_PARAM].getValue() > 0.5) { 
            bothMuted = false;
            bothTrigger.reset();
        }

        haveOutputs = false;
        for (int i = 0; i < 2; i++) {
            channels[i] = inputs[A_INPUT + i].isConnected()
                    ? inputs[A_INPUT + i].getChannels()
                    : 1;
            if (outputs[A_OUTPUT + i].isConnected()) {
                haveOutputs = true;               
            }
            outputs[A_OUTPUT + i].setChannels(channels[i]);

            // Check if trigger-input is not connected, or in Gate-mode
            if (!inputs[A_MUTE_INPUT + i].isConnected() || 
                params[A_GATE_TRIG_PARAM + i].getValue() > 0.5) { // Gate-mode
                triggerMuted[i] = false;
                muteTrigger[i].reset();
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
            // Check if both is muted
            bool isBothMuted = params[BOTH_MUTE_PARAM].getValue() > 0.5f;
            if (inputs[BOTH_MUTE_INPUT].isConnected()) {
                if (params[BOTH_GATE_TRIG_PARAM].getValue() > 0.5) { // Gate
                    isBothMuted = isBothMuted || 
                    (gateMuteMode.act == gmm_highGate && inputs[BOTH_MUTE_INPUT].getVoltage() >= trueDetectValues[gateDetHigh.act]) ||
                    (gateMuteMode.act == gmm_lowGate && inputs[BOTH_MUTE_INPUT].getVoltage() < trueDetectValues[gateDetHigh.act]);
                }
                else { // Trigger
                    if (bothTrigger.process(inputs[BOTH_MUTE_INPUT].getVoltage(),
                        trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act]))
                        bothMuted = !bothMuted;
                    isBothMuted = isBothMuted || bothMuted;
                }
            }

            // Process A/B-sections
            for (int i = 0; i < 2; i++) {
                if (outputs[A_OUTPUT + i].isConnected()) {
                    // Check if A/B is muted
                    bool isMuted = isBothMuted || params[A_MUTE_PARAM + i].getValue() > 0.5f;
                    if (inputs[A_MUTE_INPUT + i].isConnected()) {
                        if (params[A_GATE_TRIG_PARAM + i].getValue() > 0.5) // Gate
                            isMuted = isMuted || 
                            (gateMuteMode.act == gmm_highGate && inputs[A_MUTE_INPUT + i].getVoltage() >= trueDetectValues[gateDetHigh.act]) ||
                            (gateMuteMode.act == gmm_lowGate && inputs[A_MUTE_INPUT + i].getVoltage() < trueDetectValues[gateDetHigh.act]);
                        else { // Trigger
                            if (muteTrigger[i].process(inputs[A_MUTE_INPUT + i].getVoltage(),
                                trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act]))
                                triggerMuted[i] = !triggerMuted[i];
                            isMuted = isMuted || triggerMuted[i];
                        }
                    }

                    // Update muted-lights
                    lights[A_MUTED_LIGHT + i].setBrightness(isMuted ? 1.f : 0.f);

                    // Output A/B-section
                    int inPortIdx = (i == 0) 
                        ? A_INPUT 
                        : inputs[B_INPUT].isConnected() 
                            ? B_INPUT
                            : A_INPUT;
                    for (int c = 0; c < channels[i]; c++) {
    		            float voltage = (isMuted)
                            ? voltValues[muteVoltage.act]
                            : inputs[inPortIdx].isConnected()
								? inputs[inPortIdx].getVoltage(c)
								: 0.f;
                        voltage = clipToVoltRange(voltage, outClipRange.act);
                        outputs[A_OUTPUT + i].setVoltage(voltage, c);
                    }
                }
			}
        }

        cycle256++;
    }
};

struct Mute2ModuleWidget : InfNoiseModuleWidget {
    infNoiseSmallButton<bc_green, true>* bothBtn;
    infNoiseSmallButton<bc_green, true>* sectBtn[2];

    Mute2ModuleWidget(Mute2Module *module) {
        initializeWidget(module, "res/Mute2");

        const float cntrCol = 15.f;
        const float latchCol = 25.641f;
        const float lightClm = 4.981f;

        // Mute both
        bothBtn = createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(cntrCol, 50.741f), module, Mute2Module::BOTH_MUTE_PARAM);
        addParam(bothBtn);
        addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(latchCol, 61.912f), module, Mute2Module::BOTH_LATCH_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrCol, 73.695f), module, Mute2Module::BOTH_MUTE_INPUT));
        addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_red, bc_green>>(
            Vec(latchCol, 85.230f), module, Mute2Module::BOTH_GATE_TRIG_PARAM));

        // A-Mute
        sectBtn[0] = createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(cntrCol, 110.926f), module, Mute2Module::A_MUTE_PARAM);
        addParam(sectBtn[0]);
        addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(latchCol, 122.096f), module, Mute2Module::A_LATCH_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrCol, 133.880f), module, Mute2Module::A_MUTE_INPUT));
        addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_red, bc_green>>(
            Vec(latchCol, 145.414f), module, Mute2Module::A_GATE_TRIG_PARAM));

        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 168.953f), module, Mute2Module::A_INPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 204.027f), module, Mute2Module::A_OUTPUT));
        addChild(createLightCentered<SmallLight<RedLight>>(Vec(lightClm, 188.762f), module, Mute2Module::A_MUTED_LIGHT));

        // B-Mute
        sectBtn[1] = createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(cntrCol, 239.593f), module, Mute2Module::B_MUTE_PARAM);
        addParam(sectBtn[1]);
        addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(latchCol, 250.763f), module, Mute2Module::B_LATCH_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrCol, 262.547f), module, Mute2Module::B_MUTE_INPUT));
        addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_red, bc_green>>(
            Vec(latchCol, 274.081f), module, Mute2Module::B_GATE_TRIG_PARAM));

        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 297.620f), module, Mute2Module::B_INPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 332.694f), module, Mute2Module::B_OUTPUT));
        addChild(createLightCentered<SmallLight<RedLight>>(Vec(lightClm, 317.429f), module, Mute2Module::B_MUTED_LIGHT));
    }

    void step() override {
        if (module) {
            bothBtn->momentary = module->params[Mute2Module::BOTH_LATCH_PARAM].getValue() < 0.5f;
            for (int i = 0; i < 2; i++) {
                sectBtn[i]->momentary = module->params[Mute2Module::A_LATCH_PARAM + i].getValue() < 0.5f;
            }
        }

        InfNoiseModuleWidget::step();
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        Mute2Module* module = dynamic_cast<Mute2Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> gateMuteModeNames = { "High-gate will mute", "Low-gate will mute"};
        menu->addChild(createIndexPtrSubmenuItem("Gate mute-mode", gateMuteModeNames,
            &module->gateMuteMode.req));

        std::vector<std::string> voltNames = getVoltValuesNames();
        menu->addChild(createIndexPtrSubmenuItem("Mute voltage", voltNames,
            &module->muteVoltage.req));
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelMute2 = createModel<Mute2Module, Mute2ModuleWidget>("Mute2");