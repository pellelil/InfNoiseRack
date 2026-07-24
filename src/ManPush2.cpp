// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inUtil.hpp"

struct ManPush2Module : InfNoiseModule {
    enum ParamId {
        PUSH1_PARAM,
        PUSH2_PARAM,
        PUSH1_LATCH_PARAM,
        PUSH2_LATCH_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        PUSH1_INPUT,
        PUSH2_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        GATE1_OUTPUT,
        GATE2_OUTPUT,
        TRIGGER_HG1_OUTPUT,
        TRIGGER_HG2_OUTPUT,
        TRIGGER_LW1_OUTPUT,
        TRIGGER_LW2_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        LIGHTS_LEN
    };

    bool haveOutputs = false;
    bool haveInputs = false;
    infNoiseOutTrigger highTrigger[2] = { 
        infNoiseOutTrigger(), 
        infNoiseOutTrigger() 
    };
    infNoiseOutTrigger lowTrigger[2] = { 
        infNoiseOutTrigger(), 
        infNoiseOutTrigger() 
    };
    actReqValue<polyphonyMode> poly[2] = {
        actReqValue<polyphonyMode>(mono_1),
        actReqValue<polyphonyMode>(mono_1)
    };
    infNoiseInEdgeDetector gateIn[2] = {
        infNoiseInEdgeDetector(trueDetectValues[td_gateHigh]),
        infNoiseInEdgeDetector(trueDetectValues[td_gateHigh])
    };

    ManPush2Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        for (int i = 0; i < 2; i++) {
            configSwitch(PUSH1_PARAM + i, 0.0f, 1.0f, 0.0f, string::f("Push-%d", i + 1), { "Off", "On" });
            configSwitch(PUSH1_LATCH_PARAM + i, 0.0f, 1.0f, 0.0f, string::f("Push-%d latch", i + 1), { "Unlatched", "Latched" });
            configInput(PUSH1_INPUT + i, string::f("Push gate-%d", i + 1));
            configOutput(GATE1_OUTPUT + i, string::f("Gate-%d", i + 1));
            configOutput(TRIGGER_HG1_OUTPUT + i, string::f("Trigger-%d High", i + 1));
            configOutput(TRIGGER_LW1_OUTPUT + i, string::f("Trigger-%d Low", i + 1));
        }

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
        haveAutoProcQuality = true;
        haveOutQuantize = false;
        haveOutClipRange = false;
        haveGateDetect = true;
		haveGateHighLow = true;
		haveTrigDetect = false;
		haveTrigHighLow = true;
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);

        for (int i = 0; i < 2; i++) {
            gateIn[i].reset();       
            poly[i].setBoth(mono_1);
            highTrigger[i].reset();
            lowTrigger[i].reset();
		}
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        for (int i = 0; i < 2; i++) {
			poly[i].setBoth((polyphonyMode)getJsonInt(rootJ, string::f("poly%d", i).c_str(), (int)mono_1));
		}
    }

    void dataToJson(json_t* rootJ) override {
        for (int i = 0; i < 2; i++) {
            json_object_set_new(rootJ, string::f("poly%d", i).c_str(), json_integer((int)poly[i].req));
        }
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        gateIn[0].setThreshold(trueDetectValues[gateDetHigh.act]);
        gateIn[1].setThreshold(trueDetectValues[gateDetHigh.act]);

        haveInputs = inputs[PUSH1_INPUT].isConnected() || inputs[PUSH2_INPUT].isConnected();
        haveOutputs = false;
        for (int i = 0; i < 2; i++) {
            if (outputs[GATE1_OUTPUT + i].isConnected() ||
                outputs[TRIGGER_HG1_OUTPUT + i].isConnected() ||
                outputs[TRIGGER_LW1_OUTPUT + i].isConnected()) {
                    
				haveOutputs = true;

                poly[i].updateActual();
                outputs[GATE1_OUTPUT + i].setChannels(polyphonyModeChannels[poly[i].act]);
                outputs[TRIGGER_HG1_OUTPUT + i].setChannels(polyphonyModeChannels[poly[i].act]);
                outputs[TRIGGER_LW1_OUTPUT + i].setChannels(polyphonyModeChannels[poly[i].act]);
			}
        }

        // Handle auto-quality
        if (autoProcQuality.act) {
            if (haveOutputs && haveInputs)
                procQuality.setBoth(pq_audioRate, false);
            else
                procQuality.setBoth(pq_balancedRate, false);
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
            for (int i = 0; i < 2; i++) {
                float input = params[PUSH1_PARAM + i].getValue() > 0.5f
                    ? 10.f
                    : inputs[PUSH1_INPUT + i].isConnected()
                        ? inputs[PUSH1_INPUT + i].getVoltage()
                        : 0.f;
                bool edgeDetect = gateIn[i].process(input);
                bool edgeHigh = gateIn[i].isHigh();

				if (outputs[GATE1_OUTPUT + i].isConnected()) {
                    float voltage = edgeHigh
                        ? voltValues[gateOutHigh.act]
                        : voltValues[gateOutLow.act];
                    for (int c = 0; c < polyphonyModeChannels[poly[i].act]; c++)
					    outputs[GATE1_OUTPUT + i].setVoltage(voltage, c);
				}

                if (outputs[TRIGGER_HG1_OUTPUT + i].isConnected()) {
                    if (!highTrigger[i].process(procSampleTime) && 
                        edgeDetect && edgeHigh) {
                        highTrigger[i].trigger();
                    }

                    float trigValue = (highTrigger[i].isHigh())
                        ? voltValues[trigOutHigh.act]
                        : voltValues[trigOutLow.act];
                    for (int c = 0; c < polyphonyModeChannels[poly[i].act]; c++)
                        outputs[TRIGGER_HG1_OUTPUT + i].setVoltage(trigValue, c);
                }

                if (outputs[TRIGGER_LW1_OUTPUT + i].isConnected()) {
                    if (!lowTrigger[i].process(procSampleTime) && 
                        edgeDetect && !edgeHigh) {
                        lowTrigger[i].trigger();
                    }

                    float trigValue = (lowTrigger[i].isHigh())
                        ? voltValues[trigOutHigh.act]
                        : voltValues[trigOutLow.act];
                    for (int c = 0; c < polyphonyModeChannels[poly[i].act]; c++)
                        outputs[TRIGGER_LW1_OUTPUT + i].setVoltage(trigValue, c);
                }
            }
        }

        cycle256++;
    }
};

