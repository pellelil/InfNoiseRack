// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"

struct SHTH2x4Module : InfNoiseModule {
    enum ParamId {
        A_MODE_PARAM,
        B_MODE_PARAM,
        A_CLOCK_RATE_PARAM,
        B_CLOCK_RATE_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        A_CLOCK_INPUT,
        B_CLOCK_INPUT,
        A1_INPUT,
        A2_INPUT,
        A3_INPUT,
        A4_INPUT,
        B1_INPUT,
        B2_INPUT,
        B3_INPUT,
        B4_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        A1_OUTPUT,
        A2_OUTPUT,
        A3_OUTPUT,
        A4_OUTPUT,
        B1_OUTPUT,
        B2_OUTPUT,
        B3_OUTPUT,
        B4_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        LIGHTS_LEN
    };

    bool haveOutputs[2] = { false, false }; // Output for A/B sections
    int channels[8] = { 1, 1, 1, 1, 1, 1, 1, 1 }; // Number of channels (A1, A2, A3, A4, B1, B2, B3, B4)
    float noiseScale[2] = { 10.f, 10.f }; // Scale of noise (based on range)
    float noiseOffset[2] = { 5.f, -5.f };  // Offset of noise (based on range)
    dsp::SchmittTrigger holdTrigger[2];  // Trigger for each A/B sections
    enum holdModeType { hmt_SmpAndHld, hmt_TrckAndHld, hmt_HldAndTrck };
    actReqValue<holdModeType> holdMode[2] = {
        actReqValue<holdModeType>(hmt_SmpAndHld),
        actReqValue<holdModeType>(hmt_SmpAndHld)
    };
    actReqValue<voltRange> noiseRange[2] = {
        actReqValue<voltRange>((voltRange)vr_Bipolar),
        actReqValue<voltRange>((voltRange)vr_Bipolar)
    };
    actReqValue<polyphonyMode> noisePolyphony[2] = {
        actReqValue<polyphonyMode>(mono_1),
        actReqValue<polyphonyMode>(mono_1)
    };
    enum lfoRatioModeType { lrm_1090, lrm_2080, lrm_3070, lrm_4060, lrm_5050, lrm_6040, lrm_7030, lrm_8020, lrm_9010 };
    actReqValue<lfoRatioModeType> lfoRatioMode[2] = {
        actReqValue<lfoRatioModeType>(lrm_5050),
        actReqValue<lfoRatioModeType>(lrm_5050)
    };
    float lfoHighPhase[9] = { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f };
    float sampleRate = 44100.f;  // Re-obtained in processParams
    float sampleTime = 1.f / 44100.f;  // Re-obtained in processParams
    actReqValue<rateChaos> lfoRateChaos[2] = {
        actReqValue<rateChaos>(rc_default),
        actReqValue<rateChaos>(rc_default)
    };
    float phase[2] = { 0.f, 0.f }; // Phase of each LFO (0-1)
    float freq[2] = { 2.f, 2.f }; // Frequency of each LFO (Hz)
    float phaseStep[2] = { sampleTime,sampleTime }; // Phase-step of each LFO (per process)
    float chaosAmount[2] = { 0.f, 0.f }; // Cached rate-chaos amount (0-1) per section
    float chaosFactor[2] = { 1.f, 1.f }; // Current phase-step factor per section (new each cycle)
    bool lfoInUse[2] = { false, false }; // LFO in use for each section
    float heldValue[8][PORT_MAX_CHANNELS] = { { 0.f } }; // Last output per port/channel

    void setPortOutput(int k, int c, float voltage) {
        heldValue[k][c] = voltage;
        outputs[A1_OUTPUT + k].setVoltage(voltage, c);
    }

    void applyLoadedOutputs() {
        for (int k = 0; k < 8; k++) {
            if (outputs[A1_OUTPUT + k].isConnected()) {
                for (int c = 0; c < channels[k]; c++)
                    outputs[A1_OUTPUT + k].setVoltage(heldValue[k][c], c);
            }
        }
        holdTrigger[0].reset();
        holdTrigger[1].reset();
    }

    SHTH2x4Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        for (int i = 0; i < 2; i++) {
            std::string letter = (i == 0) ? "A" : "B";
            configSwitch(A_MODE_PARAM + i, 0.0, 2.0, 0.0, letter + "-Mode", { "S&H: Sample-and-hold", "T&H: Track-and-hold", "H&T: Hold-and-track" });
            configParam<infNoiseLfoFreqQnt>(A_CLOCK_RATE_PARAM + i, -8.f, 10.f, 1.f, letter + "-LFO Frequency", " Hz", 2, 1);
            configInput(A_CLOCK_INPUT + i, letter + "-Clock trigger/gate");
            for (int j = 0; j < 4; j++) {
                int k = i * 4 + j;               
                configInput(A1_INPUT + k, letter + string::f("%d-Signal (white-noise if not connected)", j + 1));
                configOutput(A1_OUTPUT + k, letter + string::f("%d-Sampled/tracked/hold value", j + 1));
            }
        }

