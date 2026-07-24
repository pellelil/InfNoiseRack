// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct PolySplitModule : InfNoiseModule {
    enum ParamId {
        OUTPUT_MODE_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        POLY_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        MONO1_OUTPUT,
        MONO2_OUTPUT,
        MONO3_OUTPUT,
        MONO4_OUTPUT,
        MONO5_OUTPUT,
        MONO6_OUTPUT,
        MONO7_OUTPUT,
        MONO8_OUTPUT,
        MONO9_OUTPUT,
        MONO10_OUTPUT,
        MONO11_OUTPUT,
        MONO12_OUTPUT,
        MONO13_OUTPUT,
        MONO14_OUTPUT,
        MONO15_OUTPUT,
        MONO16_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        MONO1_LIGHT,
        MONO2_LIGHT,
        MONO3_LIGHT,
        MONO4_LIGHT,
        MONO5_LIGHT,
        MONO6_LIGHT,
        MONO7_LIGHT,
        MONO8_LIGHT,
        MONO9_LIGHT,
        MONO10_LIGHT,
        MONO11_LIGHT,
        MONO12_LIGHT,
        MONO13_LIGHT,
        MONO14_LIGHT,
        MONO15_LIGHT,
        MONO16_LIGHT,
        LIGHTS_LEN
    };

    bool monoMode = true;
    bool haveOutputs = false;
    int firstIdx = -1; // Index of first connected output
    int lastIdx = -1; // Index of last connected output
    int channelCount = 1;  // input channel count
    // Poly mode: routing built in processParams, used in process
    int polyLastInputChannel = 0;
    int polyOutOutputIndex[16] = { 0 };
    int polyOutChannelIndex[16] = { 0 };

	PolySplitModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configInput(POLY_INPUT, "Polyphonic");
        configSwitch(OUTPUT_MODE_PARAM, 0.0, 1.0, 0.0, "Output mode", { "Monophonic", "Polyphonic" });

        for (int i = 0; i < 16; i++) {
			configOutput(MONO1_OUTPUT + i, string::f("Mono/poly %d", i + 1));
			configLight(MONO1_LIGHT + i, string::f("Mono/poly %d", i + 1));
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

        monoMode = params[OUTPUT_MODE_PARAM].getValue() < 0.5f;
        firstIdx = -1;  // Index of first connected output
        lastIdx = -1;  // Index of last connected output
        haveOutputs = false;
        int inputChannels = inputs[POLY_INPUT].isConnected()
            ? std::max(inputs[POLY_INPUT].getChannels(), 1)
            : 0;
        channelCount = inputs[POLY_INPUT].isConnected()
            ? inputChannels
            : 1;
        polyLastInputChannel = 0;
        int prevConnected = -1;  // -1 so first connected output at index i gets (i+1) channels
        for (int i = 0; i < 16; i++) {
            bool outputConnected = outputs[MONO1_OUTPUT + i].isConnected();
            if (outputConnected) {
                haveOutputs = true;
                if (firstIdx < 0)
                    firstIdx = i;
                lastIdx = i;
            }

            lights[MONO1_LIGHT + i].setBrightness(i < inputChannels ? 1.0f : 0.0f);

            if (monoMode) {
                outputs[MONO1_OUTPUT + i].setChannels(1);
            } else { // Poly mode: connected outputs get contiguous slices; output at index i gets (i - prevConnected) channels (extra channels 0V if beyond input)
                if (outputConnected) {
                    int nCh = i - prevConnected;
                    if (nCh < 0)
                        nCh = 0;
                    outputs[MONO1_OUTPUT + i].setChannels(nCh);
                    for (int c = 0; c < nCh; c++) {
                        polyOutOutputIndex[polyLastInputChannel + c] = i;
                        polyOutChannelIndex[polyLastInputChannel + c] = c;
                    }
                    polyLastInputChannel += nCh;
                    prevConnected = i;
                } else {
                    outputs[MONO1_OUTPUT + i].setChannels(1);
                }
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

        if (doProcess && haveOutputs) {
            if (monoMode) { // Mono mode
                for (int c = firstIdx; c <= lastIdx; c++) {
                    float voltage = (c < channelCount)
                        ? inputs[POLY_INPUT].getVoltage(c)
                        : 0.0f;
                    outputs[MONO1_OUTPUT + c].setVoltage(clipToVoltRange(voltage, outClipRange.act));
                }
            }
            else { // Poly mode: route logical channels to outputs; channels beyond input get 0V
                for (int c = 0; c < polyLastInputChannel; c++) {
                    float voltage = (c < channelCount)
                        ? inputs[POLY_INPUT].getVoltage(c)
                        : 0.0f;
                    outputs[MONO1_OUTPUT + polyOutOutputIndex[c]].setVoltage(
                        clipToVoltRange(voltage, outClipRange.act), polyOutChannelIndex[c]);
                }
            }
        }

        cycle256++;
    }
};

struct PolySplitModuleWidget : InfNoiseModuleWidget {
    PolySplitModuleWidget(PolySplitModule *module) {
        initializeWidget(module, "res/PolySplit");

        const float clm1 = 14.810f;
        const float clm2 = 43.646f;
        float  row = 51.428f;
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(clm1, row), module, PolySplitModule::POLY_INPUT));
        addParam(createParamCentered<CKSS>(Vec(35.514f, row), module, PolySplitModule::OUTPUT_MODE_PARAM));
        
        const float lightOffset = 10.319f;
        const float rowSpacing = 35.0735f;
        row = 87.179f;
        for (int i = 0; i < 8; i++) {
            addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(clm1, row), module, PolySplitModule::MONO1_OUTPUT + i));
            addChild(createLightCentered<TinyLight<GreenLight>>(Vec(clm1 + lightOffset, row - lightOffset), module, PolySplitModule::MONO1_LIGHT + i));
            
            addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(clm2, row), module, PolySplitModule::MONO1_OUTPUT + i + 8));
            addChild(createLightCentered<TinyLight<GreenLight>>(Vec(clm2 + lightOffset, row - lightOffset), module, PolySplitModule::MONO1_LIGHT + i + 8));

			row += rowSpacing;
		}
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        PolySplitModule* module = dynamic_cast<PolySplitModule*>(this->module);
        assert(module);

        //menu->addChild(new MenuSeparator);
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelPolySplit = createModel<PolySplitModule, PolySplitModuleWidget>("PolySplit");