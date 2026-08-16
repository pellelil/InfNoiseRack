// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct FoldModule : InfNoiseModule {
    enum ParamId {
        GAIN_PARAM,
        GAIN_TRIM_PARAM,
        BIAS_PARAM,
        BIAS_TRIM_PARAM,
        BIAS_MODE_PARAM,
        MODE_PARAM,
        RANGE_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        GAIN_INPUT,
        BIAS_INPUT,
        CV_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        CV_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT, 2),
        ENUMS(CLIP_RANGE_LIGHT, 2),
        LIGHTS_LEN
    };

    bool haveOutput = false;
    bool haveInput = false;
    int channels = 1;
    float minRange = -5.f;
    float maxRange = 5.f;
    float rangeCenter = 0.f;
    enum foldModeType { fm_Fold, fm_Wrap };
    foldModeType foldMode = fm_Fold;
    enum oversampModeType { os_single, os_2x };
    actReqValue<oversampModeType> oversampMode = actReqValue<oversampModeType>(os_single);
    dsp::Upsampler<2, 8> upsampler[PORT_MAX_CHANNELS];
    dsp::Decimator<2, 8, float> decimator[PORT_MAX_CHANNELS];
    bool biasAsymGain = false;

    static inline float processSample(float inVoltage,
        float gain,
        float bias,
        float minRange,
        float maxRange,
        float rangeCenter,
        foldModeType foldMode,
        bool biasAsymGain)
    {
        const float rangeSpan = maxRange - minRange;
        const float rangeSpan2 = rangeSpan + rangeSpan;
        constexpr float eps = 1e-6f;

        if (rangeSpan <= eps) {
            return minRange;
        }

        float voltage = inVoltage;
        if (biasAsymGain) {
            float gainFactor = 1.f;
            if (inVoltage >= rangeCenter && bias <= 0.f) {
                gainFactor = 1.f + bias;
            } else if (inVoltage < rangeCenter && bias >= 0.f) {
                gainFactor = 1.f - bias;
            }
            gain *= gainFactor;
        }    
        voltage = rangeCenter + (inVoltage - rangeCenter) * gain + bias * (rangeSpan * 0.5f);

        if (voltage < minRange || voltage > maxRange) {
            if (foldMode == fm_Fold) {
                float folded = std::fmod(voltage - minRange, rangeSpan2);
                if (folded < 0.f)
                    folded += rangeSpan2;
                if (folded > rangeSpan)
                    folded = rangeSpan2 - folded;

                voltage = minRange + folded;
            }
            else { // fm_Wrap
                const float vIn = voltage;
                float wrapped = std::fmod(vIn - minRange, rangeSpan);
                if (wrapped < 0.f)
                    wrapped += rangeSpan;
                voltage = minRange + wrapped;

                if (wrapped <= eps && vIn > maxRange)
                    voltage = maxRange;
            }
        }

        return voltage;
    }
    
	FoldModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
        
        configParam(GAIN_PARAM, 1.f, 10.f, 1.f, "Gain (1x to 10x)", " x", 0, 1);
        configParam(GAIN_TRIM_PARAM, -1.f, 1.f, 0.f, "Gain CV-trim (-100% to +100%)", " %", 0, 100);
        configInput(GAIN_INPUT, "Gain CV");

        configSwitch(BIAS_MODE_PARAM, 0.0, 1.0, 0.0, "Bias-Mode", { "Offset", "Asymmetric Gain" });
        configParam(BIAS_PARAM, -1.f, 1.f, 0.f, "Bias (-1 to +1)", "", 0, 1);
        configParam(BIAS_TRIM_PARAM, -1.f, 1.f, 0.f, "Bias CV-trim (-100% to +100%)", " %", 0, 100);
        configInput(BIAS_INPUT, "Bias CV");

        configSwitch(MODE_PARAM, 0.0, 1.0, 0.0, "Mode", { "Fold", "Wrap" });
        configSwitch(RANGE_PARAM, 0.0, 1.0, 0.0, "Range", { "Bipolar (-5 to +5)", "Unipolar (0 to 10)" });

        configInput(CV_INPUT, "CV");
        configOutput(CV_OUTPUT, "CV");

        configBypass(CV_INPUT, CV_OUTPUT);

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

    void onSampleRateChange() override {
        InfNoiseModule::onSampleRateChange();
        for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
            upsampler[c].reset();
            decimator[c].reset();
        }
    }

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);        
        oversampMode.setBoth(oversampModeType::os_single);
        for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
            upsampler[c].reset();
            decimator[c].reset();
        }
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        oversampMode.setBoth((oversampModeType)getJsonInt(rootJ, "oversampMode", (int)oversampModeType::os_single));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "oversampMode", json_integer((int)oversampMode.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        biasAsymGain = params[BIAS_MODE_PARAM].getValue() > 0.5f;

        if (oversampMode.needsUpdate()) {
            oversampMode.updateActual();
            for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
                upsampler[c].reset();
                decimator[c].reset();
            }
        }
        
        haveOutput = outputs[CV_OUTPUT].isConnected();
        haveInput = inputs[CV_INPUT].isConnected();
        channels = haveInput 
            ? inputs[CV_INPUT].getChannels() 
            : 1;
        outputs[CV_OUTPUT].setChannels(channels);

        foldMode = (params[MODE_PARAM].getValue() < 0.5f) ? fm_Fold : fm_Wrap;
        if (params[RANGE_PARAM].getValue() < 0.5f) {
            minRange = -5.f;
            maxRange = 5.f;
            rangeCenter = 0.f;
        }
        else {
            minRange = 0.f;
            maxRange = 10.f;
            rangeCenter = 5.f;
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

        if (doProcess && haveOutput) {
            // No input, nothing to process
            if (!haveInput) {
                outputs[CV_OUTPUT].setVoltage(0.f);
                return;
            }
            
            // We have input
            float voltage = 0.0f;
            float gain = params[GAIN_PARAM].getValue();
            if (inputs[GAIN_INPUT].isConnected()) {
                gain += (params[GAIN_TRIM_PARAM].getValue() * inputs[GAIN_INPUT].getVoltage() / 10.f) * 9.f;
                gain = clamp(gain, 1.f, 10.f);
            }

            float bias = params[BIAS_PARAM].getValue();
            if (inputs[BIAS_INPUT].isConnected()) {
                bias += params[BIAS_TRIM_PARAM].getValue() * inputs[BIAS_INPUT].getVoltage() / 10.f;
                bias = clamp(bias, -1.f, 1.f);
            }

            if (oversampMode.act == oversampModeType::os_single) {
                for (int c = 0; c < channels; c++) {
                    voltage = inputs[CV_INPUT].getVoltage(c);
                    voltage = processSample(voltage,
                        gain,
                        bias,
                        minRange,
                        maxRange,
                        rangeCenter,
                        foldMode,
                        biasAsymGain);
                    outputs[CV_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), c);
                }   
            }
            else {
                float inBuf[2];
                float outBuf[2];
                for (int c = 0; c < channels; c++) {
                    voltage = inputs[CV_INPUT].getVoltage(c);
                    upsampler[c].process(voltage, inBuf);
                    outBuf[0] = processSample(inBuf[0],
                        gain,
                        bias,
                        minRange,
                        maxRange,
                        rangeCenter,
                        foldMode,
                        biasAsymGain);
                    outBuf[1] = processSample(inBuf[1],
                        gain,
                        bias,
                        minRange,
                        maxRange,
                        rangeCenter,
                        foldMode,
                        biasAsymGain);
                    voltage = decimator[c].process(outBuf);                        
                    outputs[CV_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), c);
                }
            }
        }

        cycle256++;
    }
};