        configBypass(A1_INPUT, A1_OUTPUT);
        configBypass(A2_INPUT, A2_OUTPUT);
        configBypass(A3_INPUT, A3_OUTPUT);
        configBypass(A4_INPUT, A4_OUTPUT);
        configBypass(B1_INPUT, B1_OUTPUT);
        configBypass(B2_INPUT, B2_OUTPUT);
        configBypass(B3_INPUT, B3_OUTPUT);
        configBypass(B4_INPUT, B4_OUTPUT);

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
        haveAutoProcQuality = true;
        haveOutQuantize = true;
        haveOutClipRange = true;
        haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = true;
		haveTrigHighLow = false;
        autoProcQuality.setBoth(true);
    }

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);

        for (int i = 0; i < 2; i++) {
            holdTrigger[i].reset();
            holdMode[i].req = hmt_SmpAndHld;
            noiseRange[i].setBoth((voltRange)vr_Bipolar);
            noisePolyphony[i].setBoth(mono_1);
            lfoRatioMode[i].setBoth(lrm_5050);
            lfoRateChaos[i].setBoth(rc_default);
            lfoInUse[i] = false;
            freq[i] = 2.f;
            phase[i] = 0.f;
            phaseStep[i] = sampleTime;
            chaosAmount[i] = 0.f;
            chaosFactor[i] = 1.f;
        }
        for (int k = 0; k < 8; k++) {
            for (int c = 0; c < PORT_MAX_CHANNELS; c++)
                heldValue[k][c] = 0.f;
        }
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);

        int noiseRangeTmp[2];
        int noisePolyTmp[2];
        int lfoRatioModeTmp[2];
        int lfoRateChaosTmp[2];
        getJsonIntArray(rootJ, "noiseRange", noiseRangeTmp, 2, (int)vr_Bipolar);
        getJsonIntArray(rootJ, "noisePoly", noisePolyTmp, 2, (int)polyphonyMode::mono_1);
        getJsonIntArray(rootJ, "lfoRatioMode", lfoRatioModeTmp, 2, (int)lfoRatioModeType::lrm_5050);
        getJsonIntArray(rootJ, "lfoRateChaos", lfoRateChaosTmp, 2, (int)rc_default);
        for (int i = 0; i < 2; i++) {
            noiseRange[i].setBoth((voltRange)noiseRangeTmp[i]);
            noisePolyphony[i].setBoth((polyphonyMode)noisePolyTmp[i]);
            lfoRatioMode[i].setBoth((lfoRatioModeType)lfoRatioModeTmp[i]);
            lfoRateChaos[i].setBoth((rateChaos)lfoRateChaosTmp[i]);
        }
        getJsonFloatArray(rootJ, "held", &heldValue[0][0], 8 * PORT_MAX_CHANNELS, 0.f);
        getJsonFloatArray(rootJ, "lfoPhase", phase, 2, 0.f);
        getJsonFloatArray(rootJ, "lfoChaosFactor", chaosFactor, 2, 1.f);
        holdTrigger[0].reset();
        holdTrigger[1].reset();
    }

    void dataToJson(json_t* rootJ) override {
        int noiseRangeTmp[2];
        int noisePolyTmp[2];
        int lfoRatioModeTmp[2];
        int lfoRateChaosTmp[2];
        for (int i = 0; i < 2; i++) {
            noiseRangeTmp[i] = (int)noiseRange[i].req;
            noisePolyTmp[i] = (int)noisePolyphony[i].req;
            lfoRatioModeTmp[i] = (int)lfoRatioMode[i].req;
            lfoRateChaosTmp[i] = (int)lfoRateChaos[i].req;
        }
        setJsonIntArray(rootJ, "noiseRange", noiseRangeTmp, 2);
        setJsonIntArray(rootJ, "noisePoly", noisePolyTmp, 2);
        setJsonIntArray(rootJ, "lfoRatioMode", lfoRatioModeTmp, 2);
        setJsonIntArray(rootJ, "lfoRateChaos", lfoRateChaosTmp, 2);
        setJsonFloatArray(rootJ, "held", &heldValue[0][0], 8 * PORT_MAX_CHANNELS);
        setJsonFloatArray(rootJ, "lfoPhase", phase, 2);
        setJsonFloatArray(rootJ, "lfoChaosFactor", chaosFactor, 2);
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        haveOutputs[0] = false; // A-section (A1, A2 and A3)
        haveOutputs[1] = false; // B-section (B1, B2 and B3)
        for (int i = 0; i < 2; i++) {
            noiseRange[i].updateActual();
            noiseScale[i] = voltRangeNormScale[noiseRange[i].act];
            noiseOffset[i] = voltRangeNormOffset[noiseRange[i].act];
            noisePolyphony[i].updateActual();
            lfoRatioMode[i].updateActual();
            lfoRateChaos[i].updateActual();
            chaosAmount[i] = rateChaosValues[lfoRateChaos[i].act];
            for (int j = 0; j < 4; j++) {
                int k = i * 4 + j;
                channels[k] = (inputs[A1_INPUT + k].isConnected())
                    ? inputs[A1_INPUT + k].getChannels()
                    : polyphonyModeChannels[noisePolyphony[i].act];
                if (outputs[A1_OUTPUT + k].isConnected()) {
                    outputs[A1_OUTPUT + k].setChannels(channels[k]);
                    haveOutputs[i] = true;
                }
            }

            if (haveOutputs[i]) {
                lfoInUse[i] = (i == 0)
                    ? !inputs[A_CLOCK_INPUT].isConnected()
                    : !(inputs[A_CLOCK_INPUT].isConnected() || inputs[B_CLOCK_INPUT].isConnected());
                if (lfoInUse[i]) {
                    float clockFreq = 2.f;  // 2 Hz
                    float pitch = params[A_CLOCK_RATE_PARAM + i].getValue();
                    freq[i] = clockFreq / 2.f * dsp::exp2_taylor5(pitch);
                    phaseStep[i] = freq[i] * sampleTime;
                }
            }

            int hldMd = (int)params[A_MODE_PARAM + i].getValue();
            holdMode[i].setBoth((holdModeType)hldMd, false);
        }

        if (autoProcQuality.act) {
            if (outputs[A1_OUTPUT].isConnected() || outputs[B1_OUTPUT].isConnected() ||
            outputs[A2_OUTPUT].isConnected() || outputs[B2_OUTPUT].isConnected() ||
            outputs[A3_OUTPUT].isConnected() || outputs[B3_OUTPUT].isConnected() ||
            outputs[A4_OUTPUT].isConnected() || outputs[B4_OUTPUT].isConnected()) {
                if ((inputs[A_CLOCK_INPUT].isConnected() && (outputs[A1_OUTPUT].isConnected() ||
                    outputs[A2_OUTPUT].isConnected() || outputs[A3_OUTPUT].isConnected() || outputs[A4_OUTPUT].isConnected())) ||
                    (inputs[B_CLOCK_INPUT].isConnected() && (outputs[B1_OUTPUT].isConnected() ||
                    outputs[B2_OUTPUT].isConnected() || outputs[B3_OUTPUT].isConnected() || outputs[B4_OUTPUT].isConnected())))
                    procQuality.setBoth(pq_audioRate, false);
                else
                {
                    float highestFreq = 0;
                    if (!inputs[A_CLOCK_INPUT].isConnected() && (outputs[A1_OUTPUT].isConnected() || outputs[A2_OUTPUT].isConnected() || 
                        outputs[A3_OUTPUT].isConnected() || outputs[A4_OUTPUT].isConnected())) {
                        highestFreq = std::max(highestFreq, freq[0] * rateChaosMaxFactor[lfoRateChaos[0].act]);
                    }
                    if (!inputs[B_CLOCK_INPUT].isConnected() && (outputs[B1_OUTPUT].isConnected() || outputs[B2_OUTPUT].isConnected() || 
                        outputs[B3_OUTPUT].isConnected() || outputs[B4_OUTPUT].isConnected())) {
                        highestFreq = std::max(highestFreq, freq[1] * rateChaosMaxFactor[lfoRateChaos[1].act]);
                    }

                    procQuality.setBoth(getEstimatedLfoProcessQuality(sampleRate, highestFreq), false);
                }
            }
            else
                procQuality.setBoth(pq_veryLowRate, false); // No outputs
        }

        float cycleStep = processQualityCycles[procQuality.act];
        phaseStep[0] = (freq[0] * cycleStep) / sampleRate;
        phaseStep[1] = (freq[1] * cycleStep) / sampleRate;

        if (wasJustLoaded && (haveOutputs[0] || haveOutputs[1]))
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

        if (doProcess) {
            for (int i = 0; i < 2; i++) {
                if (haveOutputs[i]) {
                    bool updateOutput = false;
                    if (lfoInUse[i]) {
                        phase[i] += phaseStep[i] * chaosFactor[i];
                        if (phase[i] >= 1.f) {
                            phase[i] -= std::truncf(phase[i]); // robust if a fast cycle overshoots past 1.0
                            chaosFactor[i] = rateChaosFactor(chaosAmount[i]);
                            if (holdMode[i].act == hmt_SmpAndHld) {
                                updateOutput = true;
                            }
                        }

                        // Simulate a Pulse as gate
                        if (holdMode[i].act == hmt_TrckAndHld)
                            updateOutput = phase[i] < lfoHighPhase[(int)lfoRatioMode[i].act];
                        else if (holdMode[i].act == hmt_HldAndTrck)
                            updateOutput = phase[i] >= lfoHighPhase[(int)lfoRatioMode[i].act];
                    }
                    else {
                        float holdInput = (i == 0)
                            ? inputs[A_CLOCK_INPUT].getVoltage()
                            : (inputs[B_CLOCK_INPUT].isConnected())
                                ? inputs[B_CLOCK_INPUT].getVoltage()
                                : inputs[A_CLOCK_INPUT].getVoltage();
                        updateOutput = (holdMode[i].act == hmt_SmpAndHld)
                            ? holdTrigger[i].process(holdInput,
                                trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act])
                            : (holdMode[i].act == hmt_TrckAndHld)
                                ? holdInput >= trueDetectValues[trigDetHigh.act]
                                : holdInput < trueDetectValues[trigDetHigh.act];
                    }

                    if (updateOutput) {
                        for (int j = 0; j < 4; j++) {
							int k = i * 4 + j;
                            for (int c = 0; c < channels[k]; c++) {
                                float voltage = (inputs[A1_INPUT + k].isConnected())
                                    ? inputs[A1_INPUT + k].getVoltage(c)
                                    : rack::random::get<float>() * noiseScale[i] + noiseOffset[i];
                                voltage = quantizeToMode(voltage, outQuantize.act);
                                voltage = clipToVoltRange(voltage, outClipRange.act);
                                setPortOutput(k, c, voltage);
                            }
                        }
                    }
                }
            }
        }

        cycle256++;
    }
};

