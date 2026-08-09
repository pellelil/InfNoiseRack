// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inUtil.hpp"

struct ManTrGtCvModule : InfNoiseModule {
    enum ParamId {
        TRIG_PARAM,
        GATE1_PARAM,
        GATE2_PARAM,
        GATE1_LATCH_PARAM,
        GATE2_LATCH_PARAM,
        CV1_PARAM,
        CV2_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        //SOME_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        TRIG_OUTPUT,
        GATE1_OUTPUT,
        GATE2_OUTPUT,
        CV1_OUTPUT,
        CV2_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        LIGHTS_LEN
    };

    bool haveTrigger = false;
    bool haveGate = false;
    bool haveCv = false;
    infNoiseOutTrigger outputTrigger = infNoiseOutTrigger();  // Timing of trigger (1 ms high, 1 ms low)
    dsp::SchmittTrigger btnTrigger; // Prevents multiple triger-fires
    actReqValue<polyphonyMode> trigPoly = actReqValue<polyphonyMode>(mono_1);
    actReqValue<polyphonyMode> gatePoly = actReqValue<polyphonyMode>(mono_1);
    actReqValue<polyphonyMode> cvPoly = actReqValue<polyphonyMode>(mono_1);

    ManTrGtCvModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configSwitch(TRIG_PARAM, 0.0f, 1.0f, 0.0f, "Trigger", {"Low", "High"});
        configSwitch(GATE1_PARAM, 0.0f, 1.0f, 0.0f, "Gate-1", { "Low", "High" });
        configSwitch(GATE2_PARAM, 0.0f, 1.0f, 0.0f, "Gate-2", { "Low", "High" });
        configSwitch(GATE1_LATCH_PARAM, 0.0f, 1.0f, 0.0f, "Gate-1 Latch", { "Unlatched", "Latched" });
        configSwitch(GATE2_LATCH_PARAM, 0.0f, 1.0f, 0.0f, "Gate-2 Latch", { "Unlatched", "Latched" });
        configParam(CV1_PARAM, -10.0f, 10.0f, 0.0f, "CV-1 (-10 to 10)", " V");
        configParam(CV2_PARAM, -10.0f, 10.0f, 0.0f, "CV-2 (-10 to 10)", " V");