struct FoldModuleWidget : InfNoiseModuleWidget {
    FoldModuleWidget(FoldModule *module) {
        initializeWidget(module, "res/Fold");

        const float cntrCol = 15.f;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrCol, 50.016f), module, FoldModule::GAIN_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(cntrCol, 77.755f), module, FoldModule::GAIN_TRIM_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrCol, 105.844f), module, FoldModule::GAIN_INPUT));

        addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(
            Vec(4.955f, 127.443f),
            module, FoldModule::BIAS_MODE_PARAM));
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrCol, 140.519f), module, FoldModule::BIAS_PARAM));
            addParam(createParamCentered<Trimpot>(Vec(cntrCol, 168.258f), module, FoldModule::BIAS_TRIM_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrCol, 196.347f), module, FoldModule::BIAS_INPUT));
        
        const float switchCol = 8.858f;
        addParam(createParamCentered<CKSS>(Vec(switchCol, 230.156f), module, FoldModule::MODE_PARAM));
        addParam(createParamCentered<CKSS>(Vec(switchCol, 262.384f), module, FoldModule::RANGE_PARAM));

        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 297.267f), module, FoldModule::CV_INPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrCol,332.738), module, FoldModule::CV_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        FoldModule* module = dynamic_cast<FoldModule*>(this->module);
        assert(module);
        
        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexPtrSubmenuItem("Oversampling", { "Single", "2x" },
            &module->oversampMode.req));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelFold = createModel<FoldModule, FoldModuleWidget>("Fold");