struct SHTH2x4ModuleWidget : InfNoiseModuleWidget {
    SHTH2x4ModuleWidget(SHTH2x4Module* module) {
        initializeWidget(module, "res/SHTH2x4");

        const float leftCol = 14.814f;
        const float rightCol = 43.549f;
        for (int i = 0; i < 2; i++) {
            float toggleCol = (i == 0) ? 9.680f : 38.416f;
            float toggleRow = (i == 0) ? 154.327f : 224.574f;
            addParam(createParamCentered<CKSSThree>(Vec(toggleCol, toggleRow), module, SHTH2x4Module::A_MODE_PARAM + i));

            float freqCol = (i == 0) ? rightCol : leftCol;
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(freqCol, 189.515f), module, SHTH2x4Module::A_CLOCK_RATE_PARAM + i));

            float holdCol = (i == 0) ? rightCol : leftCol;
            float holdRow = (i == 0) ? 154.427f : 224.574f;
            addInput(createInputCentered<ThemedPJ301MPort>(Vec(holdCol, holdRow), module, SHTH2x4Module::A_CLOCK_INPUT + i));

            float row = (i == 0) ? 49.659f : 258.797f;
            const float rowSpacing = 24.632f;
            for (int j = 0; j < 4; j++) {
                int k = i * 4 + j;
                addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftCol, row), module, SHTH2x4Module::A1_INPUT + k));
                addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightCol, row), module, SHTH2x4Module::A1_OUTPUT + k));
                row += rowSpacing;
            }
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        SHTH2x4Module* module = dynamic_cast<SHTH2x4Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> outputRangeNames = getVoltRangesNames(false);
        std::vector<std::string> polyNames = getPolyphonyModeNames(false);
        std::vector<std::string> lfoRatioNames = { "10%/90%", "20%/80%", "30%/70%", "40%/60%", "50%/50% (default)", "60%/70%", "70%/30%", "80%/20%", "90%/10%" };
        std::vector<std::string> rateChaosNames = getRateChaosNames();
        const int vrOffOffset = 1; // vr[0] ("off") excluded from noise-range menu
        for (int i = 0; i < 2; i++) {
            std::string letter = (i == 0) ? "A" : "B";
            menu->addChild(createIndexSubmenuItem(letter + "-Noise range", outputRangeNames,
                [=]() {
                    return (int)(module->noiseRange[i].req - vrOffOffset);
                },
                [=](int range) {
                    module->noiseRange[i].req = (voltRange)(range + vrOffOffset);
                }
            ));
            menu->addChild(createIndexPtrSubmenuItem(letter + "-Noise polyphony", polyNames,
                &module->noisePolyphony[i].req));
            menu->addChild(createIndexPtrSubmenuItem(letter+"-LFO high/low ratio", lfoRatioNames,
                &module->lfoRatioMode[i].req));
            menu->addChild(createIndexPtrSubmenuItem(letter+"-LFO rate chaos", rateChaosNames,
                &module->lfoRateChaos[i].req));
            }

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model* modelSHTH2x4 = createModel<SHTH2x4Module, SHTH2x4ModuleWidget>("SHTH2x4");