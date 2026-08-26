// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct LFO1Module : InfNoiseModule {
    enum ParamId {
        FREQKNOB_PARAM,
        FREQ_TRIM_PARAM,
        RANGE_TOGGLE_PARAM,
        RNGKNOB_PARAM,
        PWMKNOB_PARAM,
        MODKNOB_PARAM,
        RNG_TRIM_PARAM,
        PWM_TRIM_PARAM,
        MOD_TRIM_PARAM,
        SYNC_ONESHOT_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        FREQ_INPUT,
        RNG_INPUT,
        PWM_INPUT,
        MOD_INPUT,
        SYNC_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        LFO_SQR_OUTPUT,
        LFO_TRI_OUTPUT,
        LFO_SAW_OUTPUT,
        LFO_SIN_OUTPUT,
        SYNC_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        FREQ_LIGHT,
        INVERTED_LIGHT,
        LIGHTS_LEN
    };

    bool haveSqrOutputs = false;
    bool haveTriOutputs = false; 
    bool haveSawOutputs = false;
    bool haveSinOutputs = false;
    bool haveOutputs = false; // True if any LFO is in use
    bool lfoInUse = false; // LFO's in use (have outputs, or is sync-master)
    float sampleRate = 44100.f;  // Re-obtained in processParams
    float sampleTime = 1.f / 44100.f;  // Re-obtained in processParams
    float phase[PORT_MAX_CHANNELS] = { 0.f }; // Phase of each channel
    float waveScale = 10.f; // Scale of each LFO (based on range)
    float waveOffset = -5.f;  // Offset of each LFO (based on range)

    int prevLfoChannels = -1; // -1 = not yet seen; used to init newly added poly channels
    dsp::SchmittTrigger syncInTrigger[PORT_MAX_CHANNELS] = {
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
    infNoiseOutTrigger syncOutTrigger[PORT_MAX_CHANNELS] = { 
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
    actReqValue<bool> invWaveforms = actReqValue<bool>(false);
    actReqValue<bool> phaseLights = actReqValue<bool>(true);
    enum calcPhaseMode { ppm_allWaveforms, ppm_onlySquare };
    actReqValue<calcPhaseMode> pwmMode = actReqValue<calcPhaseMode>(ppm_allWaveforms);
    actReqValue<infNoisePwmRngQnt::pwmRange> pwmRange = 
        actReqValue<infNoisePwmRngQnt::pwmRange>(infNoisePwmRngQnt::pwmRange::pwm_01_99);
    float minPwm = 0.01f;
    float maxPwm = 0.99f;
    enum syncOutModeType { som_both, som_onlySyncIn, som_onlyPhaseEnd };
    actReqValue<syncOutModeType> syncOutMode = actReqValue<syncOutModeType>(som_both);
    enum syncModeType { sm_hard, sm_soft };
    actReqValue<syncModeType> syncMode = actReqValue<syncModeType>(sm_hard); // Hard or soft-sync
    float syncSign[PORT_MAX_CHANNELS] = { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f }; // Sign for soft-sync (1 or -1)
    float chaosFactor[PORT_MAX_CHANNELS] = { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f }; // Per-channel phase-step factor (new each cycle)
    enum oneShotValueTypes { osv_Last, osv_Zero, osv_Center, osv_Min, osv_Max };
    actReqValue<oneShotValueTypes> oneShotValue = actReqValue<oneShotValueTypes>(osv_Zero);
    bool prevOneShot = false;
    int oneShotCount[PORT_MAX_CHANNELS] = { 0 };
    enum oneShotCyclesTypes { osc_1, osc_2, osc_3, osc_4, osc_5, osc_6, osc_7, osc_8, 
        osc_9, osc_10, osc_11, osc_12, osc_13, osc_14, osc_15, osc_16, osc_17, osc_19, osc_23, osc_24, osc_29, osc_32 };
    actReqValue<oneShotCyclesTypes> oneShotCycles = actReqValue<oneShotCyclesTypes>(osc_1);
    const int osCycleCount[22] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 19, 23, 24, 29, 32 };
    actReqValue<rateChaos> lfoRateChaos = actReqValue<rateChaos>(rc_default);
    float chaosAmount = 0.f; // Cached rate-chaos amount (0-1), module-wide
    float freqModTrim = 0.f;
    float lastSinOutput = 0.f;

    LFO1Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configParam<infNoiseLfoFreqQnt>(FREQKNOB_PARAM, -8.f, 10.f, 1.f, "LFO Frequency", " Hz", 2, 1);
        configParam(FREQ_TRIM_PARAM, -1.f, 1.f, 0.f, "LFO Freq CV-trim", "%", 0, 100);
        configInput(FREQ_INPUT, "LFO Freq-CV (normalized to Sine-output)");

        configSwitch(RANGE_TOGGLE_PARAM, 0.0, 1.0, 0.0, "LFO range-mode", { "Bipolar (-5 to 5)", "Unipolar (0 to 10)" });
        configParam(RNGKNOB_PARAM, 0, 10.0f, 10.0f, "Range", " v", 0, 1);
        configParam(RNG_TRIM_PARAM, -1.f, 1.f, 0.f, "Range CV-trim", "%", 0, 100);
        configInput(RNG_INPUT, "Range");

        configParam<infNoisePwmRngQnt>(PWMKNOB_PARAM, -1.f, 1.f, 0.f, "Pulse width modulation", "%", 0, 100);
        configParam(PWM_TRIM_PARAM, -1.f, 1.f, 0.f, "PWM CV-trim", "%", 0, 100);
        configInput(PWM_INPUT, "PWM CV");

        configParam(MODKNOB_PARAM, -1.f, 1.0f, 0.0f, "Modulation", "", 0, 1);
        configParam(MOD_TRIM_PARAM, -1.f, 1.f, 0.f, "MOD CV-trim", "%", 0, 100);
        configInput(MOD_INPUT, "MOD CV");
        
        configInput(SYNC_INPUT, "LFO Sync");
        configOutput(SYNC_OUTPUT, "LFO Sync");

        configSwitch(SYNC_ONESHOT_PARAM, 0.0, 1.0, 0.0, "n-shot", { "Disabled (continous)", "Enabled (single)" });

        configOutput(LFO_SQR_OUTPUT, "LFO Square-waveform");
        configOutput(LFO_TRI_OUTPUT, "LFO Triangle-waveform");
        configOutput(LFO_SAW_OUTPUT, "LFO Saw-waveform");
        configOutput(LFO_SIN_OUTPUT, "LFO Sine-waveform");

        configLight(FREQ_LIGHT, "LFO Phase");
        configLight(INVERTED_LIGHT, "Inverted waveforms (if lit)");

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
        haveAutoProcQuality = true;
        haveOutClipRange = true;
        haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = true;
		haveTrigHighLow = true;
        autoProcQuality.setBoth(true);
        ensureFiveSineExpLogLuts();
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);

        phaseLights.setBoth(true);
        lfoInUse = false;
        for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
            phase[c] = 0.f;
            oneShotCount[c] = 0;
            syncInTrigger[c].reset();
            syncOutTrigger[c].reset();
            syncSign[c] = 1.f;
            chaosFactor[c] = 1.f;
        }
        lfoRateChaos.setBoth(rc_default);
        chaosAmount = 0.f;
        waveScale = 10.f;
        waveOffset = -5.f;
        pwmMode.setBoth(ppm_allWaveforms);
        pwmRange.setBoth(infNoisePwmRngQnt::pwmRange::pwm_01_99);
        invWaveforms.setBoth(false);
        syncMode.setBoth(sm_hard);
        syncOutMode.setBoth(som_both);
        prevOneShot = false;
        prevLfoChannels = -1;
        oneShotValue.setBoth(osv_Zero);
        oneShotCycles.setBoth(osc_1);
        lastSinOutput = 0.f;
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        phaseLights.setBoth(getJsonBool(rootJ, "phaseLights", true));
        pwmMode.setBoth((calcPhaseMode)getJsonInt(rootJ, "pwmMode", (int)calcPhaseMode::ppm_allWaveforms));            
        pwmRange.setBoth((infNoisePwmRngQnt::pwmRange)getJsonInt(rootJ, "pwmRange", (int)infNoisePwmRngQnt::pwmRange::pwm_01_99));
        invWaveforms.setBoth(getJsonBool(rootJ, "invWaveforms", false));
        syncMode.setBoth((syncModeType)getJsonInt(rootJ, "syncMode", (int)sm_hard));
        syncOutMode.setBoth((syncOutModeType)getJsonInt(rootJ, "syncOutMode", (int)som_both));
        oneShotValue.setBoth((oneShotValueTypes)getJsonInt(rootJ, "oneShotValue", (int)osv_Zero));
        oneShotCycles.setBoth((oneShotCyclesTypes)getJsonInt(rootJ, "oneShotCycles", (int)osc_1));
        lfoRateChaos.setBoth((rateChaos)getJsonInt(rootJ, "lfoRateChaos", (int)rc_default));
        getJsonFloatArray(rootJ, "phase", phase, PORT_MAX_CHANNELS, 0.f);
        getJsonFloatArray(rootJ, "syncSign", syncSign, PORT_MAX_CHANNELS, 1.f);
        getJsonFloatArray(rootJ, "chaosFactor", chaosFactor, PORT_MAX_CHANNELS, 1.f);
        getJsonIntArray(rootJ, "oneShotCount", oneShotCount, PORT_MAX_CHANNELS, 0);
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "phaseLights", json_boolean(phaseLights.req));
        json_object_set_new(rootJ, "pwmMode", json_integer((int)pwmMode.req));
        json_object_set_new(rootJ, "pwmRange", json_integer((int)pwmRange.req));
        json_object_set_new(rootJ, "invWaveforms", json_boolean(invWaveforms.req));
        json_object_set_new(rootJ, "syncMode", json_integer((int)syncMode.req));
        json_object_set_new(rootJ, "syncOutMode", json_integer((int)syncOutMode.req));
        json_object_set_new(rootJ, "oneShotValue", json_integer(oneShotValue.req));
        json_object_set_new(rootJ, "oneShotCycles", json_integer(oneShotCycles.req));
        json_object_set_new(rootJ, "lfoRateChaos", json_integer((int)lfoRateChaos.req));
        setJsonFloatArray(rootJ, "phase", phase, PORT_MAX_CHANNELS);
        setJsonFloatArray(rootJ, "syncSign", syncSign, PORT_MAX_CHANNELS);
        setJsonFloatArray(rootJ, "chaosFactor", chaosFactor, PORT_MAX_CHANNELS);
        setJsonIntArray(rootJ, "oneShotCount", oneShotCount, PORT_MAX_CHANNELS);
    }

