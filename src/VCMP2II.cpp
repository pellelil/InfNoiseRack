// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct VCMP2IIModule : InfNoiseModule {
    enum ParamId {
        ABS_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        A_INPUT,
        B_INPUT,
        C_INPUT,
        D_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        AB_MIN_OUTPUT,
        AB_MAX_OUTPUT,
        AB_NTZ_OUTPUT,
        AB_FFZ_OUTPUT,
        AB_ABSDIFF_OUTPUT,
        AB_AVG_OUTPUT,
        C_INT_OUTPUT,
        C_FRAC_OUTPUT,
        CD_PLUS_OUTPUT,
        CD_MUL_OUTPUT,
        CD_MINUS_OUTPUT,
        DC_MINUS_OUTPUT,
        CD_DIV_OUTPUT,
        DC_DIV_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        LIGHTS_LEN
    };

    // Divisor magnitude below this is treated as zero (avoids huge/inf quotient).
    static constexpr float kDivisorEpsilon = 1e-6f;

    enum divByZeroModeType {
        dbzKeep,
        dbzZero
    };
    actReqValue<divByZeroModeType> divByZeroMode = actReqValue<divByZeroModeType>(divByZeroModeType::dbzKeep);
    bool abOutputsConnected = false;
    bool cdOutputsConnected = false;
    int abChannels = 1;
    int cdChannels = 1;
    float lastCdivD[PORT_MAX_CHANNELS] = { 0.f };
    float lastDdivC[PORT_MAX_CHANNELS] = { 0.f };
    bool absValues = false;

    VCMP2IIModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configInput(A_INPUT, "A-Signal (normalized to 0V)");
        configInput(B_INPUT, "B-Signal (normalized to 0V)");
        configInput(C_INPUT, "C-Signal (normalized to A, if not connected)");
        configInput(D_INPUT, "D-Signal (normalized to B, if not connected)");

        configOutput(AB_MIN_OUTPUT, "Lowest: A or B");
        configOutput(AB_MAX_OUTPUT, "Highest: A or B");
        configOutput(AB_NTZ_OUTPUT, "Nearest to 0: A or B");
		configOutput(AB_FFZ_OUTPUT, "Farthest from 0: A or B");
        configOutput(AB_ABSDIFF_OUTPUT, "Absolute difference between A and B: abs(A-B)");
        configOutput(AB_AVG_OUTPUT, "Average/Mix of A and B: (A+B)/2");

        configSwitch(ABS_PARAM, 0.0, 1.0, 0.0, "Int/Frac values", { "Signed", "Absolute" });
        configOutput(C_INT_OUTPUT, "Integer part of C");
        configOutput(C_FRAC_OUTPUT, "Fractional part of C");
        configOutput(CD_PLUS_OUTPUT, "Sum: C+D");
        configOutput(CD_MUL_OUTPUT, "Product: C*D");
        configOutput(CD_MINUS_OUTPUT, "Difference: C-D");
        configOutput(DC_MINUS_OUTPUT, "Difference: D-C");
        configOutput(CD_DIV_OUTPUT, "Division: C/D (div-by-zero set via menu)");
        configOutput(DC_DIV_OUTPUT, "Division: D/C (div-by-zero set via menu)");

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
        divByZeroMode.setBoth(divByZeroModeType::dbzKeep);

        for (int i = 0; i < PORT_MAX_CHANNELS; i++) {
			lastCdivD[i] = 0.f;
			lastDdivC[i] = 0.f;
		}
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        divByZeroMode.setBoth((divByZeroModeType)getJsonInt(rootJ, "divByZeroMode", (int)divByZeroModeType::dbzKeep));

        getJsonFloatArray(rootJ, "lastCdivD", lastCdivD, PORT_MAX_CHANNELS, 0.f);
        getJsonFloatArray(rootJ, "lastDdivC", lastDdivC, PORT_MAX_CHANNELS, 0.f);
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "divByZeroMode", json_integer((int)divByZeroMode.req));
        setJsonFloatArray(rootJ, "lastCdivD", lastCdivD, PORT_MAX_CHANNELS);
        setJsonFloatArray(rootJ, "lastDdivC", lastDdivC, PORT_MAX_CHANNELS);
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        divByZeroMode.updateActual();

        abOutputsConnected =
            inputs[AB_MIN_OUTPUT].isConnected() ||
            inputs[AB_MAX_OUTPUT].isConnected() ||
            inputs[AB_NTZ_OUTPUT].isConnected() ||
            inputs[AB_FFZ_OUTPUT].isConnected() ||
            inputs[AB_ABSDIFF_OUTPUT].isConnected() ||
            inputs[AB_AVG_OUTPUT].isConnected();

        int aChannels = std::max(inputs[A_INPUT].getChannels(), 1);
        int bChannels = std::max(inputs[A_INPUT].getChannels(), 1);
        abChannels = std::max(aChannels, bChannels);
        if (abOutputsConnected) {
            outputs[AB_MIN_OUTPUT].setChannels(abChannels);
            outputs[AB_MAX_OUTPUT].setChannels(abChannels);
            outputs[AB_NTZ_OUTPUT].setChannels(abChannels);
            outputs[AB_FFZ_OUTPUT].setChannels(abChannels);
            outputs[AB_ABSDIFF_OUTPUT].setChannels(abChannels);
            outputs[AB_AVG_OUTPUT].setChannels(abChannels);
        }

        cdOutputsConnected =
            outputs[C_INT_OUTPUT].isConnected() ||
            outputs[C_FRAC_OUTPUT].isConnected() ||
            outputs[CD_PLUS_OUTPUT].isConnected() ||
            outputs[CD_MUL_OUTPUT].isConnected() ||
            outputs[CD_MINUS_OUTPUT].isConnected() ||
            outputs[DC_MINUS_OUTPUT].isConnected() ||
            outputs[CD_DIV_OUTPUT].isConnected() ||
            outputs[DC_DIV_OUTPUT].isConnected();

        int cChannels = inputs[C_INPUT].isConnected()
			? std::max(inputs[C_INPUT].getChannels(), 1)
			: aChannels;  // Normalize to A
        int dChannels = inputs[D_INPUT].isConnected()
            ? std::max(inputs[D_INPUT].getChannels(), 1)
            : bChannels;  // Normalize to B
        cdChannels = std::max(cChannels, dChannels);
        if (cdOutputsConnected) {
            outputs[C_INT_OUTPUT].setChannels(cdChannels);
            outputs[C_FRAC_OUTPUT].setChannels(cdChannels);
            outputs[CD_PLUS_OUTPUT].setChannels(cdChannels);
            outputs[CD_MUL_OUTPUT].setChannels(cdChannels);
            outputs[CD_MINUS_OUTPUT].setChannels(cdChannels);
            outputs[DC_MINUS_OUTPUT].setChannels(cdChannels);
            outputs[CD_DIV_OUTPUT].setChannels(cdChannels);
            outputs[DC_DIV_OUTPUT].setChannels(cdChannels);
        }

        absValues = params[ABS_PARAM].getValue() > 0.5f;

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

                    if (abOutputsConnected) {
                        float voltage = std::min(aInput, bInput);
                        outputs[AB_MIN_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), c);

                        voltage = std::max(aInput, bInput);
                        outputs[AB_MAX_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), c);

                        voltage = (std::fabs(aInput) < std::fabs(bInput))
                            ? aInput
                            : bInput;
                        outputs[AB_NTZ_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), c);

                        voltage = (std::fabs(aInput) > std::fabs(bInput))
                            ? aInput
                            : bInput;
                        outputs[AB_FFZ_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), c);

                        voltage = std::abs(aInput - bInput);
                        outputs[AB_ABSDIFF_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), c);

                        voltage = (aInput + bInput) / 2.f;
                        outputs[AB_AVG_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), c);
                    }
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

                    float cInt = std::trunc(cInput);
                    float cFrac = cInput - cInt;
                    if (absValues) {
                        cInt = std::abs(cInt);
                        cFrac = std::abs(cFrac);
                    }
                    outputs[C_INT_OUTPUT].setVoltage(clipToVoltRange(cInt, outClipRange.act), c);
                    outputs[C_FRAC_OUTPUT].setVoltage(clipToVoltRange(cFrac, outClipRange.act), c);

                    float voltage = cInput + dInput;
                    outputs[CD_PLUS_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), c);

                    voltage = cInput * dInput;
                    outputs[CD_MUL_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), c);

                    voltage = cInput - dInput;
                    outputs[CD_MINUS_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), c);

                    voltage = dInput - cInput;
                    outputs[DC_MINUS_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), c);

                    voltage = (std::abs(dInput) >= kDivisorEpsilon)
                        ? cInput / dInput
                        : divByZeroMode.act == divByZeroModeType::dbzKeep
                            ? lastCdivD[c]
                            : 0.f;
                    outputs[CD_DIV_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), c);
                    lastCdivD[c] = voltage;
                    
                    voltage = (std::abs(cInput) >= kDivisorEpsilon)
                        ? dInput / cInput
                        : divByZeroMode.act == divByZeroModeType::dbzKeep
                            ? lastDdivC[c]
                            : 0.f;
                    outputs[DC_DIV_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), c);
                    lastDdivC[c] = voltage;
                }
            }
        }

        cycle256++;
    }
};

