// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

static const float userCurveLightR[4] = { 0.f, 1.f, 0.f, 1.f };
static const float userCurveLightG[4] = { 1.f, 0.f, 0.f, 0.45f };
static const float userCurveLightB[4] = { 0.f, 0.f, 1.f, 0.f };

struct RandomCurveModule : InfNoiseModule {
    enum ParamId {
        RATE_PARAM,
        RANGE_PARAM,
        MIN_CNTR_MAX_PARAM,
        DIST_PARAM,
        RATE_TRIM_PARAM,
        RANGE_TRIM_PARAM,
        MIN_CNTR_MAX_TRIM_PARAM,
        DIST_TRIM_PARAM,
        MIN_CNTR_MAX_BTN_PARAM,
        DIST_MODE_PARAM,
        MINMAX_DELAY_PARAM,
        FORCED_POL_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        RATE_INPUT,
        RANGE_INPUT,
        MIN_CNTR_MAX_INPUT,
        DIST_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        TRIG_OUTPUT,
        LINEAR_OUTPUT,
        STEP_OUTPUT,
        CURVE_OUTPUT,
        SPIKY_OUTPUT,
        USER1_OUTPUT,
        USER2_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        ENUMS(FREQ_LIGHT, 2),
        MIN_LIGHT,
        CNTR_LIGHT,
        MAX_LIGHT,
        MIN_MAX_MODE_LIGHT,
        DIST_RANGE_LIGHT,
        ENUMS(USER1_MODE_LIGHT, 3),
        ENUMS(USER2_MODE_LIGHT, 3),
        LIGHTS_LEN
    };
    
    const int PREV_RND = 0;
    const int NEXT_RND = 1;
    
    enum userCurveModeType { ucm_Log, ucm_Exp, ucm_Top, ucm_Bottom };
    actReqValue<userCurveModeType> user1Mode = actReqValue<userCurveModeType>(ucm_Log);
    actReqValue<userCurveModeType> user2Mode = actReqValue<userCurveModeType>(ucm_Top);
    std::string userCurveModeTooltip[4] = {
        "Log",
        "Exp",
        "Top rounded/bottom sharp",
        "Bottom rounded/top sharp"
    };
    bool user1UseLog = true;
    bool user2UseLog = true;

    enum distRangeType { dr_pct60, dr_pct65, dr_pct70, dr_pct75, dr_pct80, dr_pct85, dr_pct90, dr_pct95, dr_pct100 };
    actReqValue<distRangeType> distRange = actReqValue<distRangeType>(distRangeType::dr_pct100);
    float distRangeFactor = 1.f;
    float dstRngFactors[9] = { 0.6f, 0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.f };
    enum minCntrMaxType { mcm_Min, mcm_Center, mcm_Max };
    actReqValue<minCntrMaxType> minCntrMax = actReqValue<minCntrMaxType>(minCntrMaxType::mcm_Center);
    std::string minCntrMaxTooltip[3] = { "Minimum", "Center", "Maximum" };
    dsp::SchmittTrigger minCntrMaxBtnPress;
    bool haveOutputs = false;
    actReqValue<rateChaos> lfoRateChaos = actReqValue<rateChaos>(rc_default);
    float frequency = 1.f;
    float phase = 0.f; // Current phase
    float chaosAmount = 0.f; // Cached rate-chaos amount (0-1)
    float chaosFactor = 1.f; // Current phase-step factor (new each cycle)
    float phaseBrght = 0.f; // Brightness for freq-light based on phase
    bool forcedPolarity = false;
    float prevSign = -1.f; // Previous sign
    float rndValue[2] = {0.f, 0.f}; // Random values [0=Curr] and [1=Next]
    float minValue[2] = {0.f, 0.f}; // Min values [0=Curr] and [1=Next]
    float maxValue[2] = {0.f, 0.f}; // Max values [0=Curr] and [1=Next
    infNoiseOutTrigger cycleTrigger = infNoiseOutTrigger(1e-3f, 1e-3f);

	RandomCurveModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configLight(FREQ_LIGHT, "LFO phase");
        configParam<infNoiseLfoFreqQnt>(RATE_PARAM, -8.f, 10.f, 1.f, "Frequency", " Hz", 2, 1);
        configParam(RATE_TRIM_PARAM, -1.f, 1.f, 0.f, "Frequency CV-trim", "%", 0, 100);
        configInput(RATE_INPUT, "Frequency-CV");
        configOutput(TRIG_OUTPUT, "Trigger (internal LFO phase)");

        configParam(RANGE_PARAM, 0.f, 10.f, 10.f, "Range (0V to 10V)", " v", 0, 1);
        configParam(RANGE_TRIM_PARAM, -1.f, 1.f, 0.f, "Range CV-trim", "%", 0, 100);
        configInput(RANGE_INPUT, "Range-CV");

        configLight(MIN_LIGHT, "Minimum when lit");
        configLight(CNTR_LIGHT, "Center when lit");
        configLight(MAX_LIGHT, "Maximum when lit");
        configSwitch(MIN_CNTR_MAX_BTN_PARAM, 0.0f, 2.0f, 1.0f, "Min/Center/Max-mode", { "Min", "Center", "Max"});
        configParam(MIN_CNTR_MAX_PARAM, -10.f, 10.f, 0.f, "Center (-10V to 10V)", " v", 0, 1);
        configParam(MIN_CNTR_MAX_TRIM_PARAM, -1.f, 1.f, 0.f, "Center CV-trim", "%", 0, 100);
        configInput(MIN_CNTR_MAX_INPUT, "Center-CV");

        configSwitch(MINMAX_DELAY_PARAM, 0.0f, 1.0f, 0.0f, "Min/max change delay", { "Disabled", "Enabled" });
        configSwitch(FORCED_POL_PARAM, 0.0f, 1.0f, 0.0f, "Forced polarity (only Cntr/Edg-mode)", { "Disabled", "Enabled" });
        configSwitch(DIST_MODE_PARAM, 0.0f, 1.0f, 1.0f, "Distribution-mode", { "Min/Max", "Center/Edge" });
        configLight(MIN_MAX_MODE_LIGHT, "Min/Max-mode enabled when lit (else Center/Edge-mode)");

        configParam(DIST_PARAM, -1.0f, 1.0f, 0.0f, "Distribution", "", 0, 1);
        configParam(DIST_TRIM_PARAM, -1.f, 1.f, 0.f, "Distribution CV-trim", "%", 0, 100);
        configInput(DIST_INPUT, "Distribution-CV");
        configLight(DIST_RANGE_LIGHT, "Distibution-range is 100% when not lit, else 60%");

        configOutput(LINEAR_OUTPUT, "Linear");
        configOutput(STEP_OUTPUT, "Step");
        configOutput(CURVE_OUTPUT, "S-curve");
        configOutput(SPIKY_OUTPUT, "Reversed S-curve");
        configOutput(USER1_OUTPUT, "User 1");
        configOutput(USER2_OUTPUT, "User 2");

        configLight(USER1_MODE_LIGHT, "User 1 curve (green=Log, red=Exp, blue=Top, orange=Bottom)");
        configLight(USER2_MODE_LIGHT, "User 2 curve (green=Log, red=Exp, blue=Top, orange=Bottom)");

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
		haveAutoProcQuality = true;
        haveOutQuantize = true;
        haveOutClipRange = true;  
		haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = false;
		haveTrigHighLow = true;
        autoProcQuality.setBoth(true);