/// @brief Applies modulation, and normalize (0-1)
    /// @param value Value to be modulated/normalized
    /// @param modFactor Modulation factor (-1 to 1)
    /// @return Modulated/normalized value (0-1)
    inline float modNorm(float value, float modFactor) {
        float absFactor = fabs(modFactor);
        absFactor = clamp(absFactor, 0.f, 1.f);
        if (absFactor < 0.00001f || value == 0.00001f)
            return (value + 1.f) * 0.5f;  // Normalize to 0-1

        float phase = fabs(value);
        float sinValue = (modFactor > 0.f)
            ? fiveSineLogIsh(phase)
            : fiveSineExpIsh(phase);

        float valueFactor = 1.f - absFactor;
        float sign = (value < 0.f) ? -1.f : 1.f;
        value = (value * valueFactor) + (sinValue * sign * absFactor);
        
        return (value + 1.f) * 0.5f; // Normalize to 0-1
	}

    /// @brief Applies square-modulation, and normalize (0-1)
    /// @param value Value to be modulated/normalized
    /// @param modFactor Modulation factor (-1 to 1)
    /// @return Modulated/normalized value (0-1)
    inline float modNormSquare(float value, float modFactor, float phase) {
        float absFactor = fabs(modFactor);
        absFactor = clamp(absFactor, 0.f, 1.f);
        if (absFactor < 0.00001f || value == 0.00001f)
            return (value + 1.f) * 0.5f;  // Normalize to 0-1

        if (invWaveforms.act) {
            phase += 0.5f;
            if (phase >= 1.f)
                phase -= 1.f;
        }

        // Transist to triangle
        if (modFactor < 0.f) { 
            double phaseStart = 0.25f * absFactor;
            double phaseEnd = 1.f - phaseStart;

            // First 90 deg of triangle
            if (phase < phaseStart)
                return 0.5f + phase * (1.f / phaseStart) * 0.5f; // Normalized

            // Last 90 deg of triangle
            if (phase > phaseEnd) 
                return ((phase - phaseEnd) * (1.f / phaseStart)) * 0.5f; // Normalized

            // 2 parts that is still square
            float sqrEnd = 0.5f - phaseStart; // End of 1st square-part
            float sqrStart = 0.5f + phaseStart;  // Start of 2nd square-part
            if (phase <= sqrEnd || phase >= sqrStart)
                return (value + 1.f) * 0.5f; // Normalize to 0-1

            // Remaining middel 180 deg of triangle
            float remPhase = sqrStart - sqrEnd;
            float midPhase = (phase - sqrEnd) * (1.f / remPhase);
            return 1.f - midPhase; // Normalized to 0-1
        }
        
        // Transist middel-part to saw
        if (invWaveforms.act) {
            double halfTrans = 0.5 * absFactor;
            if (phase < halfTrans) 
                return 0.5f + (phase / halfTrans) * 0.5f; // Upper-half saw
            if (phase <= 0.5f) // Remaining upper-square
                return 1.f;
            if (phase <= 1.0f - halfTrans) 
                return 0.f; // Remaining lower-square
            return ((phase - (1.0f - halfTrans)) / halfTrans) * 0.5f; // Lower-half saw
        }
        double halfTrans = 0.5 * absFactor;
        double phaseStart = 0.5f - halfTrans;
        double phaseEnd = 0.5 + halfTrans;
        if (phase <= phaseStart) return 1; // Normalized to 0-1
        if (phase >= phaseEnd) return 0; // Normalized to 0-1
        float sawPhase = (phase - phaseStart) / (phaseEnd - phaseStart);
        return 1.f - sawPhase; // Normalized to 0-1
	}

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------
        
        sampleRate = safeSampleRate(args.sampleRate);
        sampleTime = 1.f / sampleRate;
        freqModTrim = params[FREQ_TRIM_PARAM].getValue();

        syncMode.updateActual();
        if (syncMode.act == sm_hard || !inputs[SYNC_INPUT].isConnected()) {
            for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
                syncSign[c] = 1.f;
            }
        }
        syncOutMode.updateActual();

        oneShotValue.updateActual();
        oneShotCycles.updateActual();

        // Rate-chaos (cache amount for the per-sample path)
        lfoRateChaos.updateActual();
        chaosAmount = rateChaosValues[lfoRateChaos.act];

        // Set booleans for output-usage
        haveSqrOutputs = outputs[LFO_SQR_OUTPUT].isConnected();
        haveTriOutputs = outputs[LFO_TRI_OUTPUT].isConnected();
        haveSawOutputs = outputs[LFO_SAW_OUTPUT].isConnected();
        haveSinOutputs = outputs[LFO_SIN_OUTPUT].isConnected();
        haveOutputs = haveSqrOutputs || haveTriOutputs || haveSawOutputs || haveSinOutputs;
        lfoInUse = haveOutputs || outputs[SYNC_OUTPUT].isConnected();
       
        invWaveforms.updateActual();
        lights[INVERTED_LIGHT].setBrightness(invWaveforms.act ? 1.f : 0.f);

        // Update PWM-mode/range
        pwmMode.updateActual();
        if (pwmRange.needsUpdate()) {
            pwmRange.updateActual();
            infNoisePwmRngQnt* pwmQuantity = dynamic_cast<infNoisePwmRngQnt*>(paramQuantities[PWMKNOB_PARAM]);
            if (pwmQuantity) {
                pwmQuantity->setRange(pwmRange.act);
                minPwm = pwmQuantity->minRanges[pwmRange.act];
                maxPwm = pwmQuantity->maxRanges[pwmRange.act];
            }
        }

        // Ensure sync-output goes "off" if disconnected
        if (!outputs[SYNC_OUTPUT].isConnected()) {
            outputs[SYNC_OUTPUT].setChannels(1);
            outputs[SYNC_OUTPUT].setVoltage(0.f);
        }

        // Ensure freq-lights go off for unused LFO's, or if disabled
        phaseLights.updateActual();
        if (!lfoInUse || !phaseLights.act)
            lights[FREQ_LIGHT].setBrightness(0.f);

        // Handle auto-quality
        if (autoProcQuality.act) {
            if (lfoInUse && (inputs[FREQ_INPUT].isConnected() || freqModTrim != 0.f)) {
                procQuality.setBoth(pq_audioRate, false);
            }
            else if (haveOutputs) {
                float clockFreq = 2.f;  // 2 Hz
                float pitch = params[FREQKNOB_PARAM].getValue();
                clockFreq = clockFreq / 2.f * dsp::exp2_taylor5(pitch);
                procQuality.setBoth(getEstimatedLfoProcessQuality(sampleRate, clockFreq * rateChaosMaxFactor[lfoRateChaos.act]), false);
            }
            else
                procQuality.setBoth(pq_veryLowRate, false); // No outputs
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

        if (doProcess && lfoInUse) {
            int channels = inputs[FREQ_INPUT].isConnected()
                ? std::max(inputs[FREQ_INPUT].getChannels(), 1)
                : 1;
            outputs[SYNC_OUTPUT].setChannels(channels);
            outputs[LFO_SQR_OUTPUT].setChannels(channels);
            outputs[LFO_TRI_OUTPUT].setChannels(channels);
            outputs[LFO_SAW_OUTPUT].setChannels(channels);
            outputs[LFO_SIN_OUTPUT].setChannels(channels);

            // Get range values
            waveScale = params[RNGKNOB_PARAM].getValue();
            if (inputs[RNG_INPUT].isConnected())
                waveScale += params[RNG_TRIM_PARAM].getValue() *
                (inputs[RNG_INPUT].getVoltage(0.f));
            float waveOffset = (params[RANGE_TOGGLE_PARAM].getValue() < 0.5f)
                ? -waveScale / 2.f
                : 0.f;

            // Set one-shot value based on oneShotValue-setting and wave-offset/scale
            float osValue = 0.f; // Value to set after one-shot
            if (oneShotValue.act > osv_Zero) {
                osValue = waveOffset; // same as "Min"
                if (oneShotValue.act == osv_Center)
                    osValue += waveScale / 2.f;
                else if (oneShotValue.act == osv_Max)
                    osValue += waveScale;
            }

            // Handle one-shot button pressed
            bool oneShotBtn = params[SYNC_ONESHOT_PARAM].getValue() > 0.5f;
            if (oneShotBtn != prevOneShot) {
                if (oneShotBtn) {
                    for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
                        phase[c] = 0.f;
                        oneShotCount[c] = 0;
                        chaosFactor[c] = 1.f;
                        syncSign[c] = 1.f;
                        syncOutTrigger[c].reset();

                        if (c < channels) {
                            outputs[LFO_SQR_OUTPUT].setVoltage(osValue, c);
                            outputs[LFO_TRI_OUTPUT].setVoltage(osValue, c);
                            outputs[LFO_SAW_OUTPUT].setVoltage(osValue, c);
                            outputs[LFO_SIN_OUTPUT].setVoltage(osValue, c);
                            outputs[SYNC_OUTPUT].setVoltage(0.f, c);
                        }
                    }
                }

                prevOneShot = oneShotBtn;
            }

            // Newly added poly channels can keep leftover sync voltages/trigger
            // state from a previous (wider) run. Init them so n-shot idle is 0V.
            if (prevLfoChannels < 0) {
                prevLfoChannels = channels;
            }
            else if (channels > prevLfoChannels) {
                for (int c = prevLfoChannels; c < channels; c++) {
                    phase[c] = 0.f;
                    oneShotCount[c] = 0;
                    chaosFactor[c] = 1.f;
                    syncSign[c] = 1.f;
                    syncOutTrigger[c].reset();
                    outputs[SYNC_OUTPUT].setVoltage(0.f, c);
                    if (oneShotBtn) {
                        outputs[LFO_SQR_OUTPUT].setVoltage(osValue, c);
                        outputs[LFO_TRI_OUTPUT].setVoltage(osValue, c);
                        outputs[LFO_SAW_OUTPUT].setVoltage(osValue, c);
                        outputs[LFO_SIN_OUTPUT].setVoltage(osValue, c);
                    }
                }
                prevLfoChannels = channels;
            }
            else {
                prevLfoChannels = channels;
            }

            // Get pwm values
            float pwm = params[PWMKNOB_PARAM].getValue();
            if (inputs[PWM_INPUT].isConnected())
                pwm += params[PWM_TRIM_PARAM].getValue() *
                (inputs[PWM_INPUT].getNormalVoltage(0.f) / 5.f);
            pwm = rescale(clamp(pwm, -1.f, 1.f), -1.f, 1.f, minPwm, maxPwm);

            // Get mod values
            float modValue = params[MODKNOB_PARAM].getValue();
            if (inputs[MOD_INPUT].isConnected()) {
                modValue += params[MOD_TRIM_PARAM].getValue() *
                    (inputs[MOD_INPUT].getNormalVoltage(0.f) / 10.f);
                modValue = clamp(modValue, -1.f, 1.f);
            }

            // Sign used for inverted waveforms
            float invSign = invWaveforms.act ? -1.f : 1.f;

            // Handle LFO outputs (for each channel)
            float output;
            for (int c = 0; c < channels; c++) {
                //Handle syncIn, and set syncOut
                bool syncIn = (inputs[SYNC_INPUT].isConnected())
                    ? syncInTrigger[c].process(inputs[SYNC_INPUT].getPolyVoltage(c),
                        trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act])
                    : false;
                bool syncOut = (syncIn && syncOutMode.act != som_onlyPhaseEnd);
                if (syncIn && syncMode.act == sm_soft)
                    syncSign[c] *= -1.f;

                // Handle one-shot (set count for this channel only)
                if (syncIn && oneShotBtn) {
                    oneShotCount[c] = (int)osCycleCount[oneShotCycles.act];
                }

                // Waiting / finished n-shot for this channel (after applying a possible new sync-in)
                bool nShotIdle = oneShotBtn && oneShotCount[c] == 0;

                // Calc freq/phase-step (can be different for each channel)
                float clockFreq = 2.f;  // 2 Hz
                float pitch = params[FREQKNOB_PARAM].getValue();
                if (inputs[FREQ_INPUT].isConnected())
                    pitch += freqModTrim * inputs[FREQ_INPUT].getVoltage(c);
                else if (freqModTrim != 0.f)
                    pitch += freqModTrim * lastSinOutput;
                if (pitch < -8.f)
                    pitch = -8.f;
                clockFreq = clockFreq / 2.f * dsp::exp2_taylor5(pitch);
                float cycleStep = processQualityCycles[procQuality.act];
                float phaseStep = (clockFreq * cycleStep) / sampleRate;

                bool phaseEnd = false;
                if (nShotIdle) {
                    phase[c] = 0.f;
                    if (c == 0)
                        lights[FREQ_LIGHT].setBrightness(0.f);
                }
                else {
                    // Detect syncIn, and update syncOut
                    if (syncIn && syncMode.act == sm_hard) {
                        phase[c] = 0.f;
                        chaosFactor[c] = rateChaosFactor(chaosAmount); // Hard-sync restarts the cycle
                    }
                    else
                        phase[c] += phaseStep * syncSign[c] * chaosFactor[c];
                    phaseEnd = phase[c] < 0.f || phase[c] >= 1.f;
                    phase[c] -= std::floor(phase[c]);
                    if (phaseEnd)
                        chaosFactor[c] = rateChaosFactor(chaosAmount); // New factor each completed cycle
                    if (phaseEnd && syncOutMode.act != som_onlySyncIn)
                        syncOut = true;

                    // Last one-shot finished
                    if (oneShotBtn && !syncIn && phaseEnd && oneShotCount[c] == 1) {
                        oneShotCount[c] = 0;
                        phase[c] = 0.f;

                        if (oneShotValue.act != osv_Last)
                        {
                            outputs[LFO_SQR_OUTPUT].setVoltage(osValue, c);
                            outputs[LFO_TRI_OUTPUT].setVoltage(osValue, c);
                            outputs[LFO_SAW_OUTPUT].setVoltage(osValue, c);
                            outputs[LFO_SIN_OUTPUT].setVoltage(osValue, c);
                        }

                        if (c == 0)
                            lights[FREQ_LIGHT].setBrightness(0.f);
                    }
                    else {
                        // A one-shot cycle is finished (however not the last)
                        if (oneShotBtn && !syncIn && phaseEnd)
                            oneShotCount[c]--;

                        // Update freq-lights accoring to phase and frequency
                        if (c == 0) {
                            lights[FREQ_LIGHT].setBrightness(getFreqPhaseBrightness(clockFreq, phase[c], phaseLights.act));
                        }

                        // Calculate phase to use, based on PWM
                        float pwmPhase = phase[c]; // PWM-phase (always used for square-waveform)
                        if (pwm != 0.5f) {
                            float revPwm = 1.f - pwm;
                            pwmPhase = (phase[c] < pwm)
                                ? (phase[c] / pwm) * 0.5f
                                : ((phase[c] - pwm) / revPwm) * 0.5f + 0.5f;
                        }
                        float calcPhase = (pwmMode.act == ppm_allWaveforms)
                            ? pwmPhase
                            : phase[c];

                        // Output Square
                        if (haveSqrOutputs) {
                            output = (phase[c] < pwm)
                                ? 1.f
                                : -1.f;
                            output = modNormSquare(output * invSign, modValue, pwmPhase) * waveScale + waveOffset;
                            output = clipToVoltRange(output, outClipRange.act);
                            outputs[LFO_SQR_OUTPUT].setVoltage(output, c);
                        }

                        // Output Triangle
                        if (haveTriOutputs) {
                            output = (calcPhase < 0.25f)
                                ? (calcPhase * 4.f)
                                : (calcPhase < 0.75f)
                                ? 1.f - ((calcPhase - 0.25f) * 4.f)
                                : ((calcPhase - 0.75f) * 4.f) - 1.f;
                            output = modNorm(output * invSign, modValue) * waveScale + waveOffset;
                            output = clipToVoltRange(output, outClipRange.act);
                            outputs[LFO_TRI_OUTPUT].setVoltage(output, c);
                        }

                        // Output Saw (down)
                        if (haveSawOutputs) {
                            output = modNorm((1.f - calcPhase * 2.f) * invSign, modValue) * waveScale + waveOffset;
                            output = clipToVoltRange(output, outClipRange.act);
                            outputs[LFO_SAW_OUTPUT].setVoltage(output, c);
                        }

                        // Output Sine
                        if (haveSinOutputs || freqModTrim != 0.f) {
                            output = modNorm(bSin(calcPhase) * invSign, modValue) * waveScale + waveOffset;
                            output = clipToVoltRange(output, outClipRange.act);
                            if (haveSinOutputs)
                                outputs[LFO_SIN_OUTPUT].setVoltage(output, c);
                            lastSinOutput = output; // Used for freq-trim if no freq-input is connected
                        }
                    }
                }

                // Always write this channel's sync-out (1 ms pulse). When n-shot is
                // idle, only let an in-flight pulse finish — do not start a new one.
                if (!syncOutTrigger[c].process(procSampleTime) && syncOut && !nShotIdle)
                    syncOutTrigger[c].trigger();
                outputs[SYNC_OUTPUT].setVoltage(syncOutTrigger[c].isHigh()
                    ? voltValues[trigOutHigh.act]
                    : voltValues[trigOutLow.act], c);
            }
        }
        else if (outputs[SYNC_OUTPUT].isConnected()) {
            // Keep 1 ms sync pulses on wall-clock time when the LFO loop is skipped
            int syncChannels = outputs[SYNC_OUTPUT].getChannels();
            if (syncChannels < 1)
                syncChannels = 1;
            for (int c = 0; c < syncChannels; c++) {
                syncOutTrigger[c].process(args.sampleTime);
                outputs[SYNC_OUTPUT].setVoltage(syncOutTrigger[c].isHigh()
                    ? voltValues[trigOutHigh.act]
                    : voltValues[trigOutLow.act], c);
            }
        }

        cycle256++;
    }
};

