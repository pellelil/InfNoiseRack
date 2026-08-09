// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct IncDecOffsetModule : InfNoiseModule {
    enum ParamId {
        VALUE_PARAM,
        VALUE_TRIM_PARAM,
        INC_PARAM,
        DEC_PARAM,
        RESET_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        VALUE_INPUT,
        INC_INPUT,
        DEC_INPUT,
        RESET_INPUT,
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

    int channels = 1;
    bool haveOutput = false;
    dsp::TSchmittTrigger<float> incTrigger;
    dsp::TSchmittTrigger<float> decTrigger;
    dsp::TSchmittTrigger<float> resetTrigger;
    float chnlOffset[PORT_MAX_CHANNELS] = { 0.f };
    
	IncDecOffsetModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configParam(VALUE_PARAM, -10.0f, 10.0f, 0.0f, "Value (-10 to 10)", " V");
        configParam(VALUE_TRIM_PARAM, -1.f, 1.f, 0.f, "Value CV-trim", "%", 0, 100);
        configInput(VALUE_INPUT, "Value CV");

        configSwitch(INC_PARAM, 0.0f, 1.0f, 0.0f, "Increment Trigger", {"Low", "High"});
        configInput(INC_INPUT, "Increment Trigger");
        
        configSwitch(DEC_PARAM, 0.0f, 1.0f, 0.0f, "Decrement Trigger", {"Low", "High"});
        configInput(DEC_INPUT, "Decrement Trigger");

        configSwitch(RESET_PARAM, 0.0f, 1.0f, 0.0f, "Reset Trigger", {"Low", "High"});
        configInput(RESET_INPUT, "Reset Trigger");

        configInput(CV_INPUT, "CV");
        configOutput(CV_OUTPUT, "CV");

        configBypass(CV_INPUT, CV_OUTPUT);

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
		haveAutoProcQuality = false;
        haveOutQuantize = true;
        haveOutClipRange = true;  
		haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = true;
		haveTrigHighLow = false;
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);

        incTrigger.reset();
        decTrigger.reset();
        resetTrigger.reset();

        resetChannelOffsets();
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);

        getJsonFloatArray(rootJ, "chnlOffsets", chnlOffset, PORT_MAX_CHANNELS, 0.f);
    }

    void dataToJson(json_t* rootJ) override {
        setJsonFloatArray(rootJ, "chnlOffsets", chnlOffset, PORT_MAX_CHANNELS);
    }

    void resetChannelOffsets() {
        for (int i = 0; i < PORT_MAX_CHANNELS; i++)
            chnlOffset[i] = 0.f;
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        channels = inputs[CV_INPUT].isConnected() 
            ? inputs[CV_INPUT].getChannels() 
            : 1;
        outputs[CV_OUTPUT].setChannels(channels);

        haveOutput = outputs[CV_OUTPUT].isConnected();

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
            // Check for reset
            float resetVolt = params[RESET_PARAM].getValue() > 0.5f
                ? 10.f
                : inputs[RESET_INPUT].isConnected()
                    ? inputs[RESET_INPUT].getVoltage()
                    : 0.f;
            bool doReset = resetTrigger.process(resetVolt,
                trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act]);

            // Check for increment
            float incVolt = params[INC_PARAM].getValue() > 0.5f
                ? 10.f
                : inputs[INC_INPUT].isConnected()
                    ? inputs[INC_INPUT].getVoltage()
                    : 0.f;
            bool doInc = incTrigger.process(incVolt, 
                trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act]);

            // Check for decrement
            float decVolt = params[DEC_PARAM].getValue() > 0.5f
                ? 10.f
                : inputs[DEC_INPUT].isConnected()
                    ? inputs[DEC_INPUT].getVoltage()
                    : 0.f;
            bool doDec = decTrigger.process(decVolt,
                trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act]);
            
            if (doReset) {
                resetChannelOffsets();
                doInc = false;
                doDec = false;
            } else if (doInc && doDec) { // Can't increment and decrement at the same time
                doInc = false;
                doDec = false;
            }

            if (doInc || doDec) {
                for (int c = 0; c < channels; c++) {
                    float value = params[VALUE_PARAM].getValue();
                    if (inputs[VALUE_INPUT].isConnected()) {
                        value += params[VALUE_TRIM_PARAM].getValue() * 
                            inputs[VALUE_INPUT].getPolyVoltage(c);
                    }

                    if (doInc) 
                        chnlOffset[c] += value;
                    else // doDec
                        chnlOffset[c] -= value;

                    // Prevent overflow (by wrapping the offset to a value being a modolo of 10)
                    static constexpr float OFFSET_WRAP_THRESHOLD = 1000.f;
                    static constexpr float OFFSET_WRAP_AMOUNT    = 100.f;
                    if (chnlOffset[c] >  OFFSET_WRAP_THRESHOLD) chnlOffset[c] -= OFFSET_WRAP_AMOUNT;
                    else if (chnlOffset[c] < -OFFSET_WRAP_THRESHOLD) chnlOffset[c] += OFFSET_WRAP_AMOUNT;

                    // Output the CV signal with the offset added
                    float voltage = inputs[CV_INPUT].isConnected() 
                        ? inputs[CV_INPUT].getVoltage(c) 
                        : 0.f;
                    voltage += chnlOffset[c];
                    voltage = quantizeToMode(voltage, outQuantize.act);
                    voltage = clipToVoltRange(voltage, outClipRange.act);
                    outputs[CV_OUTPUT].setVoltage(voltage, c);
                }
            } else { // no need to change offset, hence no need to read value
                for (int c = 0; c < channels; c++) {
                    float voltage = inputs[CV_INPUT].isConnected() 
                        ? inputs[CV_INPUT].getVoltage(c) 
                        : 0.f;
                    voltage += chnlOffset[c];
                    voltage = quantizeToMode(voltage, outQuantize.act);
                    voltage = clipToVoltRange(voltage, outClipRange.act);
                    outputs[CV_OUTPUT].setVoltage(voltage, c);
                }
            }
        }

        cycle256++;
    }
};

