// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inUtil.hpp"

struct ManCV8IIModule : InfNoiseModule {
    enum ParamId {
        ALL_BUTTON_PARAM,
        ALL_BUTTON_LATCH_PARAM,
        ATTENUVERT_PARAM,
        BUTTON1_PARAM,
        BUTTON2_PARAM,
        BUTTON3_PARAM,
        BUTTON4_PARAM,
        BUTTON5_PARAM,
        BUTTON6_PARAM,
        BUTTON7_PARAM,
        BUTTON8_PARAM,
        BUTTON1_LATCH_PARAM,
        BUTTON2_LATCH_PARAM,
        BUTTON3_LATCH_PARAM,
        BUTTON4_LATCH_PARAM,
        BUTTON5_LATCH_PARAM,
        BUTTON6_LATCH_PARAM,
        BUTTON7_LATCH_PARAM,
        BUTTON8_LATCH_PARAM,
        OFF_KNOB1_PARAM,
        OFF_KNOB2_PARAM,
        OFF_KNOB3_PARAM,
        OFF_KNOB4_PARAM,
        OFF_KNOB5_PARAM,
        OFF_KNOB6_PARAM,
        OFF_KNOB7_PARAM,
        OFF_KNOB8_PARAM,
        ON_KNOB1_PARAM,
        ON_KNOB2_PARAM,
        ON_KNOB3_PARAM,
        ON_KNOB4_PARAM,
        ON_KNOB5_PARAM,
        ON_KNOB6_PARAM,
        ON_KNOB7_PARAM,
        ON_KNOB8_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        ALL_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        MAN_POLY_OUTPUT,
        MAN1_OUTPUT,
        MAN2_OUTPUT,
        MAN3_OUTPUT,
        MAN4_OUTPUT,
        MAN5_OUTPUT,
        MAN6_OUTPUT,
        MAN7_OUTPUT,
        MAN8_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        POLY_HINT_LIGHT,
        LIGHTS_LEN
    };

    enum attenuateMode { am_both, am_onlyOn, am_onlyOff };
    actReqValue<attenuateMode> attMode = actReqValue<attenuateMode>(am_both);
    enum allButtonMode { abm_Off, abm_On, abm_Toggle };
    actReqValue<allButtonMode> allMode = actReqValue<allButtonMode>(abm_Toggle);
    bool haveOutput = false;
    int firstIdx = -1;
    int lastIdx = -1;
    actReqValue<polyphonyMode> polyOutput = actReqValue<polyphonyMode>(poly_8);
    int polyChannels = 8;
    infNoiseButtonTrigger btAll = infNoiseButtonTrigger();
    dsp::TSchmittTrigger<float> allInputTrigger;
    infNoiseButtonTrigger btVirtualAll = infNoiseButtonTrigger(); // "Virtual button" for all-trigger/gate

    ManCV8IIModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
        configLight(POLY_HINT_LIGHT, "Poly output: lit when channel count is not 8");

        configSwitch(ALL_BUTTON_PARAM, 0.0f, 1.0f, 0.0f, "ALL button", { "OFF", "ON" });
        configSwitch(ALL_BUTTON_LATCH_PARAM, 0.0f, 1.0f, 0.0f, "ALL Latch ON/OFF-button", { "Unlatched", "Latched" });
        configInput(ALL_INPUT, "All-trigger/gate (trigger when latched, otherwise gate)");
        configParam(ATTENUVERT_PARAM, -1.f, 1.f, 1.f, "Attenuvert-level or -trim (-1x to +1x)", " x", 0, 1);
        configOutput(MAN_POLY_OUTPUT, "Manual CV-polyphonic");

        for (int i = 0; i < 8; i++) {
            configSwitch(BUTTON1_PARAM + i, 0.0f, 1.0f, 0.0f, string::f("ON/OFF-button %d", i + 1), { "OFF", "ON" });
            configSwitch(BUTTON1_LATCH_PARAM + i, 0.0f, 1.0f, 0.0f, string::f("Latch ON/OFF-button %d", i + 1), { "Unlatched", "Latched" });
            configParam(ON_KNOB1_PARAM + i, -10.0f, 10.0f, 0.0f, string::f("Value to output if ON %d (-10V to +10V)", i + 1), " V");
            configParam(OFF_KNOB1_PARAM + i, -10.0f, 10.0f, 0.0f, string::f("Value to output if OFF %d (-10V to +10V)", i + 1), " V");
            configOutput(MAN1_OUTPUT + i, string::f("ON/OFF %d (-10V to +10V)", i + 1));
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
        
        procQuality.setBoth(pq_balancedRate); // Ballanced rate (every 16th cycle)

        btAll.reset();
        allInputTrigger.reset();
        btVirtualAll.reset();
        allMode.setBoth(abm_Toggle);
        attMode.setBoth(am_both);
        polyOutput.setBoth(poly_8);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);

