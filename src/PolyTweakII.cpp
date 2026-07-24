// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct PolyTweakIIModule : InfNoiseModule {
    enum ParamId {
        AUTO_CHANNEL_COUNT_PARAM,
        MAN_CHANNEL_COUNT_PARAM,
        INV_MODE_PARAM,
        DISABLE_MODE_PARAM,
        DISABLE_VALUE_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        POLY_INPUT,
        ADD_VALUE_INPUT,
        INV_GATE_INPUT,
        DISABLE_GATE_INPUT,
        DISABLE_VALUE_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        POLY_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        LIGHTS_LEN
    };
   
    bool haveInput = false;
    bool useAutoChannelCount = false;
    bool haveAddValueInput = false;
    bool haveInvGateInput = false;
    bool haveDisableGateInput = false;
    bool haveDisableValueInput = false;
    bool haveOutput = false;
    int inputChannels = 0;
    float invOffset = 0.f;
    bool doBiUnipolar = true; // true = bipolar, false = unipolar
    bool doSetVal = false; // true = set to disable-value, false = exclude from output
    bool doSetAsGate = false; // true = set as gate, false = set as voltage
    float disableValue = 0.f; // value to set to when doSetVal is true (knob only)
    bool invertChannels[16] = { false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false };
    bool disableChannels[16] = { false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false };
    enum gateNonInvModeType { gnm_passthrough, gnm_gate };
    actReqValue<gateNonInvModeType> gateNonInvMode = actReqValue<gateNonInvModeType>(gnm_gate);

    PolyTweakIIModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
        
        configInput(POLY_INPUT, "Original");

        configSwitch(AUTO_CHANNEL_COUNT_PARAM, 0.0, 1.0, 1.0, "Channel-mode", { "Manual (knob)", "Automatic (input)" });
        configSwitch(MAN_CHANNEL_COUNT_PARAM, 1.0f, 16.0f, 1.0f, "Channel count", { "1 channel (monophonic)", 
            "2 channels", "3 channels", "4 channels", "5 channels", "6 channels", "7 channels", "8 channels", "9 channels", 
            "10 channels", "11 channels", "12 channels", "13 channels", "14 channels", "15 channels", "16 channels" });
        configInput(ADD_VALUE_INPUT, "Value(s) for added channels");

        configInput(INV_GATE_INPUT, "Invert-gate(s)");
        configSwitch(INV_MODE_PARAM, 0.f, 2.f, 0.f, "Invert-mode", { "Bipolar (-5V to +5V)", "Unipolar (0V to 10V)", "Gate (high/low-gate)" });

        configInput(DISABLE_GATE_INPUT, "Disable-gate(s)");
        configSwitch(DISABLE_MODE_PARAM, 0.f, 2.f, 0.f, "Disable-mode", { "Set to disable-value", "Exclude from output" });
        configParam(DISABLE_VALUE_PARAM, -10.0f, 10.0f, 0.0f, "Disable-value (-10V to +10V)", " V");
        configInput(DISABLE_VALUE_INPUT, "Disable-value(s)");

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
        
        gateNonInvMode.setBoth(gnm_gate);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        gateNonInvMode.setBoth((gateNonInvModeType)getJsonInt(rootJ, "gateNonInvMode", (int)gnm_gate));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "gateNonInvMode", json_integer((int)gateNonInvMode.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        invOffset = params[INV_MODE_PARAM].getValue() < 0.5f ? 0.f : 10.f;       
        doBiUnipolar = params[INV_MODE_PARAM].getValue() < 1.5f; // false = gate
        doSetVal = params[DISABLE_MODE_PARAM].getValue() < 0.5f;
        disableValue = params[DISABLE_VALUE_PARAM].getValue(); // Knob only
        gateNonInvMode.updateActual();
        doSetAsGate = gateNonInvMode.act == gnm_gate;

        // Detect input/output and channel-count
        haveInput = inputs[POLY_INPUT].isConnected();
        haveAddValueInput = inputs[ADD_VALUE_INPUT].isConnected();
        haveInvGateInput = inputs[INV_GATE_INPUT].isConnected();
        haveDisableGateInput = inputs[DISABLE_GATE_INPUT].isConnected();
        haveDisableValueInput = inputs[DISABLE_VALUE_INPUT].isConnected();
        haveOutput = outputs[POLY_OUTPUT].isConnected();
        inputChannels = haveInput 
            ? inputs[POLY_INPUT].getChannels() 
            : 0;

        if (!haveOutput) {
            outputs[POLY_OUTPUT].setChannels(1);
            outputs[POLY_OUTPUT].setVoltage(0.f);
        }

        // Handle auto channel-count
        useAutoChannelCount = params[AUTO_CHANNEL_COUNT_PARAM].getValue() > 0.5f;
        if (useAutoChannelCount) {
            params[MAN_CHANNEL_COUNT_PARAM].setValue(std::max(inputChannels, 1));
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
            // Figure out how many channels to output
            int maxChannels = useAutoChannelCount 
                ? inputChannels // Is 0 when no input is connected
                : (int)params[MAN_CHANNEL_COUNT_PARAM].getValue();
            int outChannels = maxChannels;
            for (int c = 0; c < maxChannels; c++) {
                invertChannels[c] = haveInvGateInput && 
                    inputs[INV_GATE_INPUT].getPolyVoltage(c) >= trueDetectValues[gateDetHigh.act];
                disableChannels[c] = haveDisableGateInput && 
                    inputs[DISABLE_GATE_INPUT].getPolyVoltage(c) >= trueDetectValues[gateDetHigh.act];
                if (disableChannels[c] && !doSetVal)
                    outChannels--;
            }

            // Output single 0v channel, when all outputs are disabled
            if (outChannels <= 0)
            {
                outputs[POLY_OUTPUT].setChannels(1);
                outputs[POLY_OUTPUT].setVoltage(0.f);
                return;
            }

            // Output channels
            outputs[POLY_OUTPUT].setChannels(outChannels);
            int outChnl = 0; // used by setVoltage()
            for (int c = 0; c < maxChannels; c++) {
                float voltage = 0.f;
                if (doSetVal || !disableChannels[c]) {
                    if (disableChannels[c]) {
                        voltage = disableValue;
                        if (haveDisableValueInput)
                            voltage += inputs[DISABLE_VALUE_INPUT].getPolyVoltage(c);
                    }
                    else {
                        voltage = (c < inputChannels && haveInput)
                            ? inputs[POLY_INPUT].getPolyVoltage(c)
                            : (haveAddValueInput) 
                                ? inputs[ADD_VALUE_INPUT].getPolyVoltage(c) 
                                : 0.f;
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

        cycle256++;
    }
};

struct PolyTweakIIModuleWidget : InfNoiseModuleWidget {
    PolyTweakIIModuleWidget(PolyTweakIIModule *module) {
        initializeWidget(module, "res/PolyTweakII");

        // Poly-input
        const float cntrClm = 15.f;
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 51.428f), module, PolyTweakIIModule::POLY_INPUT));

        // Channel-count & add-value
        infNoiseLtSmallButton* autoCountBtn = createParamCentered<infNoiseLtSmallButton>(Vec(4.780f, 72.370f), module, PolyTweakIIModule::AUTO_CHANNEL_COUNT_PARAM);
        autoCountBtn->setup(bc_green, false);
        addParam(autoCountBtn);
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrClm, 86.444f), module, PolyTweakIIModule::MAN_CHANNEL_COUNT_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 116.380f), module, PolyTweakIIModule::ADD_VALUE_INPUT));

        // Invert
        const float switchCol = 9.522f;
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 150.340f), module, PolyTweakIIModule::INV_GATE_INPUT));
        addParam(createParamCentered<CKSSThree>(Vec(switchCol, 178.404f), module, PolyTweakIIModule::INV_MODE_PARAM));

        // Disable
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 214.466f), module, PolyTweakIIModule::DISABLE_GATE_INPUT));
        addParam(createParamCentered<CKSS>(Vec(switchCol, 238.677f), module, PolyTweakIIModule::DISABLE_MODE_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrClm, 267.705f), module, PolyTweakIIModule::DISABLE_VALUE_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 298.304f), module, PolyTweakIIModule::DISABLE_VALUE_INPUT));

        // Poly-output
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 332.694f), module, PolyTweakIIModule::POLY_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        PolyTweakIIModule* module = dynamic_cast<PolyTweakIIModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);
       
        std::vector<std::string> gateNonInvModeNames = {
            "Pass through",
            "High/low-gate"
        };
        menu->addChild(createIndexPtrSubmenuItem("Gate mode: non-inverted channels", gateNonInvModeNames,
            &module->gateNonInvMode.req));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelPolyTweakII = createModel<PolyTweakIIModule, PolyTweakIIModuleWidget>("PolyTweakII");