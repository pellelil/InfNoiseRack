// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct FlipFlopModule : InfNoiseModule {
    enum ParamId {
        MODE_PARAM,
        SET_PARAM,
        RESET_PARAM,
        ENDIS_TRIG_GATE_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        TD_INPUT,
        CLOCK_INPUT,
        SET_INPUT,
        RESET_INPUT,
        ENDIS_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        Q_OUTPUT,
        INVQ_OUTPUT,
        CHQ_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        D_LIGHT,
        T_LIGHT,
        ENABLED_LIGHT,
        DISABLED_LIGHT,
        UNDEF_OPR_LIGHT,
        LIGHTS_LEN
    };

    bool qHigh[PORT_MAX_CHANNELS] = {}; // True if Q is high, else Q is low
    int channels = 1;
    bool disabled = false;
    bool haveOutputs = false;
    bool undefOpr = false;  // True if "undefined operation" detected in SR-mode (any channel)
    float lastMode = -1.f;  // Only update mode lights if mode has changed
    enum enDisGateModeType { edgm_Disable, edgm_Enable };
    actReqValue<enDisGateModeType> enDisGateMode = actReqValue<enDisGateModeType>(enDisGateModeType::edgm_Disable);
    enum undefOprOutputType { uoo_unchanged, uoo_low_low, uoo_low_high, uoo_high_low, uoo_high_high };
    actReqValue<undefOprOutputType> undefOprOutput = actReqValue<undefOprOutputType>(undefOprOutputType::uoo_unchanged);
    dsp::SchmittTrigger clockTrigger[PORT_MAX_CHANNELS];
    dsp::SchmittTrigger disableTrigger;
    infNoiseOutTrigger qChangedTrigger[PORT_MAX_CHANNELS];
    std::string tdName[2] = { "Toggle (gate)", "Data (gate)" };

    void resetTriggers() {
        disableTrigger.reset();
        for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
            clockTrigger[c].reset();
            qChangedTrigger[c].reset();
        }
    }

	FlipFlopModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configSwitch(MODE_PARAM, 0.0, 1.0, 0.0, "Flip-flop mode", { "Toggle", "Data" });

        configInput(TD_INPUT, tdName[0]);
        configLight(T_LIGHT, "Toggle-mode active (if lit)");
        configLight(D_LIGHT, "Data-mode active (if lit)");
        configInput(CLOCK_INPUT, "Clock (trigger)");

        configSwitch(SET_PARAM, 0.0f, 1.0f, 0.0f, "Set (button)");
        configInput(SET_INPUT, "Set (gate)");
        configSwitch(RESET_PARAM, 0.0f, 1.0f, 0.0f, "Reset (button)");
        configInput(RESET_INPUT, "Reset (gate)");

        configInput(ENDIS_INPUT, "Enable-gate/trigger");
        configSwitch(ENDIS_TRIG_GATE_PARAM, 0.0, 1.0, 1.0, "Enable-gate/trigger", { "Trigger", "Gate" });
        configLight(ENABLED_LIGHT, "Enabled (if lit)");
        configLight(DISABLED_LIGHT, "Disabled (if lit)");

        configLight(UNDEF_OPR_LIGHT, "Undefined operation (if lit)");
        configOutput(Q_OUTPUT, "Q");
        configOutput(INVQ_OUTPUT, "Inverted Q");
        configOutput(CHQ_OUTPUT, "Q changed (trigger)");

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
		haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = false;  
		haveGateDetect = true;
		haveGateHighLow = true;
		haveTrigDetect = true;
		haveTrigHighLow = true;
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        
        resetTriggers();
        for (int c = 0; c < PORT_MAX_CHANNELS; c++)
            qHigh[c] = false;
        disabled = false;
        lastMode = -1.f; // force update of mode lights
        undefOpr = false;
        enDisGateMode.setBoth(enDisGateModeType::edgm_Disable);
        undefOprOutput.setBoth(undefOprOutputType::uoo_unchanged);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);

        lastMode = -1.f; // force update of mode lights
        enDisGateMode.setBoth((enDisGateModeType)getJsonInt(rootJ, "enDisGateMode", (int)enDisGateModeType::edgm_Disable));
        undefOprOutput.setBoth((undefOprOutputType)getJsonInt(rootJ, "undefOprOutput", (int)undefOprOutputType::uoo_unchanged));
        if (jsonVersion == 1) {
            for (int c = 0; c < PORT_MAX_CHANNELS; c++)
                qHigh[c] = false;
            qHigh[0] = getJsonInt(rootJ, "qHigh", 0) == 1;
        }
        else {
            getJsonBoolArray(rootJ, "qHigh", qHigh, PORT_MAX_CHANNELS, false);
        }
        disabled = getJsonBool(rootJ, "disabled", false);
        undefOpr = getJsonBool(rootJ, "undefOpr", false);
        resetTriggers();
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "enDisGateMode", json_integer((int)enDisGateMode.req));
        json_object_set_new(rootJ, "undefOprOutput", json_integer((int)undefOprOutput.req));
        setJsonBoolArray(rootJ, "qHigh", qHigh, PORT_MAX_CHANNELS);
        json_object_set_new(rootJ, "disabled", json_boolean(disabled));
        json_object_set_new(rootJ, "undefOpr", json_boolean(undefOpr));
    }

    void applyLoadedOutputs() {
        resetTriggers();

        lights[ENABLED_LIGHT].setBrightness(!disabled ? 1.f : 0.f);
        lights[DISABLED_LIGHT].setBrightness(disabled ? 1.f : 0.f);
        lights[UNDEF_OPR_LIGHT].setBrightness(undefOpr ? 1.f : 0.f);

        if (!haveOutputs || disabled)
            return;

        float gateHighV = voltValues[gateOutHigh.act];
        float gateLowV = voltValues[gateOutLow.act];
        if (outputs[Q_OUTPUT].isConnected()) {
            for (int c = 0; c < channels; c++)
                outputs[Q_OUTPUT].setVoltage(qHigh[c] ? gateHighV : gateLowV, c);
        }
        if (outputs[INVQ_OUTPUT].isConnected()) {
            for (int c = 0; c < channels; c++)
                outputs[INVQ_OUTPUT].setVoltage(!qHigh[c] ? gateHighV : gateLowV, c);
        }
        if (outputs[CHQ_OUTPUT].isConnected()) {
            float trigLowV = voltValues[trigOutLow.act];
            for (int c = 0; c < channels; c++)
                outputs[CHQ_OUTPUT].setVoltage(trigLowV, c);
        }
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        enDisGateMode.updateActual();
        undefOprOutput.updateActual();

        float mode = params[MODE_PARAM].getValue();
        if (mode != lastMode) {
            lastMode = mode;
            if (mode < 0.5f)  // T-mode
            {
                inputInfos[TD_INPUT]->name = polyPortPrefix() + tdName[0];
                lights[T_LIGHT].setBrightness(1.f);
                lights[D_LIGHT].setBrightness(0.f);
            }
            else // D-mode
            {
                inputInfos[TD_INPUT]->name = polyPortPrefix() + tdName[1];
                lights[D_LIGHT].setBrightness(1.f);
                lights[T_LIGHT].setBrightness(0.f);
            }
        }
        
        haveOutputs =
            outputs[Q_OUTPUT].isConnected() ||
            outputs[INVQ_OUTPUT].isConnected() ||
            outputs[CHQ_OUTPUT].isConnected();

        channels = 1;
        if (inputs[TD_INPUT].isConnected())
            channels = std::max(channels, inputs[TD_INPUT].getChannels());
        if (inputs[CLOCK_INPUT].isConnected())
            channels = std::max(channels, inputs[CLOCK_INPUT].getChannels());
        if (inputs[SET_INPUT].isConnected())
            channels = std::max(channels, inputs[SET_INPUT].getChannels());
        if (inputs[RESET_INPUT].isConnected())
            channels = std::max(channels, inputs[RESET_INPUT].getChannels());
        outputs[Q_OUTPUT].setChannels(channels);
        outputs[INVQ_OUTPUT].setChannels(channels);
        outputs[CHQ_OUTPUT].setChannels(channels);

        // Set disabled to false when Enable-gate/trigger input is disconnected
        if (!inputs[ENDIS_INPUT].isConnected()) {
            disabled = false;
        }
        lights[ENABLED_LIGHT].setBrightness(!disabled ? 1.f : 0.f);
        lights[DISABLED_LIGHT].setBrightness(disabled ? 1.f : 0.f);

        lights[UNDEF_OPR_LIGHT].setBrightness(undefOpr ? 1.f : 0.f);

        if (wasJustLoaded && haveOutputs)
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

        if (doProcess && haveOutputs) {
            // Enable/Disable — once per process (monophonic, all channels)
            if (inputs[ENDIS_INPUT].isConnected()) {
                if (params[ENDIS_TRIG_GATE_PARAM].getValue() < 0.5f) { // Trigger-mode
                    if (disableTrigger.process(inputs[ENDIS_INPUT].getVoltage(),
                        trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act])) {
                        disabled = !disabled;
                    }
                }
                else {  // Gate-mode
                    bool trueInput = inputs[ENDIS_INPUT].getVoltage() >= trueDetectValues[gateDetHigh.act];
                    disabled = (trueInput && enDisGateMode.act == edgm_Disable) || 
                        (!trueInput && enDisGateMode.act == edgm_Enable);
                }
            }

            if (!disabled) {
                bool setBtn = params[SET_PARAM].getValue() > 0.5f;
                bool resetBtn = params[RESET_PARAM].getValue() > 0.5f;
                bool tMode = params[MODE_PARAM].getValue() < 0.5f;
                bool setConnected = inputs[SET_INPUT].isConnected();
                bool resetConnected = inputs[RESET_INPUT].isConnected();
                bool tdConnected = inputs[TD_INPUT].isConnected();
                bool clockConnected = inputs[CLOCK_INPUT].isConnected();

                undefOpr = false;
                for (int c = 0; c < channels; c++) {
                    bool prevQHigh = qHigh[c];

                    bool setGate = setBtn || (setConnected && inputs[SET_INPUT].getPolyVoltage(c) >= trueDetectValues[gateDetHigh.act]);
                    bool resetGate = resetBtn || (resetConnected && inputs[RESET_INPUT].getPolyVoltage(c) >= trueDetectValues[gateDetHigh.act]);

                    bool updateOutput = true; // false if undefOpr and unchanged output
                    bool chUndef = false;
                    if (setGate && resetGate) // Undefined operation
                    {
                        chUndef = true;
                        undefOpr = true;
                        if (undefOprOutput.req == undefOprOutputType::uoo_unchanged)
                        {
                            updateOutput = false;
                        }
                        else
                        {
                            qHigh[c] = (undefOprOutput.req == undefOprOutputType::uoo_high_low ||
                                undefOprOutput.req == undefOprOutputType::uoo_high_high);
                        }
                    }
                    else if (setGate) // Set only
                    {
                        qHigh[c] = true;
                    }
                    else if (resetGate) // Reset only
                    {
                        qHigh[c] = false;
                    }
                    else { // Process T-mode or D-mode
                        float tdInput = tdConnected
                            ? inputs[TD_INPUT].getPolyVoltage(c)
                            : 0.f;
                        float clockInput = clockConnected
                            ? inputs[CLOCK_INPUT].getPolyVoltage(c)
                            : tdInput; // Normalize to TD-input
                        if (clockTrigger[c].process(clockInput,
                            trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act])) {
                            if (tMode)
                            {
                                if (tdInput >= trueDetectValues[gateDetHigh.act])
                                    qHigh[c] = !qHigh[c];
                            }
                            else // D-mode
                            {
                                qHigh[c] = tdInput >= trueDetectValues[gateDetHigh.act];
                            }
                        }
                    }

                    if (updateOutput)
                    {
                        outputs[Q_OUTPUT].setVoltage(qHigh[c]
                            ? voltValues[gateOutHigh.act]
                            : voltValues[gateOutLow.act], c);
                        bool qInv = chUndef
                            ? (undefOprOutput.req == undefOprOutputType::uoo_low_high ||
                                undefOprOutput.req == undefOprOutputType::uoo_high_high)
                            : !qHigh[c];
                        outputs[INVQ_OUTPUT].setVoltage(qInv
                            ? voltValues[gateOutHigh.act]
                            : voltValues[gateOutLow.act], c);
                    }

                    if (!qChangedTrigger[c].process(procSampleTime) && prevQHigh != qHigh[c]) {
                        qChangedTrigger[c].trigger();
                    }
                    outputs[CHQ_OUTPUT].setVoltage(qChangedTrigger[c].isHigh()
                        ? voltValues[trigOutHigh.act]
                        : voltValues[trigOutLow.act], c);
                }
            }
        }

        cycle256++;
    }
};

