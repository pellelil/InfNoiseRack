// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct PolyShuffleModule : InfNoiseModule {
    enum ParamId {
        AUTO_CHANNEL_COUNT_PARAM,
        MAN_CHANNEL_COUNT_PARAM,
        VALUE_MODE_PARAM,
        VALUE_PARAM,
        RESET_BTN_PARAM,
        ORDER_MODE_PARAM,
        TRIGGER_BTN_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        POLY_INPUT,
        RESET_INPUT,
        TRIGGER_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        POLY_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        ENUMS(CHNL1_LIGHT, 2),
        ENUMS(CHNL2_LIGHT, 2),
        ENUMS(CHNL3_LIGHT, 2),
        ENUMS(CHNL4_LIGHT, 2),
        ENUMS(CHNL5_LIGHT, 2),
        ENUMS(CHNL6_LIGHT, 2),
        ENUMS(CHNL7_LIGHT, 2),
        ENUMS(CHNL8_LIGHT, 2),
        ENUMS(CHNL9_LIGHT, 2),
        ENUMS(CHNL10_LIGHT, 2),
        ENUMS(CHNL11_LIGHT, 2),
        ENUMS(CHNL12_LIGHT, 2),
        ENUMS(CHNL13_LIGHT, 2),
        ENUMS(CHNL14_LIGHT, 2),
        ENUMS(CHNL15_LIGHT, 2),
        ENUMS(CHNL16_LIGHT, 2),
        VALUE_MODE_VALUE_LIGHT,
        VALUE_MODE_REPEAT_LIGHT,
        LIGHTS_LEN
    };

    bool haveOutput = false;
    bool haveInput = true;
    bool updateLights = true;  // Indicate channel-count/order has changed
    int suppressResetAfterJsonLoad = 0;  // Keep saved order while startup I/O/channel state settles
    int inChannels = 1;  // Number of input channels (or 1 if no input)
    int outChannels = 1;  // Number of output channels (1..16)
    int lastOutChannels = -1;  // Used to detect when output channels have changed
    int channelOrder[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    enum valueModeType { vm_value, vm_repeat };
    actReqValue<valueModeType> valueMode = actReqValue<valueModeType>(vm_value);
    float valueKnob = 0.f;  // Value set by knob
    dsp::SchmittTrigger modeTrigger = dsp::SchmittTrigger();
    dsp::SchmittTrigger resetTrigger = dsp::SchmittTrigger();
    dsp::SchmittTrigger triggerTrigger = dsp::SchmittTrigger();
    bool orderModeNext = true;
    bool orderModeShuffle = false;
    // orderModePrev if neither orderModeNext nor orderModeShuffle is true

	PolyShuffleModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configInput(POLY_INPUT, "Polyphonic (up to 16 channels)");
        for (int i = 0; i < 16; i++)
        {
            int lgtIdx = i * 2;
            configLight(CHNL1_LIGHT + lgtIdx, string::f("Input-channel-%d", i + 1));
        }
        
        configSwitch(AUTO_CHANNEL_COUNT_PARAM, 0.0, 1.0, 1.0, "Channel-mode", { "Manual (knob)", "Automatic (input)" });
        configSwitch(MAN_CHANNEL_COUNT_PARAM, 1.0f, 16.0f, 1.0f, "Channel count", { "1 channel (monophonic)", 
            "2 channels", "3 channels", "4 channels", "5 channels", "6 channels", "7 channels", "8 channels", "9 channels", 
            "10 channels", "11 channels", "12 channels", "13 channels", "14 channels", "15 channels", "16 channels" });

        configSwitch(VALUE_MODE_PARAM, 0.0, 1.0, 0.0, "Add-channel mode (toggle)");
        configLight(VALUE_MODE_VALUE_LIGHT, "Value (value-knob)");
        configLight(VALUE_MODE_REPEAT_LIGHT, "Repeat (input)");
        configParam(VALUE_PARAM, -10.0f, 10.0f, 0.0f, "Value", " V");

        configSwitch(RESET_BTN_PARAM, 0.0, 1.0, 0.0, "Reset");
        configInput(RESET_INPUT, "Reset trigger");

        configSwitch(ORDER_MODE_PARAM, 0.0, 2.0, 0.0, "Order-mode", { "Next", "Previous", "Shuffle" });
        configSwitch(TRIGGER_BTN_PARAM, 0.0, 1.0, 0.0, "Trigger");
        configInput(TRIGGER_INPUT, "Trigger");

        configOutput(POLY_OUTPUT, "Polyphonic");

        configBypass(POLY_INPUT, POLY_OUTPUT);

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
		haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = true;  
		haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = true;
		haveTrigHighLow = false;
	}

    void channelOrderReset() {
        lastOutChannels = -1;
        for (int i = 0; i < 16; i++) {
            channelOrder[i] = i;
        }
        updateLights = true;
    }

    void channelOrderShuffle() {
        for (int i = 0; i < 16; i++) {
            if (i < outChannels && outChannels > 1) {
                int swapIdx = random::u32() % outChannels;
                if (swapIdx == i)
                    continue;

                int temp = channelOrder[i];
                channelOrder[i] = channelOrder[swapIdx];
                channelOrder[swapIdx] = temp;
            }
            else {
                channelOrder[i] = i;
            }
        }
        updateLights = true;
    }

    void channelOrderNext() {
        int lastIndex = outChannels - 1;
        int temp = channelOrder[0];
        for (int i = 0; i < lastIndex; i++) {
            channelOrder[i] = channelOrder[i + 1];
        }
        channelOrder[lastIndex] = temp;
        updateLights = true;
    }

    void channelOrderPrev() {
        int lastIndex = outChannels - 1;
        int temp = channelOrder[lastIndex];
        for (int i = lastIndex; i > 0; i--) {
            channelOrder[i] = channelOrder[i - 1];
        }
        channelOrder[0] = temp;
        updateLights = true;
    }

    void refreshChannelLightTooltips() {
        for (int i = 0; i < 16; i++) {
            int lgtIdx = i * 2;
            int lightInfoIdx = CHNL1_LIGHT + lgtIdx;
            if ((int)lightInfos.size() > lightInfoIdx && lightInfos[lightInfoIdx]) {
                if (i < outChannels) {
                    lightInfos[lightInfoIdx]->name = string::f("Input-channel-%d%s",
                        channelOrder[i] + 1,
                        channelOrder[i] >= inChannels ? " (added)" : "");
                }
                else {
                    lightInfos[lightInfoIdx]->name = string::f("Input-channel-%d (not in use)", i + 1);
                }
            }
        }
    }

    void refreshChannelOrderLights() {
        for (int i = 0; i < 16; i++) {
            float greenFactor = 0.f;
            float redFactor = 0.f;
            // Only light active output slots i < outChannels.
            if (i < outChannels) {
                if (outChannels == 1) {
                    greenFactor = 1.f;
                } else if (channelOrder[i] == 0) {  // First channel, so green
                    greenFactor = 1.f;
                } else if (channelOrder[i] + 1 == outChannels) {  // Last channel, so red
                    redFactor = 1.f;
                } else {  // "in-between" channels, fade green to red
                    greenFactor = ((float)outChannels - (float)channelOrder[i]) / (float)outChannels;
                    redFactor = 1.f - greenFactor;
                }
            }

            float brightFactor = (channelOrder[i] < inChannels) ? 1.f : 0.5f; // dim to 50%, if added channel
            int lgtIdx = i * 2;
            lights[CHNL1_LIGHT + lgtIdx].setBrightness(greenFactor * brightFactor);
            lights[CHNL1_LIGHT + lgtIdx + 1].setBrightness(redFactor * brightFactor);
        }
    }

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        valueMode.setBoth(valueModeType::vm_value);
        lastOutChannels = -1; // resetChannelOrder() will be called in processParams()
        updateLights = true;
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        haveInput = getJsonBool(rootJ, "haveInput", false);
        valueMode.setBoth((valueModeType)getJsonInt(rootJ, "valueMode", (int)valueModeType::vm_value));
        lastOutChannels = getJsonInt(rootJ, "lastOutChannels", -1);
        static const int defaultChannelOrder[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
        getJsonIntArray(rootJ, "channelOrder", channelOrder, 16, defaultChannelOrder);

        updateLights = true;  // New order might have been loaded from json
        suppressResetAfterJsonLoad = 8;
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "haveInput", json_boolean(haveInput));
        json_object_set_new(rootJ, "valueMode", json_integer((int)valueMode.req));
        json_object_set_new(rootJ, "lastOutChannels", json_integer(lastOutChannels));
        setJsonIntArray(rootJ, "channelOrder", channelOrder, 16);
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------
       
        // Reset order (delayed) when connecting/disconnecting poly-cable
        bool doReset = false;
        bool newHaveInput = inputs[POLY_INPUT].isConnected();
        if (haveInput != newHaveInput) {  
            doReset = true;
            haveInput = newHaveInput;
        }

        // Handle channel-count
        bool useAutoCount = params[AUTO_CHANNEL_COUNT_PARAM].getValue() > 0.5f;
        int rawInChannels = haveInput
            ? inputs[POLY_INPUT].getChannels() 
            : 0;
        inChannels = haveInput
            ? rawInChannels
            : 1;
        if (useAutoCount) {
            outChannels = inChannels;
            params[MAN_CHANNEL_COUNT_PARAM].setValue((int)outChannels);
        }
        else {
            outChannels = (int)params[MAN_CHANNEL_COUNT_PARAM].getValue();
        }
        
        haveOutput = outputs[POLY_OUTPUT].isConnected();
        outputs[POLY_OUTPUT].setChannels(outChannels);

        // Reset channel order if output channels have changed, or cable has been connected/disconnected
        doReset = doReset || (outChannels != lastOutChannels);
        if (suppressResetAfterJsonLoad > 0) {
            // During patch load, auto-channel count / cable state can be transient for several passes.
            suppressResetAfterJsonLoad--;
            if (doReset) {
                // Keep loaded order, but still refresh lights/tooltips to reflect current active channel count.
                updateLights = true;
            }
            doReset = false;
            lastOutChannels = outChannels;
        }
        if (doReset) {
            channelOrderReset(); // sets updateLights to true
            lastOutChannels = outChannels;
        }   

        if (updateLights) {
            refreshChannelOrderLights();
            refreshChannelLightTooltips();
            updateLights = false;
        }

        if (valueMode.needsUpdate() || doReset) {
            valueMode.updateActual();
            float brigth = outChannels > rawInChannels 
                ? 1.0f  // Outputting more channels than inputting, so light up fully
                : 0.5f; // Outputting same or less channels than inputting, so light up half
            lights[VALUE_MODE_VALUE_LIGHT].setBrightness(valueMode.act == vm_value ? brigth : 0.f);
            lights[VALUE_MODE_REPEAT_LIGHT].setBrightness(valueMode.act == vm_repeat ? brigth : 0.f);
        }

        valueKnob = params[VALUE_PARAM].getValue();

        orderModeNext = params[ORDER_MODE_PARAM].getValue() < 0.5f;
        orderModeShuffle = params[ORDER_MODE_PARAM].getValue() > 1.5f;
        // if neither then orderModePrev 

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
            // Check for mode-toggle 
            if (modeTrigger.process(params[VALUE_MODE_PARAM].getValue(), 0.1f, 0.9f))
            {
                valueMode.setBoth(valueMode.req == vm_value ? vm_repeat : vm_value);
            }

            // Check for reset
            float voltage = params[RESET_BTN_PARAM].getValue() > 0.5f 
                ? 10.f 
                : (inputs[RESET_INPUT].isConnected())
                    ? inputs[RESET_INPUT].getVoltage()
                    : 0.f;
            if (resetTrigger.process(voltage,
                        trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act]))
            {
                channelOrderReset();
            }

            // Check for trigger (do: next, shuffle, or prev)
            voltage = params[TRIGGER_BTN_PARAM].getValue() > 0.5f 
                ? 10.f 
                : (inputs[TRIGGER_INPUT].isConnected())
                    ? inputs[TRIGGER_INPUT].getVoltage()
                    : 0.f;
            if (triggerTrigger.process(voltage,
                        trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act]))
            {
                if (orderModeNext)
                    channelOrderNext();
                else if (orderModeShuffle)
                    channelOrderShuffle();
                else
                    channelOrderPrev();
            }

            if (haveOutput) { // Generate output                
                for (int c = 0; c < outChannels; c++) {
                    voltage = valueKnob; // fallback to value-mode (value-knob)
                    if (haveInput) {
                        int inChannel = channelOrder[c];
                        if (inChannel < inChannels)
                            voltage = inputs[POLY_INPUT].getVoltage(inChannel);
                        else if (valueMode.act == vm_repeat) { 
                            inChannel %= inChannels;
                            voltage = inputs[POLY_INPUT].getVoltage(inChannel);
                        }
                    }

                    outputs[POLY_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), c);
                }
            }
        }

        cycle256++;
    }
};

