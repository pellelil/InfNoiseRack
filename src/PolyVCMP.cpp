// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct PolyVCMPModule : InfNoiseModule {
    enum ParamId {
        RESET_MODE_PARAM,
        OUTPUT_MODE_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        POLY_INPUT,
        RESET_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        MIN_VAL_OUTPUT,
        MAX_VAL_OUTPUT,
        NTZ_OUTPUT,
        FFZ_OUTPUT,
        AVG_OUTPUT,
        RS_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        RANGE_MODE_LIGHT,
        SUM_MODE_LIGHT,
        LIGHTS_LEN
    };

    bool haveOutputs = false;
    dsp::SchmittTrigger resetTrigger = dsp::SchmittTrigger();
    float minVal[PORT_MAX_CHANNELS] = { 0.f };
    float maxVal[PORT_MAX_CHANNELS] = { 0.f };
    float ntzVal[PORT_MAX_CHANNELS] = { 0.f };
    float ffzVal[PORT_MAX_CHANNELS] = { 0.f };
    float sumVal[PORT_MAX_CHANNELS] = { 0.f };
    float count[PORT_MAX_CHANNELS] = { 0.f }; // avg is sum / count
    bool justResetCounters = true; // True when reset is triggered, cleared at the end of the process cycle
    bool polyOutputMode = false;  // True when polyphonic output is enabled, false when monophonic
    bool lastpolyOutputMode = true; // Last polyphonic output mode, used to detect changes
    int inputChannels = 0; // Current number of input channels
    int lastInputChannels = 0; // Last number of input channels, used to detect changes
    enum rsOutputModeType { rs_Range, rs_Sum };
    actReqValue<rsOutputModeType> rsOutputMode = actReqValue<rsOutputModeType>(rsOutputModeType::rs_Range);

    void writeOutputs(int c, int outIdx) {
        outputs[MIN_VAL_OUTPUT].setVoltage(clipToVoltRange(minVal[outIdx], outClipRange.act), c);
        outputs[MAX_VAL_OUTPUT].setVoltage(clipToVoltRange(maxVal[outIdx], outClipRange.act), c);
        outputs[NTZ_OUTPUT].setVoltage(clipToVoltRange(ntzVal[outIdx], outClipRange.act), c);
        outputs[FFZ_OUTPUT].setVoltage(clipToVoltRange(ffzVal[outIdx], outClipRange.act), c);
        outputs[AVG_OUTPUT].setVoltage(clipToVoltRange((count[outIdx] > 0.f)
            ? (sumVal[outIdx] / count[outIdx])
            : 0.f, outClipRange.act), c);
        float rsValue = rsOutputMode.act == rsOutputModeType::rs_Range
            ? (maxVal[outIdx] - minVal[outIdx])
            : sumVal[outIdx];
        outputs[RS_OUTPUT].setVoltage(clipToVoltRange(rsValue, outClipRange.act), c);
    }

    void applyLoadedOutputs() {
        bool haveInput = inputs[POLY_INPUT].isConnected();
        int outputChannels = polyOutputMode && haveInput
            ? inputChannels
            : 1;
        for (int c = 0; c < outputChannels; c++) {
            int outIdx = polyOutputMode ? c : 0;
            writeOutputs(c, outIdx);
        }
    }

	PolyVCMPModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configInput(POLY_INPUT, "Polyphonic (up to 16 channels)");
        configSwitch(RESET_MODE_PARAM, 0.0f, 1.0f, 0.0f, "Reset-mode", { "Each cycle (default)", "Trigger only" });
        configInput(RESET_INPUT, "Reset-trigger");
       
        configSwitch(OUTPUT_MODE_PARAM, 0.0f, 1.0f, 0.0f, "Output-mode", { "Monophonic (default)", "Polyphonic" });

        configOutput(MIN_VAL_OUTPUT, "Minimum (lowest)");
        configOutput(MAX_VAL_OUTPUT, "Maximum (highest)");
        configOutput(NTZ_OUTPUT, "Nearest-to-Zero");
        configOutput(FFZ_OUTPUT, "Furthest-from-Zero");
        configOutput(AVG_OUTPUT, "Average/mix");
        configOutput(RS_OUTPUT, "Range ('max-min')");
        configLight(RANGE_MODE_LIGHT, "Range mode when lit");
        configLight(SUM_MODE_LIGHT, "Sum mode when lit");

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

    // Resets all counters [0-15] when all is true, otherwise just [0], and sets justResetCounters
    void resetCounters(bool all) {
        int last = all ? PORT_MAX_CHANNELS : 1;
        for (int i = 0; i < last; i++) {
            minVal[i] = 0.f;
            maxVal[i] = 0.f;
            ntzVal[i] = 0.f;
            ffzVal[i] = 0.f;
            sumVal[i] = 0.f;
            count[i] = 0.f;
        }
        justResetCounters = true;
    }

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        resetTrigger.reset();
        resetCounters(true);
        rsOutputMode.setBoth(rsOutputModeType::rs_Range);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);

        getJsonFloatArray(rootJ, "minVal", minVal, PORT_MAX_CHANNELS, 0.f);
        getJsonFloatArray(rootJ, "maxVal", maxVal, PORT_MAX_CHANNELS, 0.f);
        getJsonFloatArray(rootJ, "ntzVal", ntzVal, PORT_MAX_CHANNELS, 0.f);
        getJsonFloatArray(rootJ, "ffzVal", ffzVal, PORT_MAX_CHANNELS, 0.f);
        getJsonFloatArray(rootJ, "sumVal", sumVal, PORT_MAX_CHANNELS, 0.f);
        getJsonFloatArray(rootJ, "count", count, PORT_MAX_CHANNELS, 0.f);
        rsOutputMode.setBoth((rsOutputModeType)getJsonInt(rootJ, "rsOutputMode", (int)rsOutputModeType::rs_Range));
        lastInputChannels = getJsonInt(rootJ, "lastInputChannels", 0);
        lastpolyOutputMode = getJsonInt(rootJ, "lastPolyOutputMode", 0) == 1;
        justResetCounters = false;
        resetTrigger.reset();
    }

    void dataToJson(json_t* rootJ) override {
        setJsonFloatArray(rootJ, "minVal", minVal, PORT_MAX_CHANNELS);
        setJsonFloatArray(rootJ, "maxVal", maxVal, PORT_MAX_CHANNELS);
        setJsonFloatArray(rootJ, "ntzVal", ntzVal, PORT_MAX_CHANNELS);
        setJsonFloatArray(rootJ, "ffzVal", ffzVal, PORT_MAX_CHANNELS);
        setJsonFloatArray(rootJ, "sumVal", sumVal, PORT_MAX_CHANNELS);
        setJsonFloatArray(rootJ, "count", count, PORT_MAX_CHANNELS);
        json_object_set_new(rootJ, "rsOutputMode", json_integer((int)rsOutputMode.req));
        json_object_set_new(rootJ, "lastInputChannels", json_integer(lastInputChannels));
        json_object_set_new(rootJ, "lastPolyOutputMode", json_integer(lastpolyOutputMode ? 1 : 0));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        bool haveInput = inputs[POLY_INPUT].isConnected();
        inputChannels = haveInput 
                ? inputs[POLY_INPUT].getChannels() 
                : 0;
        polyOutputMode = params[OUTPUT_MODE_PARAM].getValue() > 0.5f;
        int outputChannels = polyOutputMode && haveInput
            ? inputChannels
            : 1;
        outputs[MIN_VAL_OUTPUT].setChannels(outputChannels);
        outputs[MAX_VAL_OUTPUT].setChannels(outputChannels);
        outputs[NTZ_OUTPUT].setChannels(outputChannels);
        outputs[FFZ_OUTPUT].setChannels(outputChannels);
        outputs[AVG_OUTPUT].setChannels(outputChannels);
        outputs[RS_OUTPUT].setChannels(outputChannels);

        if (rsOutputMode.needsUpdate()) {
            rsOutputMode.updateActual();
            lights[RANGE_MODE_LIGHT].setBrightness(rsOutputMode.act == rsOutputModeType::rs_Range ? 1.f : 0.f);
            lights[SUM_MODE_LIGHT].setBrightness(rsOutputMode.act == rsOutputModeType::rs_Sum ? 1.f : 0.f);

            if (outputInfos.size() > (unsigned)RS_OUTPUT && outputInfos[RS_OUTPUT]) {
                const char* modeName = rsOutputMode.act == rsOutputModeType::rs_Range
                    ? "Range ('max-min')"
                    : "Sum";
                outputInfos[RS_OUTPUT]->name = polyPortPrefix() + modeName;
            }
        }

        // Changed output mode or connected/disconnected input
        if (polyOutputMode != lastpolyOutputMode ||
            inputChannels != lastInputChannels) {
            if (!wasJustLoaded) {
                resetCounters(true);

                for (int c = 0; c < outputChannels; c++) {
                    outputs[MIN_VAL_OUTPUT].setVoltage(0, c);
                    outputs[MAX_VAL_OUTPUT].setVoltage(0, c);
                    outputs[NTZ_OUTPUT].setVoltage(0, c);
                    outputs[FFZ_OUTPUT].setVoltage(0, c);
                    outputs[AVG_OUTPUT].setVoltage(0, c);
                    outputs[RS_OUTPUT].setVoltage(0, c);
                }
            }
            lastpolyOutputMode = polyOutputMode;
            lastInputChannels = inputChannels;
        }

        // Check for outputs
        haveOutputs = outputs[MIN_VAL_OUTPUT].isConnected() || outputs[MAX_VAL_OUTPUT].isConnected() || 
            outputs[NTZ_OUTPUT].isConnected() || outputs[FFZ_OUTPUT].isConnected() || 
            outputs[AVG_OUTPUT].isConnected() || outputs[RS_OUTPUT].isConnected();

        if (wasJustLoaded && haveOutputs)
            applyLoadedOutputs();

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
            // Check for reset
            bool doReset = params[RESET_MODE_PARAM].getValue() < 0.5f;
            if (!doReset && inputs[RESET_INPUT].isConnected())
                doReset = resetTrigger.process(inputs[RESET_INPUT].getVoltage(),
                    trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act]);
            if (doReset)
            {
                resetCounters(polyOutputMode);  // Sets justResetCounters
            }

            if (inputChannels > 0)
			{
                float value = 0.f;
				for (int c = 0; c < inputChannels; c++) {
                    int outIdx = polyOutputMode ? c : 0;
                    if (justResetCounters && (c==0 || polyOutputMode)) {
                        value = inputs[POLY_INPUT].getVoltage(c);
                        minVal[outIdx] = value;
                        maxVal[outIdx] = value;
                        ntzVal[outIdx] = value;
                        ffzVal[outIdx] = value;
                        sumVal[outIdx] = value;
                        count[outIdx] = 1.f;
                    }
                    else {
                        value = inputs[POLY_INPUT].getVoltage(c);
                        minVal[outIdx] = fmin(minVal[outIdx], value);
                        maxVal[outIdx] = fmax(maxVal[outIdx], value);
                        float absValue = fabs(value);
                        if (absValue < fabs(ntzVal[outIdx]))
                            ntzVal[outIdx] = value;
                        if (absValue > fabs(ffzVal[outIdx]))
                            ffzVal[outIdx] = value;
                        sumVal[outIdx] += value;
                        count[outIdx] += 1.f;
                    }

                    // Set poly output values
                    if (polyOutputMode) {
                        writeOutputs(c, outIdx);

                        // Keep running average stable while preventing sum overflow.
                        const float avgLimit = 16777216.f; // max value for 24-bit float (2^24)
                        if (count[outIdx] > avgLimit || fabs(sumVal[outIdx]) > avgLimit) 
                        {
                            sumVal[outIdx] *= 0.5f;
                            count[outIdx] *= 0.5f;
                        }
                    }
				}
                justResetCounters = false;

                // Set mono output values (uses [0] index)
                if (!polyOutputMode) {
                    writeOutputs(0, 0);

                    // Keep running average stable while preventing sum overflow.
                    const float avgLimit = 16777216.f; // max value for 24-bit float (2^24)
                    if (count[0] > avgLimit || fabs(sumVal[0]) > avgLimit) 
                    {
                        sumVal[0] *= 0.5f;
                        count[0] *= 0.5f;
                    }
                }
            }
        }

        cycle256++;
    }
};