struct FlipFlopModuleWidget : InfNoiseModuleWidget {
    FlipFlopModuleWidget(FlipFlopModule *module) {
        initializeWidget(module, "res/FlipFlop");

        // 3-way mode switch
        addParam(createParamCentered<CKSS>(Vec(9.613f, 41.519), module, FlipFlopModule::MODE_PARAM));

        // D/T/S input
        const float cntrClm = 15.f;
        const float dtsLgtRow = 57.401f;
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(10.245f, dtsLgtRow), module, FlipFlopModule::T_LIGHT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(19.150f, dtsLgtRow), module, FlipFlopModule::D_LIGHT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 79.801f), module, FlipFlopModule::TD_INPUT));
        
        // Clock/Reset input
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 116.844f), module, FlipFlopModule::CLOCK_INPUT));

        // Set/Reset input
        const float btnClm = 4.390f;
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, true>>(Vec(btnClm, 138.720f), module, FlipFlopModule::SET_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 150.512f), module, FlipFlopModule::SET_INPUT));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, true>>(Vec(btnClm, 172.195f), module, FlipFlopModule::RESET_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 183.987f), module, FlipFlopModule::RESET_INPUT));

        // Enable/Disable input
        const float enDisLgtRow = 201.298f;
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(7.160f, enDisLgtRow), module, FlipFlopModule::ENABLED_LIGHT));
        addChild(createLightCentered<TinyLight<RedLight>>(Vec(22.582f, enDisLgtRow), module, FlipFlopModule::DISABLED_LIGHT));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrClm, 222.253f), module, FlipFlopModule::ENDIS_INPUT));
        addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_red, bc_green>>(
            Vec(btnClm, 234.304f), module, FlipFlopModule::ENDIS_TRIG_GATE_PARAM));

        // Q/!Q outputs
        addChild(createLightCentered<TinyLight<RedLight>>(Vec(7.160f, 247.034f), module, FlipFlopModule::UNDEF_OPR_LIGHT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 262.591f), module, FlipFlopModule::Q_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 298.189f), module, FlipFlopModule::INVQ_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 332.694f), module, FlipFlopModule::CHQ_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        FlipFlopModule* module = dynamic_cast<FlipFlopModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexPtrSubmenuItem("Enable/disable high-gate",
            { "Will disable (default)", "Will enable" },
            &module->enDisGateMode.req
        ));

		menu->addChild(createIndexPtrSubmenuItem("Undefined output (SR-mode)",
		 	{"Unchanged (default)", "Q=Low / !Q=Low", "Q=High / !Q=Low", "Q=Low / !Q=High", "Q=High / !Q=High"},
		 	&module->undefOprOutput.req
        ));
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelFlipFlop = createModel<FlipFlopModule, FlipFlopModuleWidget>("FlipFlop");
