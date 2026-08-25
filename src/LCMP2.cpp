// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct LCMP2Module : InfNoiseModule {
    enum ParamId {
        A_INV_PARAM,
        B_INV_PARAM,
        C_INV_PARAM,
        D_INV_PARAM,
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
        AB_AND_OUTPUT,
        AB_NAND_OUTPUT,
        AB_OR_OUTPUT,
        AB_NOR_OUTPUT,
        AB_XOR_OUTPUT,
        AB_XNOR_OUTPUT,
        CD_AND_OUTPUT,
        CD_NAND_OUTPUT,
        CD_OR_OUTPUT,
        CD_NOR_OUTPUT,
        CD_XOR_OUTPUT,  
        CD_XNOR_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        LIGHTS_LEN
    };

    actReqValue<voltValue> trueOutput = actReqValue<voltValue>(v_GateHigh);
    actReqValue<voltValue> falseOutput = actReqValue<voltValue>(v_GateLow);
    bool abOutputsConnected = false;
    bool cdOutputsConnected = false;
    int abChannels = 1;
    int cdChannels = 1;
    int maxChannels = 1;

    LCMP2Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configInput(TRUE_INPUT, "Signal to output if true (normalized via context-menu, default 10V)");
        configInput(FALSE_INPUT, "Signal to output if false (normalized via context-menu, default 0V)");
        
        configInput(A_INPUT, "A-Signal");
        configInput(B_INPUT, "B-Signal");

        configInput(C_INPUT, "C-Signal (normalized to \"A OR B\", if not connected)");
        configInput(D_INPUT, "D-Signal (normalized to \"A AND B\", if not connected)");

        configOutput(AB_AND_OUTPUT, "A AND B");
        configOutput(AB_NAND_OUTPUT, "A NAND B (not \"A AND B\")");
        configOutput(AB_OR_OUTPUT, "A OR B");
        configOutput(AB_NOR_OUTPUT, "A NOR B (not \"A OR B\")");
        configOutput(AB_XOR_OUTPUT, "A XOR B");
        configOutput(AB_XNOR_OUTPUT, "A XNOR B (not \"A XOR B\")");

        configOutput(CD_AND_OUTPUT, "C AND D");
        configOutput(CD_NAND_OUTPUT, "C NAND D (not \"C AND D\")");
        configOutput(CD_OR_OUTPUT, "C OR D");
        configOutput(CD_NOR_OUTPUT, "C NOR D (not \"C OR D\")");
        configOutput(CD_XOR_OUTPUT, "C XOR D");
        configOutput(CD_XNOR_OUTPUT, "C XNOR D (not \"C XOR D\")");

        configSwitch(A_INV_PARAM, 0.0, 1.0, 0.0, "Invert-A", { "Normal", "Inverted" });
        configSwitch(B_INV_PARAM, 0.0, 1.0, 0.0, "Invert-B", { "Normal", "Inverted" });
        configSwitch(C_INV_PARAM, 0.0, 1.0, 0.0, "Invert-C", { "Normal", "Inverted" });
        configSwitch(D_INV_PARAM, 0.0, 1.0, 0.0, "Invert-D", { "Normal", "Inverted" });

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
        haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = true;
        haveGateDetect = true;
		haveGateHighLow = false;
		haveTrigDetect = false;
		haveTrigHighLow = false;
    }

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        trueOutput.setBoth(v_GateHigh);
        falseOutput.setBoth(v_GateLow);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        trueOutput.setBoth((voltValue)getJsonInt(rootJ, "trueOutput", (int)v_GateHigh));
        falseOutput.setBoth((voltValue)getJsonInt(rootJ, "falseOutput", (int)v_GateLow));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "trueOutput", json_integer((int)trueOutput.req));
        json_object_set_new(rootJ, "falseOutput", json_integer((int)falseOutput.req));
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

        // Available A/B channels
        abOutputsConnected = 
            outputs[AB_AND_OUTPUT].isConnected() ||
            outputs[AB_NAND_OUTPUT].isConnected() ||
            outputs[AB_OR_OUTPUT].isConnected() ||
            outputs[AB_NOR_OUTPUT].isConnected() ||
            outputs[AB_XOR_OUTPUT].isConnected() ||
            outputs[AB_XNOR_OUTPUT].isConnected();
        abChannels = std::max(
            std::max(inputs[A_INPUT].getChannels(), 1),
            std::max(inputs[B_INPUT].getChannels(), 1));
        if (abOutputsConnected){
            outputs[AB_AND_OUTPUT].setChannels(abChannels);
            outputs[AB_NAND_OUTPUT].setChannels(abChannels);
            outputs[AB_OR_OUTPUT].setChannels(abChannels);
            outputs[AB_NOR_OUTPUT].setChannels(abChannels);
            outputs[AB_XOR_OUTPUT].setChannels(abChannels);
            outputs[AB_XNOR_OUTPUT].setChannels(abChannels);
        }

        // Available C/D channels
        cdOutputsConnected = 
            outputs[CD_AND_OUTPUT].isConnected() ||
            outputs[CD_NAND_OUTPUT].isConnected() ||
            outputs[CD_OR_OUTPUT].isConnected() ||
            outputs[CD_NOR_OUTPUT].isConnected() ||
            outputs[CD_XOR_OUTPUT].isConnected() ||
            outputs[CD_XNOR_OUTPUT].isConnected();
        cdChannels = std::max(
            std::max(inputs[C_INPUT].getChannels(), 1),
            std::max(inputs[D_INPUT].getChannels(), 1));
        if (!inputs[C_INPUT].isConnected() || !inputs[D_INPUT].isConnected())
            cdChannels = std::max(abChannels, cdChannels);
        if (cdOutputsConnected) {
            outputs[CD_AND_OUTPUT].setChannels(cdChannels);
            outputs[CD_NAND_OUTPUT].setChannels(cdChannels);
            outputs[CD_OR_OUTPUT].setChannels(cdChannels);
            outputs[CD_NOR_OUTPUT].setChannels(cdChannels);
            outputs[CD_XOR_OUTPUT].setChannels(cdChannels);
            outputs[CD_XNOR_OUTPUT].setChannels(cdChannels);
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
            // Get/set true/false values to use
            float trueInput[PORT_MAX_CHANNELS] = { 0.f };
            float falseInput[PORT_MAX_CHANNELS] = { 0.f };
            for (int c = 0; c < maxChannels; c++) {
                trueInput[c] = inputs[TRUE_INPUT].isConnected() 
                    ? clipToVoltRange(inputs[TRUE_INPUT].getPolyVoltage(c), outClipRange.act)
                    : clipToVoltRange(voltValues[trueOutput.act], outClipRange.act);
                falseInput[c] = inputs[FALSE_INPUT].isConnected() 
                    ? clipToVoltRange(inputs[FALSE_INPUT].getPolyVoltage(c), outClipRange.act)
                    : clipToVoltRange(voltValues[falseOutput.act], outClipRange.act);
            }

            bool abOr[PORT_MAX_CHANNELS] = { false };  // used as normalized input for C
            bool abAnd[PORT_MAX_CHANNELS] = { false }; // used as normalized input for D

            if (abOutputsConnected) {
                for (int c = 0; c < abChannels; c++) {
                    bool aInput = inputs[A_INPUT].isConnected() 
                        ? inputs[A_INPUT].getPolyVoltage(c) >= trueDetectValues[gateDetHigh.act]
                        : false;
                    if (params[A_INV_PARAM].getValue() > 0.5f)
                        aInput = !aInput;
                    bool bInput = inputs[B_INPUT].isConnected() 
                        ? inputs[B_INPUT].getPolyVoltage(c) >= trueDetectValues[gateDetHigh.act]
                        : false;
                    if (params[B_INV_PARAM].getValue() > 0.5f)
                        bInput = !bInput;

                    abAnd[c] = aInput && bInput;
                    abOr[c] = aInput || bInput;
                    bool abXor = aInput ^ bInput;

                    outputs[AB_AND_OUTPUT].setVoltage(abAnd[c] ? trueInput[c] : falseInput[c], c);
                    outputs[AB_NAND_OUTPUT].setVoltage(!abAnd[c] ? trueInput[c] : falseInput[c], c);
                    outputs[AB_OR_OUTPUT].setVoltage(abOr[c] ? trueInput[c] : falseInput[c], c);
                    outputs[AB_NOR_OUTPUT].setVoltage(!abOr[c] ? trueInput[c] : falseInput[c], c);
                    outputs[AB_XOR_OUTPUT].setVoltage(abXor ? trueInput[c] : falseInput[c], c);
                    outputs[AB_XNOR_OUTPUT].setVoltage(!abXor ? trueInput[c] : falseInput[c], c);
                }
            }

            if (cdOutputsConnected) {
                for (int c = 0; c < cdChannels; c++) {
                    bool cInput = inputs[C_INPUT].isConnected() 
                        ? inputs[C_INPUT].getPolyVoltage(c) >= trueDetectValues[gateDetHigh.act]
                        : abOr[c];
                    if (params[C_INV_PARAM].getValue() > 0.5f)
						cInput = !cInput;
                    bool dInput = inputs[D_INPUT].isConnected() 
                        ? inputs[D_INPUT].getPolyVoltage(c) >= trueDetectValues[gateDetHigh.act]
                        : abAnd[c];
                    if (params[D_INV_PARAM].getValue() > 0.5f)
                        dInput = !dInput;

                    bool cdAnd = cInput && dInput;
                    bool cdOr = cInput || dInput;
                    bool cdXor = cInput ^ dInput;
                    outputs[CD_AND_OUTPUT].setVoltage(cdAnd ? trueInput[c] : falseInput[c], c);
                    outputs[CD_NAND_OUTPUT].setVoltage(!cdAnd ? trueInput[c] : falseInput[c], c);
                    outputs[CD_OR_OUTPUT].setVoltage(cdOr ? trueInput[c] : falseInput[c], c);
                    outputs[CD_NOR_OUTPUT].setVoltage(!cdOr ? trueInput[c] : falseInput[c], c);
                    outputs[CD_XOR_OUTPUT].setVoltage(cdXor ? trueInput[c] : falseInput[c], c);
                    outputs[CD_XNOR_OUTPUT].setVoltage(!cdXor ? trueInput[c] : falseInput[c], c);
                }
            }
        }

        cycle256++;
    }
};

