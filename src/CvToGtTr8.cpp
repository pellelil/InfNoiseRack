// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inUtil.hpp"

struct CvToGtTr8Module : InfNoiseModule {
    enum ParamId {
        TOGGLE_CROSS_PARAM,
        TOGGLE_MIN_INCL_PARAM,
        TOGGLE_MAX_INCL_PARAM,
        ENUMS(CROSS1_PARAM, 8),  // Cross switch (on/off)
        ENUMS(MIN1_PARAM, 8),  // Min knob (-10 to +10)
        ENUMS(MIN_INCL1_PARAM, 8),  // Min-incl switch (on/off)
        ENUMS(MAX1_PARAM, 8),  // Max knob (-10 to +10)
        ENUMS(MAX_INCL1_PARAM, 8),  // Max-incl switch (on/off)   
        ENUMS(GATE_TRIGGER1_PARAM, 8),  // Output as gate/trigger-switch
        PARAMS_LEN
    };
    enum InputsId {
        ENUMS(CV1_INPUT, 8),
        INPUTS_LEN
    };
    enum OutputsId {
        POLY_OUTPUT,
        ENUMS(CV1_OUTPUT, 8),
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        LIGHTS_LEN
    };

    bool outputsInUse = false;
    int firstIdx = -1;
    int lastIdx = -1;
    bool cross[8] = { false, false, false, false, false, false, false, false };
    bool inclMin[8] = { true, true, true, true, true, true, true, true };
    bool inclMax[8] = { false, false, false, false, false, false, false, false };
    bool outAsGate[8] = { true, true, true, true, true, true, true, true };
    float outputVoltage[8] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
    float prevInput[8] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };  // Used to detect crossing
    infNoiseOutTrigger outputTrigger[8] = { infNoiseOutTrigger(), infNoiseOutTrigger(), 
        infNoiseOutTrigger(), infNoiseOutTrigger(), infNoiseOutTrigger(), 
        infNoiseOutTrigger(), infNoiseOutTrigger(), infNoiseOutTrigger() };
    dsp::SchmittTrigger inputTrigger[8] = { dsp::SchmittTrigger(), dsp::SchmittTrigger(), dsp::SchmittTrigger(), dsp::SchmittTrigger(), 
    			dsp::SchmittTrigger(), dsp::SchmittTrigger(), dsp::SchmittTrigger(), dsp::SchmittTrigger() };
    actReqValue<polyphonyMode> polyOutput = actReqValue<polyphonyMode>(poly_8);
    int polyChannels = 8;

    dsp::SchmittTrigger toggleCrossTrigger = dsp::SchmittTrigger();
    dsp::SchmittTrigger toggleMinInclTrigger = dsp::SchmittTrigger();
    dsp::SchmittTrigger toggleMaxInclTrigger = dsp::SchmittTrigger();

    void applyLoadedOutputs() {
        if (!outputsInUse)
            return;
        for (int i = firstIdx; i <= lastIdx; i++) {
            if (outputs[CV1_OUTPUT + i].isConnected())
                outputs[CV1_OUTPUT + i].setVoltage(outputVoltage[i]);
            if (i < polyChannels)
                outputs[POLY_OUTPUT].setVoltage(outputVoltage[i], i);
        }
    }

    CvToGtTr8Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configSwitch(TOGGLE_CROSS_PARAM, 0.0f, 1.0f, 0.0f, "Toggle all crossings");
        configSwitch(TOGGLE_MIN_INCL_PARAM, 0.0f, 1.0f, 0.0f, "Toggle all min-includes");
        configSwitch(TOGGLE_MAX_INCL_PARAM, 0.0f, 1.0f, 0.0f, "Toggle all max-includes");
        configOutput(POLY_OUTPUT, "Polyphonic");

        for (int i = 0; i < 8; i++) {
            std::string normalizedPrev = (i > 0) ? " (normalized to previous)" : "";
            configInput(CV1_INPUT + i, string::f("CV-%d", i + 1) + normalizedPrev);
            configSwitch(CROSS1_PARAM + i, 0.0f, 1.0f, 0.0f, string::f("Crossing-%d (trigger)", i + 1),
                { "Disabled", "Enabled" });

            configParam(MIN1_PARAM + i, -10.0f, 10.0f, 0.0f, string::f("Min-%d", i + 1), " V");
            configSwitch(MIN_INCL1_PARAM + i, 0.0f, 1.0f, 1.0f, string::f("Incl. min-%d", i + 1),
                { "Excluded", "Included" });

            configParam(MAX1_PARAM + i, -10.0f, 10.0f, 0.0f, string::f("Max-%d", i + 1), " V");
            configSwitch(MAX_INCL1_PARAM + i, 0.0f, 1.0f, 0.0f, string::f("Incl. max-%d", i + 1),
                { "Excluded", "Included" });

            configSwitch(GATE_TRIGGER1_PARAM + i, 0.0f, 1.0f, 1.0f, string::f("Gate/Trigger-%d", i + 1),
                { "Trigger when red", "Gate when green" });

            configOutput(CV1_OUTPUT + i, string::f("Gate/trigger-%d", i + 1));
            configSwitch(GATE_TRIGGER1_PARAM + i, 0.0f, 1.0f, 1.0f, string::f("Output type-%d", i + 1),
                { "Trigger when red", "Gate when green" });
        }

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
        haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = false;
        haveGateDetect = false;
		haveGateHighLow = true;
		haveTrigDetect = false;
		haveTrigHighLow = true;
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);

        for (int i = 0; i < 8; i++) {
            outputTrigger[i].reset();
            inputTrigger[i].reset();
            prevInput[i] = 0.f;
            outputVoltage[i] = 0.f;
		}
        polyOutput.setBoth(poly_8);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        polyOutput.setBoth((polyphonyMode)getJsonInt(rootJ, "polyOutput",
            getJsonInt(rootJ, "anyPoly", (int)polyphonyMode::poly_8)));
        getJsonFloatArray(rootJ, "outputVoltage", outputVoltage, 8, 0.f);
        getJsonFloatArray(rootJ, "prevInput", prevInput, 8, 0.f);
        for (int i = 0; i < 8; i++) {
            outputTrigger[i].reset();
            inputTrigger[i].reset();
        }
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "polyOutput", json_integer((int)polyOutput.req));
        setJsonFloatArray(rootJ, "outputVoltage", outputVoltage, 8);
        setJsonFloatArray(rootJ, "prevInput", prevInput, 8);
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        outputsInUse = false;
        firstIdx = -1;
        lastIdx = -1;
        for (int i = 0; i < 8; i++)
        {
            if (inputs[CV1_INPUT + i].isConnected() || outputs[CV1_OUTPUT + i].isConnected())
            {
                if (firstIdx < 0)
					firstIdx = i;
                lastIdx = i;

                if (outputs[CV1_OUTPUT + i].isConnected())
                    outputsInUse = true;
            }

            cross[i] = params[CROSS1_PARAM + i].getValue() > 0.5f;
            inclMin[i] = params[MIN_INCL1_PARAM + i].getValue() > 0.5f;
            inclMax[i] = params[MAX_INCL1_PARAM + i].getValue() > 0.5f;
            outAsGate[i] = params[GATE_TRIGGER1_PARAM + i].getValue() > 0.5f;
        }

        if (polyOutput.needsUpdate()) {
            polyOutput.updateActual();
            if (outputInfos.size() > (unsigned)POLY_OUTPUT && outputInfos[POLY_OUTPUT]) {
                outputInfos[POLY_OUTPUT]->name = polyPortPrefix() + "Poly output: " + getPolyphonyModeName(polyOutput.act);
            }
        }
        polyChannels = 0;
        if (outputs[POLY_OUTPUT].isConnected()) {
            outputsInUse = true;
            polyChannels = polyphonyModeChannels[polyOutput.act];
            outputs[POLY_OUTPUT].setChannels(polyChannels);
            firstIdx = 0;
            int lastPoly = polyChannels - 1;
            if (lastIdx < lastPoly)
                lastIdx = lastPoly;
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

        if (doProcess) {
            // Handle Cross toggle
            float voltage = params[TOGGLE_CROSS_PARAM].getValue() > 0.5f ? 10.f : 0.f;
            if (toggleCrossTrigger.process(voltage)) {
                for (int i = 0; i < 8; i++) {
                    params[CROSS1_PARAM + i].setValue(!cross[i]);
                    cross[i] = !cross[i];
                }
            }

            // Handle Min-include toggle
            voltage = params[TOGGLE_MIN_INCL_PARAM].getValue() > 0.5f ? 10.f : 0.f;
            if (toggleMinInclTrigger.process(voltage)) {
                for (int i = 0; i < 8; i++) {
                    params[MIN_INCL1_PARAM + i].setValue(!inclMin[i]);
                    inclMin[i] = !inclMin[i];
                }
            }

            // Handle Max-include toggle
            voltage = params[TOGGLE_MAX_INCL_PARAM].getValue() > 0.5f ? 10.f : 0.f;
            if (toggleMaxInclTrigger.process(voltage)) {
                for (int i = 0; i < 8; i++) {
                    params[MAX_INCL1_PARAM + i].setValue(!inclMax[i]);
                    inclMax[i] = !inclMax[i];
                }
            }

            if (outputsInUse) {
                float input = 0.f;
                for (int i = firstIdx; i <= lastIdx; i++) {
                    if (inputs[CV1_INPUT + i].isConnected()) {
                        input = inputs[CV1_INPUT + i].getVoltage();
                    }

                    // Check in-range (crossing only for triggers)
                    float minVal = params[MIN1_PARAM + i].getValue();
                    float maxVal = params[MAX1_PARAM + i].getValue();
                    bool inRange = (input > minVal || (inclMin[i] && input == minVal)) &&
                        (input < maxVal || (inclMax[i] && input == maxVal));

                    float channelOutput = 0.f;
                    if (outAsGate[i]) { // output as gate
                        channelOutput = inRange
                            ? voltValues[gateOutHigh.act]
                            : voltValues[gateOutLow.act];
                    }
                    else { // output as trigger
                        // Detect crossing (if not in range)
                        if (cross[i] && !inRange) {
                            inRange = (prevInput[i] < minVal && input > maxVal) ||
                                (prevInput[i] > maxVal && input < minVal);
                        }

                        // If trigger not running, check for new trigger
                        if (!outputTrigger[i].process(procSampleTime)) {
                            float virtualTrigger = inRange ? 10.0f : 0.0f;
                            if (inputTrigger[i].process(virtualTrigger,
                                trueDetectValues[td_triggerLow], trueDetectValues[td_triggerHigh])) {
                                outputTrigger[i].trigger();
                            }
                        }

                        channelOutput = (outputTrigger[i].isHigh())
                            ? voltValues[trigOutHigh.act]
                            : voltValues[trigOutLow.act];
                    }

                    outputVoltage[i] = channelOutput;
                    if (outputs[CV1_OUTPUT + i].isConnected())
                        outputs[CV1_OUTPUT + i].setVoltage(channelOutput);
                    if (i < polyChannels)
                        outputs[POLY_OUTPUT].setVoltage(channelOutput, i);

                    prevInput[i] = input;  // Used to detect crossing
                }
            }
        }

        cycle256++;
    }
};

