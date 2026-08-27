// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inUtil.hpp"

struct VCMP1Module : InfNoiseModule {
    enum ParamId {
        A_OFFSET_PARAM,
        B_OFFSET_PARAM,
        TOLERANCE_PARAM,
        TOLERANCE_TRIM_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        TRUE_INPUT,
        FALSE_INPUT,
        A_INPUT,
        B_INPUT,
        TOLERANCE_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        AB_EQ_OUTPUT,
        AB_NEQ_OUTPUT,
        AB_EGT_OUTPUT,
        AB_GT_OUTPUT,
        AB_ELT_OUTPUT,
        AB_LT_OUTPUT,
        AB_CROSS_OUTPUT,
        AB_CLAMP_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        LIGHTS_LEN
    };

    enum CrossDetectState {
		cds_Equal,
		cds_aAboveB,
		cds_aBelowB
	};
    CrossDetectState lastCrossState[PORT_MAX_CHANNELS];
    infNoiseOutTrigger crossTrigger[PORT_MAX_CHANNELS];
    bool crossWasConnected = false;
    actReqValue<voltValue> trueOutput = actReqValue<voltValue>(v_GateHigh);
    actReqValue<voltValue> falseOutput = actReqValue<voltValue>(v_GateLow);
    int abChannels = 1;
    bool outputsConnected = false;
    float toleranceParam = 0.f;
    float toleranceTrim = 0.f;
    bool readToleranceInput = false;

    VCMP1Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configInput(TRUE_INPUT, "Signal to output if true (normalized via context-menu, default 10V)");
        configInput(FALSE_INPUT, "Signal to output if false (normalized via context-menu, default 0V)");

        configInput(A_INPUT, "A-Signal (normalized to 0V)");
        configInput(B_INPUT, "B-Signal (normalized to 0V)");
        configParam(A_OFFSET_PARAM, -10.0f, 10.0f, 0.0f, "A-offset (added to A-Signal)", " V");
        configParam(B_OFFSET_PARAM, -10.0f, 10.0f, 0.0f, "B-offset (added to B-signal)", " V");

        configParam(TOLERANCE_PARAM, 0.0f, 10.0f, voltTolValues[vt_hnt], 
            "Tolerance (equal when abs(a-b) < tolerance)", " V");
        configParam(TOLERANCE_TRIM_PARAM, -1.f, 1.f, 0.f, "Tolerance CV-trim", "%", 0, 100);
        configInput(TOLERANCE_INPUT, "Tolerance CV");

        configOutput(AB_CROSS_OUTPUT, "A/B-crossing (trigger)");
        configOutput(AB_CLAMP_OUTPUT, "Clamp A within tolerance of B");
        configOutput(AB_EQ_OUTPUT, "A = B");
        configOutput(AB_NEQ_OUTPUT, "A != B");
        configOutput(AB_EGT_OUTPUT, "A >= B");
        configOutput(AB_GT_OUTPUT, "A > B");
        configOutput(AB_ELT_OUTPUT, "A <= B");
        configOutput(AB_LT_OUTPUT, "A < B");

        // Set InfNoise features (e.g. menu-items) 
		haveProcQuality = true;
        haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = true;
        haveTrigDetect = false;
        haveTrigHighLow = true;
        haveGateDetect = false;
        haveGateHighLow = false;