struct LFO1ModuleWidget : InfNoiseModuleWidget {
    LFO1ModuleWidget(LFO1Module *module) {
        initializeWidget(module, "res/LFO1");

        const float leftPortCol = 14.810f;
        const float rightPortCol = 43.545;
        const float freqKnobCol = 40.093f;
        const float freqKnobRow = 57.587f;
        const float freqLightCol = 26.183f;
        const float freqLightRow = 39.669f;
        const float rangeCol = 7.852;
        const float rangeRow = 57.587;
        const float trimCol = 46.305f;

        // Frequency and Range
        const float cvTrimRow = 86.419f; 
        addParam(createParamCentered<CKSS>(Vec(rangeCol, rangeRow), module, LFO1Module::RANGE_TOGGLE_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(Vec(freqKnobCol, freqKnobRow), module, LFO1Module::FREQKNOB_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftPortCol, cvTrimRow), module, LFO1Module::FREQ_INPUT));
        addParam(createParamCentered<Trimpot>(Vec(trimCol, cvTrimRow), module, LFO1Module::FREQ_TRIM_PARAM));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(freqLightCol, freqLightRow), module, LFO1Module::FREQ_LIGHT));

        // Range
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(30.705f, 114.938f), module, LFO1Module::RNGKNOB_PARAM));
        const float rngTrimRow = 134.591f;
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(leftPortCol, rngTrimRow), module, LFO1Module::RNG_INPUT));
        addParam(createParamCentered<Trimpot>(Vec(trimCol, rngTrimRow), module, LFO1Module::RNG_TRIM_PARAM));

        // PWM
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(30.705f, 163.213f), module, LFO1Module::PWMKNOB_PARAM));
        const float pwmTrimRow = 182.866f;
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(leftPortCol, pwmTrimRow), module, LFO1Module::PWM_INPUT));
        addParam(createParamCentered<Trimpot>(Vec(trimCol, pwmTrimRow), module, LFO1Module::PWM_TRIM_PARAM));

        // MOD
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(30.705f, 211.590f), module, LFO1Module::MODKNOB_PARAM));
        const float modTrimRow = 231.243f;
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(leftPortCol, modTrimRow), module, LFO1Module::MOD_INPUT));
        addParam(createParamCentered<Trimpot>(Vec(trimCol, modTrimRow), module, LFO1Module::MOD_TRIM_PARAM));

        // Sync-input/output
        float syncRow =268.996f;
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftPortCol, syncRow), module, LFO1Module::SYNC_INPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightPortCol, syncRow), module, LFO1Module::SYNC_OUTPUT));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(Vec(26.349f, 280.423f), module, LFO1Module::SYNC_ONESHOT_PARAM));

        // Inverted waveforms - lights
        addChild(createLightCentered<TinyLight<RedLight>>(Vec(29.578f, 288.897f), module, LFO1Module::INVERTED_LIGHT));

        // Waveoutputs
        const float portSpacing = 29.943f;
        float row = 302.751f;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftPortCol, row), module, LFO1Module::LFO_SAW_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightPortCol, row), module, LFO1Module::LFO_TRI_OUTPUT));
        row += portSpacing;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftPortCol, row), module, LFO1Module::LFO_SQR_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightPortCol, row), module, LFO1Module::LFO_SIN_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        LFO1Module* module = dynamic_cast<LFO1Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexPtrSubmenuItem("Pulse-width mode",
            {"All waveforms", "Square only"},
            &module->pwmMode.req));

        std::vector<std::string> pwmRangeNames = { "1% to 99%", "5% to 95%", "10% to 90%", "15% to 85%", "20% to 80%", "25% to 75%" };
        menu->addChild(createIndexPtrSubmenuItem("Pulse-width range",
            pwmRangeNames, &module->pwmRange.req));

        menu->addChild(createBoolPtrMenuItem("Invert (all) waveforms", "", &module->invWaveforms.req));       

        std::vector<std::string> syncModeNames = { "Hard-sync", "Soft-sync" };
        menu->addChild(createIndexPtrSubmenuItem("Sync-mode", syncModeNames,
            &module->syncMode.req));

        std::vector<std::string> syncOutModeNames = { "Both on sync-in and phase-end", "Only on sync-in", "Only on phase-end"};
        menu->addChild(createIndexPtrSubmenuItem("Send sync-out", syncOutModeNames,
            &module->syncOutMode.req));

        std::vector<std::string> oneShotValueNames = { "Keep last", "Zero (default)", "Center", "Min", "Max"};
        menu->addChild(createIndexPtrSubmenuItem("Value after n-shot",
            oneShotValueNames, &module->oneShotValue.req));

        std::vector<std::string> oneShotCycleNames;
        for (int i = 0; i < 22; i++) {
            std::string defaultValue = (i == 0) ? " (default)" : "";
			oneShotCycleNames.push_back(std::to_string(module->osCycleCount[i]) + defaultValue);
		}
        menu->addChild(createIndexPtrSubmenuItem("n-shot cycles",
            oneShotCycleNames, &module->oneShotCycles.req));

        menu->addChild(createIndexPtrSubmenuItem("Phase/frequency-lights",
            {"Disabled", "Enabled"},
            &module->phaseLights.req));

        menu->addChild(createIndexPtrSubmenuItem("LFO rate chaos",
            getRateChaosNames(), &module->lfoRateChaos.req));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelLFO1 = createModel<LFO1Module, LFO1ModuleWidget>("LFO1");