// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"

struct CrossFadeSwitch1to4Module : InfNoiseModule {
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
        CV_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        CV1_OUTPUT,
        CV2_OUTPUT,
        CV3_OUTPUT,
        CV4_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        OUT1_LIGHT,
        OUT2_LIGHT,
        OUT3_LIGHT,
        OUT4_LIGHT,
        LIGHTS_LEN
    };

    int inputCount = 2; // 2, 3 or 4 (params[COUNT_PARAM] + 2)
    float fadeRange = 2.f;
    int channels = 1;
    bool haveOutputs = false;
    enum triggerOrderMode { tom_Next, tom_Prev, tom_PingPong, tom_Rnd, tom_RndDiff };
    actReqValue<triggerOrderMode> trigOrder = actReqValue<triggerOrderMode>(tom_Next);
    enum triggerDirection { td_Up, td_Down };
    triggerDirection pingPongDir = td_Down;
    dsp::SchmittTrigger selectTrig;
    dsp::SchmittTrigger resetTrig;
    
	CrossFadeSwitch1to4Module() {
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

        configInput(CV_INPUT, "CV");
        configOutput(CV1_OUTPUT, "CV-1");
        configOutput(CV2_OUTPUT, "CV-2");
        configOutput(CV3_OUTPUT, "CV-3");
        configOutput(CV4_OUTPUT, "CV-4");

        configLight(OUT1_LIGHT, "Output-1 intensity");
        configLight(OUT2_LIGHT, "Output-2 intensity");
        configLight(OUT3_LIGHT, "Output-3 intensity");
        configLight(OUT4_LIGHT, "Output-4 intensity");

        configBypass(CV_INPUT, CV1_OUTPUT);

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
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        trigOrder.setBoth((triggerOrderMode)getJsonInt(rootJ, "trigOrder", (int)tom_Next));
        pingPongDir = (triggerDirection)getJsonInt(rootJ, "pingPongDir", (int)td_Down);
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "trigOrder", json_integer((int)trigOrder.req));
        json_object_set_new(rootJ, "pingPongDir", json_integer((int)pingPongDir));
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

        // Cross-fade not available when in trigger-mode and input connected
        if (inputs[SELECT_INPUT].isConnected() && params[CVMODE_PARAM].getValue() > 0.5f) {
            params[SELECTMODE_PARAM].setValue(0.f);
		}

        inputCount = 2 + (int)params[COUNT_PARAM].getValue();
        fadeRange = inputCount; // 2, 3 or 4

        channels = (inputs[CV_INPUT].isConnected()) 
            ? inputs[CV_INPUT].getChannels()
            : 1;
        
        haveOutputs = outputs[CV1_OUTPUT].isConnected() || outputs[CV2_OUTPUT].isConnected() ||
            outputs[CV3_OUTPUT].isConnected() || outputs[CV4_OUTPUT].isConnected();
        outputs[CV1_OUTPUT].setChannels(channels);
        outputs[CV2_OUTPUT].setChannels(channels);
        outputs[CV3_OUTPUT].setChannels(channels);
        outputs[CV4_OUTPUT].setChannels(channels);

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
            float inVoltage = inputs[CV_INPUT].isConnected() 
                ? inputs[CV_INPUT].getVoltage() 
                : 0.f;
            for (int c = 0; c < channels; c++)
            {
                outputs[CV1_OUTPUT].setVoltage(0.f, c);
                outputs[CV2_OUTPUT].setVoltage(0.f, c);
                outputs[CV3_OUTPUT].setVoltage(0.f, c);
                outputs[CV4_OUTPUT].setVoltage(0.f, c);

                if (factLw > 0.f) {
                    float voltage = inVoltage * factLw;
                    voltage = clipToVoltRange(voltage, outClipRange.act);
                    outputs[CV1_OUTPUT + slctLw].setVoltage(voltage, c);
                }
                if (factHi > 0.f) {
                    float voltage = inVoltage * factHi;
                    voltage = clipToVoltRange(voltage, outClipRange.act);
                    outputs[CV1_OUTPUT + slctHi].setVoltage(voltage, c);
                }
            }

            // Only update lights every 256th cycle
            if (doProcessParams) {  
                lights[OUT1_LIGHT].setBrightness(0.f);
                lights[OUT2_LIGHT].setBrightness(0.f);
                lights[OUT3_LIGHT].setBrightness(0.f);
                lights[OUT4_LIGHT].setBrightness(0.f);

                lights[OUT1_LIGHT + slctLw].setBrightness(factLw);
                if (slctLw != slctHi) {
                    lights[OUT1_LIGHT + slctHi].setBrightness(factHi);
                }
            }
        }

        cycle256++;
    }
};

struct CrossFadeSwitch1to4ModuleWidget : InfNoiseModuleWidget {
    CrossFadeSwitch1to4ModuleWidget(CrossFadeSwitch1to4Module *module) {
        initializeWidget(module, "res/CrossFadeSwitch1to4");

        const float cntClm = 15.f;
        const float btnClm = 4.734f;
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(Vec(btnClm, 35.487f), module, CrossFadeSwitch1to4Module::SELECTMODE_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntClm, 47.843f), module, CrossFadeSwitch1to4Module::SELECT_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(cntClm, 70.656f), module, CrossFadeSwitch1to4Module::SELECT_TRIM_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntClm, 93.825f), module, CrossFadeSwitch1to4Module::SELECT_INPUT));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(btnClm, 108.785f), module, CrossFadeSwitch1to4Module::CVMODE_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntClm, 128.124f), module, CrossFadeSwitch1to4Module::RESET_INPUT));

        addParam(createParamCentered<CKSSThree>(Vec(11.291f, 155.807f), module, CrossFadeSwitch1to4Module::COUNT_PARAM));
        
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntClm, 190.973f), module, CrossFadeSwitch1to4Module::CV_INPUT));
        const float lightClm = 4.879;
        const float lightOfs = 10.422f;
        const float rowSpacing = 35.4805f;
        float row = 226.454f;
        for (int i = 0; i < 4; i++) {
            addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntClm, row), module, CrossFadeSwitch1to4Module::CV1_OUTPUT + i));
            addChild(createLightCentered<TinyLight<GreenLight>>(Vec(lightClm, row + lightOfs), module, CrossFadeSwitch1to4Module::OUT1_LIGHT + i));
            row += rowSpacing;
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        CrossFadeSwitch1to4Module* module = dynamic_cast<CrossFadeSwitch1to4Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> trigOrderNames = { "Next", "Prev", "Ping-pong", "Random", "Random Diff" };
        menu->addChild(createIndexPtrSubmenuItem("Trigger-order", trigOrderNames,
            &module->trigOrder.req));
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelCrossFadeSwitch1to4 = createModel<CrossFadeSwitch1to4Module, CrossFadeSwitch1to4ModuleWidget>("CrossFadeSwitch1to4");