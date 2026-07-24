// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"

struct SHTH2Module : InfNoiseModule {
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
        A_INPUT,
        B_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        A_OUTPUT,
        B_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        LIGHTS_LEN
    };

    bool haveOutputs = false;
    int channels[2] = { 1, 1 }; // Number of channels for each section
    float noiseScale[2] = { 10.f, 10.f }; // Scale of noise (based on range)
    float noiseOffset[2] = { 5.f, -5.f };  // Offset of noise (based on range)
    dsp::SchmittTrigger holdTrigger[2][PORT_MAX_CHANNELS];  // Per-channel when external hold is poly
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
    float heldValue[2][PORT_MAX_CHANNELS] = { { 0.f } }; // Last output per section/channel

    void setSectionOutput(int sect, int c, float voltage) {
        heldValue[sect][c] = voltage;
        outputs[A_OUTPUT + sect].setVoltage(voltage, c);
    }

    void applyLoadedOutputs() {
        for (int i = 0; i < 2; i++) {
            if (outputs[A_OUTPUT + i].isConnected()) {
                for (int c = 0; c < channels[i]; c++)
                    outputs[A_OUTPUT + i].setVoltage(heldValue[i][c], c);
            }
            for (int c = 0; c < PORT_MAX_CHANNELS; c++)
                holdTrigger[i][c].reset();
        }
    }

    SHTH2Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        for (int i = 0; i < 2; i++) {
            std::string letter = (i == 0) ? "A" : "B";
            configSwitch(A_MODE_PARAM + i, 0.0, 2.0, 0.0, letter+"-Mode", {"S&H: Sample-and-hold", "T&H: Track-and-hold", "H&T: Hold-and-track"});
            configParam<infNoiseLfoFreqQnt>(A_CLOCK_RATE_PARAM + i, -8.f, 10.f, 1.f, letter+"-LFO Frequency", " Hz", 2, 1);
            configInput(A_CLOCK_INPUT + i, letter+"-Clock trigger/gate");
            configInput(A_INPUT + i, letter+"-Signal (white-noise if not connected)");
            configOutput(A_OUTPUT + i, letter+"-Sampled/tracked/hold value");
        }

        configBypass(A_INPUT, A_OUTPUT);
        configBypass(B_INPUT, B_OUTPUT);

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
            for (int c = 0; c < PORT_MAX_CHANNELS; c++)
                holdTrigger[i][c].reset();
            holdMode[i].req = hmt_SmpAndHld;  // Controled by 
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
            for (int c = 0; c < PORT_MAX_CHANNELS; c++)
                heldValue[i][c] = 0.f;
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
        getJsonFloatArray(rootJ, "heldA", heldValue[0], PORT_MAX_CHANNELS, 0.f);
        getJsonFloatArray(rootJ, "heldB", heldValue[1], PORT_MAX_CHANNELS, 0.f);
        getJsonFloatArray(rootJ, "lfoPhase", phase, 2, 0.f);
        getJsonFloatArray(rootJ, "lfoChaosFactor", chaosFactor, 2, 1.f);
        for (int i = 0; i < 2; i++) {
            for (int c = 0; c < PORT_MAX_CHANNELS; c++)
                holdTrigger[i][c].reset();
        }
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
        setJsonFloatArray(rootJ, "heldA", heldValue[0], PORT_MAX_CHANNELS);
        setJsonFloatArray(rootJ, "heldB", heldValue[1], PORT_MAX_CHANNELS);
        setJsonFloatArray(rootJ, "lfoPhase", phase, 2);
        setJsonFloatArray(rootJ, "lfoChaosFactor", chaosFactor, 2);
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        sampleRate = safeSampleRate(args.sampleRate);
        sampleTime = 1.f / sampleRate;

        haveOutputs = false;
        for (int i = 0; i < 2; i++) {
            bool haveInput = inputs[A_INPUT + i].isConnected();
            lfoInUse[i] = !inputs[A_CLOCK_INPUT + i].isConnected();
            if (lfoInUse[i]) {
                float clockFreq = 2.f;  // 2 Hz
                float pitch = params[A_CLOCK_RATE_PARAM + i].getValue();
                freq[i] = clockFreq / 2.f * dsp::exp2_taylor5(pitch);
                phaseStep[i] = freq[i] * sampleTime;
            }
            else if (!haveInput) { // Hold-input is connected, but signal-input is not connected
                int noiseChannels = inputs[A_CLOCK_INPUT + i].getChannels(); 
                noisePolyphony[i].setBoth((polyphonyMode)(noiseChannels - 1));
            }

            lfoInUse[i] = !inputs[A_CLOCK_INPUT + i].isConnected();
            noiseRange[i].updateActual();
            noiseScale[i] = voltRangeNormScale[noiseRange[i].act];
            noiseOffset[i] = voltRangeNormOffset[noiseRange[i].act];
            noisePolyphony[i].updateActual();
            lfoRatioMode[i].updateActual();
            lfoRateChaos[i].updateActual();
            chaosAmount[i] = rateChaosValues[lfoRateChaos[i].act];
            channels[i] = (haveInput)
                ? inputs[A_INPUT + i].getChannels()
                : polyphonyModeChannels[noisePolyphony[i].act];
            if (outputs[A_OUTPUT + i].isConnected()) {
                outputs[A_OUTPUT + i].setChannels(channels[i]);
                haveOutputs = true;
            }

            int hldMd = (int)params[A_MODE_PARAM + i].getValue();
            holdMode[i].setBoth((holdModeType)hldMd, false);
		}

        if (autoProcQuality.act) {
            if (outputs[A_OUTPUT].isConnected() || outputs[B_OUTPUT].isConnected()) {
                if ((inputs[A_CLOCK_INPUT].isConnected() && outputs[A_OUTPUT].isConnected()) ||
                    (inputs[B_CLOCK_INPUT].isConnected() && outputs[B_OUTPUT].isConnected()))
                    procQuality.setBoth(pq_audioRate, false);
                else
                {
                    float highestFreq = 0;
                    if (!inputs[A_CLOCK_INPUT].isConnected() && outputs[A_OUTPUT].isConnected())
                        highestFreq = std::max(highestFreq, freq[0] * rateChaosMaxFactor[lfoRateChaos[0].act]);
                    if (!inputs[B_CLOCK_INPUT].isConnected() && outputs[B_OUTPUT].isConnected())
                        highestFreq = std::max(highestFreq, freq[1] * rateChaosMaxFactor[lfoRateChaos[1].act]);

                    procQuality.setBoth(getEstimatedLfoProcessQuality(sampleRate, highestFreq), false);
                }
            }
            else
                procQuality.setBoth(pq_veryLowRate, false); // No outputs
        }

        float cycleStep = processQualityCycles[procQuality.act];
        phaseStep[0] = (freq[0] * cycleStep) / sampleRate;
        phaseStep[1] = (freq[1] * cycleStep) / sampleRate;

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
            for (int i = 0; i < 2; i++) {
                if (outputs[A_OUTPUT + i].isConnected()) {
                    if (lfoInUse[i]) {
                        bool updateOutput = false;
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

                        if (updateOutput) {
                            for (int c = 0; c < channels[i]; c++) {
                                float voltage = (inputs[A_INPUT + i].isConnected())
                                    ? inputs[A_INPUT + i].getVoltage(c)
                                    : rack::random::get<float>() * noiseScale[i] + noiseOffset[i];
                                voltage = quantizeToMode(voltage, outQuantize.act);
                                voltage = clipToVoltRange(voltage, outClipRange.act);
                                setSectionOutput(i, c, voltage);
                            }
                        }
                    }
                    else {
                        float thrHigh = trueDetectValues[trigDetHigh.act];
                        float thrLow = trueDetectValues[trigDetLow.act];
                        holdModeType hm = holdMode[i].act;
                        for (int c = 0; c < channels[i]; c++) {
                            float holdInput = inputs[A_CLOCK_INPUT + i].getPolyVoltage(c);
                            bool updateThis = false;
                            if (hm == hmt_SmpAndHld) {
                                updateThis = holdTrigger[i][c].process(holdInput, thrLow, thrHigh);
                            }
                            else if (hm == hmt_TrckAndHld) {
                                updateThis = holdInput >= thrHigh;
                            }
                            else {
                                updateThis = holdInput < thrHigh;
                            }
                            if (updateThis) {
                                float voltage = (inputs[A_INPUT + i].isConnected())
                                    ? inputs[A_INPUT + i].getVoltage(c)
                                    : rack::random::get<float>() * noiseScale[i] + noiseOffset[i];
                                voltage = quantizeToMode(voltage, outQuantize.act);
                                voltage = clipToVoltRange(voltage, outClipRange.act);
                                setSectionOutput(i, c, voltage);
                            }
                        }
                    }
                }                 
            }
        }

        cycle256++;
    }
};