        for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
            crossTrigger[c] = infNoiseOutTrigger(1e-3f, 1e-3f);
            lastCrossState[c] = cds_Equal;
        }
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        
        trueOutput.setBoth(v_GateHigh);
        falseOutput.setBoth(v_GateLow);
        
        for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
            crossTrigger[c].reset();
            lastCrossState[c] = cds_Equal;
        }
        outputs[AB_CROSS_OUTPUT].clearVoltages();  // Prevent trigger misfire
        crossWasConnected = false;
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        trueOutput.setBoth((voltValue)getJsonInt(rootJ, "trueOutput", (int)v_GateHigh));
        falseOutput.setBoth((voltValue)getJsonInt(rootJ, "falseOutput", (int)v_GateLow));
        int crossStates[PORT_MAX_CHANNELS] = {};
        getJsonIntArray(rootJ, "lastCrossState", crossStates, PORT_MAX_CHANNELS, (int)cds_Equal);
        for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
            lastCrossState[c] = (CrossDetectState)crossStates[c];
            crossTrigger[c].reset();
        }
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "trueOutput", json_integer((int)trueOutput.req));
        json_object_set_new(rootJ, "falseOutput", json_integer((int)falseOutput.req));
        int crossStates[PORT_MAX_CHANNELS] = {};
        for (int c = 0; c < PORT_MAX_CHANNELS; c++)
            crossStates[c] = (int)lastCrossState[c];
        setJsonIntArray(rootJ, "lastCrossState", crossStates, PORT_MAX_CHANNELS);
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        if (trueOutput.needsUpdate()) {
            trueOutput.updateActual();
            if (inputInfos.size() > (unsigned)TRUE_INPUT && inputInfos[TRUE_INPUT]) {
                inputInfos[TRUE_INPUT]->name = polyPortPrefix() + "True-CV (normalized via menu to: " + getVoltName(trueOutput.act) + ")";
            }
        }

        if (falseOutput.needsUpdate()) {
            falseOutput.updateActual();
            if (inputInfos.size() > (unsigned)FALSE_INPUT && inputInfos[FALSE_INPUT]) {
                inputInfos[FALSE_INPUT]->name = polyPortPrefix() + "False-CV (normalized via menu to: " + getVoltName(falseOutput.act) + ")";
            }
        }

        if (inputs[A_INPUT].isConnected() || inputs[B_INPUT].isConnected()) {
            int aChannels = inputs[A_INPUT].isConnected()
                ? std::max(inputs[A_INPUT].getChannels(), 1)
                : 1; // uses knob-value
            int bChannels = inputs[B_INPUT].isConnected()
                ? std::max(inputs[B_INPUT].getChannels(), 1)
                : 1; // uses knob-value
            abChannels = std::max(aChannels, bChannels);
        }
		else
			abChannels = 1;  // Knob-values only

        outputsConnected = outputs[AB_CROSS_OUTPUT].isConnected() ||
            outputs[AB_CLAMP_OUTPUT].isConnected() ||
            outputs[AB_EQ_OUTPUT].isConnected() || outputs[AB_NEQ_OUTPUT].isConnected() ||
            outputs[AB_EGT_OUTPUT].isConnected() || outputs[AB_GT_OUTPUT].isConnected() ||
            outputs[AB_ELT_OUTPUT].isConnected() || outputs[AB_LT_OUTPUT].isConnected();

        if (outputsConnected) {
            outputs[AB_EQ_OUTPUT].setChannels(abChannels);
            outputs[AB_NEQ_OUTPUT].setChannels(abChannels);
            outputs[AB_EGT_OUTPUT].setChannels(abChannels);
            outputs[AB_GT_OUTPUT].setChannels(abChannels);
            outputs[AB_ELT_OUTPUT].setChannels(abChannels);
            outputs[AB_LT_OUTPUT].setChannels(abChannels);
        
            outputs[AB_CROSS_OUTPUT].setChannels(abChannels);
            outputs[AB_CLAMP_OUTPUT].setChannels(abChannels);
        }

        bool crossIsConnected = outputs[AB_CROSS_OUTPUT].isConnected();
        if (!crossIsConnected && crossWasConnected && !wasJustLoaded) {
            for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
                crossTrigger[c].reset();
                lastCrossState[c] = cds_Equal;
            }
            outputs[AB_CROSS_OUTPUT].clearVoltages();  // Prevent trigger misfire
        }
        crossWasConnected = crossIsConnected;

        toleranceParam = params[TOLERANCE_PARAM].getValue();
        toleranceTrim = params[TOLERANCE_TRIM_PARAM].getValue();
        readToleranceInput = inputs[TOLERANCE_INPUT].isConnected() && toleranceTrim != 0.f;

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

        if (doProcess && outputsConnected) {
            float tol = clamp(toleranceParam, 0.f, 10.0f); // used when no tolerance input is connected
            for (int c = 0; c < abChannels; c++) {
                if (readToleranceInput) {
                    tol = toleranceParam + inputs[TOLERANCE_INPUT].getPolyVoltage(c) * toleranceTrim;
                    tol = clamp(tol, 0.f, 10.0f);
                }
                float aInput = inputs[A_INPUT].isConnected()
                    ? inputs[A_INPUT].getPolyVoltage(c) + params[A_OFFSET_PARAM].getValue()
                    : params[A_OFFSET_PARAM].getValue();
                float bInput = inputs[B_INPUT].isConnected()
                    ? inputs[B_INPUT].getPolyVoltage(c) + params[B_OFFSET_PARAM].getValue()
                    : params[B_OFFSET_PARAM].getValue();

                float trueInput = inputs[TRUE_INPUT].isConnected()
                    ? clipToVoltRange(inputs[TRUE_INPUT].getPolyVoltage(c), outClipRange.act)
                    : clipToVoltRange(voltValues[trueOutput.act], outClipRange.act);
                float falseInput = inputs[FALSE_INPUT].isConnected()
                    ? clipToVoltRange(inputs[FALSE_INPUT].getPolyVoltage(c), outClipRange.act)
                    : clipToVoltRange(voltValues[falseOutput.act], outClipRange.act);

                // == !=
                bool inTol = std::abs(aInput - bInput) <= tol;
                outputs[AB_EQ_OUTPUT].setVoltage(inTol
                    ? trueInput
                    : falseInput, c);
                outputs[AB_NEQ_OUTPUT].setVoltage(!inTol
                    ? trueInput
                    : falseInput, c);

                // >= >
                bool gt = aInput > bInput;
                outputs[AB_EGT_OUTPUT].setVoltage(inTol || gt
                    ? trueInput
                    : falseInput, c);
                outputs[AB_GT_OUTPUT].setVoltage(!inTol && gt
                    ? trueInput
                    : falseInput, c);

                // <= <
                bool lt = aInput < bInput;
                outputs[AB_ELT_OUTPUT].setVoltage(inTol || lt
                    ? trueInput
                    : falseInput, c);
                outputs[AB_LT_OUTPUT].setVoltage(!inTol && lt
                    ? trueInput
                    : falseInput, c);


                // Clamp A within tolerance of B
                float clampVoltage = (tol > 0.f) 
                    ? clamp(aInput, bInput - tol, bInput + tol)
                    : bInput;
                outputs[AB_CLAMP_OUTPUT].setVoltage(clipToVoltRange(clampVoltage, outClipRange.act), c);

                // A/B-crossing
                if (outputs[AB_CROSS_OUTPUT].isConnected()) {
                    crossTrigger[c].process(procSampleTime);    
                    if (!inTol) {
                        CrossDetectState crossState = gt ? cds_aAboveB : cds_aBelowB;
                        if (crossState != lastCrossState[c]) {
                            if (crossTrigger[c].trigger())
                                lastCrossState[c] = crossState;
                        }
                    }

                    float triggerVoltage = crossTrigger[c].isHigh()
						? voltValues[trigOutHigh.act]
						: voltValues[trigOutLow.act];
                    outputs[AB_CROSS_OUTPUT].setVoltage(triggerVoltage, c);
                }
            }
        }

        cycle256++;
    }
};

