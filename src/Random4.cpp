// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct Random4Module : InfNoiseModule {
    enum ParamId {
        TRIG_FREQ_PARAM,
        RANGE_PARAM,
        MIN_CNTR_MAX_PARAM,
        DIST_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        TRIG_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        RND1_OUTPUT,
        RND2_OUTPUT,
        RND3_OUTPUT,
        RND4_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        ENUMS(FREQ_LIGHT, 2),
        MIN_LIGHT,
        CNTR_LIGHT,
        MAX_LIGHT,
        FP_LIGHT,
        CNTR_EDGE_LIGHT,
        MIN_MAX_LIGHT,
        DIST_RANGE_LIGHT,
        FIXED_CHANNEL_LIGHT,
        ENUMS(SLEW_TIME_LIGHT, 3),
        LIGHTS_LEN
    };

    enum rndSlewShape {
        rss_linear, rss_sCurve, rss_exp, rss_log,
        rss_len
    };

    enum distRangeType { dr_pct60, dr_pct65, dr_pct70, dr_pct75, dr_pct80, dr_pct85, dr_pct90, dr_pct95, dr_pct100, dr_len };
    actReqValue<distRangeType> distRange = actReqValue<distRangeType>(distRangeType::dr_pct100);
    float distRangeFactor = 1.f;
    float dstRngFactors[9] = { 0.6f, 0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.f };
    enum minCntrMaxType { mcm_Min, mcm_Center, mcm_Max, mcm_len };
    actReqValue<minCntrMaxType> minCntrMax = actReqValue<minCntrMaxType>(minCntrMaxType::mcm_Center);
    std::string minCntrMaxTooltip[3] = { "Minimum (-10V to 10V)", "Center (-10V to 10V)", "Maximum (-10V to 10V)" };
    actReqValue<bool> forcedPolarity = actReqValue<bool>(false);
    enum distModeType { dm_CntrEdge, dm_MinMax, dm_len };
    actReqValue<distModeType> distMode = actReqValue<distModeType>(distModeType::dm_CntrEdge);
    actReqValue<polyphonyMode> polyphony = actReqValue<polyphonyMode>(poly_auto);
    int channels = 1;
    bool haveTrigInput = false;
    bool haveOutputs = false;
    int firstIdx = -1;
    int lastIdx = -1;
    float prevSign[4*16] = {-1.f};
    float minValue = -5.f;
    float maxValue = 5.f;
    float dist = 0.f;
    dsp::SchmittTrigger trigger[PORT_MAX_CHANNELS];
    actReqValue<rateChaos> lfoRateChaos = actReqValue<rateChaos>(rc_default);
    float trigPhaseStep = 0.f;
    float trigPhase = 0.f;
    float clockFreq = 2.f;
    float chaosAmount = 0.f; // Cached rate-chaos amount (0-1)
    float chaosFactor = 1.f; // Current phase-step factor (new each cycle)
    float phaseBrght = 0.f; // Brightness for freq-light based on trigPhase
    float heldValue[4][PORT_MAX_CHANNELS] = { { 0.f } };
    actReqValue<fixedSlewTimes> slewTime = actReqValue<fixedSlewTimes>(fst_default);
    actReqValue<rndSlewShape> slewShape = actReqValue<rndSlewShape>(rss_linear);
    float slewTimeSecs = 0.f;
    float slewTimeSecsCh[PORT_MAX_CHANNELS] = {};
    bool useAdaptiveSlewTime = false;
    bool adaptivePerChannel = false;
    int trigTrackCount = 0;
    infNoiseSlew slew[4][PORT_MAX_CHANNELS];
    infNoiseEventTracker trigEvents[PORT_MAX_CHANNELS];
    infNoisePeriodTracker periodTracker[PORT_MAX_CHANNELS];

    static std::vector<std::string> getSlewShapeNames() {
        return { "Linear (default)", "S-curve", "Exponential", "Logarithmic" };
    }

    void applySlewTimesAll() {
        for (int i = 0; i < 4; i++) {
            for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
                slew[i][c].setTimes(slewTimeSecs, slewTimeSecs);
                slew[i][c].setMode(sm_constantTime);
            }
        }
    }

    void applySlewTimesForChannel(int c, float t) {
        for (int i = 0; i < 4; i++)
            slew[i][c].setTimes(t, t);
    }

    void snapSlewsToHeld() {
        for (int i = 0; i < 4; i++) {
            for (int c = 0; c < PORT_MAX_CHANNELS; c++)
                slew[i][c].snap(heldValue[i][c]);
        }
    }

    void resetSlewTrackers() {
        for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
            trigEvents[c].reset();
            periodTracker[c].reset();
            slewTimeSecsCh[c] = periodTracker[c].get();
        }
    }

    void applySlewShapes() {
        float shape = 0.f;
        infNoiseLinearMode linearMode = lm_linear;
        switch (slewShape.act) {
            case rss_linear:
                shape = 0.f;
                linearMode = lm_linear;
                break;
            case rss_sCurve:
                shape = 0.f;
                linearMode = lm_sCurve;
                break;
            case rss_exp:
                shape = -1.f;
                linearMode = lm_linear;
                break;
            case rss_log:
                shape = 1.f;
                linearMode = lm_linear;
                break;
            default:
                break;
        }
        for (int i = 0; i < 4; i++) {
            for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
                slew[i][c].setShapes(shape, shape);
                slew[i][c].setLinearMode(linearMode);
            }
        }
    }

    void applyLoadedOutputs() {
        if (firstIdx < 0)
            return;
        snapSlewsToHeld();
        for (int i = firstIdx; i <= lastIdx; i++) {
            if (!outputs[RND1_OUTPUT + i].isConnected())
                continue;
            for (int c = 0; c < channels; c++)
                outputs[RND1_OUTPUT + i].setVoltage(heldValue[i][c], c);
        }
    }

   	Random4Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configInput(TRIG_INPUT, "Trigger (normalized to Trigger-LFO)");
        configParam<infNoiseLfoFreqQnt>(TRIG_FREQ_PARAM, -8.f, 10.f, 1.f, "Trigger-LFO frequency", " Hz", 2, 1);
        configLight(FREQ_LIGHT, "LFO phase");
        configLight(FIXED_CHANNEL_LIGHT, "Fixed polyphony if lit");
        configLight(SLEW_TIME_LIGHT, "Slew time: " + getFixedSlewTimesName(fst_default));

        configParam(RANGE_PARAM, 0.f, 10.f, 10.f, "Range (0V to 10V)", " v", 0, 1);
        configParam(MIN_CNTR_MAX_PARAM, -10.f, 10.f, 0.f, "Center (-10V to 10V)", " v", 0, 1);
        configParam(DIST_PARAM, -1.0f, 1.0f, 0.0f, "Distribution", "");

        configOutput(RND1_OUTPUT, "A-Random");
        configOutput(RND2_OUTPUT, "B-Random");
        configOutput(RND3_OUTPUT, "C-Random");
        configOutput(RND4_OUTPUT, "D-Random");

        configLight(MIN_LIGHT, "Minimum when lit");
        configLight(CNTR_LIGHT, "Center when lit");
        configLight(MAX_LIGHT, "Maximum when lit");
        configLight(FP_LIGHT, "Forced polarity when lit (only Center/Edge)");
        configLight(DIST_RANGE_LIGHT, "Distibution-range is 100% when not lit, else 60%");
        configLight(CNTR_EDGE_LIGHT, "Center/Edge distition when lit");
        configLight(MIN_MAX_LIGHT, "Min/Max distition when lit");

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
        ensureNormExpLogLuts();
        for (int i = 0; i < 4; i++) {
            for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
                slew[i][c].setMode(sm_constantTime);
                slewTimeSecsCh[c] = 0.1f;
            }
        }
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);

        trigPhase = 0.f;
        phaseBrght = 0.f;
        clockFreq = 2.f;
        lfoRateChaos.setBoth(rc_default);
        chaosAmount = 0.f;
        chaosFactor = 1.f;

        minCntrMax.setBoth(minCntrMaxType::mcm_Center);
        forcedPolarity.setBoth(false);
        distMode.setBoth(distModeType::dm_CntrEdge);
        polyphony.setBoth(poly_auto);
        distRange.setBoth(distRangeType::dr_pct100);
        distRangeFactor = dstRngFactors[(int)dr_pct100];
        slewTime.setBoth(fst_default);
        slewShape.setBoth(rss_linear);
        useAdaptiveSlewTime = false;
        adaptivePerChannel = false;
        trigTrackCount = 0;
        slewTimeSecs = 0.f;
        resetSlewTrackers();

        for (int c = 0; c < PORT_MAX_CHANNELS; c++)
            trigger[c].reset();
        for (int i = 0; i < 4 * 16; i++)
            prevSign[i] = (randomNorm() < 0.5f) ? -1.f : 1.f;
        for (int i = 0; i < 4; i++) {
            for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
                heldValue[i][c] = 0.f;
                slew[i][c].reset();
                slew[i][c].setMode(sm_constantTime);
            }
        }
        applySlewTimesAll();
        applySlewShapes();
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);

        minCntrMax.setBoth((minCntrMaxType)getJsonInt(rootJ, "minCntrMax", (int)minCntrMaxType::mcm_Center, (int)mcm_len - 1));
        forcedPolarity.setBoth(getJsonBool(rootJ, "forcedPolarity", false));
        distMode.setBoth((distModeType)getJsonInt(rootJ, "distMode", (int)distModeType::dm_CntrEdge, (int)dm_len - 1));
        distRange.setBoth((distRangeType)getJsonInt(rootJ, "distRange", (int)distRangeType::dr_pct100, (int)dr_len - 1));
        polyphony.setBoth((polyphonyMode)getJsonInt(rootJ, "polyphony", (int)polyphonyMode::poly_auto));
        lfoRateChaos.setBoth((rateChaos)getJsonInt(rootJ, "lfoRateChaos", (int)rc_default));
        slewTime.setBoth((fixedSlewTimes)getJsonInt(rootJ, "slewTime", (int)fst_default));
        slewShape.setBoth((rndSlewShape)getJsonInt(rootJ, "slewShape", (int)rss_linear, (int)rss_len - 1));
        getJsonFloatArray(rootJ, "held", &heldValue[0][0], 4 * PORT_MAX_CHANNELS, 0.f);
        getJsonFloatArray(rootJ, "prevSign", prevSign, 4 * PORT_MAX_CHANNELS, -1.f);
        trigPhase = getJsonFloat(rootJ, "trigPhase", 0.f);
        if (trigPhase >= 1.f)
            trigPhase -= std::truncf(trigPhase);
        chaosFactor = getJsonFloat(rootJ, "chaosFactor", 1.f);
        for (int c = 0; c < PORT_MAX_CHANNELS; c++)
            trigger[c].reset();
    }

    void dataToJson(json_t* rootJ) override {

        json_object_set_new(rootJ, "minCntrMax", json_integer((int)minCntrMax.req));
        json_object_set_new(rootJ, "forcedPolarity", json_boolean(forcedPolarity.req));
        json_object_set_new(rootJ, "distMode", json_integer((int)distMode.req));
        json_object_set_new(rootJ, "distRange", json_integer((int)distRange.req));
        json_object_set_new(rootJ, "polyphony", json_integer((int)polyphony.req));
        json_object_set_new(rootJ, "lfoRateChaos", json_integer((int)lfoRateChaos.req));
        json_object_set_new(rootJ, "slewTime", json_integer((int)slewTime.req));
        json_object_set_new(rootJ, "slewShape", json_integer((int)slewShape.req));
        setJsonFloatArray(rootJ, "held", &heldValue[0][0], 4 * PORT_MAX_CHANNELS);
        setJsonFloatArray(rootJ, "prevSign", prevSign, 4 * PORT_MAX_CHANNELS);
        json_object_set_new(rootJ, "trigPhase", json_real(trigPhase));
        json_object_set_new(rootJ, "chaosFactor", json_real(chaosFactor));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        // Handle min/cntr/max mode-change (update lights and param-name)
        if (minCntrMax.needsUpdate()) {
            minCntrMax.updateActual();

            lights[MIN_LIGHT].setBrightness(minCntrMax.act == minCntrMaxType::mcm_Min ? 1.f : 0.f);
            lights[CNTR_LIGHT].setBrightness(minCntrMax.act == minCntrMaxType::mcm_Center ? 1.f : 0.f);
            lights[MAX_LIGHT].setBrightness(minCntrMax.act == minCntrMaxType::mcm_Max ? 1.f : 0.f);

            std::string mode = minCntrMaxTooltip[(int)minCntrMax.act];   
            paramQuantities[MIN_CNTR_MAX_PARAM]->name = minCntrMaxTooltip[(int)minCntrMax.act];
        }

        // Set minValue and maxValue
        if (minCntrMax.act == mcm_Min) {
            minValue = params[MIN_CNTR_MAX_PARAM].getValue();
            maxValue = params[MIN_CNTR_MAX_PARAM].getValue() + params[RANGE_PARAM].getValue();
        } else if (minCntrMax.act == mcm_Center) {
            float halfRange = params[RANGE_PARAM].getValue() / 2.f;
            maxValue = params[MIN_CNTR_MAX_PARAM].getValue() + halfRange;
            minValue = params[MIN_CNTR_MAX_PARAM].getValue() - halfRange;
        } else { // mcm_Max
            minValue = params[MIN_CNTR_MAX_PARAM].getValue() - params[RANGE_PARAM].getValue();
            maxValue = params[MIN_CNTR_MAX_PARAM].getValue();
        }

        // Handle dist-mode and forced-polarity lights
        distMode.updateActual();
        lights[CNTR_EDGE_LIGHT].setBrightness(distMode.act == distModeType::dm_CntrEdge ? 1.f : 0.f);
        lights[MIN_MAX_LIGHT].setBrightness(distMode.act == distModeType::dm_MinMax ? 1.f : 0.f);

        if (distMode.act != distModeType::dm_CntrEdge)
            forcedPolarity.setBoth(false);
        forcedPolarity.updateActual();
        lights[FP_LIGHT].setBrightness(forcedPolarity.act ? 1.f : 0.f);

        // Handle dist-range
        if (distRange.needsUpdate()) {
            distRange.updateActual();
            float brightness = distRange.act == distRangeType::dr_pct100
                ? 0.f
                : 1.f - (0.1f * (int)distRange.act);
            lights[DIST_RANGE_LIGHT].setBrightness(brightness);
            distRangeFactor = dstRngFactors[(int)distRange.act];
        }

        // Check for outputs in use
        bool wasTrig = haveTrigInput;
        haveTrigInput = inputs[TRIG_INPUT].isConnected();
        polyphony.updateActual();
        if (polyphony.act == poly_auto) {
            channels = haveTrigInput
                ? std::max(1, inputs[TRIG_INPUT].getChannels())
                : 1;
            lights[FIXED_CHANNEL_LIGHT].setBrightness(0.f);
        } else {
            channels = polyphonyModeChannels[polyphony.act];
            lights[FIXED_CHANNEL_LIGHT].setBrightness(1.f);
        }
        haveOutputs = false;
        firstIdx = -1;
        lastIdx = -1;
        for (int i = 0; i < 4; i++) {
            if (outputs[RND1_OUTPUT + i].isConnected()) {
                outputs[RND1_OUTPUT + i].setChannels(channels);
                if (!haveOutputs) {
                    haveOutputs = true;
                    firstIdx = i;
                }
                lastIdx = i;
            }
            else {
                outputs[RND1_OUTPUT + i].setChannels(1);
                outputs[RND1_OUTPUT + i].setVoltage(0.f);
            }
        }

        // Update dist
        if (haveOutputs) {
            dist = params[DIST_PARAM].getValue() * distRangeFactor;
        }

        // Handle phase-light
        if (!haveOutputs) {
            lights[FREQ_LIGHT].setBrightness(0.f);
            lights[FREQ_LIGHT + 1].setBrightness(0.f);
        }
        else
        {
            float phaseRedFactor = 0.f; //TODO: set based on "Value-based freq"
            float phaseGreenFactor = (haveTrigInput || phaseRedFactor == 1.f) ? 0.f : 1.f;
            lights[FREQ_LIGHT].setBrightness(phaseBrght * phaseGreenFactor);
            lights[FREQ_LIGHT + 1].setBrightness(phaseBrght * phaseRedFactor);
        }

        // Get clock frequency
        float sampleRate = safeSampleRate(args.sampleRate);
        clockFreq = 2.f;  // 2 Hz
        float pitch = params[TRIG_FREQ_PARAM].getValue();
        clockFreq = clockFreq / 2.f * dsp::exp2_taylor5(pitch);

        // Rate-chaos (cache amount for the per-sample path)
        lfoRateChaos.updateActual();
        chaosAmount = rateChaosValues[lfoRateChaos.act];

        bool slewTimeChanged = slewTime.needsUpdate() || wasJustLoaded;
        if (slewTime.needsUpdate())
            slewTime.updateActual();
        if (slewTimeChanged) {
            useAdaptiveSlewTime = slewTime.act == fst_adaptive;
            if (useAdaptiveSlewTime) {
                resetSlewTrackers();
                slewTimeSecs = periodTracker[0].get();
            }
            else {
                slewTimeSecs = fixedSlewTimesValues[(int)slewTime.act];
            }
            applySlewTimesAll();
            if (slewTime.act == fst_0)
                snapSlewsToHeld();
            setFixedSlewTimesLight(this, SLEW_TIME_LIGHT, slewTime.act);
            if ((int)lightInfos.size() > SLEW_TIME_LIGHT && lightInfos[SLEW_TIME_LIGHT])
                lightInfos[SLEW_TIME_LIGHT]->name = "Slew time: " + getFixedSlewTimesName(slewTime.act);
        }
        useAdaptiveSlewTime = slewTime.act == fst_adaptive;

        if (slewShape.needsUpdate() || wasJustLoaded) {
            if (slewShape.needsUpdate())
                slewShape.updateActual();
            applySlewShapes();
        }

        bool wasAdaptivePerChannel = adaptivePerChannel;
        adaptivePerChannel = useAdaptiveSlewTime && haveTrigInput
            && inputs[TRIG_INPUT].getChannels() > 1;
        trigTrackCount = 0;
        if (useAdaptiveSlewTime && haveTrigInput)
            trigTrackCount = adaptivePerChannel
                ? std::min(std::max(inputs[TRIG_INPUT].getChannels(), 1), channels)
                : 1;
        if (wasTrig != haveTrigInput || wasAdaptivePerChannel != adaptivePerChannel) {
            resetSlewTrackers();
            if (useAdaptiveSlewTime) {
                slewTimeSecs = periodTracker[0].get();
                applySlewTimesAll();
            }
        }

        // Set Auto process-quality
        if (autoProcQuality.act) {
            if (haveOutputs) {
                if (haveTrigInput)
                    procQuality.setBoth(pq_audioRate, false);
                else
                {
                    procQuality.setBoth(getEstimatedLfoProcessQuality(sampleRate, clockFreq * rateChaosMaxFactor[lfoRateChaos.act]), false);
                }
            }
            else
                procQuality.setBoth(pq_veryLowRate, false); // No outputs
        }

        float cycleStep = processQualityCycles[procQuality.act];
        trigPhaseStep = (clockFreq * cycleStep) / sampleRate;

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
            float lfoTrig = 0.f;
            if (!haveTrigInput) {
                lfoTrig = trigPhase < 0.5f ? 10.f : 0.f;
                trigPhase += trigPhaseStep * chaosFactor;
                if (trigPhase >= 1.f) {
                    trigPhase -= std::truncf(trigPhase); // robust if a fast cycle overshoots past 1.0
                    chaosFactor = rateChaosFactor(chaosAmount);
                }
            }

            // Calc brightness for freq-light (updated in processParams)
            phaseBrght = clockFreq >= 60.f
                ? 1.0f
                : trigPhase < 0.75f
                    ? 1.f - trigPhase
                    : 0.f;

            float thrLow = trueDetectValues[trigDetLow.act];
            float thrHigh = trueDetectValues[trigDetHigh.act];
            if (useAdaptiveSlewTime && haveTrigInput) {
                if (adaptivePerChannel) {
                    for (int c = 0; c < trigTrackCount; c++)
                        trigEvents[c].process(procSampleTime);
                }
                else
                    trigEvents[0].process(procSampleTime);
            }
            for (int c = 0; c < channels; c++) {
                float trig = haveTrigInput
                    ? inputs[TRIG_INPUT].getPolyVoltage(c)
                    : lfoTrig;
                if (trigger[c].process(trig, thrLow, thrHigh)) {
                    for (int i = firstIdx; i <= lastIdx; i++) {
                        if (outputs[RND1_OUTPUT + i].isConnected()) {
                            float voltage = randomMinMaxDist(minValue, maxValue, dist,
                                distMode.act == dm_MinMax, forcedPolarity.act, prevSign[i*16 + c]);
                            voltage = quantizeToMode(voltage, outQuantize.act);
                            voltage = clipToVoltRange(voltage, outClipRange.act);
                            heldValue[i][c] = voltage;
                        }
                    }
                    if (useAdaptiveSlewTime) {
                        if (!haveTrigInput) {
                            float stepHz = clockFreq * chaosFactor;
                            if (stepHz > 0.f) {
                                periodTracker[0].snapTo((1.f - trigPhase) / stepHz);
                                slewTimeSecs = periodTracker[0].get();
                                applySlewTimesAll();
                            }
                        }
                        else if (adaptivePerChannel) {
                            if (c < trigTrackCount && trigEvents[c].onEvent()) {
                                periodTracker[c].blendToward(trigEvents[c].period);
                                slewTimeSecsCh[c] = rack::math::clamp(periodTracker[c].get(), 0.00001f, 10.f);
                                applySlewTimesForChannel(c, slewTimeSecsCh[c]);
                            }
                        }
                        else if (c == 0) {
                            if (trigEvents[0].onEvent()) {
                                periodTracker[0].blendToward(trigEvents[0].period);
                                slewTimeSecs = rack::math::clamp(periodTracker[0].get(), 0.00001f, 10.f);
                                applySlewTimesAll();
                            }
                        }
                    }
                }
            }

            bool doSlew = slewTime.act != fst_0;
            for (int i = firstIdx; i <= lastIdx; i++) {
                if (!outputs[RND1_OUTPUT + i].isConnected())
                    continue;
                for (int c = 0; c < channels; c++) {
                    float voltage = heldValue[i][c];
                    if (doSlew)
                        voltage = slew[i][c].next(voltage, procSampleTime);
                    outputs[RND1_OUTPUT + i].setVoltage(clipToVoltRange(voltage, outClipRange.act), c);
                }
            }
        }

        cycle256++;
    }
};