struct ManPush2ModuleWidget : InfNoiseModuleWidget {
    infNoiseSmallButton* pushBtn[2];
    infNoiseLtSmallButton* pushLatchBtn[2];

    ManPush2ModuleWidget(ManPush2Module *module) {
        initializeWidget(module, "res/ManPush2");

        const float centerClm = 15.0f;
        const float latchClm = 25.980f;
        const float latchOffset = 11.078f;       
        const float inputOffset = 29.411f;
        const float gateOffset = 65.048f;
        const float trigHgOffset = 97.611f;
        const float trigLwOffset = 122.572f;
        const float rowSpacing = 158.886f;
        float row = 50.868f;
        for (int i= 0; i  < 2; i++) {
            pushBtn[i] = createParamCentered<infNoiseSmallButton>(Vec(centerClm, row), module, ManPush2Module::PUSH1_PARAM + i);
            pushBtn[i]->setup(bc_green, true);
            addParam(pushBtn[i]);
            pushLatchBtn[i] = createParamCentered<infNoiseLtSmallButton>(Vec(latchClm, row + latchOffset), module, ManPush2Module::PUSH1_LATCH_PARAM + i);
            pushLatchBtn[i]->setup(bc_red, false);
            addParam(pushLatchBtn[i]);
            addInput(createInputCentered<ThemedPJ301MPort>(Vec(centerClm, row + inputOffset), module, ManPush2Module::PUSH1_INPUT + i));
            addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerClm, row + gateOffset), module, ManPush2Module::GATE1_OUTPUT + i));
            addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerClm, row + trigHgOffset), module, ManPush2Module::TRIGGER_HG1_OUTPUT + i));
            addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerClm, row + trigLwOffset), module, ManPush2Module::TRIGGER_LW1_OUTPUT + i));

            row += rowSpacing;
		}
    }

    void step() override {
        if (module) {
            for (int i=0; i < 2; i++) {
                pushBtn[i]->momentary = module->params[ManPush2Module::PUSH1_LATCH_PARAM + i].getValue() < 0.5f;
			}
        }

        InfNoiseModuleWidget::step();
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        ManPush2Module* module = dynamic_cast<ManPush2Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createSubmenuItem("Set buttons 1-2", "",
            [=](Menu* menu) {
                menu->addChild(createMenuItem("Latched", "", [=]() {
                    for (int i = 0; i < 2; i++)
                        module->params[ManPush2Module::PUSH1_LATCH_PARAM + i].setValue(1.0f);
                    }));
                menu->addChild(createMenuItem("Unlatched", "", [=]() {
                    for (int i = 0; i < 2; i++)
                        module->params[ManPush2Module::PUSH1_LATCH_PARAM + i].setValue(0.0f);
                    }));
            }
        ));

        std::vector<std::string> polyNames = getPolyphonyModeNames(false);
        for (int i=0; i<2; i++) {
			menu->addChild(createIndexPtrSubmenuItem(string::f("Push-%d Polyphony", i + 1), polyNames,
				&module->poly[i].req));
		}
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelManPush2 = createModel<ManPush2Module, ManPush2ModuleWidget>("ManPush2");