        ensureNormExpLogLuts();
	}

    static bool resolveUserCurveUseLog(userCurveModeType mode, float deltaRnd) {
        switch (mode) {
            case ucm_Log:    return true;
            case ucm_Exp:    return false;
            case ucm_Top:    return deltaRnd >= 0.f;
            case ucm_Bottom: return deltaRnd < 0.f;
        }
        return true;
    }

    void applyUserModeLight(int lightId, userCurveModeType mode) {
        int i = (int)mode;
        lights[lightId].setBrightness(userCurveLightR[i]);
        lights[lightId + 1].setBrightness(userCurveLightG[i]);
        lights[lightId + 2].setBrightness(userCurveLightB[i]);
    }

    void updateUserCurveLuts() {
        float deltaRnd = rndValue[NEXT_RND] - rndValue[PREV_RND];
        user1UseLog = resolveUserCurveUseLog(user1Mode.act, deltaRnd);
        user2UseLog = resolveUserCurveUseLog(user2Mode.act, deltaRnd);
    }

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        
        minCntrMax.setBoth(minCntrMaxType::mcm_Center);
        params[MIN_CNTR_MAX_BTN_PARAM].setValue(1.f);
        minCntrMaxBtnPress.reset();
        phase = 1.f;  // Force phase-update on first process
        lfoRateChaos.setBoth(rc_default);
        chaosAmount = 0.f;
        chaosFactor = 1.f;
        phaseBrght = 0.f;
        prevSign = -1.f;
        distRange.setBoth(distRangeType::dr_pct100);
        distRangeFactor = dstRngFactors[(int)dr_pct100];
        rndValue[PREV_RND] = 0.f;
        rndValue[NEXT_RND] = 0.f; // Will be updated on first process, due to phase=1.f
        minValue[PREV_RND] = -5.f;
        minValue[NEXT_RND] = -5.f;
        maxValue[PREV_RND] = -5.f;
        maxValue[NEXT_RND] = -5.f;
        user1Mode.setBoth(ucm_Log);
        user2Mode.setBoth(ucm_Top);
        applyUserModeLight(USER1_MODE_LIGHT, user1Mode.act);
        applyUserModeLight(USER2_MODE_LIGHT, user2Mode.act);
        outputInfos[USER1_OUTPUT]->name = monoPortPrefix() + userCurveModeTooltip[(int)user1Mode.act];
        outputInfos[USER2_OUTPUT]->name = monoPortPrefix() + userCurveModeTooltip[(int)user2Mode.act];
        updateUserCurveLuts();
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        minCntrMax.setBoth((minCntrMaxType)getJsonInt(rootJ, "minCntrMax", (int)minCntrMaxType::mcm_Center));
        distRange.setBoth((distRangeType)getJsonInt(rootJ, "distRange", (int)distRangeType::dr_pct100));
        lfoRateChaos.setBoth((rateChaos)getJsonInt(rootJ, "lfoRateChaos", (int)rc_default));
        phase = getJsonFloat(rootJ, "phase", 0.f);
        if (phase >= 1.f)
            phase -= std::truncf(phase);
        rndValue[PREV_RND] = getJsonFloat(rootJ, "rndPrev", 0.f);
        rndValue[NEXT_RND] = getJsonFloat(rootJ, "rndNext", 0.f);
        minValue[PREV_RND] = getJsonFloat(rootJ, "minPrev", -5.f);
        minValue[NEXT_RND] = getJsonFloat(rootJ, "minNext", -5.f);
        maxValue[PREV_RND] = getJsonFloat(rootJ, "maxPrev", -5.f);
        maxValue[NEXT_RND] = getJsonFloat(rootJ, "maxNext", -5.f);
        prevSign = getJsonFloat(rootJ, "prevSign", -1.f);
        chaosFactor = getJsonFloat(rootJ, "chaosFactor", 1.f);
        user1Mode.setBoth((userCurveModeType)getJsonInt(rootJ, "user1Mode", (int)ucm_Log));
        user2Mode.setBoth((userCurveModeType)getJsonInt(rootJ, "user2Mode", (int)ucm_Top));
        applyUserModeLight(USER1_MODE_LIGHT, user1Mode.act);
        applyUserModeLight(USER2_MODE_LIGHT, user2Mode.act);
        outputInfos[USER1_OUTPUT]->name = monoPortPrefix() + userCurveModeTooltip[(int)user1Mode.act];
        outputInfos[USER2_OUTPUT]->name = monoPortPrefix() + userCurveModeTooltip[(int)user2Mode.act];
        updateUserCurveLuts();

        cycleTrigger.reset();
        minCntrMaxBtnPress.reset();
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "minCntrMax", json_integer((int)minCntrMax.req));
        json_object_set_new(rootJ, "distRange", json_integer((int)distRange.req));
        json_object_set_new(rootJ, "lfoRateChaos", json_integer((int)lfoRateChaos.req));
        json_object_set_new(rootJ, "phase", json_real(phase));
        json_object_set_new(rootJ, "rndPrev", json_real(rndValue[PREV_RND]));
        json_object_set_new(rootJ, "rndNext", json_real(rndValue[NEXT_RND]));
        json_object_set_new(rootJ, "minPrev", json_real(minValue[PREV_RND]));
        json_object_set_new(rootJ, "minNext", json_real(minValue[NEXT_RND]));
        json_object_set_new(rootJ, "maxPrev", json_real(maxValue[PREV_RND]));
        json_object_set_new(rootJ, "maxNext", json_real(maxValue[NEXT_RND]));
        json_object_set_new(rootJ, "prevSign", json_real(prevSign));
        json_object_set_new(rootJ, "chaosFactor", json_real(chaosFactor));
        json_object_set_new(rootJ, "user1Mode", json_integer((int)user1Mode.req));
        json_object_set_new(rootJ, "user2Mode", json_integer((int)user2Mode.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        // Handle min/cntr/max mode-change (update lights and param/input-names)
        // Button idles at 1 (center), so map to 0-based before edge detection.
        if (minCntrMaxBtnPress.process(params[MIN_CNTR_MAX_BTN_PARAM].getValue() - 1.f, 0.5f, 0.1f)) {
            minCntrMax.setBoth(minCntrMax.act == mcm_Max ? mcm_Min : static_cast<minCntrMaxType>(minCntrMax.act + 1 ));
        }

        if (minCntrMax.needsUpdate()) {
            minCntrMax.updateActual();
            lights[MIN_LIGHT].setBrightness(minCntrMax.act == minCntrMaxType::mcm_Min ? 1.f : 0.f);
            lights[CNTR_LIGHT].setBrightness(minCntrMax.act == minCntrMaxType::mcm_Center ? 1.f : 0.f);
            lights[MAX_LIGHT].setBrightness(minCntrMax.act == minCntrMaxType::mcm_Max ? 1.f : 0.f);

            std::string mode = minCntrMaxTooltip[(int)minCntrMax.act];   
            paramQuantities[MIN_CNTR_MAX_PARAM]->name = mode + " (-10V to 10V)";
            paramQuantities[MIN_CNTR_MAX_TRIM_PARAM]->name = mode + " CV-trim";
            inputInfos[MIN_CNTR_MAX_INPUT]->name = monoPortPrefix() + mode + "-CV";
        }

        if (user1Mode.needsUpdate()) {
            user1Mode.updateActual();
            int i = (int)user1Mode.act;
            applyUserModeLight(USER1_MODE_LIGHT, user1Mode.act);
            outputInfos[USER1_OUTPUT]->name = monoPortPrefix() + userCurveModeTooltip[i];
            updateUserCurveLuts();
        }
        if (user2Mode.needsUpdate()) {
            user2Mode.updateActual();
            int i = (int)user2Mode.act;
            applyUserModeLight(USER2_MODE_LIGHT, user2Mode.act);
            outputInfos[USER2_OUTPUT]->name = monoPortPrefix() + userCurveModeTooltip[i];
            updateUserCurveLuts();
        }

        haveOutputs = outputs[LINEAR_OUTPUT].isConnected() ||
            outputs[STEP_OUTPUT].isConnected() ||
            outputs[CURVE_OUTPUT].isConnected() ||
            outputs[SPIKY_OUTPUT].isConnected() ||
            outputs[USER1_OUTPUT].isConnected() ||
            outputs[USER2_OUTPUT].isConnected();

        if (!haveOutputs) {
            lights[FREQ_LIGHT].setBrightness(0.f);
            lights[FREQ_LIGHT + 1].setBrightness(0.f);
        }
        else {
            float phaseRedFactor = 0.f; //TODO: set based on "Value-based freq"
            float phaseGreenFactor = (inputs[RATE_INPUT].isConnected() || phaseRedFactor == 1.f) ? 0.f : 1.f;
            lights[FREQ_LIGHT].setBrightness(phaseBrght * phaseGreenFactor);
            lights[FREQ_LIGHT + 1].setBrightness(phaseBrght * phaseRedFactor);
		}

        forcedPolarity = params[FORCED_POL_PARAM].getValue() > 0.5f;
        lights[MIN_MAX_MODE_LIGHT].setBrightness(params[DIST_MODE_PARAM].getValue() > 0.5f ? 0.f : 1.f);   

        // Handle dist-range
        if (distRange.needsUpdate()) {
            distRange.updateActual();
            distRangeFactor = dstRngFactors[(int)distRange.act];
            float brightness = distRange.act == distRangeType::dr_pct100 
                ? 0.f 
                : 1.f - (0.1f * (int)distRange.act);
            lights[DIST_RANGE_LIGHT].setBrightness(brightness);
        }

        // Rate-chaos (cache amount for the per-sample path)
        lfoRateChaos.updateActual();
        chaosAmount = rateChaosValues[lfoRateChaos.act];

        if (autoProcQuality.act)
        {
            processQuality pq = pq_veryLowRate; // Default for no outputs
            if (haveOutputs) {
                pq = inputs[RATE_INPUT].isConnected() 
                    ? pq_audioRate
                    : getEstimatedLfoProcessQuality(safeSampleRate(args.sampleRate), frequency * rateChaosMaxFactor[lfoRateChaos.act]);
            }
            procQuality.setBoth(pq, false);
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
            // Check for rate-input / trigger-input
            const float clockFreq = 2.f;  // 2 Hz
            float pitch = params[RATE_PARAM].getValue();
            if (inputs[RATE_INPUT].isConnected()) {
                pitch += params[RATE_TRIM_PARAM].getValue() * inputs[RATE_INPUT].getVoltage();
                frequency = clockFreq / 2.f * dsp::exp2_taylor5(pitch);
            } 
            else
                frequency = clockFreq / 2.f * dsp::exp2_taylor5(pitch);

            // Calculate phase-step from frequency and sample-rate
            float phaseStep = (frequency * processQualityCycles[procQuality.act]) / safeSampleRate(args.sampleRate);

            // Update next-min/max when applicable
            if (phase >= 1.f || params[MINMAX_DELAY_PARAM].getValue() < 0.5f) {
                float rangePrmInp = params[RANGE_PARAM].getValue();
                if (inputs[RANGE_INPUT].isConnected()) {
                    rangePrmInp += params[RANGE_TRIM_PARAM].getValue() * inputs[RANGE_INPUT].getVoltage();
                    if (rangePrmInp < 0.f)
                        rangePrmInp = 0.f;
                }

                float minCntrMaxPrmInp = params[MIN_CNTR_MAX_PARAM].getValue();
                if (inputs[MIN_CNTR_MAX_INPUT].isConnected())
                    minCntrMaxPrmInp += params[MIN_CNTR_MAX_TRIM_PARAM].getValue() * inputs[MIN_CNTR_MAX_INPUT].getVoltage();

                // Set minValue and maxValue
                if (minCntrMax.act == mcm_Min) {
                    minValue[NEXT_RND] = minCntrMaxPrmInp;
                    maxValue[NEXT_RND] = minCntrMaxPrmInp + rangePrmInp;
                } else if (minCntrMax.act == mcm_Center) {
                    float halfRange = rangePrmInp / 2.f;
                    maxValue[NEXT_RND] = minCntrMaxPrmInp + halfRange;
                    minValue[NEXT_RND] = minCntrMaxPrmInp - halfRange;
                } else { // mcm_Max
                    minValue[NEXT_RND] = minCntrMaxPrmInp - rangePrmInp;
                    maxValue[NEXT_RND] = minCntrMaxPrmInp;
                }
			}
			
            // Check for phase-end, and update random value
            bool fireTrigger = false;
            if (phase >= 1.f) {
                phase -= std::truncf(phase);
                chaosFactor = rateChaosFactor(chaosAmount);
                fireTrigger = true;
                float dist = params[DIST_PARAM].getValue();
                if (inputs[DIST_INPUT].isConnected()) {
                    dist += params[DIST_TRIM_PARAM].getValue() * inputs[DIST_INPUT].getVoltage() / 5.f;
                    dist = clamp(dist, -1.f, 1.f);
                }
                dist *= distRangeFactor;
                bool distModeMinMax = params[DIST_MODE_PARAM].getValue() < 0.5f;
                
                minValue[PREV_RND] = minValue[NEXT_RND];
                maxValue[PREV_RND] = maxValue[NEXT_RND];
                
                rndValue[PREV_RND] = rndValue[NEXT_RND];
                rndValue[NEXT_RND] = randomMinMaxDist(minValue[NEXT_RND], maxValue[NEXT_RND], 
                    dist, distModeMinMax,  forcedPolarity, prevSign);
                updateUserCurveLuts();
            }

            if (outputs[TRIG_OUTPUT].isConnected()) {
                if (!cycleTrigger.process(procSampleTime) && fireTrigger) {
                    cycleTrigger.trigger();
                }
                outputs[TRIG_OUTPUT].setVoltage(cycleTrigger.isHigh()
                    ? voltValues[trigOutHigh.act]
                    : voltValues[trigOutLow.act]);
            }

            // Calc brightness for freq-light (updated in processParams)
            phaseBrght = frequency >= 60.f
                ? 1.0f
                : phase < 0.75f
                    ? 1.f - phase
                    : 0.f;

            // Output according to shape
            bool doScale = (params[MINMAX_DELAY_PARAM].getValue() < 0.5f) &&
                (minValue[NEXT_RND] != minValue[PREV_RND] || maxValue[NEXT_RND] != maxValue[PREV_RND]);
            float deltaRnd = rndValue[NEXT_RND] - rndValue[PREV_RND];
            float voltage = 0.f;
            
            if (outputs[LINEAR_OUTPUT].isConnected()) {
                voltage = rndValue[PREV_RND] + deltaRnd * phase;

                if (doScale) {
                    // 0-diff or near 0-diff causes spikes when scaling
                    if (fabs(maxValue[PREV_RND] - minValue[PREV_RND]) > 0.00001f && 
                        fabs(maxValue[NEXT_RND] - minValue[NEXT_RND]) > 0.00001f) {
                        voltage = rescale(voltage, minValue[PREV_RND], maxValue[PREV_RND], 
                            minValue[NEXT_RND], maxValue[NEXT_RND]);
                    }
                }
                voltage = quantizeToMode(voltage, outQuantize.act);
                voltage = clipToVoltRange(voltage, outClipRange.act);
                outputs[LINEAR_OUTPUT].setVoltage(voltage);
            }

            if (outputs[STEP_OUTPUT].isConnected()) {
                voltage = phase < 0.5f
					? rndValue[PREV_RND]
					: rndValue[NEXT_RND];

                if (doScale)
                    voltage = rescale(voltage, minValue[PREV_RND], maxValue[PREV_RND], minValue[NEXT_RND], maxValue[NEXT_RND]);
                voltage = quantizeToMode(voltage, outQuantize.act);
                voltage = clipToVoltRange(voltage, outClipRange.act);
                outputs[STEP_OUTPUT].setVoltage(voltage);
            }

            if (outputs[CURVE_OUTPUT].isConnected()) {
                voltage = rndValue[PREV_RND] + sCurve(phase) * deltaRnd;

                if (doScale)
                    voltage = rescale(voltage, minValue[PREV_RND], maxValue[PREV_RND], minValue[NEXT_RND], maxValue[NEXT_RND]);
                voltage = quantizeToMode(voltage, outQuantize.act);
                voltage = clipToVoltRange(voltage, outClipRange.act);
                outputs[CURVE_OUTPUT].setVoltage(voltage);
            }

            if (outputs[SPIKY_OUTPUT].isConnected()) {
                voltage = rndValue[PREV_RND] + sCurveRev(phase) * deltaRnd;

                if (doScale)
                    voltage = rescale(voltage, minValue[PREV_RND], maxValue[PREV_RND], minValue[NEXT_RND], maxValue[NEXT_RND]);
                voltage = quantizeToMode(voltage, outQuantize.act);
                voltage = clipToVoltRange(voltage, outClipRange.act);
                outputs[SPIKY_OUTPUT].setVoltage(voltage);
            }

            if (outputs[USER1_OUTPUT].isConnected()) {
                float shape = user1UseLog ? normLog(phase) : normExp(phase);
                voltage = rndValue[PREV_RND] + shape * deltaRnd;

                if (doScale)
                    voltage = rescale(voltage, minValue[PREV_RND], maxValue[PREV_RND], minValue[NEXT_RND], maxValue[NEXT_RND]);
                voltage = quantizeToMode(voltage, outQuantize.act);
                voltage = clipToVoltRange(voltage, outClipRange.act);
                outputs[USER1_OUTPUT].setVoltage(voltage);
            }

            if (outputs[USER2_OUTPUT].isConnected()) {
                float shape = user2UseLog ? normLog(phase) : normExp(phase);
                voltage = rndValue[PREV_RND] + shape * deltaRnd;

                if (doScale)
                    voltage = rescale(voltage, minValue[PREV_RND], maxValue[PREV_RND], minValue[NEXT_RND], maxValue[NEXT_RND]);
                voltage = quantizeToMode(voltage, outQuantize.act);
                voltage = clipToVoltRange(voltage, outClipRange.act);
                outputs[USER2_OUTPUT].setVoltage(voltage);
            }

            phase += phaseStep * chaosFactor;
        }

        cycle256++;
    }
};

