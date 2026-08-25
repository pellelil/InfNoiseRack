// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct VCMP2IModule : InfNoiseModule {
    enum ParamId {
        //SOME_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        TRUE_INPUT,
        FALSE_INPUT,
        A_INPUT,
        B_INPUT,
        C_INPUT,
        D_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        AB_EQ_OUTPUT,
        AB_NEQ_OUTPUT,
        AB_EGT_OUTPUT,
        AB_GT_OUTPUT,
        AB_ELT_OUTPUT,
        AB_LT_OUTPUT,
        CD_EQ_OUTPUT,
        CD_NEQ_OUTPUT,
        CD_EGT_OUTPUT,
        CD_GT_OUTPUT,
        CD_ELT_OUTPUT,  
        CD_LT_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        LIGHTS_LEN
    };

    actReqValue<voltValue> trueOutput = actReqValue<voltValue>(v_GateHigh);
    actReqValue<voltValue> falseOutput = actReqValue<voltValue>(v_GateLow);
    actReqValue<voltTolValue> tolerance = actReqValue< voltTolValue>(voltTolValue::vt_hnt);
    bool abOutputsConnected = false;
    bool cdOutputsConnected = false;
    int abChannels = 1;
    int cdChannels = 1;
    int maxChannels = 1;

    VCMP2IModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configInput(TRUE_INPUT, "Signal to output if true (normalized via context-menu, default 10V)");
        configInput(FALSE_INPUT, "Signal to output if false (normalized via context-menu, default 0V)");
        
        configInput(A_INPUT, "A-Signal (normalized to 0V)");
        configInput(B_INPUT, "B-Signal (normalized to 0V)");

        configInput(C_INPUT, "C-Signal (normalized to A, if not connected)");
        configInput(D_INPUT, "D-Signal (normalized to B, if not connected");

        configOutput(AB_EQ_OUTPUT, "A = B");
        configOutput(AB_NEQ_OUTPUT, "A != B (not \"A = B\")");
        configOutput(AB_EGT_OUTPUT, "A >= B");
        configOutput(AB_GT_OUTPUT, "A > B");
        configOutput(AB_ELT_OUTPUT, "A <= B");
        configOutput(AB_LT_OUTPUT, "A < B");

        configOutput(CD_EQ_OUTPUT, "C = D");
        configOutput(CD_NEQ_OUTPUT, "C != D (not \"C = D\")");
        configOutput(CD_EGT_OUTPUT, "C >= D");
        configOutput(CD_GT_OUTPUT, "C >");
        configOutput(CD_ELT_OUTPUT, "C <= D");
        configOutput(CD_LT_OUTPUT, "C < D");

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
        haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = true;
        haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = false;
		haveTrigHighLow = false;
    }

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);

        trueOutput.setBoth(v_GateHigh);
        falseOutput.setBoth(v_GateLow);
        tolerance.setBoth(voltTolValue::vt_hnt);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        trueOutput.setBoth((voltValue)getJsonInt(rootJ, "trueOutput", (int)v_GateHigh));
        falseOutput.setBoth((voltValue)getJsonInt(rootJ, "falseOutput", (int)v_GateLow));
        tolerance.setBoth((voltTolValue)getJsonInt(rootJ, "tolerance", (int)voltTolValue::vt_hnt));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "trueOutput", json_integer((int)trueOutput.req));
        json_object_set_new(rootJ, "falseOutput", json_integer((int)falseOutput.req));
        json_object_set_new(rootJ, "tolerance", json_integer((int)tolerance.req));
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

        tolerance.updateActual();

        // Available A/B channels
        abOutputsConnected =
            outputs[AB_EQ_OUTPUT].isConnected() ||
            outputs[AB_NEQ_OUTPUT].isConnected() ||
            outputs[AB_EGT_OUTPUT].isConnected() ||
            outputs[AB_GT_OUTPUT].isConnected() ||
            outputs[AB_ELT_OUTPUT].isConnected() ||
            outputs[AB_LT_OUTPUT].isConnected();
        int aChannels = std::max(inputs[A_INPUT].getChannels(), 1);
        int bChannels = std::max(inputs[B_INPUT].getChannels(), 1);
        abChannels = std::max(aChannels, bChannels);
        if (inputs[B_INPUT].isConnected())
			abChannels = std::max(abChannels, 
                std::max(inputs[B_INPUT].getChannels(), 1));
        if (abOutputsConnected) {
            outputs[AB_EQ_OUTPUT].setChannels(abChannels);
            outputs[AB_NEQ_OUTPUT].setChannels(abChannels);
            outputs[AB_EGT_OUTPUT].setChannels(abChannels);
            outputs[AB_GT_OUTPUT].setChannels(abChannels);
            outputs[AB_ELT_OUTPUT].setChannels(abChannels);
            outputs[AB_LT_OUTPUT].setChannels(abChannels);
        }

        // Available C/D channels
        cdOutputsConnected =
            outputs[CD_EQ_OUTPUT].isConnected() ||
            outputs[CD_NEQ_OUTPUT].isConnected() ||
            outputs[CD_EGT_OUTPUT].isConnected() ||
            outputs[CD_GT_OUTPUT].isConnected() ||
            outputs[CD_ELT_OUTPUT].isConnected() ||
            outputs[CD_LT_OUTPUT].isConnected();
        int cChannels = inputs[C_INPUT].isConnected()
            ? std::max(inputs[C_INPUT].getChannels(), 1)
            : aChannels;  // Normalize to A
        int dChannels = inputs[D_INPUT].isConnected()
            ? std::max(inputs[D_INPUT].getChannels(), 1)
            : bChannels;  // Normalize to B
        cdChannels = std::max(cChannels, dChannels);
        if (cdOutputsConnected) {
            outputs[CD_EQ_OUTPUT].setChannels(cdChannels);
            outputs[CD_NEQ_OUTPUT].setChannels(cdChannels);
            outputs[CD_EGT_OUTPUT].setChannels(cdChannels);
            outputs[CD_GT_OUTPUT].setChannels(cdChannels);
            outputs[CD_ELT_OUTPUT].setChannels(cdChannels);
            outputs[CD_LT_OUTPUT].setChannels(cdChannels);
        }

        maxChannels = std::max(abChannels, cdChannels);

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

        if (doProcess && (abOutputsConnected || cdOutputsConnected)) {
            float tol = voltTolValues[tolerance.act];

            // Get/set true/false values to use
            float trueInput[PORT_MAX_CHANNELS] = { 0.f };
            float falseInput[PORT_MAX_CHANNELS] = { 0.f };
            bool trueConnected = inputs[TRUE_INPUT].isConnected();
            bool falseConnected = inputs[FALSE_INPUT].isConnected();
            for (int c = 0; c < maxChannels; c++) {
                trueInput[c] = trueConnected
                    ? clipToVoltRange(inputs[TRUE_INPUT].getPolyVoltage(c), outClipRange.act)
                    : clipToVoltRange(voltValues[trueOutput.act], outClipRange.act);
                falseInput[c] = falseConnected
                    ? clipToVoltRange(inputs[FALSE_INPUT].getPolyVoltage(c), outClipRange.act)
                    : clipToVoltRange(voltValues[falseOutput.act], outClipRange.act);
            }

            bool aConnected = inputs[A_INPUT].isConnected();
            bool bConnected = inputs[B_INPUT].isConnected();

            if (abOutputsConnected) {
                for (int c = 0; c < abChannels; c++) {
                    float aInput = aConnected
                        ? inputs[A_INPUT].getPolyVoltage(c)
                        : 0.f;
                    float bInput = bConnected
                        ? inputs[B_INPUT].getPolyVoltage(c)
                        : 0.f;
                   
                    // == !=
                    bool inTol = std::abs(aInput - bInput) <= tol;
                    outputs[AB_EQ_OUTPUT].setVoltage(inTol
                        ? trueInput[c] 
                        : falseInput[c], c);
                    outputs[AB_NEQ_OUTPUT].setVoltage(!inTol
                        ? trueInput[c] 
                        : falseInput[c], c);

                    // >= >
                    bool gt = aInput > bInput;
                    outputs[AB_EGT_OUTPUT].setVoltage(inTol || gt
                        ? trueInput[c] 
                        : falseInput[c], c);
                    outputs[AB_GT_OUTPUT].setVoltage(!inTol && gt
                        ? trueInput[c] 
                        : falseInput[c], c);

                    // <= <
                    bool lt = aInput < bInput;
                    outputs[AB_ELT_OUTPUT].setVoltage(inTol || lt
                        ? trueInput[c] 
                        : falseInput[c], c);
                    outputs[AB_LT_OUTPUT].setVoltage(!inTol && lt
                        ? trueInput[c] 
                        : falseInput[c], c);
                }
            }

            if (cdOutputsConnected) {
                bool cConnected = inputs[C_INPUT].isConnected();
                bool dConnected = inputs[D_INPUT].isConnected();
    
                for (int c = 0; c < cdChannels; c++) {
                    float cInput = cConnected
                        ? inputs[C_INPUT].getPolyVoltage(c)
                        : aConnected
                            ? inputs[A_INPUT].getPolyVoltage(c)
                            : 0.f;
                    float dInput = dConnected
                        ? inputs[D_INPUT].getPolyVoltage(c)
                        : bConnected
                            ? inputs[B_INPUT].getPolyVoltage(c)
                            : 0.f;

                    bool inTol = std::abs(cInput - dInput) < tol;
                    outputs[CD_EQ_OUTPUT].setVoltage(inTol
                        ? trueInput[c]
                        : falseInput[c], c);
                    outputs[CD_NEQ_OUTPUT].setVoltage(!inTol
                        ? trueInput[c]
                        : falseInput[c], c);

                    bool gt = cInput > dInput;
                    outputs[CD_EGT_OUTPUT].setVoltage(inTol || gt
                        ? trueInput[c]
                        : falseInput[c], c);
                    outputs[CD_GT_OUTPUT].setVoltage(!inTol && gt
                        ? trueInput[c]
                        : falseInput[c], c);

                    bool lt = cInput < dInput;
                    outputs[CD_ELT_OUTPUT].setVoltage(inTol || lt
                        ? trueInput[c]
                        : falseInput[c], c);
                    outputs[CD_LT_OUTPUT].setVoltage(!inTol && lt
                        ? trueInput[c]
                        : falseInput[c], c);
                }
            }
        }

        cycle256++;
    }
};

