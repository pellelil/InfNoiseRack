// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct TLFOModule : InfNoiseModule {
    enum ParamId {
        FREQKNOB_PARAM,
        FREQ_TRIM_PARAM,
        RANGE_TOGGLE_PARAM,
        RNGKNOB_PARAM,
        PWMKNOB_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        FREQ_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        LFO_SQR_OUTPUT,
        LFO_TRI_OUTPUT,
        LFO_SAW_OUTPUT,
        LFO_SIN_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        FREQ_LIGHT,
        INV_SAW_LIGHT,
        INV_SQR_LIGHT,
        INV_TRI_LIGHT,
        INV_SIN_LIGHT,
        LIGHTS_LEN
    };

    bool haveSqrOutputs = false;
    bool haveTriOutputs = false;
    bool haveSawOutputs = false;
    bool haveSinOutputs = false;
    bool haveOutputs = false; // True if any LFO is in use
    float sampleRate = 44100.f;  // Re-obtained in processParams
    float sampleTime = 1.f / 44100.f;  // Re-obtained in processParams
    float phase[PORT_MAX_CHANNELS] = { 0.f }; // Phase of each channel
    float chaosFactor[PORT_MAX_CHANNELS] = { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f }; // Per-channel phase-step factor (new each cycle)
    float waveScale = 10.f; // Scale of each LFO (based on range)
    float waveOffset = -5.f;  // Offset of each LFO (based on range)

    actReqValue<bool> invWaveforms[4] {
        actReqValue<bool>(false),
        actReqValue<bool>(false),
        actReqValue<bool>(false),
        actReqValue<bool>(false)
    };
    actReqValue<bool> phaseLights = actReqValue<bool>(true);
    enum calcPhaseMode { ppm_allWaveforms, ppm_onlySquare };
    actReqValue<calcPhaseMode> pwmMode = actReqValue<calcPhaseMode>(ppm_allWaveforms);
    actReqValue<infNoisePwmRngQnt::pwmRange> pwmRange = actReqValue<infNoisePwmRngQnt::pwmRange>(infNoisePwmRngQnt::pwmRange::pwm_01_99);
    float minPwm = 0.01f;
    float maxPwm = 0.99f;
    actReqValue<rateChaos> lfoRateChaos = actReqValue<rateChaos>(rc_default);
    float chaosAmount = 0.f; // Cached rate-chaos amount (0-1), module-wide
    float freqModTrim = 0.f;
    float lastSinOutput = 0.f;

    TLFOModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configInput(FREQ_INPUT, "LFO Freq-CV (normalized to Sine-output)");
        configParam<infNoiseLfoFreqQnt>(FREQKNOB_PARAM, -8.f, 10.f, 1.f, "LFO Frequency", " Hz", 2, 1);
        configParam(FREQ_TRIM_PARAM, -1.f, 1.f, 0.f, "LFO Freq CV-trim", "%", 0, 100);

        configSwitch(RANGE_TOGGLE_PARAM, 0.0, 1.0, 0.0, "LFO range-mode", { "Bipolar (-5 to 5)", "Unipolar (0 to 10)" });
        configParam(RNGKNOB_PARAM, 0, 10.0f, 10.0f, "Range", " v", 0, 1);

        configParam<infNoisePwmRngQnt>(PWMKNOB_PARAM, -1.f, 1.f, 0.f, "Pulse width modulation", "%", 0, 100);

        configOutput(LFO_SQR_OUTPUT, "LFO Square-waveform");
        configOutput(LFO_TRI_OUTPUT, "LFO Triangle-waveform");
        configOutput(LFO_SAW_OUTPUT, "LFO Saw-waveform");
        configOutput(LFO_SIN_OUTPUT, "LFO Sine-waveform");

        configLight(FREQ_LIGHT, "LFO Phase");
        configLight(INV_SAW_LIGHT, "Inverted Saw (if lit)");
        configLight(INV_SQR_LIGHT, "Inverted Square (if lit)");
        configLight(INV_TRI_LIGHT, "Inverted Triangle (if lit)");
        configLight(INV_SIN_LIGHT, "Inverted Sine (if lit)");

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
        haveAutoProcQuality = true;
        haveOutClipRange = true;
        haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = false;
		haveTrigHighLow = false;
        autoProcQuality.setBoth(true);
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        
        phaseLights.setBoth(true);
        for (int c=0; c< PORT_MAX_CHANNELS; c++) {
			phase[c] = 0.f;
			chaosFactor[c] = 1.f;
		}
        lfoRateChaos.setBoth(rc_default);
        chaosAmount = 0.f;
        pwmMode.setBoth(ppm_allWaveforms);
        pwmRange.setBoth(infNoisePwmRngQnt::pwmRange::pwm_01_99);

        for (int i = 0; i < 4; i++) {
            invWaveforms[i].setBoth(false);
        }
        lastSinOutput = 0.f;
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        phaseLights.setBoth(getJsonBool(rootJ, "phaseLights", true));
        pwmMode.setBoth((calcPhaseMode)getJsonInt(rootJ, "pwmMode", (int)calcPhaseMode::ppm_allWaveforms));
        pwmRange.setBoth((infNoisePwmRngQnt::pwmRange)getJsonInt(rootJ, "pwmRange", (int)infNoisePwmRngQnt::pwmRange::pwm_01_99));
        bool invTmp[4];
        getJsonBoolArray(rootJ, "invWaveforms", invTmp, 4, false);
        for (int i = 0; i < 4; i++)
            invWaveforms[i].setBoth(invTmp[i]);
        lfoRateChaos.setBoth((rateChaos)getJsonInt(rootJ, "lfoRateChaos", (int)rc_default));
        getJsonFloatArray(rootJ, "phase", phase, PORT_MAX_CHANNELS, 0.f);
        getJsonFloatArray(rootJ, "chaosFactor", chaosFactor, PORT_MAX_CHANNELS, 1.f);
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "phaseLights", json_boolean(phaseLights.req));
        json_object_set_new(rootJ, "pwmMode", json_integer((int)pwmMode.req));
        json_object_set_new(rootJ, "pwmRange", json_integer((int)pwmRange.req));
        bool invTmp[4];
        for (int i = 0; i < 4; i++)
            invTmp[i] = invWaveforms[i].req;
        setJsonBoolArray(rootJ, "invWaveforms", invTmp, 4);
        json_object_set_new(rootJ, "lfoRateChaos", json_integer((int)lfoRateChaos.req));
        setJsonFloatArray(rootJ, "phase", phase, PORT_MAX_CHANNELS);
        setJsonFloatArray(rootJ, "chaosFactor", chaosFactor, PORT_MAX_CHANNELS);
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        sampleRate = safeSampleRate(args.sampleRate);
        sampleTime = 1.f / sampleRate;
        freqModTrim = params[FREQ_TRIM_PARAM].getValue();

        // Set booleans for output-usage
        haveSqrOutputs = outputs[LFO_SQR_OUTPUT].isConnected();
        haveTriOutputs = outputs[LFO_TRI_OUTPUT].isConnected();
        haveSawOutputs = outputs[LFO_SAW_OUTPUT].isConnected();
        haveSinOutputs = outputs[LFO_SIN_OUTPUT].isConnected();
        haveOutputs = haveSqrOutputs || haveTriOutputs || haveSawOutputs || haveSinOutputs;

        if (!haveSqrOutputs) {
            outputs[LFO_SQR_OUTPUT].setChannels(1);
            outputs[LFO_SQR_OUTPUT].setVoltage(0.f);
        }
        if (!haveTriOutputs) {
            outputs[LFO_TRI_OUTPUT].setChannels(1);
            outputs[LFO_TRI_OUTPUT].setVoltage(0.f);
        }
        if (!haveSawOutputs) {
            outputs[LFO_SAW_OUTPUT].setChannels(1);
            outputs[LFO_SAW_OUTPUT].setVoltage(0.f);
        }
        if (!haveSinOutputs) {
            outputs[LFO_SIN_OUTPUT].setChannels(1);
            outputs[LFO_SIN_OUTPUT].setVoltage(0.f);
        }

        // Get range values
        waveScale = params[RNGKNOB_PARAM].getValue();
        waveOffset = (params[RANGE_TOGGLE_PARAM].getValue() < 0.5f) 
            ? -waveScale / 2.f 
            : 0.f;

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

        // Ensure freq-lights go off for unused LFO's, or if disabled
        phaseLights.updateActual();
        if (!haveOutputs || !phaseLights.act)
            lights[FREQ_LIGHT].setBrightness(0.f);

        // Inverted-waveforms lights
        for(int i = 0; i < 4; i++) {
            invWaveforms[i].updateActual();
            lights[INV_SAW_LIGHT + i].setBrightness(invWaveforms[i].act ? 1.0f : 0.f);
        }

        // Rate-chaos (cache amount for the per-sample path)
        lfoRateChaos.updateActual();
        chaosAmount = rateChaosValues[lfoRateChaos.act];

        // Handle auto-quality
        if (autoProcQuality.act) {
            if ((haveOutputs && (inputs[FREQ_INPUT].isConnected() || freqModTrim != 0.f))) {
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

        if (doProcess && haveOutputs) {
            int channels = inputs[FREQ_INPUT].isConnected()
                ? std::max(inputs[FREQ_INPUT].getChannels(), 1)
                : 1;
            outputs[LFO_SQR_OUTPUT].setChannels(channels);
            outputs[LFO_TRI_OUTPUT].setChannels(channels);
            outputs[LFO_SAW_OUTPUT].setChannels(channels);
            outputs[LFO_SIN_OUTPUT].setChannels(channels);

            for (int c = 0; c < channels; c++) {
                // Calc freq/phase-step
				// Calc in processParam, and only recalc if inputs[FREQ_INPUT].IsConnected()
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

                // Update freq-lights accoring to phase and frequency
                if (c == 0) { // Lights only updated for first channel
                    lights[FREQ_LIGHT].setBrightness(
                        phaseLights.act
                        ? clockFreq >= 60.f
                            ? 1.0f
                            : phase[c] < 0.75f
                                ? 1.f - phase[c]
                                : 0.f
                        : 0.f);
                }

                // Calculate phase to use, based on PWM
                float pwmPhase = phase[c]; // PWM-phase (always used for square-waveform)
                float pwm = rescale(params[PWMKNOB_PARAM].getValue(), -1.f, 1.f, minPwm, maxPwm);
                if (pwm != 0.5f) {
                    pwm = clamp(pwm, minPwm, maxPwm);
                    float revPwm = 1.f - pwm;
                    pwmPhase = (phase[c] < pwm)
                        ? (phase[c] / pwm) * 0.5f
                        : ((phase[c] - pwm) / revPwm) * 0.5f + 0.5f;
                }
                float calcPhase = (pwmMode.act == ppm_allWaveforms)
                    ? pwmPhase
                    : phase[c];

                // Output Saw (down)
                float output;
                float invSign = 1.f;
                if (haveSawOutputs)
                {
                    invSign = invWaveforms[0].act ? -1.f : 1.f;
                    output = 1.f - calcPhase * 2.f;
                    output = ((output * invSign + 1.f) / 2.f) * waveScale + waveOffset;
                    output = clipToVoltRange(output, outClipRange.act);
                    outputs[LFO_SAW_OUTPUT].setVoltage(output, c);
                }            // Output Square

                if (haveSqrOutputs) {
                    invSign = invWaveforms[1].act ? -1.f : 1.f;
                    output = (phase[c] < pwm)
                        ? 1.f
                        : -1.f;
                    output = ((output * invSign + 1.f) / 2.f) * waveScale + waveOffset;
                    output = clipToVoltRange(output, outClipRange.act);
                    outputs[LFO_SQR_OUTPUT].setVoltage(output, c);
                }

                // Output Triangle
                if (haveTriOutputs) {
                    invSign = invWaveforms[2].act ? -1.f : 1.f;
                    output = (calcPhase < 0.25f)
                        ? (calcPhase * 4.f)
                        : (calcPhase < 0.75f)
                        ? 1.f - ((calcPhase - 0.25f) * 4.f)
                        : ((calcPhase - 0.75f) * 4.f) - 1.f;
                    output = ((output * invSign + 1.f) / 2.f) * waveScale + waveOffset;
                    output = clipToVoltRange(output, outClipRange.act);
                    outputs[LFO_TRI_OUTPUT].setVoltage(output, c);
                }

                // Output Sine
                if (haveSinOutputs || freqModTrim != 0.f) {
                    invSign = invWaveforms[3].act ? -1.f : 1.f;
                    output = bSin(calcPhase);
                    output = ((output * invSign + 1.f) / 2.f) * waveScale + waveOffset;
                    output = clipToVoltRange(output, outClipRange.act);
                    if (haveSinOutputs)
                        outputs[LFO_SIN_OUTPUT].setVoltage(output, c);
                    lastSinOutput = output; // Used for freq-trim if no freq-input is connected
                }

                // Increment phase
                phase[c] += phaseStep * chaosFactor[c];
                if (phase[c] >= 1.f) {
                    phase[c] -= std::truncf(phase[c]);  // Remove integer part
                    chaosFactor[c] = rateChaosFactor(chaosAmount); // New factor each completed cycle
                }
            }
        }

        cycle256++;
    }
};

