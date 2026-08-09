// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct SampleAndUpdateModule : InfNoiseModule {
    enum ParamId {
        SAMPLE_PARAM,
        SAMPLE_MODE_PARAM,
        UPDATE_PARAM,
        UPDATE_MODE_PARAM,
        RESET_PARAM,
        RESET_MODE_PARAM,
        MODE_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        SAMPLE_INPUT,
        UPDATE_INPUT,
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
        COUNT_LIGHT,  // Trigger count achieved light
        LIGHTS_LEN
    };

    enum resetModeType { rm_reset, rm_resetAndUpdate };
    actReqValue<resetModeType> resetMode = actReqValue<resetModeType>(rm_resetAndUpdate);
    enum countResetModeType { cr_resetOnly, cr_updateOnly, cr_resetAndUpdate };
    actReqValue<countResetModeType> countResetMode = actReqValue<countResetModeType>(cr_resetAndUpdate);
    enum triggerCountModeType { tcm_1, tcm_2, tcm_3, tcm_4, tcm_5, tcm_6, tcm_7, tcm_8, 
        tcm_9, tcm_10, tcm_11, tcm_12, tcm_13, tcm_14, tcm_15, tcm_16 };
    actReqValue<triggerCountModeType> triggerCountMode = actReqValue<triggerCountModeType>(tcm_1);
    int minTrigCount[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    actReqValue<voltValue> resetValue = actReqValue<voltValue>(v_zero);
    enum modeType { m_value, m_gate, m_triggerCount };
    actReqValue<modeType> mode = actReqValue<modeType>(m_value);
    bool haveInput = false;
    bool haveOutput = false;
    int channels = 1;
    int trigCount[PORT_MAX_CHANNELS] = { 0 };
    int minTriggerCount = 1;
    dsp::SchmittTrigger sampleTrigger;
    dsp::SchmittTrigger updateTrigger;
    dsp::SchmittTrigger resetTrigger;
    dsp::SchmittTrigger inCountTrigger[PORT_MAX_CHANNELS] = { 
        dsp::SchmittTrigger(), 
        dsp::SchmittTrigger(), 
        dsp::SchmittTrigger(), 
        dsp::SchmittTrigger(), 
        dsp::SchmittTrigger(), 
        dsp::SchmittTrigger(), 
        dsp::SchmittTrigger(), 
        dsp::SchmittTrigger(), 
        dsp::SchmittTrigger(), 
        dsp::SchmittTrigger(), 
        dsp::SchmittTrigger(), 
        dsp::SchmittTrigger(), 
        dsp::SchmittTrigger(), 
        dsp::SchmittTrigger(), 
        dsp::SchmittTrigger(), 
        dsp::SchmittTrigger() 
    };
    infNoiseOutTrigger outCountTrigger[PORT_MAX_CHANNELS] = { 
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f)
    };
    float samples[PORT_MAX_CHANNELS] = { 0.f };
    float countBrightness = 0.f;
    
	SampleAndUpdateModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configSwitch(SAMPLE_PARAM, 0.0f, 1.0f, 0.0f, "Sample");
        configSwitch(SAMPLE_MODE_PARAM, 0.0f, 1.0f, 0.0f, "Sample-mode", {"Single", "Continuous"});
        configInput(SAMPLE_INPUT, "Sample-trigger/gate");

        configSwitch(UPDATE_PARAM, 0.0f, 1.0f, 0.0f, "Update");
        configSwitch(UPDATE_MODE_PARAM, 0.0f, 1.0f, 0.0f, "Update-mode", {"Single", "Continuous"});
        configInput(UPDATE_INPUT, "Update-trigger/gate");

        configSwitch(RESET_PARAM, 0.0f, 1.0f, 0.0f, "Reset");
        configSwitch(RESET_MODE_PARAM, 0.0f, 1.0f, 0.0f, "Reset-mode", {"Single", "Continuous"});
        configInput(RESET_INPUT, "Reset-trigger/gate");

        configLight(COUNT_LIGHT, "Min trigger-count achieved");

        configSwitch(MODE_PARAM, 0.0f, 2.0f, 0.0f, "Mode", {"Value", "Gate", "Trigger count"});      

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
		haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = true;  
		haveGateDetect = true;
		haveGateHighLow = true;
		haveTrigDetect = true;
		haveTrigHighLow = true;
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        
        resetMode.setBoth(rm_resetAndUpdate);
        countResetMode.setBoth(cr_resetAndUpdate);
        triggerCountMode.setBoth(tcm_1);
        resetValue.setBoth(v_zero);

        sampleTrigger.reset();
        updateTrigger.reset();
        resetTrigger.reset();

        for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
            samples[c] = 0.f;
            trigCount[c] = 0;
            inCountTrigger[c].reset();
            outCountTrigger[c].reset();
        }

        countBrightness = 0.f;
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);

        resetMode.setBoth((resetModeType)getJsonInt(rootJ, "resetMode", (int)rm_resetAndUpdate));
        countResetMode.setBoth((countResetModeType)getJsonInt(rootJ, "countResetMode", (int)cr_resetAndUpdate));
        triggerCountMode.setBoth((triggerCountModeType)getJsonInt(rootJ, "triggerCountMode", (int)tcm_1));
        resetValue.setBoth((voltValue)getJsonInt(rootJ, "resetValue", (int)v_zero));
        getJsonFloatArray(rootJ, "samples", samples, PORT_MAX_CHANNELS, 0.f);
        getJsonIntArray(rootJ, "trigCount", trigCount, PORT_MAX_CHANNELS, 0);
        sampleTrigger.reset();
        updateTrigger.reset();
        resetTrigger.reset();
        for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
            inCountTrigger[c].reset();
            outCountTrigger[c].reset();
        }
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "resetMode", json_integer((int)resetMode.req));
        json_object_set_new(rootJ, "countResetMode", json_integer((int)countResetMode.req));
        json_object_set_new(rootJ, "triggerCountMode", json_integer((int)triggerCountMode.req));
        json_object_set_new(rootJ, "resetValue", json_integer((int)resetValue.req));
        setJsonFloatArray(rootJ, "samples", samples, PORT_MAX_CHANNELS);
        setJsonIntArray(rootJ, "trigCount", trigCount, PORT_MAX_CHANNELS);
    }

    void applyLoadedOutputs() {
        if (!haveOutput || mode.act == m_triggerCount)
            return;
        for (int c = 0; c < channels; c++) {
            float voltage = (mode.act == m_gate)
                ? (samples[c] >= trueDetectValues[gateDetHigh.act])
                    ? voltValues[gateOutHigh.act]
                    : voltValues[gateOutLow.act]
                : samples[c];
            outputs[CV_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), c);
        }
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        resetMode.updateActual();
        countResetMode.updateActual();
        triggerCountMode.updateActual();
        minTriggerCount = minTrigCount[(int)triggerCountMode.act];
        resetValue.updateActual();

        if (params[MODE_PARAM].getValue() < 0.5f)
            mode.setBoth(m_value);
        else if (params[MODE_PARAM].getValue() > 1.5f)
            mode.setBoth(m_triggerCount);
        else
            mode.setBoth(m_gate);

        // Update input names according to trigger/gate mode for each section.
        if ((proParCalls256 & 0x3F) == 0x00) { 
            // Every 64th processParams is every 16384 cycle at 48 kHz
            const char* sectionNames[] = { "Sample", "Update", "Reset" };
            const int sectionInputs[] = { SAMPLE_INPUT, UPDATE_INPUT, RESET_INPUT };
            const int sectionModeParams[] = { SAMPLE_MODE_PARAM, UPDATE_MODE_PARAM, RESET_MODE_PARAM };
            for (int i = 0; i < 3; i++) {
                bool triggerMode = params[sectionModeParams[i]].getValue() < 0.5f;
                inputInfos[sectionInputs[i]]->name = monoPortPrefix() + std::string(sectionNames[i]) + (triggerMode ? "-trigger" : "-gate");
            }
        }

        haveInput = inputs[CV_INPUT].isConnected();
        haveOutput = outputs[CV_OUTPUT].isConnected();
        channels = haveInput ? inputs[CV_INPUT].getChannels() : 1;
        outputs[CV_OUTPUT].setChannels(channels);

        // Update count light (100% if any chanel achieved min count)
        if (mode.act == m_triggerCount) {
            // Dimming speed after trigger cleared, approx 0.53s @48kHz, 0.58s @44.1kHz
            countBrightness -= 0.01f; 
            if (countBrightness < 0.01f)
                countBrightness = 0.f;
        } else countBrightness = 0.f;
        lights[COUNT_LIGHT].setBrightness(countBrightness);

        if (wasJustLoaded && haveOutput)
            applyLoadedOutputs();

        //--------------------
        postProcessParams(args);
    }

    inline bool doBtnInput(int btnIdx, int inpIdx, int modeIdx, dsp::SchmittTrigger& trigger) {
        // Obtain voltage from button or input
        float voltage = params[btnIdx].getValue() > 0.5f
            ? 10.f
            : inputs[inpIdx].isConnected()
                ? inputs[inpIdx].getVoltage()
                : 0.f;

        // Detect trigger/gate
        bool triggerMode = params[modeIdx].getValue() < 0.5f;
        return (triggerMode) 
            ? trigger.process(voltage, trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act])
            : voltage >= trueDetectValues[gateDetHigh.act];
    }


    void process(const ProcessArgs& args) override {
        bool doProcessParams = mustProcessParams || 
            ((cycle256 & patternProcessParams) == patternProcessParams);
        if (doProcessParams)
            processParams(args);

        bool doProcess = (doProcessParams ||
            ((cycle256 & processQualityPatterns[procQuality.act]) == processQualityPatterns[procQuality.act]));

        if (doProcess && haveOutput) {
            bool doSample = doBtnInput(SAMPLE_PARAM, SAMPLE_INPUT, SAMPLE_MODE_PARAM, sampleTrigger);
            bool doReset = doBtnInput(RESET_PARAM, RESET_INPUT, RESET_MODE_PARAM, resetTrigger);
            bool doUpdate = doBtnInput(UPDATE_PARAM, UPDATE_INPUT, UPDATE_MODE_PARAM, updateTrigger);

            if (mode.act == m_triggerCount) {
                for (int c = 0; c < channels; c++) {
                    float voltage = (haveInput) ? inputs[CV_INPUT].getVoltage(c) : 0.f;
                    if (doReset) {
                        samples[c] = voltValues[resetValue.act];
                        inCountTrigger[c].reset();
                    } else if (doSample) {
                        samples[c] = voltage;
                        if (inCountTrigger[c].process(samples[c],
                            trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act])) {
                            trigCount[c]++;
                        }
                    }

                    bool procOutTrigger = outCountTrigger[c].process(procSampleTime);
                    if (doReset && countResetMode.act != cr_updateOnly) {
                        trigCount[c] = 0;
                        outCountTrigger[c].reset();
                    } else if (trigCount[c] >= minTriggerCount) {
                        countBrightness = 1.f; // 100% brightness while min trigger-count achieved
                        if (doUpdate && !procOutTrigger) {
                            outCountTrigger[c].trigger();
                            if (countResetMode.act != cr_resetOnly)
                                trigCount[c] = 0;
                        }
                    }
                    
                    // Always output (need to go low when finished whether doUpdate or not)
                    voltage = (outCountTrigger[c].isHigh())
                        ? voltValues[trigOutHigh.act]
                        : voltValues[trigOutLow.act];
                    outputs[CV_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), c);
                }
            } else { // m_value or m_gate
                for (int c = 0; c < channels; c++) {
                    if (doSample) {
                        samples[c] = (haveInput) 
                            ? inputs[CV_INPUT].getVoltage(c) 
                            : 0.f;
                    }

                    if (doReset) {
                        samples[c] = voltValues[resetValue.act];
                    }

                    if (doUpdate || (doReset && resetMode.act == rm_resetAndUpdate)) {
                        float voltage = (mode.act == m_gate)
                            ? (samples[c] >= trueDetectValues[gateDetHigh.act])
                                ? voltValues[gateOutHigh.act]
                                : voltValues[gateOutLow.act]
                            : samples[c];
                        outputs[CV_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), c);
                    }
                }
            }
        }

        cycle256++;
    }
};

