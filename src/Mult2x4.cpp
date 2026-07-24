// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct Mult2x4Module : InfNoiseModule {
    enum ParamId {
        PARAMS_LEN
    };
    enum InputsId {
        MULTA_INPUT,
        MULTB_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        MULTA1_OUTPUT,
        MULTA2_OUTPUT,
        MULTA3_OUTPUT,
        MULTA4_OUTPUT,
        MULTB1_OUTPUT,
        MULTB2_OUTPUT,
        MULTB3_OUTPUT,
        MULTB4_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT, 2),
        LIGHTS_LEN
    };

    bool haveInputs[2] = { false, false };
    bool haveOutputs[2] = { false, false };
    int channels[2] = { 1, 1 };
    
    Mult2x4Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configInput(MULTA_INPUT, "Mult-A");
        configOutput(MULTA1_OUTPUT, "Mult-A1");
        configOutput(MULTA2_OUTPUT, "Mult-A2");
        configOutput(MULTA3_OUTPUT, "Mult-A3");
        configOutput(MULTA4_OUTPUT, "Mult-A4");
        
        configInput(MULTB_INPUT, "Mult-B");
        configOutput(MULTB1_OUTPUT, "Mult-B1");
        configOutput(MULTB2_OUTPUT, "Mult-B2");
        configOutput(MULTB3_OUTPUT, "Mult-B3");
        configOutput(MULTB4_OUTPUT, "Mult-B4");

        configBypass(MULTA_INPUT, MULTA1_OUTPUT);
        configBypass(MULTA_INPUT, MULTA2_OUTPUT);
        configBypass(MULTA_INPUT, MULTA3_OUTPUT);
        configBypass(MULTA_INPUT, MULTA4_OUTPUT);

        configBypass(MULTB_INPUT, MULTB1_OUTPUT);
        configBypass(MULTB_INPUT, MULTB2_OUTPUT);
        configBypass(MULTB_INPUT, MULTB3_OUTPUT);
        configBypass(MULTB_INPUT, MULTB4_OUTPUT);
        
        // Set InfNoise features (e.g. menu-items)
        haveProcQuality = true;
        haveAutoProcQuality = false;
        haveOutQuantize = true;
        haveOutClipRange = true;
        haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = false;
		haveTrigHighLow = false;
    }

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
    }

    void dataToJson(json_t* rootJ) override {
        //
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        for (int i = 0; i < 2; i++) {
            haveInputs[i] = inputs[MULTA_INPUT + i].isConnected();
            channels[i] = haveInputs[i] 
                ? std::max(inputs[MULTA_INPUT + i].getChannels(), 1) 
                : (i == 1)
                    ? (haveInputs[0] ? channels[0] : 1)
                    : 1;

            int secIdx = i * 4;
            haveOutputs[i] = outputs[MULTA1_OUTPUT + secIdx].isConnected() ||
                outputs[MULTA2_OUTPUT + secIdx].isConnected() ||
                outputs[MULTA3_OUTPUT + secIdx].isConnected() ||
                outputs[MULTA4_OUTPUT + secIdx].isConnected();
            if (!haveOutputs[i]) {
                    outputs[MULTA1_OUTPUT + secIdx].setVoltage(0.f);
                    outputs[MULTA2_OUTPUT + secIdx].setVoltage(0.f);
                    outputs[MULTA3_OUTPUT + secIdx].setVoltage(0.f);
                    outputs[MULTA4_OUTPUT + secIdx].setVoltage(0.f);
            }
    
            outputs[MULTA1_OUTPUT + secIdx].setChannels(channels[i]);
            outputs[MULTA2_OUTPUT + secIdx].setChannels(channels[i]);
            outputs[MULTA3_OUTPUT + secIdx].setChannels(channels[i]);
            outputs[MULTA4_OUTPUT + secIdx].setChannels(channels[i]);
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

        if (doProcess && (haveOutputs[0] || haveOutputs[1])) {
            float multIn[PORT_MAX_CHANNELS] = { 0.f };

            for (int i = 0; i < 2; i++) {
                if (haveOutputs[i] || haveInputs[i]) {
                    for (int c = 0; c < channels[i]; c++) {
                        if (haveInputs[i]) {
                            multIn[c] = inputs[MULTA_INPUT + i].getVoltage(c);
                            multIn[c] = quantizeToMode(multIn[c], outQuantize.act);
                            multIn[c] = clipToVoltRange(multIn[c], outClipRange.act);
                        }

                        if (haveOutputs[i]) {
                            int secIdx = i * 4;
                            outputs[MULTA1_OUTPUT + secIdx].setVoltage(multIn[c], c);
                            outputs[MULTA2_OUTPUT + secIdx].setVoltage(multIn[c], c);
                            outputs[MULTA3_OUTPUT + secIdx].setVoltage(multIn[c], c);
                            outputs[MULTA4_OUTPUT + secIdx].setVoltage(multIn[c], c);
                        }
                    }
                }
            }
        }

        cycle256++;
    }
};

struct Mult2x4ModuleWidget : InfNoiseModuleWidget {
    Mult2x4ModuleWidget(Mult2x4Module *module) {
        initializeWidget(module, "res/Mult2x4");

        const float centerCol = 15.f;
        const float rowSpacing = 29.9425f;
        const float sectionSpacing = 41.025f;

        // Mult-A/B inputs/outputs
        float row = 52.106f;
        for (int i = 0; i < 2; i++) {
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, row), module, Mult2x4Module::MULTA_INPUT + i));
            for (int j = 0; j < 4; j++) {
                row += rowSpacing;
                int outIdx = i * 4 + j;
                addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, row), module, Mult2x4Module::MULTA1_OUTPUT + outIdx));
            }
            row += sectionSpacing;
        }
    }

    void appendContextMenu(Menu* menu) override {
        Mult2x4Module* module = dynamic_cast<Mult2x4Module*>(this->module);
        assert(module);

        //menu->addChild(new MenuSeparator);

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelMult2x4 = createModel<Mult2x4Module, Mult2x4ModuleWidget>("Mult2x4");