struct TLFOModuleWidget : InfNoiseModuleWidget {
    TLFOModuleWidget(TLFOModule *module) {
        initializeWidget(module, "res/TLFO");

        float centerCol = 15.0f;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, 47.843f), module, TLFOModule::FREQKNOB_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(centerCol, 75.792f), module, TLFOModule::FREQ_TRIM_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 104.365f), module, TLFOModule::FREQ_INPUT));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(4.275f, 31.585f), module, TLFOModule::FREQ_LIGHT));

        addParam(createParamCentered<CKSS>(Vec(11.620f, 142.125f), module, TLFOModule::RANGE_TOGGLE_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, 166.151f), module, TLFOModule::RNGKNOB_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, 205.222f), module, TLFOModule::PWMKNOB_PARAM));

        // Waveform outputs
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 242.866f), module, TLFOModule::LFO_SAW_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 272.808f), module, TLFOModule::LFO_SQR_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 302.751f), module, TLFOModule::LFO_TRI_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 332.694f), module, TLFOModule::LFO_SIN_OUTPUT));

        // Inverted waveforms - lights
        const float invLgtRow = 5.019f;
        addChild(createLightCentered<TinyLight<RedLight>>(Vec(invLgtRow, 228.767f), module, TLFOModule::INV_SAW_LIGHT));
        addChild(createLightCentered<TinyLight<RedLight>>(Vec(invLgtRow, 258.710f), module, TLFOModule::INV_SQR_LIGHT));
        addChild(createLightCentered<TinyLight<RedLight>>(Vec(invLgtRow, 288.653f), module, TLFOModule::INV_TRI_LIGHT));
        addChild(createLightCentered<TinyLight<RedLight>>(Vec(invLgtRow, 318.595f), module, TLFOModule::INV_SIN_LIGHT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        TLFOModule* module = dynamic_cast<TLFOModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexPtrSubmenuItem("Pulse-width mode",
            { "All waveforms", "Square only" },
            &module->pwmMode.req));

        std::vector<std::string> pwmRangeNames =  { "1% to 99%", "5% to 95%", "10% to 90%", "15% to 85%", "20% to 80%", "25% to 75%" };
        menu->addChild(createIndexPtrSubmenuItem("Pulse-width range",
            pwmRangeNames, &module->pwmRange.req));

        menu->addChild(createSubmenuItem("Invert waveforms", "",
		    	[=](Menu* menu) {
                    menu->addChild(createBoolPtrMenuItem("Invert Saw", "", &module->invWaveforms[0].req));
                    menu->addChild(createBoolPtrMenuItem("Invert Square", "", &module->invWaveforms[1].req));
                    menu->addChild(createBoolPtrMenuItem("Invert Triangle", "", &module->invWaveforms[2].req));
                    menu->addChild(createBoolPtrMenuItem("Invert Sine", "", &module->invWaveforms[3].req));
                }
		        ));

        menu->addChild(createIndexPtrSubmenuItem("Phase/frequency-lights",
            { "Disabled", "Enabled" },
            &module->phaseLights.req));

        menu->addChild(createIndexPtrSubmenuItem("LFO rate chaos",
            getRateChaosNames(), &module->lfoRateChaos.req));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelTLFO = createModel<TLFOModule, TLFOModuleWidget>("TLFO");