struct PolyVCMPModuleWidget : InfNoiseModuleWidget {
    PolyVCMPModuleWidget(PolyVCMPModule *module) {
        initializeWidget(module, "res/PolyVCMP");

        const float cntrCol = 15.f;
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 51.326f), module, PolyVCMPModule::POLY_INPUT));

        addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(4.765f, 74.884f), module, PolyVCMPModule::RESET_MODE_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrCol, 86.897f), module, PolyVCMPModule::RESET_INPUT));

        addParam(createParamCentered<CKSS>(Vec(8.558f, 122.079f), module, PolyVCMPModule::OUTPUT_MODE_PARAM));

        const float rowSpacing = 35.0736f;
        float row = 157.326f;
        for (int i = 0; i < 6; i++) {
			addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, row), module, PolyVCMPModule::MIN_VAL_OUTPUT + i));
			row += rowSpacing;
		}
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(5.638f, 317.247f), module, PolyVCMPModule::RANGE_MODE_LIGHT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(24.329f, 317.247f), module, PolyVCMPModule::SUM_MODE_LIGHT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        PolyVCMPModule* module = dynamic_cast<PolyVCMPModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);
        menu->addChild(createIndexPtrSubmenuItem("Range/Sum output mode",
            { "Range (max-min, default)", "Sum" },
            &module->rsOutputMode.req));
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelPolyVCMP = createModel<PolyVCMPModule, PolyVCMPModuleWidget>("PolyVCMP");