struct VCMP1ModuleWidget : InfNoiseModuleWidget {
    VCMP1ModuleWidget(VCMP1Module *module) {
        initializeWidget(module, "res/VCMP1");

        // True/False inputs
        const float leftColumn = 15.132f;
        const float rightColumn = 43.868f;
        const float trueFalseRow = 50.679;
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, trueFalseRow), module, VCMP1Module::TRUE_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, trueFalseRow), module, VCMP1Module::FALSE_INPUT));

        // A/B inputs/knobs
        const float abInputRow = 85.753f;
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, abInputRow), module, VCMP1Module::A_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, abInputRow), module, VCMP1Module::B_INPUT));
        const float abOffsetKnobRow = 120.938f;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(leftColumn, abOffsetKnobRow), module, VCMP1Module::A_OFFSET_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(rightColumn, abOffsetKnobRow), module, VCMP1Module::B_OFFSET_PARAM));

        // Tolerance knob
        const float centerColumn = 30.f;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerColumn, 164.922f), module, VCMP1Module::TOLERANCE_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, 185.010f), module, VCMP1Module::TOLERANCE_INPUT));
        addParam(createParamCentered<Trimpot>(Vec(rightColumn, 185.010f), module, VCMP1Module::TOLERANCE_TRIM_PARAM));

        // A/B-crossing output
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, 227.473f), module, VCMP1Module::AB_CROSS_OUTPUT));

        // Clamp output
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, 227.473f), module, VCMP1Module::AB_CLAMP_OUTPUT));

        // A/B logic outputs
        float row = 262.547f;
        const float logRowSpacing = 35.0735f;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, row), module, VCMP1Module::AB_EQ_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, row), module, VCMP1Module::AB_NEQ_OUTPUT));
        row += logRowSpacing;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, row), module, VCMP1Module::AB_EGT_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, row), module, VCMP1Module::AB_GT_OUTPUT));
        row += logRowSpacing;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, row), module, VCMP1Module::AB_ELT_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, row), module, VCMP1Module::AB_LT_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        VCMP1Module* module = dynamic_cast<VCMP1Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> voltNames = getVoltValuesNames();
        menu->addChild(createIndexPtrSubmenuItem("True output level", voltNames,
            &module->trueOutput.req));
        menu->addChild(createIndexPtrSubmenuItem("False output level", voltNames,
            &module->falseOutput.req));
        
        std::vector<std::string> voltTolNames = getVoltTolValuesNames();
        menu->addChild(createSubmenuItem("Set tolerance", "", [=](Menu* menu) {
            for (int i = 0; i < voltTolValueCount; i++) {
                voltTolValue vtv = (voltTolValue)i;
                float voltage = i == 0 ? 0.f : voltTolValues[vtv];
                menu->addChild(createMenuItem(voltTolNames[i], "", [=]() {
                    module->params[module->TOLERANCE_PARAM].setValue(voltage);
                }));
			}
        }));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelVCMP1 = createModel<VCMP1Module, VCMP1ModuleWidget>("VCMP1");