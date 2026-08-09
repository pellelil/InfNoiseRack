// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct CrossFadeSwitch4to1Module : InfNoiseModule {
    enum ParamId {
        SELECT_PARAM,
        SELECT_TRIM_PARAM,
        SELECTMODE_PARAM,
        CVMODE_PARAM,
        COUNT_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        SELECT_INPUT,
        RESET_INPUT,
        CV1_INPUT,
        CV2_INPUT,
        CV3_INPUT,
        CV4_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        CV_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        IN1_LIGHT,
        IN2_LIGHT,
        IN3_LIGHT,
        IN4_LIGHT,
        LIGHTS_LEN
    };

    int inputCount = 2; // 2, 3 or 4 (params[COUNT_PARAM] + 2)
    float fadeRange = 2.f;
    int channels = 1;
    bool haveOutputs = false;
    enum triggerOrderMode { tom_Next, tom_Prev, tom_PingPong, tom_Rnd, tom_RndDiff };
    actReqValue<triggerOrderMode> trigOrder = actReqValue<triggerOrderMode>(tom_Next);
    actReqValue<voltValue> normCvInVolt[4] = {
        actReqValue<voltValue>(v_zero),
        actReqValue<voltValue>(v_zero),
        actReqValue<voltValue>(v_zero),
        actReqValue<voltValue>(v_zero)
    };
    enum triggerDirection { td_Up, td_Down };
    triggerDirection pingPongDir = td_Down;
    dsp::SchmittTrigger selectTrig;
    dsp::SchmittTrigger resetTrig;
    
	CrossFadeSwitch4to1Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configParam(SELECT_PARAM, 0.f, 1.f, 0.f, "Switch/cross-fade (1 to n)", "", 0, 1);
        configSwitch(SELECTMODE_PARAM, 0.0f, 1.0f, 0.0f, "Switch/cross-fade", {"Switch mode", "Cross-fade mode"});
        configParam(SELECT_TRIM_PARAM, -1.f, 1.f, 0.f, "Switch/cross-fade CV-trim", "%", 0, 100);
        configInput(SELECT_INPUT, "Switch (trigger) / cross-fade (CV)");
        configInput(RESET_INPUT, "Reset (only trigger-mode)");
        configSwitch(CVMODE_PARAM, 0.0f, 1.0f, 0.0f, "CV-Mode", { "CV-Switch/Fade", "Trigger (switch only)" });

        configSwitch(COUNT_PARAM, 0.0, 2.0, 0.0, "Port count", { "2", "3", "4" });

        configInput(CV1_INPUT, "CV-1 (normalized via context-menu)");
        configInput(CV2_INPUT, "CV-2 (normalized via context-menu)");
        configInput(CV3_INPUT, "CV-3 (normalized via context-menu)");
        configInput(CV4_INPUT, "CV-4 (normalized via context-menu)");
        configOutput(CV_OUTPUT, "Switched/cross-faded");

        configLight(IN1_LIGHT, "Input-1 intensity");
        configLight(IN2_LIGHT, "Input-2 intensity");
        configLight(IN3_LIGHT, "Input-3 intensity");
        configLight(IN4_LIGHT, "Input-4 intensity");

        configBypass(CV1_INPUT, CV_OUTPUT);

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

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);

        trigOrder.setBoth(tom_Next);
        pingPongDir = td_Down;
        selectTrig.reset();
        resetTrig.reset();
        for (int i = 0; i < 4; i++) {
            normCvInVolt[i].setBoth(v_zero);
        }
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        trigOrder.setBoth((triggerOrderMode)getJsonInt(rootJ, "trigOrder", (int)tom_Next));
        pingPongDir = (triggerDirection)getJsonInt(rootJ, "pingPongDir", (int)td_Down);
        int normCvTmp[4];
        getJsonIntArray(rootJ, "normCvInVolt", normCvTmp, 4, (int)v_zero);
        for (int i = 0; i < 4; i++)
            normCvInVolt[i].setBoth((voltValue)normCvTmp[i]);
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "trigOrder", json_integer((int)trigOrder.req));
        json_object_set_new(rootJ, "pingPongDir", json_integer((int)pingPongDir));
        int normCvTmp[4];
        for (int i = 0; i < 4; i++)
            normCvTmp[i] = (int)normCvInVolt[i].req;
        setJsonIntArray(rootJ, "normCvInVolt", normCvTmp, 4);
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        if (trigOrder.needsUpdate())
        {
            trigOrder.updateActual();
            if (trigOrder.act != tom_PingPong)  // Reset direction for non-ping-pong
                pingPongDir = td_Down;
        }
        for (int i = 0; i < 4; i++) {
            if (normCvInVolt[i].needsUpdate()) {
                normCvInVolt[i].updateActual();
                if (inputInfos.size() > (unsigned)(CV1_INPUT + i) && inputInfos[CV1_INPUT + i]) {
                    inputInfos[CV1_INPUT + i]->name = polyPortPrefix() + string::f(
                        "CV-%d (normalized via menu to: %s)",
                        i + 1,
                        getVoltName(normCvInVolt[i].act).c_str()
                    );
                }
            }
        }

        // Cross-fade not available when in trigger-mode and input connected
        if (inputs[SELECT_INPUT].isConnected() && params[CVMODE_PARAM].getValue() > 0.5f) {
            params[SELECTMODE_PARAM].setValue(0.f);
		}

        inputCount = 2 + (int)params[COUNT_PARAM].getValue();
        fadeRange = inputCount; // 2, 3 or 4

        channels = 1;
        if (inputs[CV1_INPUT].isConnected()) 
            channels = std::max(channels, inputs[CV1_INPUT].getChannels());
        if (inputs[CV2_INPUT].isConnected())
            channels = std::max(channels, inputs[CV2_INPUT].getChannels());
        if (inputs[CV3_INPUT].isConnected())
            channels = std::max(channels, inputs[CV3_INPUT].getChannels());
        if (inputs[CV4_INPUT].isConnected())
            channels = std::max(channels, inputs[CV4_INPUT].getChannels());
        
        haveOutputs = outputs[CV_OUTPUT].isConnected();
        outputs[CV_OUTPUT].setChannels(channels);

        if (!inputs[RESET_INPUT].isConnected()) {
			resetTrig.reset();
		}

        //--------------------
        postProcessParams(args);
    }

    inline void getSelection(int &slctLw, int &slctHi, float &factLw, float &factHi) {
        float rawSlct = params[SELECT_PARAM].getValue();
        if (params[CVMODE_PARAM].getValue() < 0.5f && inputs[SELECT_INPUT].isConnected()) {
            rawSlct += inputs[SELECT_INPUT].getVoltage() / 10.f * params[SELECT_TRIM_PARAM].getValue();
            rawSlct = clamp(rawSlct, 0.f, 1.f);
        }

        if (params[SELECTMODE_PARAM].getValue() > 0.5) {  // Cross-fade mode
            // Scale rawSlct to the range of available inputs
            float scaledSlct = rawSlct * (inputCount - 1);
            scaledSlct = clamp(scaledSlct, 0.0f, inputCount - 1.0f);

            // Determine lower and upper input indices
            slctLw = static_cast<int>(std::floor(scaledSlct));
            slctHi = std::min(slctLw + 1, inputCount - 1);

            // Calculate interpolation factors
            factHi = scaledSlct - slctLw;
            factLw = 1.0f - factHi;
        }
        else {  // Select mode
            // Select Mode: Divide the range into equal sections and pick the corresponding input
            float sectionSize = 1.0f / inputCount;
            slctLw = slctHi = static_cast<int>(std::floor(rawSlct / sectionSize));

            // Clamp to ensure valid range
            slctLw = slctHi = clamp(slctLw, 0, inputCount - 1);

            // In select mode, only one input is active
            factLw = 1.0f;
            factHi = 0.0f;
        }
	}

    void process(const ProcessArgs& args) override {
        bool doProcessParams = mustProcessParams || 
            ((cycle256 & patternProcessParams) == patternProcessParams);
        if (doProcessParams)
            processParams(args);

        bool doProcess = (doProcessParams ||
            ((cycle256 & processQualityPatterns[procQuality.act]) == processQualityPatterns[procQuality.act]));

        if (doProcess && haveOutputs) {
            int slctLw = 0, slctHi = 0;
            float factLw = 1.f, factHi = 0.f;
            getSelection(slctLw, slctHi, factLw, factHi);

            // Handle reset-input
            bool wasReset = false;
            if (inputs[RESET_INPUT].isConnected() && params[CVMODE_PARAM].getValue() > 0.5f) {
                wasReset = resetTrig.process(inputs[RESET_INPUT].getVoltage(), trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act]);
                if (wasReset) {
                    pingPongDir = td_Down;
                    params[SELECT_PARAM].setValue(0.f);
                    getSelection(slctLw, slctHi, factLw, factHi);
                }
            }

            // Handle trigger-input (when in trigger-mode)
            if (!wasReset && params[CVMODE_PARAM].getValue() > 0.5f && inputs[SELECT_INPUT].isConnected() &&
                selectTrig.process(inputs[SELECT_INPUT].getVoltage(), trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act])) {
                switch (trigOrder.act) {
                    case tom_Next:
                        params[SELECT_PARAM].setValue(fmod(params[SELECT_PARAM].getValue() + 1.f / inputCount, 1.f));
                        getSelection(slctLw, slctHi, factLw, factHi);
                        break;
                    case tom_Prev:
                        params[SELECT_PARAM].setValue(fmod(params[SELECT_PARAM].getValue() - 1.f / inputCount + 1.f, 1.f));
                        getSelection(slctLw, slctHi, factLw, factHi);
                        break;
                    case tom_PingPong:
                        if (pingPongDir == td_Up && slctLw == 0) pingPongDir = td_Down;
                        else if (pingPongDir == td_Down && slctLw == inputCount-1) pingPongDir = td_Up;

                        if (pingPongDir == td_Down) {
                            params[SELECT_PARAM].setValue(fmod(params[SELECT_PARAM].getValue() + 1.f / inputCount, 1.f));
                        }
                        else { // td_Up
                            params[SELECT_PARAM].setValue(fmod(params[SELECT_PARAM].getValue() - 1.f / inputCount + 1.f, 1.f));
                        }
                        getSelection(slctLw, slctHi, factLw, factHi);
                        break;
                    case tom_Rnd:
                        params[SELECT_PARAM].setValue(random::uniform());
                        getSelection(slctLw, slctHi, factLw, factHi);
                        break;
                    case tom_RndDiff:
                        int currSel = slctLw;
                        while (currSel == slctLw) {
                            params[SELECT_PARAM].setValue(random::uniform());
                            getSelection(slctLw, slctHi, factLw, factHi);
                        }
                        break;
                }
            }

            // Cross-fade or switch inputs
            for (int c = 0; c < channels; c++)
            {
                float lowVoltage = inputs[CV1_INPUT + slctLw].isConnected()
                    ? inputs[CV1_INPUT + slctLw].getPolyVoltage(c)
                    : voltValues[normCvInVolt[slctLw].act];
                float highVoltage = inputs[CV1_INPUT + slctHi].isConnected()
                    ? inputs[CV1_INPUT + slctHi].getPolyVoltage(c)
                    : voltValues[normCvInVolt[slctHi].act];
                float voltage = lowVoltage * factLw + highVoltage * factHi;
                voltage = clipToVoltRange(voltage, outClipRange.act);
                outputs[CV_OUTPUT].setVoltage(voltage, c);
            }

            // Only update lights every 256th cycle
            if (doProcessParams) {  
                lights[IN1_LIGHT].setBrightness(0.f);
                lights[IN2_LIGHT].setBrightness(0.f);
                lights[IN3_LIGHT].setBrightness(0.f);
                lights[IN4_LIGHT].setBrightness(0.f);

                lights[IN1_LIGHT + slctLw].setBrightness(factLw);
                if (slctLw != slctHi) {
                    lights[IN1_LIGHT + slctHi].setBrightness(factHi);
                }
            }
        }

        cycle256++;
    }
};