        allMode.setBoth((allButtonMode)getJsonInt(rootJ, "allButtonMode", (int)allButtonMode::abm_Toggle));
        attMode.setBoth((attenuateMode)getJsonInt(rootJ, "attMode", (int)attenuateMode::am_both));
        polyOutput.setBoth((polyphonyMode)getJsonInt(rootJ, "polyOutput", (int)polyphonyMode::poly_8));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "allButtonMode", json_integer((int)allMode.req));
        json_object_set_new(rootJ, "attMode", json_integer((int)attMode.req));
        json_object_set_new(rootJ, "polyOutput", json_integer((int)polyOutput.req));
    }

    void setAllOffKnobs(std::vector<float> values) {
        assert(values.size() == 8);
        for (int i = 0; i < 8; i++)
            params[OFF_KNOB1_PARAM + i].setValue(values[i]);    
    }

    void setAllOnKnobs(std::vector<float> values) {
        assert(values.size() == 8);
        for (int i = 0; i < 8; i++)
            params[ON_KNOB1_PARAM + i].setValue(values[i]);    
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        attMode.updateActual();
        allMode.updateActual();

        // Handle All-input (if in use)
        if (inputs[ALL_INPUT].isConnected()) {
            bool virtualAllButton = allInputTrigger.process(inputs[ALL_INPUT].getVoltage(0), 
                trueDetectValues[td_triggerLow], trueDetectValues[td_triggerHigh]);
            if (btVirtualAll.process(virtualAllButton))
            {
                bool useAsGate = params[ALL_BUTTON_LATCH_PARAM].getValue() < 0.5f;
                if (btVirtualAll.isPressed()) {
                    if (!useAsGate) { // Toggle when latched (trigger)
                        float value = (params[ALL_BUTTON_PARAM].getValue() > 0.5f)
                            ? 0.0f 
                            : 1.0f;
                        params[ALL_BUTTON_PARAM].setValue(value);
					}
                    else { // "Press"(non-latched) All-button (gate)
                        params[ALL_BUTTON_PARAM].setValue(1.f);
					}
				}
				else if (useAsGate) { // "Release" (non-latched) All-button (gate)
                    params[ALL_BUTTON_PARAM].setValue(0.f);
                }
            }
		}
        else if (btVirtualAll.isPressed()) {  // Cable disconnected while "virtual button pressed"
            allInputTrigger.reset();
            btVirtualAll.reset();
            bool useAsGate = params[ALL_BUTTON_LATCH_PARAM].getValue() < 0.5f;
            if (useAsGate)
				params[ALL_BUTTON_PARAM].setValue(0.f); // "Release" (non-latched) All-button (gate)
		}

        // Handle All-button
        if (btAll.process(params[ALL_BUTTON_PARAM].getValue() > 0.5f)) {
            // only once when it fires
            if (btAll.isPressed()) {  
                if (allMode.act == abm_Toggle) {
                    for (int i = 0; i < 8; i++) {
                        float buttonValue = (params[BUTTON1_PARAM + i].getValue() > 0.5f) ? 0.0f : 1.0f;
                        params[BUTTON1_PARAM + i].setValue(buttonValue);
                    }
                }
			}
            else { // release non-latched buttons
                for (int i = 0; i < 8; i++) {
                    if (params[BUTTON1_LATCH_PARAM + i].getValue() < 0.5f)
                        params[BUTTON1_PARAM + i].setValue(0.f);
                }
            }
        }
        // Always
        if (btAll.isPressed()) {
            if (allMode.act == abm_On) {
                for (int i = 0; i < 8; i++) {
				    params[BUTTON1_PARAM + i].setValue(1.0f);
			    }
		    }   
            else if (allMode.act == abm_Off) {
                for (int i = 0; i < 8; i++) {
                    params[BUTTON1_PARAM + i].setValue(0.f);
                }
            }
        }

        // Check connected outputs, update portChannels set buttons and lights
        haveOutput = outputs[MAN_POLY_OUTPUT].isConnected();
        firstIdx = -1;
        lastIdx = -1;
        for (int i = 0; i < 8; i++) {
            // Check if output is connected and update portChannels
            outputs[MAN1_OUTPUT + i].setChannels(1);
            if (outputs[MAN1_OUTPUT + i].isConnected()) {
                haveOutput = true;
                if (firstIdx < 0)
					firstIdx = i;
				lastIdx = i;
            }
        }
        
        if (polyOutput.needsUpdate()) {
            polyOutput.updateActual();
            lights[POLY_HINT_LIGHT].setBrightness(polyOutput.req != poly_8 ? 1.f : 0.f);
            if (outputInfos.size() > (unsigned)MAN_POLY_OUTPUT && outputInfos[MAN_POLY_OUTPUT]) {
                outputInfos[MAN_POLY_OUTPUT]->name = polyPortPrefix() + "Poly output: " + getPolyphonyModeName(polyOutput.act);
            }
        }
        polyChannels = polyphonyModeChannels[polyOutput.act];
        outputs[MAN_POLY_OUTPUT].setChannels(polyChannels);

        //--------------------
        postProcessParams(args);
    }

    void process(const ProcessArgs& args) override {
        // React to possible ALL-input trigger/gate changes
        bool doProcessParams = mustProcessParams || inputs[ALL_INPUT].isConnected() ||
            ((cycle256 & patternProcessParams) == patternProcessParams);
        if (doProcessParams)
            processParams(args);

        bool doProcess = (doProcessParams ||
            ((cycle256 & processQualityPatterns[procQuality.act]) == processQualityPatterns[procQuality.act]));

        if (doProcess && haveOutput)
        {
            float att = params[ATTENUVERT_PARAM].getValue();
            float attOn = (attMode.act == am_onlyOff) ? 1.f : att;
            float attOff = (attMode.act == am_onlyOn) ? 1.f : att;

            // Manual knobs multiplied by attenuvert
            for (int i = 0; i < 8; i++) {
                bool isOn = params[BUTTON1_PARAM + i].getValue() > 0.5f;
                float voltage = (isOn)
                    ? params[ON_KNOB1_PARAM + i].getValue() * attOn
                    : params[OFF_KNOB1_PARAM + i].getValue() * attOff;
                voltage = quantizeToMode(voltage, outQuantize.act);
                voltage = clipToVoltRange(voltage, outClipRange.act);
                outputs[MAN1_OUTPUT + i].setVoltage(voltage);
                if (i < polyChannels)
                    outputs[MAN_POLY_OUTPUT].setVoltage(voltage, i);
            }
        }

        cycle256++;
    }
};