struct RandomCurveModuleWidget : InfNoiseModuleWidget {
    RandomCurveModuleWidget(RandomCurveModule *module) {
        initializeWidget(module, "res/RandomCurve");

        const float lftClm = 15.132f;
        const float rgtClm = 43.868f;
        const float inpOfs = 26.929f;
        const float rowSpacing = 53.1903f;
        float row = 40.843f;
        // 4 Sections of input, knob and trim
        for (int i = 0; i < 4; i++) {
            addParam(createParamCentered<Trimpot>(Vec(lftClm, row), module, RandomCurveModule::RATE_TRIM_PARAM + i));
            float knobOfs = (i == 0) ? 4.58f : 13.045f;
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(rgtClm, row + knobOfs), module, RandomCurveModule::RATE_PARAM + i));
            addInput(createInputCentered<ThemedPJ301MPort>(Vec(lftClm, row + inpOfs), module, RandomCurveModule::RATE_INPUT + i));

            row += rowSpacing;
        }
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(rgtClm, 71.542f), module, RandomCurveModule::TRIG_OUTPUT));

        // Misc other (freq-light, delay-toggle, Convex/Concave-mode)
        addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(31.373f, 30.733f), module, RandomCurveModule::FREQ_LIGHT));

        addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(Vec(42.933f, 131.307f), module, RandomCurveModule::MINMAX_DELAY_PARAM));

        const float minCntrMaxLgtRow = 140.709f;
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(31.190f, minCntrMaxLgtRow), module, RandomCurveModule::MIN_LIGHT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(41.847f, minCntrMaxLgtRow), module, RandomCurveModule::CNTR_LIGHT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(53.790f, minCntrMaxLgtRow), module, RandomCurveModule::MAX_LIGHT));

        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, true>>(Vec(26.273f, 161.918f), module, RandomCurveModule::MIN_CNTR_MAX_BTN_PARAM));

        addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(Vec(42.933f, 185.508f), module, RandomCurveModule::FORCED_POL_PARAM));

        addChild(createLightCentered<TinyLight<BlueLight>>(Vec(53.790f, 197.092f), module, RandomCurveModule::DIST_RANGE_LIGHT));

        addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(Vec(31.822f, 228.511f), module, RandomCurveModule::DIST_MODE_PARAM));

        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(31.919f, 233.830f), module, RandomCurveModule::MIN_MAX_MODE_LIGHT));
        

        // Shape outputs
        row = 262.591f;
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(lftClm, row), module, RandomCurveModule::LINEAR_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(rgtClm, row), module, RandomCurveModule::STEP_OUTPUT));

        row = 297.664f;
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(lftClm, row), module, RandomCurveModule::CURVE_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(rgtClm, row), module, RandomCurveModule::SPIKY_OUTPUT));

        row = 332.738f;
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(lftClm, row), module, RandomCurveModule::USER1_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(rgtClm, row), module, RandomCurveModule::USER2_OUTPUT));
        addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(Vec(4.774f, 323.470f), module, RandomCurveModule::USER1_MODE_LIGHT));
        addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(Vec(33.509f, 323.470f), module, RandomCurveModule::USER2_MODE_LIGHT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        RandomCurveModule* module = dynamic_cast<RandomCurveModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);
        
        menu->addChild(createIndexPtrSubmenuItem("User 1 curve mode",
            { "Log", "Exp", "Top rounded/bottom sharp", "Bottom rounded/top sharp" },
            &module->user1Mode.req
        ));

        menu->addChild(createIndexPtrSubmenuItem("User 2 curve mode",
            { "Log", "Exp", "Top rounded/bottom sharp", "Bottom rounded/top sharp" },
            &module->user2Mode.req
        ));
        
        const int vrOfs = 2; // vr[0] (vr_off), vr[1] (vr_mp12) excluded from menu
        menu->addChild(createSubmenuItem("Set min/max-range", "",
		    	[=](Menu* menu) {
                    for (int i=0; i<voltRangeCount-vrOfs; i++) {
                        menu->addChild(createMenuItem(getVoltRangeName((voltRange)(i + vrOfs)), "", [=]() {
                           module->params[RandomCurveModule::MIN_CNTR_MAX_PARAM].setValue(voltRangeMin[i + vrOfs]);
                           module->params[RandomCurveModule::RANGE_PARAM].setValue(voltRangeMax[i + vrOfs]);
                       }));
                    }
                }
		        ));

        menu->addChild(createIndexPtrSubmenuItem("Distibution-range", { "60% (value-range not affected)",
            "65%", "70%", "75%", "80%", "85%", "90%", "95%", "100% (default)" },
            &module->distRange.req
        ));

        menu->addChild(createIndexPtrSubmenuItem("LFO rate chaos", getRateChaosNames(),
            &module->lfoRateChaos.req));
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelRandomCurve = createModel<RandomCurveModule, RandomCurveModuleWidget>("RandomCurve");