struct VCMP2IModuleWidget : InfNoiseModuleWidget {
    VCMP2IModuleWidget(VCMP2IModule *module) {
        initializeWidget(module, "res/VCMP2I");

        const float leftColumn = 15.132f;
        const float rightColumn = 43.868f;
        const float trueFalseRow = 50.679;
        const float abRow = 85.753f;
        const float cdRow = 227.361f;
        const float Log1stRow = 120.939f;
        const float Log2ndRow = 262.547f;
        const float logRowSpacing = 35.0735f;

        // True/False inputs
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, trueFalseRow), module, VCMP2IModule::TRUE_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, trueFalseRow), module, VCMP2IModule::FALSE_INPUT));

        // A/B inputs
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, abRow), module, VCMP2IModule::A_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, abRow), module, VCMP2IModule::B_INPUT));

        // A/B logic outputs
        float row = Log1stRow;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, row), module, VCMP2IModule::AB_EQ_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, row), module, VCMP2IModule::AB_NEQ_OUTPUT));
        row += logRowSpacing;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, row), module, VCMP2IModule::AB_EGT_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, row), module, VCMP2IModule::AB_GT_OUTPUT));
        row += logRowSpacing;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, row), module, VCMP2IModule::AB_ELT_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, row), module, VCMP2IModule::AB_LT_OUTPUT));

        // C/D inputs
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, cdRow), module, VCMP2IModule::C_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, cdRow), module, VCMP2IModule::D_INPUT));

        // C/D logic outputs
        row = Log2ndRow;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, row), module, VCMP2IModule::CD_EQ_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, row), module, VCMP2IModule::CD_NEQ_OUTPUT));
        row += logRowSpacing;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, row), module, VCMP2IModule::CD_EGT_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, row), module, VCMP2IModule::CD_GT_OUTPUT));
        row += logRowSpacing;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, row), module, VCMP2IModule::CD_ELT_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, row), module, VCMP2IModule::CD_LT_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        VCMP2IModule* module = dynamic_cast<VCMP2IModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> voltNames = getVoltValuesNames();
        menu->addChild(createIndexPtrSubmenuItem("True output level", voltNames, 
           &module->trueOutput.req));
        menu->addChild(createIndexPtrSubmenuItem("False output level", voltNames, 
           &module->falseOutput.req));

        std::vector<std::string> voltTolNames = getVoltTolValuesNames();
        menu->addChild(createIndexPtrSubmenuItem("Tolerance", voltTolNames,
            &module->tolerance.req));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelVCMP2I = createModel<VCMP2IModule, VCMP2IModuleWidget>("VCMP2I");