struct IncDecOffsetModuleWidget : InfNoiseModuleWidget {
    IncDecOffsetModuleWidget(IncDecOffsetModule *module) {
        initializeWidget(module, "res/IncDecOffset");

        // Value
        const float centerCol = 15.f;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, 50.024f), module, IncDecOffsetModule::VALUE_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(centerCol, 72.897f), module, IncDecOffsetModule::VALUE_TRIM_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 96.005f), module, IncDecOffsetModule::VALUE_INPUT));

        // Increment
        addParam(createParamCentered<infNoiseSmallButton<bc_red, true>>(Vec(centerCol, 129.576f), module, IncDecOffsetModule::INC_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(centerCol, 152.918f), module, IncDecOffsetModule::INC_INPUT));
    

        // Decrement
        addParam(createParamCentered<infNoiseSmallButton<bc_red, true>>(Vec(centerCol, 186.389f), module, IncDecOffsetModule::DEC_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(centerCol, 209.831f), module, IncDecOffsetModule::DEC_INPUT));

        // Reset
        addParam(createParamCentered<infNoiseSmallButton<bc_red, true>>(Vec(centerCol, 242.525f), module, IncDecOffsetModule::RESET_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(centerCol, 266.501f), module, IncDecOffsetModule::RESET_INPUT));

        // CV input/output
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 299.361f), module, IncDecOffsetModule::CV_INPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 332.694f), module, IncDecOffsetModule::CV_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        IncDecOffsetModule* module = dynamic_cast<IncDecOffsetModule*>(this->module);
        assert(module);

        //menu->addChild(new MenuSeparator);
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelIncDecOffset = createModel<IncDecOffsetModule, IncDecOffsetModuleWidget>("IncDecOffset");