struct SampleAndUpdateModuleWidget : InfNoiseModuleWidget {
    infNoiseSmallButton<bc_green, true>* sampleBtn;
    infNoiseSmallButton<bc_green, true>* updateBtn;
    infNoiseSmallButton<bc_green, true>* resetBtn;

    SampleAndUpdateModuleWidget(SampleAndUpdateModule *module) {
        initializeWidget(module, "res/SampleAndUpdate");

        const float cntrCol = 15.f;
        const float modeBtnCol = 23.336f;

        // Sample
        sampleBtn = createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(cntrCol, 50.868f), module, SampleAndUpdateModule::SAMPLE_PARAM);
        addParam(sampleBtn);
        addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_red, bc_green>>(
            Vec(modeBtnCol, 65.542f), module, SampleAndUpdateModule::SAMPLE_MODE_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrCol, 79.779f), module, SampleAndUpdateModule::SAMPLE_INPUT));

        // Update
        updateBtn = createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(cntrCol, 119.482f), module, SampleAndUpdateModule::UPDATE_PARAM);
        addParam(updateBtn);
        addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_red, bc_green>>(
            Vec(modeBtnCol, 134.455f), module, SampleAndUpdateModule::UPDATE_MODE_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrCol, 149.092f), module, SampleAndUpdateModule::UPDATE_INPUT));

        // Reset
        resetBtn = createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(cntrCol, 187.048f), module, SampleAndUpdateModule::RESET_PARAM);
        addParam(resetBtn);
        addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_red, bc_green>>(
            Vec(modeBtnCol, 201.321f), module, SampleAndUpdateModule::RESET_MODE_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrCol, 215.958f), module, SampleAndUpdateModule::RESET_INPUT));

        // Mode (Value, Gate, Trigger count)
        addParam(createParamCentered<CKSSThree>(Vec(10.101f, 257.450f), module, SampleAndUpdateModule::MODE_PARAM));

        // Count light
        const float lightCol = 4.981f;
        addChild(createLightCentered<TinyLight<RedLight>>(Vec(lightCol, 282.358f), module, SampleAndUpdateModule::COUNT_LIGHT));

        // Input/output
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 298.120f), module, SampleAndUpdateModule::CV_INPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 332.826f), module, SampleAndUpdateModule::CV_OUTPUT));

    }

    void step() override {
        InfNoiseModuleWidget::step();

        if (module) {
            sampleBtn->momentary = module->params[SampleAndUpdateModule::SAMPLE_MODE_PARAM].getValue() < 0.5f;
            updateBtn->momentary = module->params[SampleAndUpdateModule::UPDATE_MODE_PARAM].getValue() < 0.5f;
            resetBtn->momentary = module->params[SampleAndUpdateModule::RESET_MODE_PARAM].getValue() < 0.5f;
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        SampleAndUpdateModule* module = dynamic_cast<SampleAndUpdateModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> resetModeNames = { "Only reset", "Reset and update" };
        menu->addChild(createIndexPtrSubmenuItem("Reset-mode", resetModeNames,
            &module->resetMode.req));

        std::vector<std::string> countResetModeNames = { "Only on Reset", "Only on Update (count reached)", "Reset and Update (count reached)" };
        menu->addChild(createIndexPtrSubmenuItem("Reset trigger count", countResetModeNames,
            &module->countResetMode.req));

        std::vector<std::string> triggerCountModeNames = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" };
        menu->addChild(createIndexPtrSubmenuItem("Trigger count", triggerCountModeNames,
            &module->triggerCountMode.req));

        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelSampleAndUpdate = createModel<SampleAndUpdateModule, SampleAndUpdateModuleWidget>("SampleAndUpdate");