struct VCMP2IIModuleWidget : InfNoiseModuleWidget {
    VCMP2IIModuleWidget(VCMP2IIModule *module) {
        initializeWidget(module, "res/VCMP2II");

        const float cntrClm = 30.0f;
        const float leftColumn = 15.132f;
        const float rightColumn = 43.868f;
        const float section1Row = 54.577f;
        const float section2Row = 228.400f;
        const float rowSpacing = 34.765f;

        // Top section (first 6 outputs, then 2 inputs)
        float row = section1Row;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, row), module, VCMP2IIModule::AB_MIN_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, row), module, VCMP2IIModule::AB_MAX_OUTPUT));
        row += rowSpacing;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, row), module, VCMP2IIModule::AB_NTZ_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, row), module, VCMP2IIModule::AB_FFZ_OUTPUT));
        row += rowSpacing;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, row), module, VCMP2IIModule::AB_ABSDIFF_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, row), module, VCMP2IIModule::AB_AVG_OUTPUT));

        const float abRow = 158.871f;
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, abRow), module, VCMP2IIModule::A_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, abRow), module, VCMP2IIModule::B_INPUT));

        // Bottoms section (first 2 inputs, then 8 outputs)
        const float cdRow = 193.635f;
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, cdRow), module, VCMP2IIModule::C_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, cdRow), module, VCMP2IIModule::D_INPUT));

        infNoiseLtSmallButton* absBtn = createParamCentered<infNoiseLtSmallButton>(Vec(cntrClm, 220.397f), module, VCMP2IIModule::ABS_PARAM);
        absBtn->setup(bc_green, false);
        addParam(absBtn);

        row = section2Row;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, row), module, VCMP2IIModule::C_INT_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, row), module, VCMP2IIModule::C_FRAC_OUTPUT));
        row += rowSpacing;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, row), module, VCMP2IIModule::CD_PLUS_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, row), module, VCMP2IIModule::CD_MUL_OUTPUT));
        row += rowSpacing;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, row), module, VCMP2IIModule::CD_MINUS_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, row), module, VCMP2IIModule::DC_MINUS_OUTPUT));
        row += rowSpacing;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftColumn, row), module, VCMP2IIModule::CD_DIV_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightColumn, row), module, VCMP2IIModule::DC_DIV_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        VCMP2IIModule* module = dynamic_cast<VCMP2IIModule*>(this->module);
        assert(module);
        
        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexPtrSubmenuItem("Div-by-zero mode",
             	{"Keep last value", "Output 0V"},
             	&module->divByZeroMode.req
        ));


        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelVCMP2II = createModel<VCMP2IIModule, VCMP2IIModuleWidget>("VCMP2II");