struct CrossFadeSwitch4to1ModuleWidget : InfNoiseModuleWidget {
    CrossFadeSwitch4to1ModuleWidget(CrossFadeSwitch4to1Module *module) {
        initializeWidget(module, "res/CrossFadeSwitch4to1");

        const float cntClm = 15.f;
        const float btnClm = 4.734f;
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(Vec(btnClm, 35.487f), module, CrossFadeSwitch4to1Module::SELECTMODE_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntClm, 47.843f), module, CrossFadeSwitch4to1Module::SELECT_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(cntClm, 70.656f), module, CrossFadeSwitch4to1Module::SELECT_TRIM_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntClm, 93.825f), module, CrossFadeSwitch4to1Module::SELECT_INPUT));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(btnClm, 108.785f), module, CrossFadeSwitch4to1Module::CVMODE_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntClm, 128.124f), module, CrossFadeSwitch4to1Module::RESET_INPUT));

        addParam(createParamCentered<CKSSThree>(Vec(11.291f, 155.807f), module, CrossFadeSwitch4to1Module::COUNT_PARAM));

        const float lightClm = 4.879;
        const float lightOfs = 10.422f;
        const float rowSpacing = 35.4805f;
        float row = 190.973f;
        for (int i = 0; i < 4; i++) {
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntClm, row), module, CrossFadeSwitch4to1Module::CV1_INPUT + i));
            addChild(createLightCentered<TinyLight<GreenLight>>(Vec(lightClm, row + lightOfs), module, CrossFadeSwitch4to1Module::IN1_LIGHT + i));
            row += rowSpacing;
        }

        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntClm, row), module, CrossFadeSwitch4to1Module::CV_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        CrossFadeSwitch4to1Module* module = dynamic_cast<CrossFadeSwitch4to1Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> trigOrderNames = { "Next", "Prev", "Ping-pong", "Random", "Random Diff" };
        menu->addChild(createIndexPtrSubmenuItem("Trigger-order", trigOrderNames,
            &module->trigOrder.req));

        std::vector<std::string> voltNames = getVoltValuesNames();
        menu->addChild(createIndexPtrSubmenuItem("CV-1 normalized input", voltNames,
            &module->normCvInVolt[0].req));
        menu->addChild(createIndexPtrSubmenuItem("CV-2 normalized input", voltNames,
            &module->normCvInVolt[1].req));
        menu->addChild(createIndexPtrSubmenuItem("CV-3 normalized input", voltNames,
            &module->normCvInVolt[2].req));
        menu->addChild(createIndexPtrSubmenuItem("CV-4 normalized input", voltNames,
            &module->normCvInVolt[3].req));
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelCrossFadeSwitch4to1 = createModel<CrossFadeSwitch4to1Module, CrossFadeSwitch4to1ModuleWidget>("CrossFadeSwitch4to1");