struct SHTH2ModuleWidget : InfNoiseModuleWidget {
    SHTH2ModuleWidget(SHTH2Module *module) {
        initializeWidget(module, "res/SHTH2");

        const float centerCol = 15.f;
        for (int i = 0; i < 2; i++) {
            float offset = i * 158.392;

            addParam(createParamCentered<CKSSThree>(Vec(8.632, 49.648f + offset), module, SHTH2Module::A_MODE_PARAM + i));

            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, 79.519f + offset), module, SHTH2Module::A_CLOCK_RATE_PARAM + i));
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 109.616f + offset), module, SHTH2Module::A_CLOCK_INPUT + i));
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 139.652f + offset), module, SHTH2Module::A_INPUT + i));
            addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 174.346f + offset), module, SHTH2Module::A_OUTPUT + i));
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        SHTH2Module* module = dynamic_cast<SHTH2Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> outputRangeNames = getVoltRangesNames(false);
        std::vector<std::string> polyNames = getPolyphonyModeNames(false);
        std::vector<std::string> lfoRatioNames = { "10%/90%", "20%/80%", "30%/70%", "40%/60%", "50%/50% (default)", "60%/70%", "70%/30%", "80%/20%", "90%/10%" };
        std::vector<std::string> rateChaosNames = getRateChaosNames();
        const int vrOffOffset = 1; // vr[0] ("off") excluded from noise-range menu
        for (int i = 0; i < 2; i++) {
            std::string letter = (i == 0) ? "A" : "B";
            menu->addChild(createIndexSubmenuItem(letter+"-Noise range", outputRangeNames,
                [=]() {
                    return (int)(module->noiseRange[i].req - vrOffOffset);
                },
                [=](int range) {
                    module->noiseRange[i].req = (voltRange)(range + vrOffOffset);
                }
            ));
            menu->addChild(createIndexPtrSubmenuItem(letter+"-Noise polyphony", polyNames,
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

Model *modelSHTH2 = createModel<SHTH2Module, SHTH2ModuleWidget>("SHTH2");