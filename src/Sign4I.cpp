// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct Sign4IModule : InfNoiseModule {
    enum ParamId {
        OPRMODE_PARAM,
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
        A_OUTPUT,
        B_OUTPUT,
        C_OUTPUT,
        D_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        LIGHTS_LEN
    };

    actReqValue<voltInvRange> inversionRange = actReqValue<voltInvRange>(voltInvRange::vir_zt10);
    enum oprModeType {opr_invertGate, opr_invertBipolar, opr_invertRange};
    oprModeType oprMode = opr_invertGate;
    int firstIdx = -1;
    int lastIdx = -1;
    bool haveOutputs = false;
    int channels[4] = {1, 1, 1, 1};
        
	Sign4IModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configSwitch(OPRMODE_PARAM, 0.0, 2.0, 0.0, "Operation", {"Invert Gate", "Invert bipolar (-5 to 5)", "Invert range (e.g. unipolar)"});
        
        configInput(A_INPUT, "A");
        configInput(B_INPUT, "B");
        configInput(C_INPUT, "C");
        configInput(D_INPUT, "D");

        
        configOutput(A_OUTPUT, "A");
        configOutput(B_OUTPUT, "B");
        configOutput(C_OUTPUT, "C");
        configOutput(D_OUTPUT, "D");

        configBypass(A_INPUT, A_OUTPUT);
        configBypass(B_INPUT, B_OUTPUT);
        configBypass(C_INPUT, C_OUTPUT);
        configBypass(D_INPUT, D_OUTPUT);

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
		haveAutoProcQuality = false;
        haveOutQuantize = false;
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
            engine::SwitchQuantity* oprModeParam = dynamic_cast<engine::SwitchQuantity*>(getParamQuantity(OPRMODE_PARAM));
            if (oprModeParam && oprModeParam->labels.size() > 2) {
                oprModeParam->labels[2] = "Invert range: " + getVoltInvRangeName(inversionRange.act);
            }
        }

        oprMode = opr_invertBipolar;
        if (params[OPRMODE_PARAM].getValue() < 0.5f)
            oprMode = opr_invertGate;
        else if (params[OPRMODE_PARAM].getValue() > 1.5f)
            oprMode = opr_invertRange;

        haveOutputs = false;
        firstIdx = -1;
        lastIdx = -1;
        for (int i = 0; i < 4; i++) {
            channels[i] = 1;
            if (inputs[A_INPUT + i].isConnected()) {
                haveOutputs = true;
				if (firstIdx < 0)
					firstIdx = i;
				lastIdx = i;
                channels[i] = inputs[A_INPUT + i].getChannels();
            }

            outputs[A_OUTPUT + i].setChannels(channels[i]);
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
            for (int i = firstIdx; i <= lastIdx; i++) {
                if (outputs[A_OUTPUT + i].isConnected()) {
                    bool haveInput = inputs[A_INPUT + i].isConnected();
                    if (oprMode == opr_invertGate) {
                        for (int c = 0; c < channels[i]; c++) {
                            float voltage = haveInput
                                ? inputs[A_INPUT + i].getVoltage(c)
                                : 0.f;
                            bool invGate = voltage < trueDetectValues[gateDetHigh.act];
                            voltage = invGate
								? voltValues[gateOutHigh.act]
								: voltValues[gateOutLow.act];
                            voltage = clipToVoltRange(voltage, outClipRange.act);
                            outputs[A_OUTPUT + i].setVoltage(voltage, c);
                        }
                    } 
                    else if (oprMode == opr_invertBipolar) {
                        for (int c = 0; c < channels[i]; c++) {
                            float voltage = haveInput
                                ? inputs[A_INPUT + i].getVoltage(c) * -1.0f
                                : 0.f;
                            voltage = clipToVoltRange(voltage, outClipRange.act);
                            outputs[A_OUTPUT + i].setVoltage(voltage, c);
                        }
                    }
                    else {  // oprMode = opr_invertRange
                        for (int c = 0; c < channels[i]; c++) {
                            float voltage = haveInput
                                ? inputs[A_INPUT + i].getVoltage(c)
                                : 0.f;
                            voltage = invertToVoltInvRange(voltage, inversionRange.act);
                            voltage = clipToVoltRange(voltage, outClipRange.act);
                            outputs[A_OUTPUT + i].setVoltage(voltage, c);
                        }
                    }
                }
			}
        }

        cycle256++;
    }
};

struct Sign4IModuleWidget : InfNoiseModuleWidget {
    Sign4IModuleWidget(Sign4IModule *module) {
        initializeWidget(module, "res/Sign4I");

        addParam(createParamCentered<CKSSThree>(Vec(8.508f, 52.383f), module, Sign4IModule::OPRMODE_PARAM));

        const float cntrCol = 15.f;
        const float rowSpacing = 35.258f;
        float row = 87.641f;
        for (int i = 0; i < 4; i++) // Inputs
        {
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, row), module, Sign4IModule::A_INPUT + i));
			row += rowSpacing;
		}
        for (int i = 0; i < 4; i++) // Outputs
        {
            addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, row), module, Sign4IModule::A_OUTPUT + i));
            row += rowSpacing;
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        Sign4IModule* module = dynamic_cast<Sign4IModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> InvRangeNames = getVoltInvRangesNames();
        menu->addChild(createIndexPtrSubmenuItem("Inversion-range", InvRangeNames,
            &module->inversionRange.req));
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelSign4I = createModel<Sign4IModule, Sign4IModuleWidget>("Sign4I");