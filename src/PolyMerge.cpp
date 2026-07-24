// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct PolySplitModule; // Forward declaration of PolySplit module used as expander

struct PolyMergeModule : InfNoiseModule {
    enum ParamId {
        CHANNEL_COUNT_PARAM,
        INPUT_MODE_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        MONO1_INPUT,
        MONO2_INPUT,
        MONO3_INPUT,
        MONO4_INPUT,
        MONO5_INPUT,
        MONO6_INPUT,
        MONO7_INPUT,
        MONO8_INPUT,
        MONO9_INPUT,
        MONO10_INPUT,
        MONO11_INPUT,
        MONO12_INPUT,
        MONO13_INPUT,
        MONO14_INPUT,
        MONO15_INPUT,
        MONO16_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        POLY_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        POLY1_LIGHT,
        POLY2_LIGHT,
        POLY3_LIGHT,
        POLY4_LIGHT,
        POLY5_LIGHT,
        POLY6_LIGHT,
        POLY7_LIGHT,
        POLY8_LIGHT,
        POLY9_LIGHT,
        POLY10_LIGHT,
        POLY11_LIGHT,
        POLY12_LIGHT,
        POLY13_LIGHT,
        POLY14_LIGHT,
        POLY15_LIGHT,
        POLY16_LIGHT,
        LIGHTS_LEN
    };

    bool monoMode = true;
    int outputChannels = 0; // Number of channels (1..16)
    bool haveOutputs = false;
    // Poly mode: for each output channel c, source is inputs[MONO1_INPUT + mergeInputIndex[c]].getVoltage(mergeChannelIndex[c]); -1 = 0V
    int mergeInputIndex[16] = { 0 };
    int mergeChannelIndex[16] = { 0 };

	PolyMergeModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
        
        configSwitch(CHANNEL_COUNT_PARAM, 1.0f, 16.0f, 1.0f, "Polyphonic channel-count", { "1 channel (monophonic)",
                "2 channels", "3 channels", "4 channels", "5 channels", "6 channels", "7 channels", "8 channels", "9 channels",
                "10 channels", "11 channels", "12 channels", "13 channels", "14 channels", "15 channels", "16 channels" });
        configSwitch(INPUT_MODE_PARAM, 0.0f, 1.0f, 0.0f, "Input mode", { "Monophonic", "Polyphonic" });
        configOutput(POLY_OUTPUT, "Polyphonic");

        for (int i = 0; i < 16; i++) {
			configInput(MONO1_INPUT + i, string::f("Mono/poly-%d", i + 1));
			configLight(POLY1_LIGHT + i, string::f("Mono/poly-%d (lit if channels included)", i + 1));
		}

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

        haveOutputs = false; // Will be set in processParams
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        haveOutputs = false; // Will be set in processParams
    }

    void dataToJson(json_t* rootJ) override {
        //        
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        monoMode = params[INPUT_MODE_PARAM].getValue() < 0.5f;
        outputChannels = clamp((int)params[CHANNEL_COUNT_PARAM].getValue(), 1, 16);
        outputs[POLY_OUTPUT].setChannels(outputChannels);
        haveOutputs = outputs[POLY_OUTPUT].isConnected();

        if (haveOutputs) {
            if (!monoMode) {
                // Poly mode: if any connected input is polyphonic, use packed layout (port 0 ch, port 1 ch, ...);
                // otherwise use position layout (output c = input port c)
                bool anyPoly = false;
                for (int i = 0; i < 16; i++) {
                    if (inputs[MONO1_INPUT + i].isConnected() && inputs[MONO1_INPUT + i].getChannels() > 1) {
                        anyPoly = true;
                        break;
                    }
                }
                if (anyPoly) {
                    int outC = 0;
                    for (int i = 0; i < 16 && outC < outputChannels; i++) {
                        int nCh = inputs[MONO1_INPUT + i].isConnected()
                            ? std::max(inputs[MONO1_INPUT + i].getChannels(), 1)
                            : 0;
                        for (int k = 0; k < nCh && outC < outputChannels; k++) {
                            mergeInputIndex[outC] = i;
                            mergeChannelIndex[outC] = k;
                            outC++;
                        }
                    }
                    for (; outC < outputChannels; outC++) {
                        mergeInputIndex[outC] = -1;
                    }
                } else {
                    for (int c = 0; c < outputChannels; c++) {
                        if (c < 16 && inputs[MONO1_INPUT + c].isConnected()) {
                            mergeInputIndex[c] = c;
                            mergeChannelIndex[c] = 0;
                        } else {
                            mergeInputIndex[c] = -1;
                        }
                    }
                }
            }
        }

        for (int i = 0; i < 16; i++) {
            lights[POLY1_LIGHT + i].setBrightness(i < outputChannels ? 1.0f : 0.0f);
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
            if (monoMode) {
                for (int c = 0; c < outputChannels; c++) {
                    float voltage = (inputs[MONO1_INPUT + c].isConnected())
                        ? inputs[MONO1_INPUT + c].getVoltage()
                        : 0.0f;
                    outputs[POLY_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), c);
                }
            } else { // Poly mode: single loop over output channels using precomputed merge arrays
                for (int c = 0; c < outputChannels; c++) {
                    float voltage = (mergeInputIndex[c] < 0)
                        ? 0.0f
                        : inputs[MONO1_INPUT + mergeInputIndex[c]].getVoltage(mergeChannelIndex[c]);
                    outputs[POLY_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), c);
                }
            }
        }   

        cycle256++;
    }
};

struct PolyMergeModuleWidget : InfNoiseModuleWidget {
    PolyMergeModuleWidget(PolyMergeModule *module) {
        initializeWidget(module, "res/PolyMerge");

        float  row = 51.428f;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(14.810f, row), module, PolyMergeModule::CHANNEL_COUNT_PARAM));
        addParam(createParamCentered<CKSS>(Vec(36.783f, row), module, PolyMergeModule::INPUT_MODE_PARAM));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(74.810f, row), module, PolyMergeModule::POLY_OUTPUT));

        const float portClm1 = 26.094f;
        const float portClm2 = 62.906f;
        const float lightOffset = 10.319f;
        const float rowSpacing = 35.0735f;
        row = 87.179f;
        for (int i = 0; i < 8; i++) {
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(portClm1, row), module, PolyMergeModule::MONO1_INPUT + i));
            addChild(createLightCentered<TinyLight<GreenLight>>(Vec(portClm1 + lightOffset, row - lightOffset), module, PolyMergeModule::POLY1_LIGHT + i));

            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(portClm2, row), module, PolyMergeModule::MONO1_INPUT + i + 8));
            addChild(createLightCentered<TinyLight<GreenLight>>(Vec(portClm2 + lightOffset, row - lightOffset), module, PolyMergeModule::POLY1_LIGHT + i + 8));

            row += rowSpacing;
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        PolyMergeModule* module = dynamic_cast<PolyMergeModule*>(this->module);
        assert(module);

        //menu->addChild(new MenuSeparator);
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelPolyMerge = createModel<PolyMergeModule, PolyMergeModuleWidget>("PolyMerge");