struct CvToGtTr8ModuleWidget : InfNoiseModuleWidget {
    CvToGtTr8ModuleWidget(CvToGtTr8Module *module) {
        initializeWidget(module, "res/CvToGtTr8");

        const float inputColumn = 14.810f;
        const float minKnobColumn = 43.498f;
        const float maxKnobColumn = 72.233f;
        const float outputColumn = 103.545f;
        const float tglRow = 52.812f;
        addParam(createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(inputColumn, tglRow), module, CvToGtTr8Module::TOGGLE_CROSS_PARAM));

        addParam(createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(minKnobColumn, tglRow), module, CvToGtTr8Module::TOGGLE_MIN_INCL_PARAM));

        addParam(createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(maxKnobColumn, tglRow), module, CvToGtTr8Module::TOGGLE_MAX_INCL_PARAM));

        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(outputColumn, tglRow), module, CvToGtTr8Module::POLY_OUTPUT));

        const float crossColumn = 25.718f;
        const float minInclColumn = 54.062f;
        const float InclOffset = 12.267f;
        const float maxInclColumn = 82.767f;
        const float gateTrigOffset = -12.674f;
        const float gateTrigColumn = 94.686f;
        const float rowSpacing = 35.0735f;
        float row = 87.179f;
        for (int i = 0; i < 8; i++) {
            addInput(createInputCentered<ThemedPJ301MPort>(Vec(inputColumn, row), module, CvToGtTr8Module::CV1_INPUT + i));
            addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(Vec(crossColumn, row + InclOffset), module, CvToGtTr8Module::CROSS1_PARAM + i));

            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(minKnobColumn, row), module, CvToGtTr8Module::MIN1_PARAM + i));
            addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(Vec(minInclColumn, row + InclOffset), module, CvToGtTr8Module::MIN_INCL1_PARAM + i));

            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(maxKnobColumn, row), module, CvToGtTr8Module::MAX1_PARAM + i));
            addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(Vec(maxInclColumn, row + InclOffset), module, CvToGtTr8Module::MAX_INCL1_PARAM + i));

            addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(outputColumn, row), module, CvToGtTr8Module::CV1_OUTPUT + i));
            addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_red, bc_green>>(
                Vec(gateTrigColumn, row + gateTrigOffset), module, CvToGtTr8Module::GATE_TRIGGER1_PARAM + i));

            row += rowSpacing;
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        CvToGtTr8Module* module = dynamic_cast<CvToGtTr8Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createSubmenuItem("Set crossing 1-8", "", [=](Menu* menu) {
            menu->addChild(createMenuItem("Disabled", "", [=]() {
                for (int i=0; i<8; i++)
                    module->params[CvToGtTr8Module::CROSS1_PARAM + i].setValue(0.f);
                }));
            menu->addChild(createMenuItem("Enabled", "", [=]() {
                for (int i=0; i<8; i++)
                    module->params[CvToGtTr8Module::CROSS1_PARAM + i].setValue(1.f);
                }));
        }));

        menu->addChild(createSubmenuItem("Set min-include 1-8", "", [=](Menu* menu) {
            menu->addChild(createMenuItem("Exclude", "", [=]() {
                for (int i=0; i<8; i++)
                    module->params[CvToGtTr8Module::MIN_INCL1_PARAM + i].setValue(0.f);
                }));
            menu->addChild(createMenuItem("Include", "", [=]() {
                for (int i=0; i<8; i++)
                    module->params[CvToGtTr8Module::MIN_INCL1_PARAM + i].setValue(1.f);
                }));
        }));

        menu->addChild(createSubmenuItem("Set max-include 1-8", "", [=](Menu* menu) {
            menu->addChild(createMenuItem("Exclude", "", [=]() {
                for (int i=0; i<8; i++)
                    module->params[CvToGtTr8Module::MAX_INCL1_PARAM + i].setValue(0.f);
                }));
            menu->addChild(createMenuItem("Include", "", [=]() {
                for (int i=0; i<8; i++)
                    module->params[CvToGtTr8Module::MAX_INCL1_PARAM + i].setValue(1.f);
                }));
        }));

        menu->addChild(createSubmenuItem("Set gate/trigger-output 1-8", "", [=](Menu* menu) {
            menu->addChild(createMenuItem("To gate", "", [=]() {
                for (int i=0; i<8; i++)
                    module->params[CvToGtTr8Module::GATE_TRIGGER1_PARAM + i].setValue(1.f);
                }));
            menu->addChild(createMenuItem("To trigger", "", [=]() {
                for (int i=0; i<8; i++)
                    module->params[CvToGtTr8Module::GATE_TRIGGER1_PARAM + i].setValue(0.f);
                }));
        }));

        std::vector<std::string> polyNames = getPolyphonyModeNames(false);
        polyNames.resize(8);
        menu->addChild(createIndexPtrSubmenuItem("Poly output channels", polyNames,
            &module->polyOutput.req));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelCvToGtTr8 = createModel<CvToGtTr8Module, CvToGtTr8ModuleWidget>("CvToGtTr8");