struct LCMP2ModuleWidget : InfNoiseModuleWidget {
    LCMP2ModuleWidget(LCMP2Module *module) {
        initializeWidget(module, "res/LCMP2");

        float leftColumn = 15.132f;
        float rightColumn = 43.868f;
        float trueFalseRow = 50.679;
        float abRow = 85.753f;
        float cdRow = 227.361f;
        float Log1stRow = 120.939f;
        float Log2ndRow = 262.547f;
        float logRowSpacing = 35.0735f;

        // True/False inputs
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, trueFalseRow), module, LCMP2Module::TRUE_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, trueFalseRow), module, LCMP2Module::FALSE_INPUT));

        // A/B inputs
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, abRow), module, LCMP2Module::A_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, abRow), module, LCMP2Module::B_INPUT));

        addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(5.599f, 73.546f), module, LCMP2Module::A_INV_PARAM));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(54.288f, 73.546f), module, LCMP2Module::B_INV_PARAM));

        // A/B logic outputs
        float row = Log1stRow;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, row), module, LCMP2Module::AB_AND_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, row), module, LCMP2Module::AB_NAND_OUTPUT));
        row += logRowSpacing;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, row), module, LCMP2Module::AB_OR_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, row), module, LCMP2Module::AB_NOR_OUTPUT));
        row += logRowSpacing;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, row), module, LCMP2Module::AB_XOR_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, row), module, LCMP2Module::AB_XNOR_OUTPUT));

        // C/D inputs
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, cdRow), module, LCMP2Module::C_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, cdRow), module, LCMP2Module::D_INPUT));

        addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(5.599f, 215.321f), module, LCMP2Module::C_INV_PARAM));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(54.288f, 215.321f), module, LCMP2Module::D_INV_PARAM));

        // C/D logic outputs
        row = Log2ndRow;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, row), module, LCMP2Module::CD_AND_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, row), module, LCMP2Module::CD_NAND_OUTPUT));
        row += logRowSpacing;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, row), module, LCMP2Module::CD_OR_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, row), module, LCMP2Module::CD_NOR_OUTPUT));
        row += logRowSpacing;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, row), module, LCMP2Module::CD_XOR_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, row), module, LCMP2Module::CD_XNOR_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        LCMP2Module* module = dynamic_cast<LCMP2Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> voltNames = getVoltValuesNames();
        menu->addChild(createIndexPtrSubmenuItem("True output level", voltNames, 
           &module->trueOutput.req));
        menu->addChild(createIndexPtrSubmenuItem("False output level", voltNames, 
           &module->falseOutput.req));
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelLCMP2 = createModel<LCMP2Module, LCMP2ModuleWidget>("LCMP2");