        configOutput(TRIG_OUTPUT, "Trig");
        configOutput(GATE1_OUTPUT, "Gate-1");
        configOutput(GATE2_OUTPUT, "Gate-2");
        configOutput(CV1_OUTPUT, "CV-1");
        configOutput(CV2_OUTPUT, "CV-2");

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
        haveAutoProcQuality = false;
        haveOutQuantize = true;
        haveOutClipRange = true;
        haveGateDetect = false;
		haveGateHighLow = true;
		haveTrigDetect = false;
		haveTrigHighLow = true;
        procQuality.setBoth(pq_balancedRate); // Ballanced rate (every 16th cycle)
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        
        procQuality.setBoth(pq_balancedRate); // Ballanced rate (every 16th cycle)
        trigPoly.setBoth(polyphonyMode::mono_1);
        gatePoly.setBoth(polyphonyMode::mono_1);
        cvPoly.setBoth(polyphonyMode::mono_1);
        outputTrigger.reset();
        btnTrigger.reset();
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        trigPoly.setBoth((polyphonyMode)getJsonInt(rootJ, "trigPoly", (int)polyphonyMode::mono_1));
        gatePoly.setBoth((polyphonyMode)getJsonInt(rootJ, "gatePoly", (int)polyphonyMode::mono_1));
        cvPoly.setBoth((polyphonyMode)getJsonInt(rootJ, "cvPoly", (int)polyphonyMode::mono_1));

    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "trigPoly", json_integer((int)trigPoly.req));
        json_object_set_new(rootJ, "gatePoly", json_integer((int)gatePoly.req));
        json_object_set_new(rootJ, "cvPoly", json_integer((int)cvPoly.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        haveTrigger = outputs[TRIG_OUTPUT].isConnected();
        if (haveTrigger) {
            trigPoly.updateActual();
            outputs[TRIG_OUTPUT].setChannels(polyphonyModeChannels[trigPoly.act]);
        }

        haveGate = outputs[GATE1_OUTPUT].isConnected() || outputs[GATE2_OUTPUT].isConnected();
        if (haveGate) {
            gatePoly.updateActual();
            outputs[GATE1_OUTPUT].setChannels(polyphonyModeChannels[gatePoly.act]);
            outputs[GATE2_OUTPUT].setChannels(polyphonyModeChannels[gatePoly.act]);
        }

        haveCv = outputs[CV1_OUTPUT].isConnected() || outputs[CV2_OUTPUT].isConnected();
        if (haveCv) {
            cvPoly.updateActual();
            outputs[CV1_OUTPUT].setChannels(polyphonyModeChannels[cvPoly.act]);
            outputs[CV2_OUTPUT].setChannels(polyphonyModeChannels[cvPoly.act]);
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

        if (doProcess) {
            if (haveTrigger) {
                outputTrigger.process(procSampleTime);  
                float trigValue = params[TRIG_PARAM].getValue();
                if (btnTrigger.process(trigValue,
                    trueDetectValues[td_triggerLow], trueDetectValues[td_triggerHigh])) {
                    outputTrigger.trigger();
                }
                trigValue = (outputTrigger.isHigh())
                    ? voltValues[trigOutHigh.act]
                    : voltValues[trigOutLow.act];
                for (int c = 0; c < polyphonyModeChannels[trigPoly.act]; c++)
                    outputs[TRIG_OUTPUT].setVoltage(trigValue, c);
            }

            if (haveGate) {
                for (int i = 0; i < 2; i++) {
                    float gateValue = (params[GATE1_PARAM + i].getValue() > 0.5f)
                        ? voltValues[gateOutHigh.act]
                        : voltValues[gateOutLow.act];
                    for (int c = 0; c < polyphonyModeChannels[gatePoly.act]; c++) {
                        outputs[GATE1_OUTPUT + i].setVoltage(gateValue, c);
                    }
                }
            }

            if (haveCv) {
                for (int i = 0; i < 2; i++) {
                    float voltage = (params[CV1_PARAM + i].getValue());
                    voltage = quantizeToMode(voltage, outQuantize.act);
                    voltage = clipToVoltRange(voltage, outClipRange.act);
                    for (int c = 0; c < polyphonyModeChannels[cvPoly.act]; c++) {
                        outputs[CV1_OUTPUT + i].setVoltage(voltage, c);
                    }
                }
            }
        }

        cycle256++;
    }
};

struct ManTrGtCvModuleWidget : InfNoiseModuleWidget {
    infNoiseSmallButton<bc_green, true>* gateBtn[2];

    ManTrGtCvModuleWidget(ManTrGtCvModule *module) {
        initializeWidget(module, "res/ManTrGtCv");

        // Trigger
        const float cntClm = 15.0f;
        addParam(createParamCentered<infNoiseSmallButton<bc_red, true>>(Vec(cntClm, 50.868f), module, ManTrGtCvModule::TRIG_PARAM));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntClm, 77.302f), module, ManTrGtCvModule::TRIG_OUTPUT));

        //  Gates
        const float latchClm = 25.586f;
        gateBtn[0] = createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(cntClm, 114.842f), module, ManTrGtCvModule::GATE1_PARAM);
        addParam(gateBtn[0]);
        addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(latchClm, 125.920f), module, ManTrGtCvModule::GATE1_LATCH_PARAM));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntClm, 142.301f), module, ManTrGtCvModule::GATE1_OUTPUT));

        gateBtn[1] = createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(cntClm, 170.350f), module, ManTrGtCvModule::GATE2_PARAM);
        addParam(gateBtn[1]);
        addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(latchClm, 181.427f), module, ManTrGtCvModule::GATE2_LATCH_PARAM));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntClm, 197.909f), module, ManTrGtCvModule::GATE2_OUTPUT));

        // CV
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntClm, 241.786f), module, ManTrGtCvModule::CV1_PARAM));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntClm, 268.235), module, ManTrGtCvModule::CV1_OUTPUT));

        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntClm, 306.286f), module, ManTrGtCvModule::CV2_PARAM));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntClm, 332.735f), module, ManTrGtCvModule::CV2_OUTPUT));
    }

    void step() override {
        if (module) {
            gateBtn[0]->momentary = module->params[ManTrGtCvModule::GATE1_LATCH_PARAM].getValue() < 0.5f;
            gateBtn[1]->momentary = module->params[ManTrGtCvModule::GATE2_LATCH_PARAM].getValue() < 0.5f;
        }

        InfNoiseModuleWidget::step();
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        ManTrGtCvModule* module = dynamic_cast<ManTrGtCvModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> polyNames = getPolyphonyModeNames(false);
        menu->addChild(createIndexPtrSubmenuItem("Trigger Polyphony", polyNames,
            &module->trigPoly.req));
        menu->addChild(createIndexPtrSubmenuItem("Gate Polyphony", polyNames,
            &module->gatePoly.req));
        menu->addChild(createIndexPtrSubmenuItem("CV Polyphony", polyNames,
            &module->cvPoly.req));
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelManTrGtCv = createModel<ManTrGtCvModule, ManTrGtCvModuleWidget>("ManTrGtCv");