struct PolyShuffleModuleWidget : InfNoiseModuleWidget {
    PolyShuffleModuleWidget(PolyShuffleModule *module) {
        initializeWidget(module, "res/PolyShuffle");

        // Poly-input
        const float cntrCol = 15.f;
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 51.428f), module, PolyShuffleModule::POLY_INPUT));

        // Order lights
        const float lftLgtCol = 12.244f;
        const float rgtLgtCol = 17.556f;
        const float lgtSpacing = 7.003f;
        float ltgRow = 67.623f;
        for (int i = 0; i < 8; i++) {
            int lgtIdx = i * 2;
			addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(lftLgtCol, ltgRow), module, PolyShuffleModule::CHNL1_LIGHT + lgtIdx));
			addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(rgtLgtCol, ltgRow), module, PolyShuffleModule::CHNL9_LIGHT + lgtIdx));
			ltgRow += lgtSpacing;
		}

        // Channel-count
        const float smallBtnCol = 4.852f;
        infNoiseLtSmallButton* autoCountBtn = createParamCentered<infNoiseLtSmallButton>(Vec(smallBtnCol, 126.389f), module, PolyShuffleModule::AUTO_CHANNEL_COUNT_PARAM);
        autoCountBtn->setup(bc_green, false);
        addParam(autoCountBtn);
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrCol, 142.023f), module, PolyShuffleModule::MAN_CHANNEL_COUNT_PARAM));

        // Mode and value
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(11.958f, 157.521f), module, PolyShuffleModule::VALUE_MODE_VALUE_LIGHT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(20.271f, 157.521f), module, PolyShuffleModule::VALUE_MODE_REPEAT_LIGHT));
        infNoiseLtSmallButton* modeBtn = createParamCentered<infNoiseLtSmallButton>(Vec(smallBtnCol, 161.772f), module, PolyShuffleModule::VALUE_MODE_PARAM);
        modeBtn->setup(bc_green, true);
        addParam(modeBtn);
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrCol, 176.485f), module, PolyShuffleModule::VALUE_PARAM));

        // Reset-input
        infNoiseLtSmallButton* resetBtn = createParamCentered<infNoiseLtSmallButton>(Vec(smallBtnCol, 203.796f), module, PolyShuffleModule::RESET_BTN_PARAM);
        resetBtn->setup(bc_red, true);
        addParam(resetBtn);
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrCol, 220.046f), module, PolyShuffleModule::RESET_INPUT));


        // Order-mode/trigger
        addParam(createParamCentered<CKSSThree>(Vec(8.976f, 260.843f), module, PolyShuffleModule::ORDER_MODE_PARAM));
        infNoiseLtSmallButton* triggerBtn = createParamCentered<infNoiseLtSmallButton>(Vec(smallBtnCol, 281.935f), module, PolyShuffleModule::TRIGGER_BTN_PARAM);
        triggerBtn->setup(bc_red, true);
        addParam(triggerBtn);
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrCol, 297.632f), module, PolyShuffleModule::TRIGGER_INPUT));
        

        // Poly-output
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 332.694f), module, PolyShuffleModule::POLY_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        PolyShuffleModule* module = dynamic_cast<PolyShuffleModule*>(this->module);
        assert(module);

        //menu->addChild(new MenuSeparator);
        
        /*
		menu->addChild(createIndexPtrSubmenuItem("Light brightness-scale",
		 	{"5V (e.g. bipolar -5V to +5V)", "10V (e.g. unipolar 0V to +10V)"},
		 	&module->lightScaleMode.req
        ));
        */
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelPolyShuffle = createModel<PolyShuffleModule, PolyShuffleModuleWidget>("PolyShuffle");