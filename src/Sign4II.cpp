// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct Sign4IIModule : InfNoiseModule {
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

    enum oprModeType {opr_Abs, opr_CutMinus, opr_CutPlus};
    oprModeType oprMode = opr_Abs;
    int firstIdx = -1;
    int lastIdx = -1;
    bool haveOutputs = false;
    int channels[4] = {1, 1, 1, 1};
        
	Sign4IIModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configSwitch(OPRMODE_PARAM, 0.0, 2.0, 0.0, "Operation", {"Absolute", "Cut-minus (negative=0)", "Cut-plus (positive=0)"});
        
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
        //
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        //
    }

    void dataToJson(json_t* rootJ) override {
        //
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        oprMode = opr_CutMinus;
        if (params[OPRMODE_PARAM].getValue() < 0.5f)
            oprMode = opr_Abs;
        else if (params[OPRMODE_PARAM].getValue() > 1.5f)
            oprMode = opr_CutPlus;

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
                    if (oprMode == opr_Abs) {
                        for (int c = 0; c < channels[i]; c++) {
                            float voltage = haveInput
                                ? fabs(inputs[A_INPUT + i].getVoltage(c))
                                : 0.f;
                            voltage = clipToVoltRange(voltage, outClipRange.act);
                            outputs[A_OUTPUT + i].setVoltage(voltage, c);
                        }
                    } 
                    else if (oprMode == opr_CutMinus) {
                        for (int c = 0; c < channels[i]; c++) {
                            float voltage = haveInput
                                ? std::max(inputs[A_INPUT + i].getVoltage(c), 0.f)
                                : 0.f;
                            voltage = clipToVoltRange(voltage, outClipRange.act);
                            outputs[A_OUTPUT + i].setVoltage(voltage, c);
                        }
                    }
                    else {  // oprMode = opr_cutPlus
                        for (int c = 0; c < channels[i]; c++) {
                            float voltage = haveInput
                                ? std::min(inputs[A_INPUT + i].getVoltage(c), 0.f)
                                : 0.f;
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

struct Sign4IIModuleWidget : InfNoiseModuleWidget {
    Sign4IIModuleWidget(Sign4IIModule *module) {
        initializeWidget(module, "res/Sign4II");

        addParam(createParamCentered<CKSSThree>(Vec(8.508f, 52.383f), module, Sign4IIModule::OPRMODE_PARAM));

        const float cntrCol = 15.f;
        const float rowSpacing = 35.258f;
        float row = 87.641f;
        for (int i = 0; i < 4; i++) // Inputs
        {
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, row), module, Sign4IIModule::A_INPUT + i));
			row += rowSpacing;
		}
        for (int i = 0; i < 4; i++) // Outputs
        {
            addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, row), module, Sign4IIModule::A_OUTPUT + i));
            row += rowSpacing;
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        Sign4IIModule* module = dynamic_cast<Sign4IIModule*>(this->module);
        assert(module);

        //menu->addChild(new MenuSeparator);
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelSign4II = createModel<Sign4IIModule, Sign4IIModuleWidget>("Sign4II");