struct Random4ModuleWidget : InfNoiseModuleWidget {
    Random4ModuleWidget(Random4Module *module) {
        initializeWidget(module, "res/Random4");

        const float cntrClm = 15.f;
        const float trigRow = 50.374f;
        const float lgtOfs = 10.021f;
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, trigRow), module, Random4Module::TRIG_INPUT));
        addChild(createLightCentered<TinyLight<RedLight>>(Vec(cntrClm - lgtOfs, trigRow - lgtOfs), module, Random4Module::FIXED_CHANNEL_LIGHT));
        addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(Vec(cntrClm + 10.f, trigRow - 10.f), module, Random4Module::SLEW_TIME_LIGHT));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrClm, 79.838f), module, Random4Module::TRIG_FREQ_PARAM));
        addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(5.770f, 65.836f), module, Random4Module::FREQ_LIGHT));

        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrClm, 111.291f), module, Random4Module::RANGE_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrClm, 149.212f), module, Random4Module::MIN_CNTR_MAX_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrClm, 183.529f), module, Random4Module::DIST_PARAM));

        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 228.017f), module, Random4Module::RND1_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 263.091f), module, Random4Module::RND2_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 298.164f), module, Random4Module::RND3_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 333.238f), module, Random4Module::RND4_OUTPUT));

        const float minCntrMaxLgtRow = 131.051f;
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(6.194f, minCntrMaxLgtRow), module, Random4Module::MIN_LIGHT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(14.900f, minCntrMaxLgtRow), module, Random4Module::CNTR_LIGHT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(24.556f, minCntrMaxLgtRow), module, Random4Module::MAX_LIGHT));

        const float lightClm = 2.950f;
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(lightClm, 169.204f), module, Random4Module::FP_LIGHT));
        addChild(createLightCentered<TinyLight<BlueLight>>(Vec(24.499f, 169.204f), module, Random4Module::DIST_RANGE_LIGHT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(lightClm, 198.204f), module, Random4Module::CNTR_EDGE_LIGHT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(lightClm, 202.977f), module, Random4Module::MIN_MAX_LIGHT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        Random4Module* module = dynamic_cast<Random4Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexPtrSubmenuItem("Min/Center/Max-mode", { "Min", "Center", "Max" },
            &module->minCntrMax.req
        ));
        menu->addChild(createBoolPtrMenuItem("Forced-polarity (center/edge)", "", &module->forcedPolarity.req));
        menu->addChild(createIndexPtrSubmenuItem("Distibution-mode", { "Center/Edge", "Min/Max" },
            &module->distMode.req
        ));
        menu->addChild(createIndexPtrSubmenuItem("Distibution-range", { "60% (value-range not affected)",
            "65%", "70%", "75%", "80%", "85%", "90%", "95%", "100% (default)" },
            &module->distRange.req
        ));

        std::vector<std::string> polyNames = getPolyphonyModeNames(true);
        menu->addChild(createIndexPtrSubmenuItem("Polyphony", polyNames,
            &module->polyphony.req));

        menu->addChild(createIndexPtrSubmenuItem("LFO rate chaos", getRateChaosNames(),
            &module->lfoRateChaos.req));
        menu->addChild(createIndexPtrSubmenuItem("Slew time", getFixedSlewTimesNames(true),
            &module->slewTime.req));
        menu->addChild(createIndexPtrSubmenuItem("Slew shape", Random4Module::getSlewShapeNames(),
            &module->slewShape.req));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelRandom4 = createModel<Random4Module, Random4ModuleWidget>("Random4");