// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct PhaseDrivenLFOModule : InfNoiseModule {
    enum ParamId {
        PHASE_RANGE_PARAM,
        PHASE_EXCD_PARAM,
        RANGE_TOGGLE_PARAM,
        RNGKNOB_PARAM,
        RNG_TRIM_PARAM,
        PWMKNOB_PARAM,
        PWM_TRIM_PARAM,
        MODKNOB_PARAM,
        MOD_TRIM_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        PHASE_INPUT,
        RNG_INPUT,
        PWM_INPUT,
        MOD_INPUT,
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
        ENUMS(PROCQUAL_LIGHT, 2),
        ENUMS(CLIP_RANGE_LIGHT, 2),
        INVERTED_LIGHT,
        LIGHTS_LEN
    };

    bool bipolarPhase = true;
    float phaseMin = -5.f;
    float phaseMax = 5.f;
    static constexpr float phaseRange = 10.f;
    bool clipMode = true;
    int channels = 1;
    bool haveSqrOutputs = false;
    bool haveTriOutputs = false; 
    bool haveSawOutputs = false;
    bool haveSinOutputs = false;
    bool haveOutputs = false; // True if any waveform-output is connected
    float waveScale = 10.f; // Scale of each LFO (based on range)
    float waveOffset = -5.f;  // Offset of each LFO (based on range)
    actReqValue<bool> invWaveforms = actReqValue<bool>(false);
    enum calcPhaseMode { ppm_allWaveforms, ppm_onlySquare };
    actReqValue<calcPhaseMode> pwmMode = actReqValue<calcPhaseMode>(ppm_allWaveforms);
    actReqValue<infNoisePwmRngQnt::pwmRange> pwmRange = 
        actReqValue<infNoisePwmRngQnt::pwmRange>(infNoisePwmRngQnt::pwmRange::pwm_01_99);
    float minPwm = 0.01f;
    float maxPwm = 0.99f;
    
	PhaseDrivenLFOModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
        
        configInput(PHASE_INPUT, "Phase-CV");
        configSwitch(PHASE_RANGE_PARAM, 0.0, 1.0, 0.0, "Phase range-mode", { "Bipolar (-5 to 5)", "Unipolar (0 to 10)" });
        configSwitch(PHASE_EXCD_PARAM, 0.0, 1.0, 0.0, "Phase exceed-mode", { "Clamp", "Wrap" });

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
        
        configOutput(LFO_SQR_OUTPUT, "LFO Square-waveform");
        configOutput(LFO_TRI_OUTPUT, "LFO Triangle-waveform");
        configOutput(LFO_SAW_OUTPUT, "LFO Saw-waveform");
        configOutput(LFO_SIN_OUTPUT, "LFO Sine-waveform");

        configLight(INVERTED_LIGHT, "Inverted waveforms (if lit)");

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

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);

        waveScale = 10.f;
        waveOffset = -5.f;
        pwmMode.setBoth(ppm_allWaveforms);
        pwmRange.setBoth(infNoisePwmRngQnt::pwmRange::pwm_01_99);
        invWaveforms.setBoth(false);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);

        pwmMode.setBoth((calcPhaseMode)getJsonInt(rootJ, "pwmMode", (int)calcPhaseMode::ppm_allWaveforms));            
        pwmRange.setBoth((infNoisePwmRngQnt::pwmRange)getJsonInt(rootJ, "pwmRange", (int)infNoisePwmRngQnt::pwmRange::pwm_01_99));
        invWaveforms.setBoth(getJsonBool(rootJ, "invWaveforms", false));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "pwmMode", json_integer((int)pwmMode.req));
        json_object_set_new(rootJ, "pwmRange", json_integer((int)pwmRange.req));
        json_object_set_new(rootJ, "invWaveforms", json_boolean(invWaveforms.req));
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

        bipolarPhase = params[PHASE_RANGE_PARAM].getValue() < 0.5f;
        if (bipolarPhase) {
            phaseMin = -5.f;
            phaseMax = 5.f;
        } else {
            phaseMin = 0.f;
            phaseMax = 10.f;
        }

        clipMode = params[PHASE_EXCD_PARAM].getValue() < 0.5f;

        channels = inputs[PHASE_INPUT].isConnected()
            ? inputs[PHASE_INPUT].getChannels()
            : 1;
        outputs[LFO_SQR_OUTPUT].setChannels(channels);
        outputs[LFO_TRI_OUTPUT].setChannels(channels);
        outputs[LFO_SAW_OUTPUT].setChannels(channels);
        outputs[LFO_SIN_OUTPUT].setChannels(channels);

        haveSqrOutputs = outputs[LFO_SQR_OUTPUT].isConnected();
        haveTriOutputs = outputs[LFO_TRI_OUTPUT].isConnected();
        haveSawOutputs = outputs[LFO_SAW_OUTPUT].isConnected();
        haveSinOutputs = outputs[LFO_SIN_OUTPUT].isConnected();
        haveOutputs = haveSqrOutputs || haveTriOutputs || haveSawOutputs || haveSinOutputs;

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

            // Get range values
            waveScale = params[RNGKNOB_PARAM].getValue();
            if (inputs[RNG_INPUT].isConnected())
                waveScale += params[RNG_TRIM_PARAM].getValue() *
                (inputs[RNG_INPUT].getVoltage(0.f));
            float waveOffset = (params[RANGE_TOGGLE_PARAM].getValue() < 0.5f)
                ? -waveScale / 2.f
                : 0.f;

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
            for (int c = 0; c < channels; c++) {
                // Handle phase input
                float phaseVoltage = (inputs[PHASE_INPUT].isConnected()) 
                    ? inputs[PHASE_INPUT].getVoltage(c)
                    : phaseMin;
                if (phaseVoltage < phaseMin || phaseVoltage > phaseMax) {
                    if (!clipMode) { // wrap mode
                        const float phaseIn = phaseVoltage; // preserve original
                        float wrapped = std::fmod(phaseIn - phaseMin, phaseRange);
                        if (wrapped < 0.f)
                            wrapped += phaseRange;
                        phaseVoltage = phaseMin + wrapped;

                        // Ensure +15 with a -5 to +5 range outputs as +5 and not -5:
                        constexpr float eps = 1e-6f;
                        if (wrapped <= eps && phaseIn > phaseMax)
                            phaseVoltage = phaseMax;
                    }
                    phaseVoltage = clamp(phaseVoltage, phaseMin, phaseMax);
                }

                // convert voltage-phase to normalized phase (0-1)
                float normPhase = (bipolarPhase) 
                    ? (phaseVoltage + 5.f) * 0.1f 
                    : phaseVoltage * 0.1f;                    
                    normPhase = clamp(normPhase, 0.f, 1.f);

                // Calculate phase to use, based on PWM
                float pwmPhase = normPhase; // PWM-phase (always used for square-waveform)
                if (pwm != 0.5f) {
                    float revPwm = 1.f - pwm;
                    pwmPhase = (normPhase < pwm)
                        ? (normPhase / pwm) * 0.5f
                        : ((normPhase - pwm) / revPwm) * 0.5f + 0.5f;
                }
                float calcPhase = (pwmMode.act == ppm_allWaveforms)
                    ? pwmPhase
                    : normPhase;
                
                // Output Square
                float output; // will be set for each output in use
                if (haveSqrOutputs) {
                    output = (normPhase < pwm)
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
                if (haveSinOutputs) {
                    output = modNorm(bSin(calcPhase) * invSign, modValue) * waveScale + waveOffset;
                    output = clipToVoltRange(output, outClipRange.act);
                    outputs[LFO_SIN_OUTPUT].setVoltage(output, c);
                }
            }
        }

        cycle256++;
    }
};

