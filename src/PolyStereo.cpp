// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct PolyStereoModule : InfNoiseModule {
    enum ParamId {
        //SOME_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        L1_INPUT,
        L2_INPUT,
        R1_INPUT,
        R2_INPUT,
        P3_INPUT,
        P4_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        P1_OUTPUT,
        P2_OUTPUT,
        L3_OUTPUT,
        R3_OUTPUT,
        L4_OUTPUT,
        R4_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT, 2),
        ENUMS(CLIP_RANGE_LIGHT, 2),
        LIGHTS_LEN
    };

    bool haveOutputs = false;
    bool haveSectionOutput[4] = { false, false, false, false };

	PolyStereoModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
        
        configInput(L1_INPUT, "Left-1");
        configInput(R1_INPUT, "Right-1 (normalized to Left-1)");
        configOutput(P1_OUTPUT, "Poly-1 (channel 1=Left, 2=Right)");

        configInput(L2_INPUT, "Left-2");
        configInput(R2_INPUT, "Right-2 (normalized to Left-2)");
        configOutput(P2_OUTPUT, "Poly-2 (channel 1=Left, 2=Right)");
        
        configInput(P3_INPUT, "Poly-3 (channel 1=Left, 2=Right)");
        configOutput(L3_OUTPUT, "Left-3");
        configOutput(R3_OUTPUT, "Right-3 (normalized to Left-3)");

        configInput(P4_INPUT, "Poly-4 (channel 1=Left, 2=Right)");
        configOutput(L4_OUTPUT, "Left-4");
        configOutput(R4_OUTPUT, "Right-4 (normalized to Left-4)");

        configBypass(L1_INPUT, P1_OUTPUT);
        configBypass(L2_INPUT, P2_OUTPUT);
        configBypass(P3_INPUT, L3_OUTPUT);
        configBypass(P3_INPUT, R3_OUTPUT);
        configBypass(P4_INPUT, L4_OUTPUT);
        configBypass(P4_INPUT, R4_OUTPUT);

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

        haveSectionOutput[0] = outputs[P1_OUTPUT].isConnected();
        haveSectionOutput[1] = outputs[P2_OUTPUT].isConnected();
        haveSectionOutput[2] = outputs[L3_OUTPUT].isConnected() || outputs[R3_OUTPUT].isConnected();
        haveSectionOutput[3] = outputs[L4_OUTPUT].isConnected() || outputs[R4_OUTPUT].isConnected();
        haveOutputs = haveSectionOutput[0] || haveSectionOutput[1] || haveSectionOutput[2] || haveSectionOutput[3];

        outputs[P1_OUTPUT].setChannels(2);
        outputs[P2_OUTPUT].setChannels(2);

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
            if (haveSectionOutput[0]) { // Section-1: Left/Right-1 -> Poly-1
                float voltage = inputs[L1_INPUT].isConnected()
                    ? inputs[L1_INPUT].getVoltage()
                    : 0.f;
                outputs[P1_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), 0);
                if (inputs[R1_INPUT].isConnected()) // Normalized to Left-1
                    voltage = inputs[R1_INPUT].getVoltage();
                outputs[P1_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), 1);
            }

            if (haveSectionOutput[1]) { // Section-2: Left/Right-2 -> Poly-2
                float voltage = inputs[L2_INPUT].isConnected()
                    ? inputs[L2_INPUT].getVoltage()
                    : 0.f;
                outputs[P2_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), 0);
                if (inputs[R2_INPUT].isConnected()) // Normalized to Left-2
                    voltage = inputs[R2_INPUT].getVoltage();
                outputs[P2_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), 1);
            }

            if (haveSectionOutput[2]) { // Section-3: Poly-3 -> Left/Right-3
                if (inputs[P3_INPUT].isConnected()) {
                    outputs[L3_OUTPUT].setVoltage(clipToVoltRange(inputs[P3_INPUT].getPolyVoltage(0), outClipRange.act));
                    outputs[R3_OUTPUT].setVoltage(clipToVoltRange(inputs[P3_INPUT].getPolyVoltage(1), outClipRange.act));
                }
                else {
                    outputs[L3_OUTPUT].setVoltage(0.f);
                    outputs[R3_OUTPUT].setVoltage(0.f);
                }              
            }

            if (haveSectionOutput[3]) { // Section-4: Poly-4 -> Left/Right-4
                if (inputs[P4_INPUT].isConnected()) {
                    outputs[L4_OUTPUT].setVoltage(clipToVoltRange(inputs[P4_INPUT].getPolyVoltage(0), outClipRange.act));
                    outputs[R4_OUTPUT].setVoltage(clipToVoltRange(inputs[P4_INPUT].getPolyVoltage(1), outClipRange.act));
                }
                else {
                    outputs[L4_OUTPUT].setVoltage(0.f);
                    outputs[R4_OUTPUT].setVoltage(0.f);
                }
            }
        }

        cycle256++;
    }
};

struct PolyStereoModuleWidget : InfNoiseModuleWidget {
    PolyStereoModuleWidget(PolyStereoModule *module) {
        initializeWidget(module, "res/PolyStereo");

        const float leftClm = 15.f;
        const float rightClm = 45.f;
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(leftClm, 62.431f), module, PolyStereoModule::L1_INPUT));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(leftClm, 97.504f), module, PolyStereoModule::R1_INPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightClm, 79.967f), module, PolyStereoModule::P1_OUTPUT));

        addInput(createInputCentered<ThemedPJ301MPort>(Vec(leftClm, 132.577f), module, PolyStereoModule::L2_INPUT));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(leftClm, 167.651f), module, PolyStereoModule::R2_INPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightClm, 150.114f), module, PolyStereoModule::P2_OUTPUT));

        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftClm, 245.010f), module, PolyStereoModule::P3_INPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(rightClm, 227.473f), module, PolyStereoModule::L3_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(rightClm, 262.547f), module, PolyStereoModule::R3_OUTPUT));

        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftClm, 315.157f), module, PolyStereoModule::P4_INPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(rightClm, 297.620f), module, PolyStereoModule::L4_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(rightClm, 332.594f), module, PolyStereoModule::R4_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        PolyStereoModule* module = dynamic_cast<PolyStereoModule*>(this->module);
        assert(module);

        //menu->addChild(new MenuSeparator);

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelPolyStereo = createModel<PolyStereoModule, PolyStereoModuleWidget>("PolyStereo");