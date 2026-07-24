// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct SignModule : InfNoiseModule {
    enum ParamId {
	    PARAMS_LEN
	};
    enum InputsId {
        CUT_NEGATIVE_INPUT,
        CUT_POSITIVE_INPUT,
        ABS_INPUT,
        BIPOLAR_INVERT_INPUT,
        RANGE_INVERT_INPUT,
        PLUS_FIVE_INPUT,
        MINUS_FIVE_INPUT,
        GATE_ON_INPUT,
        GATE_OFF_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        CUT_NEGATIVE_OUTPUT,
        CUT_POSITIVE_OUTPUT,
        ABS_OUTPUT,
        BIPOLAR_INVERT_OUTPUT,
        RANGE_INVERT_OUTPUT,
        PLUS_FIVE_OUTPUT,
        MINUS_FIVE_OUTPUT,
        GATE_ON_OUTPUT,
        GATE_OFF_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
		LIGHTS_LEN
	};

    actReqValue<voltInvRange> inversionRange = actReqValue<voltInvRange>(voltInvRange::vir_zt10);

    const int portCount = 9; // Number of input/output-pairs in module (9)
    int firstInUse = -1;
    int lastInUse = -1;
    int portsInUse = 0;
    int portChannels[9];
    bool portInUse[9];

    SignModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configInput(CUT_NEGATIVE_INPUT, "Cut negative");
        configInput(CUT_POSITIVE_INPUT, "Cut positive (normalized from prev. input)");
        configInput(ABS_INPUT, "Absolute value (normalized from prev. input)");
        configInput(BIPOLAR_INVERT_INPUT, "Bipolar-invert (normalized from prev. input)");
        configInput(RANGE_INVERT_INPUT, "Range-invert (normalized from prev. input)");
        configInput(PLUS_FIVE_INPUT, "Add 5V (normalized from prev. input)");
        configInput(MINUS_FIVE_INPUT, "Subtract 5V (normalized from prev. input)");
        configInput(GATE_ON_INPUT, "Gate-ON (normalized from prev. input)");
        configInput(GATE_OFF_INPUT, "Gate-OFF (normalized from prev. input)");

        configOutput(CUT_NEGATIVE_OUTPUT, "Cut negative (negative values changed to 0)");
        configOutput(CUT_POSITIVE_OUTPUT, "Cut positive (positive values changed to 0)");
        configOutput(ABS_OUTPUT, "Absolute value (negative values changed to positive)");
        configOutput(BIPOLAR_INVERT_OUTPUT, "Bipolar-invert (value multiplied by -1)");
        configOutput(RANGE_INVERT_OUTPUT, "Range-invert (value inverted in range)");
        configOutput(PLUS_FIVE_OUTPUT, "Add 5V (convert bipolar to unipolar)");
        configOutput(MINUS_FIVE_OUTPUT, "Subtract 5V (convert unipolar to bipolar)");
        configOutput(GATE_ON_OUTPUT, "Gate-ON (detect gate-on condition)");
        configOutput(GATE_OFF_OUTPUT, "Gate-OFF (detect gate-off condition)");

        configBypass(CUT_NEGATIVE_INPUT, CUT_NEGATIVE_OUTPUT);
        configBypass(CUT_POSITIVE_INPUT, CUT_POSITIVE_OUTPUT);
        configBypass(ABS_INPUT, ABS_OUTPUT);
        configBypass(BIPOLAR_INVERT_INPUT, BIPOLAR_INVERT_OUTPUT);
        configBypass(RANGE_INVERT_INPUT, RANGE_INVERT_OUTPUT);
        configBypass(PLUS_FIVE_INPUT, PLUS_FIVE_OUTPUT);
        configBypass(MINUS_FIVE_INPUT, MINUS_FIVE_OUTPUT);
        configBypass(GATE_ON_INPUT, GATE_ON_OUTPUT);
        configBypass(GATE_OFF_INPUT, GATE_OFF_OUTPUT);

        // Set InfNoise features (e.g. menu-items)
        haveProcQuality = true;
        haveAutoProcQuality = false;
        haveOutQuantize = true;
        haveOutClipRange = true;
        haveGateDetect = true;
		haveGateHighLow = true;
		haveTrigDetect = false;
		haveTrigHighLow = false;
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        inversionRange.setBoth(voltInvRange::vir_zt10);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        inversionRange.setBoth((voltInvRange)getJsonInt(rootJ, "inversionRange", (int)voltInvRange::vir_zt10));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "inversionRange", json_integer((int)inversionRange.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        if (inversionRange.needsUpdate()) {
            inversionRange.updateActual();
            if (outputInfos.size() > (unsigned)RANGE_INVERT_OUTPUT && outputInfos[RANGE_INVERT_OUTPUT]) {
                outputInfos[RANGE_INVERT_OUTPUT]->name = monoPortPrefix() + "Range-invert: " + getVoltInvRangeName(inversionRange.act);
            }
        }

        firstInUse = -1;
        lastInUse = -1;
        portsInUse = 0;

        int channels = 1;
        for (int i = 0; i < portCount; i++) {
            portInUse[i] = false;
            bool haveInput = inputs[i].isConnected();

            if (haveInput)
                channels = std::max(inputs[i].getChannels(), 1);
            portChannels[i] = channels;
            outputs[i].setChannels(channels);

            bool haveOutput = outputs[i].isConnected();
            if (haveInput || haveOutput) {
                portInUse[i] = true;
                if (firstInUse == -1) firstInUse = i;
                lastInUse = i;
                portsInUse++;
            }
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

        if (doProcess && portsInUse > 0) {
            float channelVoltage[PORT_MAX_CHANNELS] = { 0.f };
            float voltage = 0.0f;
            for (int i=firstInUse; i<=lastInUse; i++) {
                if (portInUse[i]) {
                    bool haveInput = inputs[i].isConnected();
                    bool haveOutput = outputs[i].isConnected();
                    for (int c=0; c<portChannels[i]; c++) {
                        if (haveInput) {
                            voltage = inputs[i].getVoltage(c);
                            channelVoltage[c] = voltage;
                        }

                        if (haveOutput) {
                            voltage = channelVoltage[c];
                            switch (i) {
                                case CUT_NEGATIVE_INPUT:
                                    voltage = voltage < 0
                                        ? 0.f
                                        : voltage;
                                    break;
                                case CUT_POSITIVE_INPUT:
                                    voltage = voltage > 0
                                        ? 0.f
                                        : voltage;
                                    break;
                                case ABS_INPUT:
                                    voltage = std::fabs(voltage);
                                    break;
                                case BIPOLAR_INVERT_INPUT:
                                    voltage *= -1.f;
                                    break;
                                case RANGE_INVERT_INPUT:
                                    voltage = invertToVoltInvRange(voltage, inversionRange.act);
                                    break;
                                case PLUS_FIVE_INPUT:
                                    voltage += 5.f;
                                    break;
                                case MINUS_FIVE_INPUT:
                                    voltage -= 5.f;
                                    break;
                                case GATE_ON_INPUT:
                                    voltage = voltage >= trueDetectValues[gateDetHigh.act] 
                                        ? voltValues[gateOutHigh.act] 
                                        : voltValues[gateOutLow.act];
                                    break;
                                case GATE_OFF_INPUT:
                                    voltage = voltage < trueDetectValues[gateDetHigh.act] 
                                        ? voltValues[gateOutHigh.act]
                                        : voltValues[gateOutLow.act];
                                    break;
                            }

                            voltage = quantizeToMode(voltage, outQuantize.act);
                            voltage = clipToVoltRange(voltage, outClipRange.act);
                            outputs[CUT_NEGATIVE_OUTPUT + i].setVoltage(voltage, c);
                        }
                    }
                }
            }
        }

        cycle256++;
    }
};

struct SignModuleWidget : InfNoiseModuleWidget {
    SignModuleWidget(SignModule *module) {
        initializeWidget(module, "res/Sign");

        float inputColumn = 14.684f;
        float outputColumn = 43.777f;
        float row = 52.042f;
        float rowSpacing = 35.068f;

        for (int i=0; i<9; i++) {
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(inputColumn, row), module, SignModule::CUT_NEGATIVE_INPUT + i));
            addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(outputColumn, row), module, SignModule::CUT_NEGATIVE_OUTPUT + i));
            row += rowSpacing;
        }
    }

    void appendContextMenu(Menu* menu) override {
	    SignModule* module = dynamic_cast<SignModule*>(this->module);
	    assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> InvRangeNames = getVoltInvRangesNames();
        menu->addChild(createIndexPtrSubmenuItem("Inversion-range", InvRangeNames,
            &module->inversionRange.req));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
	}
};

Model *modelSign = createModel<SignModule, SignModuleWidget>("Sign");