struct PhaseDrivenLFOModuleWidget : InfNoiseModuleWidget {
    PhaseDrivenLFOModuleWidget(PhaseDrivenLFOModule *module) {
        initializeWidget(module, "res/PhaseDrivenLFO");

        const float leftPortCol = 14.810f;
        const float rightPortCol = 43.545;
        const float centerCol = 30.f;

        // Phase
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftPortCol, 51.072f), module, PhaseDrivenLFOModule::PHASE_INPUT));
        addParam(createParamCentered<CKSS>(Vec(rightPortCol, 51.072f), module, PhaseDrivenLFOModule::PHASE_RANGE_PARAM));
        addParam(createParamCentered<CKSS>(Vec(leftPortCol, 85.438f), module, PhaseDrivenLFOModule::PHASE_EXCD_PARAM));
        
        // Range
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(leftPortCol, 123.636f), module, PhaseDrivenLFOModule::RNGKNOB_PARAM));
        addParam(createParamCentered<CKSS>(Vec(rightPortCol, 123.636f), module, PhaseDrivenLFOModule::RANGE_TOGGLE_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(leftPortCol, 147.759f), module, PhaseDrivenLFOModule::RNG_INPUT));
        addParam(createParamCentered<Trimpot>(Vec(rightPortCol, 147.759f), module, PhaseDrivenLFOModule::RNG_TRIM_PARAM));

        // PWM
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, 181.256f), module, PhaseDrivenLFOModule::PWMKNOB_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(leftPortCol, 201.343f), module, PhaseDrivenLFOModule::PWM_INPUT));
        addParam(createParamCentered<Trimpot>(Vec(rightPortCol, 201.343f), module, PhaseDrivenLFOModule::PWM_TRIM_PARAM));

        // MOD
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, 235.320f), module, PhaseDrivenLFOModule::MODKNOB_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(leftPortCol, 255.407f), module, PhaseDrivenLFOModule::MOD_INPUT));
        addParam(createParamCentered<Trimpot>(Vec(rightPortCol, 255.407f), module, PhaseDrivenLFOModule::MOD_TRIM_PARAM));

        // Inverted waveforms - lights
        addChild(createLightCentered<TinyLight<RedLight>>(Vec(centerCol, 282.369f), module, PhaseDrivenLFOModule::INVERTED_LIGHT));

        // Waveoutputs
        float row = 296.194f;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftPortCol, row), module, PhaseDrivenLFOModule::LFO_SAW_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightPortCol, row), module, PhaseDrivenLFOModule::LFO_TRI_OUTPUT));
        row = 332.694f;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftPortCol, row), module, PhaseDrivenLFOModule::LFO_SQR_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightPortCol, row), module, PhaseDrivenLFOModule::LFO_SIN_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        PhaseDrivenLFOModule* module = dynamic_cast<PhaseDrivenLFOModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexPtrSubmenuItem("Pulse-width mode",
            {"All waveforms", "Square only"},
            &module->pwmMode.req));

        std::vector<std::string> pwmRangeNames = { "1% to 99%", "5% to 95%", "10% to 90%", "15% to 85%", "20% to 80%", "25% to 75%" };
        menu->addChild(createIndexPtrSubmenuItem("Pulse-width range",
            pwmRangeNames, &module->pwmRange.req));

        menu->addChild(createBoolPtrMenuItem("Invert (all) waveforms", "", &module->invWaveforms.req));       
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelPhaseDrivenLFO = createModel<PhaseDrivenLFOModule, PhaseDrivenLFOModuleWidget>("PhaseDrivenLFO");