struct ManCV8IIModuleWidget : InfNoiseModuleWidget {
    infNoiseSmallButton<bc_blue, true>* allBtn;
    infNoiseSmallButton<bc_blue>* onOffBtn[8];

    ManCV8IIModuleWidget(ManCV8IIModule *module) {
        initializeWidget(module, "res/ManCV8II");

        const float onOffButnCol = 15.806f;
        const float latchButnCol = 26.965f;
        const float onKnobCol = 43.4980f;
        const float offKnobCol = 72.233f;
        const float outputCol = 103.545f;
        const float latchOffset = 8.362f;
        const float allAttRow = 52.120f;
        const float lgtOfs = 10.021f;
        allBtn = createParamCentered<infNoiseSmallButton<bc_blue, true>>(Vec(onOffButnCol, allAttRow), module, ManCV8IIModule::ALL_BUTTON_PARAM);
        addParam(allBtn);
        addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(latchButnCol, 62.175f), module, ManCV8IIModule::ALL_BUTTON_LATCH_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(onKnobCol, allAttRow), module, ManCV8IIModule::ALL_INPUT));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(offKnobCol, allAttRow), module, ManCV8IIModule::ATTENUVERT_PARAM));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(outputCol, allAttRow), module, ManCV8IIModule::MAN_POLY_OUTPUT));
        addChild(createLightCentered<TinyLight<RedLight>>(Vec(outputCol - lgtOfs, allAttRow - lgtOfs), module, ManCV8IIModule::POLY_HINT_LIGHT));

        float row = 87.194f;
        float rowSpacing = 35.0734f;
        for (int i = 0; i < 8; i++) {
            onOffBtn[i] = createParamCentered<infNoiseSmallButton<bc_blue>>(Vec(onOffButnCol, row), module, ManCV8IIModule::BUTTON1_PARAM + i);
            addParam(onOffBtn[i]);

            addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(latchButnCol, row + latchOffset), module, ManCV8IIModule::BUTTON1_LATCH_PARAM + i));

            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(onKnobCol, row), module, ManCV8IIModule::ON_KNOB1_PARAM + i));
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(offKnobCol, row), module, ManCV8IIModule::OFF_KNOB1_PARAM + i));

            addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(outputCol, row), module, ManCV8IIModule::MAN1_OUTPUT + i));

            row += rowSpacing;
        }
    }
    
    void step() override {
        if (module) {
            applyButtonMomentary(allBtn, module->params[ManCV8IIModule::ALL_BUTTON_LATCH_PARAM].getValue() < 0.5f);
            for (int i = 0; i < 8; i++) {
                applyButtonMomentary(onOffBtn[i], module->params[ManCV8IIModule::BUTTON1_LATCH_PARAM + i].getValue() < 0.5f);
            }
        }

        InfNoiseModuleWidget::step();
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        ManCV8IIModule* module = dynamic_cast<ManCV8IIModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createSubmenuItem("Set ON-knobs 1-8", "", [=](Menu* menu) {
            menu->addChild(createMenuItem("To zero", "(0, 0, 0, 0, 0, 0, 0, 0)", [=]() {
                module->setAllOnKnobs(std::vector<float>({ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}));
                }));
            menu->addChild(createMenuItem("To Interval", "(0, 1, 2, 3, 4, 5, 7, 12 semitones)", [=]() {
                module->setAllOnKnobs(std::vector<float>({ 0.f, 1.f/12.f, 2.f/12.f, 3.f/12.f, 4.f/12.f, 5.f/12.f, 7.f/12.f, 1.f}));
                }));
            menu->addChild(createMenuItem("To positive", "(0, 1, 2, 3, 4, 5, 8, 10)", [=]() {
                module->setAllOnKnobs(std::vector<float>({ 0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 8.f, 10.f}));
                }));
            menu->addChild(createMenuItem("To negative", "(0, -1, -2, -3, -4, -5, -8, -10)", [=]() {
                module->setAllOnKnobs(std::vector<float>({ 0.f, -1.f, -2.f, -3.f, -4.f, -5.f, -8.f, -10.f}));
                }));
            menu->addChild(createMenuItem("To negative/positive", "(-10, -5, -2, -1, 1, 2, 5, 10)", [=]() {
                module->setAllOnKnobs(std::vector<float>({ -10.f, -5.f, -2.f, -1.f, 1.f, 2.f, 5.f, 10.f}));
                }));
            menu->addChild(createMenuItem("To 5V", "(5, 5, 5, 5, 5, 5, 5, 5)", [=]() {
                module->setAllOnKnobs(std::vector<float>({ 5.f, 5.f, 5.f, 5.f, 5.f, 5.f, 5.f, 5.f}));
                }));
            menu->addChild(createMenuItem("To -5V", "(-5, -5, -5, -5, -5, -5, -5, -5)", [=]() {
                module->setAllOnKnobs(std::vector<float>({ -5.f, -5.f, -5.f, -5.f, -5.f, -5.f, -5.f, -5.f }));
                }));
            menu->addChild(createMenuItem("To 10V", "(10, 10, 10, 10, 10, 10, 10, 10)", [=]() {
                module->setAllOnKnobs(std::vector<float>({ 10.f, 10.f, 10.f, 10.f, 10.f, 10.f, 10.f, 10.f}));
                }));
            menu->addChild(createMenuItem("To -10V", "(-10, -10, -10, -10, -10, -10, -10, -10)", [=]() {
                module->setAllOnKnobs(std::vector<float>({ -10.f, -10.f, -10.f, -10.f, -10.f, -10.f, -10.f, -10.f }));
                }));
            appendScaleChordSetMenuItems(menu, module, ManCV8IIModule::ON_KNOB1_PARAM, 8);
            }));

        menu->addChild(createSubmenuItem("Set OFF-knobs 1-8", "", [=](Menu* menu) {
            menu->addChild(createMenuItem("To zero", "(0, 0, 0, 0, 0, 0, 0, 0)", [=]() {
                module->setAllOffKnobs(std::vector<float>({ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}));
                }));
            menu->addChild(createMenuItem("To Interval", "(0, 1, 2, 3, 4, 5, 7, 12 semitones)", [=]() {
                module->setAllOffKnobs(std::vector<float>({ 0.f, 1.f/12.f, 2.f/12.f, 3.f/12.f, 4.f/12.f, 5.f/12.f, 7.f/12.f, 1.f}));
                }));   
            menu->addChild(createMenuItem("To positive", "(0, 1, 2, 3, 4, 5, 8, 10)", [=]() {
                module->setAllOffKnobs(std::vector<float>({ 0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 8.f, 10.f}));
                }));
            menu->addChild(createMenuItem("To negative", "(0, -1, -2, -3, -4, -5, -8, -10)", [=]() {
                module->setAllOffKnobs(std::vector<float>({ 0.f, -1.f, -2.f, -3.f, -4.f, -5.f, -8.f, -10.f}));
                }));
            menu->addChild(createMenuItem("To negative/positive", "(-10, -5, -2, -1, 1, 2, 5, 10)", [=]() {
                module->setAllOffKnobs(std::vector<float>({ -10.f, -5.f, -2.f, -1.f, 1.f, 2.f, 5.f, 10.f}));
                }));
            menu->addChild(createMenuItem("To 5V", "(5, 5, 5, 5, 5, 5, 5, 5)", [=]() {
                module->setAllOffKnobs(std::vector<float>({ 5.f, 5.f, 5.f, 5.f, 5.f, 5.f, 5.f, 5.f}));
                }));
            menu->addChild(createMenuItem("To -5V", "(-5, -5, -5, -5, -5, -5, -5, -5)", [=]() {
                module->setAllOffKnobs(std::vector<float>({ -5.f, -5.f, -5.f, -5.f, -5.f, -5.f, -5.f, -5.f }));
                }));
            menu->addChild(createMenuItem("To 10V", "(10, 10, 10, 10, 10, 10, 10, 10)", [=]() {
                module->setAllOffKnobs(std::vector<float>({ 10.f, 10.f, 10.f, 10.f, 10.f, 10.f, 10.f, 10.f}));
                }));
            menu->addChild(createMenuItem("To -10V", "(-10, -10, -10, -10, -10, -10, -10, -10)", [=]() {
                module->setAllOffKnobs(std::vector<float>({ -10.f, -10.f, -10.f, -10.f, -10.f, -10.f, -10.f, -10.f }));
                }));
            appendScaleChordSetMenuItems(menu, module, ManCV8IIModule::OFF_KNOB1_PARAM, 8);
            }));

        menu->addChild(createSubmenuItem("Set ON/OFF buttons 1-8", "",
            [=](Menu* menu) {
                menu->addChild(createMenuItem("Latched", "", [=]() {
                    for (int i = 0; i < 8; i++)
                        module->params[ManCV8IIModule::BUTTON1_LATCH_PARAM + i].setValue(1.0f);
                }));
                menu->addChild(createMenuItem("Unlatched", "", [=]()  {
                    for (int i = 0; i < 8; i++)
                        module->params[ManCV8IIModule::BUTTON1_LATCH_PARAM + i].setValue(0.0f);
                }));
            }
        ));

        menu->addChild(createIndexPtrSubmenuItem("All-button mode",
            { "All OFF", "All ON", "Toggle ALL"},
            &module->allMode.req));

        menu->addChild(createIndexPtrSubmenuItem("Attenuate mode",
            { "Both", "Only ON-levels", "Only OFF-levels" },
            &module->attMode.req));

        std::vector<std::string> polyNames = getPolyphonyModeNames(false);
        polyNames.resize(8);
        menu->addChild(createIndexPtrSubmenuItem("Poly output channels", polyNames,
            &module->polyOutput.req));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelManCV8II = createModel<ManCV8IIModule, ManCV8IIModuleWidget>("ManCV8II");
