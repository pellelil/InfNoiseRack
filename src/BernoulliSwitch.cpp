// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct BernoulliSwitchModule : InfNoiseModule {
    enum ParamId {
        PROB_MODE_PARAM,
        PROB_PARAM,
        PROB_TRIM_PARAM,
        CLOCK_FREQ_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        PROB_INPUT,
        CLOCK_INPUT,
        A_INPUT,
        B_INPUT,
        AB_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        AB_OUTPUT,
        A_OUTPUT,
        B_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        A_IN_LIGHT,
        B_IN_LIGHT,
        A_OUT_LIGHT,
        B_OUT_LIGHT,
        LIGHTS_LEN
    };

    bool haveInOutputs = false;  // "A/B ->" output is connected
    bool haveOutOutputs = false; // "-> A/B" outputs are connected
    bool haveOutputs = false; // "A/B ->" or "-> A/B" outputs are connected
    bool bSelected[2] = { false, false };  // B selected in sections (0="A/B ->", 1="-> A/B")
    int channels[2] = { 1, 1 };  // channels in sections (0="A/B ->", 1="-> A/B")
    actReqValue<voltValue> normAInVolt = actReqValue<voltValue>(v_p10);  // Normalized input voltage for A
    actReqValue<voltValue> normBInVolt = actReqValue<voltValue>(v_zero);  // Normalized input voltage for B
    actReqValue<voltValue> nonSlctOutVolt = actReqValue<voltValue>(v_zero);  // Voltage for non-selected output
    actReqValue<bool> useLastNonSlctOut = actReqValue<bool>(false);  // If true, non-selected output uses last A/B-output
    enum sectionModeType { sm_Same, sm_Individual };
    actReqValue<sectionModeType> sectionMode = actReqValue<sectionModeType>(sm_Same);  // Same selection for both sections or individual
    bool probModeSwitch = false;
    dsp::SchmittTrigger propTrigger;
    actReqValue<rateChaos> clockRateChaos = actReqValue<rateChaos>(rc_default);
    float chaosAmount = 0.f; // Cached rate-chaos amount (0-1)
    float chaosFactor = 1.f; // Clock phase-step factor (new each clock-cycle)
    float clockPhaseStep = 0.f;
    float clockPhase = 0.f;
    float lastAOut[PORT_MAX_CHANNELS] = { 0.f };
    float lastBOut[PORT_MAX_CHANNELS] = { 0.f };

	BernoulliSwitchModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configSwitch(PROB_MODE_PARAM, 0.0f, 1.0f, 0.0f, "Probability mode", { "Probability(B)", "Switch" });
        configParam(PROB_PARAM, 0.f, 1.f, 0.5f, "Probability", " %", 0, 100);
        configParam(PROB_TRIM_PARAM, -1.f, 1.f, 0.f, "Probability CV trim", " %", 0, 100);
        configInput(PROB_INPUT, "Probability-CV");

        configParam<infNoiseLfoFreqQnt>(CLOCK_FREQ_PARAM, -8.f, 10.f, 1.f, "Clock frequency (when no clock-input)", " Hz", 2, 1);
        configInput(CLOCK_INPUT, "Clock- or switch-trigger if prob-light lit");

        configInput(A_INPUT, "A-CV (normalized via context-menu)");
        configInput(B_INPUT, "B-CV (normalized via context-menu)");
        configOutput(AB_OUTPUT, "A- or B-CV");
        configLight(A_IN_LIGHT, "A-input is active if lit");
        configLight(B_IN_LIGHT, "B-input is active if lit");

        configInput(AB_INPUT, "A/B-CV (normalized to clock)");
        configOutput(A_OUTPUT, "A-CV");
        configOutput(B_OUTPUT, "B-CV");
        configLight(A_OUT_LIGHT, "A-output is active if lit");
        configLight(B_OUT_LIGHT, "B-output is active if lit");

        configBypass(A_INPUT, AB_OUTPUT);
        configBypass(AB_INPUT, A_OUTPUT);

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
		haveAutoProcQuality = true;
        haveOutQuantize = false;
        haveOutClipRange = true;  
		haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = true;
		haveTrigHighLow = false;
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        
        clockPhase = 0.f;
        clockRateChaos.setBoth(rc_default);
        chaosAmount = 0.f;
        chaosFactor = 1.f;

        propTrigger.reset();
        bSelected[0] = false;
        bSelected[1] = false;
        sectionMode.setBoth(sectionModeType::sm_Same);
        normAInVolt.setBoth(v_p10);
        normBInVolt.setBoth(v_zero);
        nonSlctOutVolt.setBoth(v_zero);
        useLastNonSlctOut.setBoth(false);
        for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
            lastAOut[c] = 0.f;
            lastBOut[c] = 0.f;
        }
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        sectionMode.setBoth((sectionModeType)getJsonInt(rootJ, "sectionMode", (int)sectionModeType::sm_Same));
        normAInVolt.setBoth((voltValue)getJsonInt(rootJ, "normAInVolt", (int)v_p10));
        normBInVolt.setBoth((voltValue)getJsonInt(rootJ, "normBInVolt", (int)v_zero));
        nonSlctOutVolt.setBoth((voltValue)getJsonInt(rootJ, "nonSlctOutVolt", (int)v_zero));
        useLastNonSlctOut.setBoth(getJsonBool(rootJ, "useLastNonSlctOut", false));
        clockRateChaos.setBoth((rateChaos)getJsonInt(rootJ, "clockRateChaos", (int)rc_default));
        getJsonFloatArray(rootJ, "lastAOut", lastAOut, PORT_MAX_CHANNELS, voltValues[normAInVolt.req]);
        getJsonFloatArray(rootJ, "lastBOut", lastBOut, PORT_MAX_CHANNELS, voltValues[normBInVolt.req]);
        getJsonBoolArray(rootJ, "bSelected", bSelected, 2, false);
        clockPhase = getJsonFloat(rootJ, "clockPhase", 0.f);
        if (clockPhase >= 1.f)
            clockPhase -= std::truncf(clockPhase);
        chaosFactor = getJsonFloat(rootJ, "chaosFactor", 1.f);
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "sectionMode", json_integer((int)sectionMode.req));
        json_object_set_new(rootJ, "normAInVolt", json_integer((int)normAInVolt.req));
        json_object_set_new(rootJ, "normBInVolt", json_integer((int)normBInVolt.req));
        json_object_set_new(rootJ, "nonSlctOutVolt", json_integer((int)nonSlctOutVolt.req));
        json_object_set_new(rootJ, "useLastNonSlctOut", json_boolean(useLastNonSlctOut.req));
        json_object_set_new(rootJ, "clockRateChaos", json_integer((int)clockRateChaos.req));
        setJsonFloatArray(rootJ, "lastAOut", lastAOut, PORT_MAX_CHANNELS);
        setJsonFloatArray(rootJ, "lastBOut", lastBOut, PORT_MAX_CHANNELS);
        setJsonBoolArray(rootJ, "bSelected", bSelected, 2);
        json_object_set_new(rootJ, "clockPhase", json_real(clockPhase));
        json_object_set_new(rootJ, "chaosFactor", json_real(chaosFactor));
    }

    /// @brief Determines if B should be selected.
    /// probModeSwitch: selection of B toggles with probability probabilityB (0=never, 1=always).
    /// !probModeSwitch: B is selected with probability P(B)=probabilityB.
    /// @param propB probability of B (0-1)
    /// @param lastUseB Last selection of B
    /// @return Returns true if B should be selected
    inline bool useB(float propB, bool lastUseB) {
        if (probModeSwitch) {
            if (propB == 0.f) return lastUseB;
            if (propB == 1.f) return !lastUseB;
            return (randomNorm() < propB) ? !lastUseB : lastUseB;
        }

        return (propB == 0.f)
            ? false
            : (propB == 1.f || randomNorm() < propB);
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        probModeSwitch = params[PROB_MODE_PARAM].getValue() > 0.5f;

        sectionMode.updateActual();
        nonSlctOutVolt.updateActual();
        useLastNonSlctOut.updateActual();
        clockRateChaos.updateActual();
        chaosAmount = rateChaosValues[clockRateChaos.act];
        if (normAInVolt.needsUpdate()) {
            normAInVolt.updateActual();
            if (inputInfos.size() > (unsigned)A_INPUT && inputInfos[A_INPUT]) {
                inputInfos[A_INPUT]->name = polyPortPrefix() + "A-CV (normalized via menu to: " + getVoltName(normAInVolt.act) + ")";
            }
        }
        if (normBInVolt.needsUpdate()) {
            normBInVolt.updateActual();
            if (inputInfos.size() > (unsigned)B_INPUT && inputInfos[B_INPUT]) {
                inputInfos[B_INPUT]->name = polyPortPrefix() + "B-CV (normalized via menu to: " + getVoltName(normBInVolt.act) + ")";
            }
        }

        haveInOutputs = outputs[AB_OUTPUT].isConnected();
        haveOutOutputs = outputs[A_OUTPUT].isConnected() || outputs[B_OUTPUT].isConnected();
        haveOutputs = haveInOutputs || haveOutOutputs;

        // Handle auto-quality
        if (autoProcQuality.act) {
            if (haveOutputs || inputs[CLOCK_INPUT].isConnected()) {
                procQuality.setBoth(pq_audioRate, false);
            }
            else {
                procQuality.setBoth(pq_balancedRate, false); // Lets the toggle-lights blink
            }
        }
        
        // Get clock frequency
        float sampleRate = safeSampleRate(args.sampleRate);
        float clockFreq = 2.f;  // 2 Hz
        float pitch = params[CLOCK_FREQ_PARAM].getValue();
        clockFreq = clockFreq / 2.f * dsp::exp2_taylor5(pitch);
        float cycleStep = processQualityCycles[procQuality.act];
        clockPhaseStep = (clockFreq * cycleStep) / sampleRate;
        if (clockPhase >= 1.f)
            clockPhase -= 1.f;

        // set channels "A/B ->" section
        channels[0] = 1;
        if (inputs[A_INPUT].isConnected())
            channels[0] = std::max(channels[0], inputs[A_INPUT].getChannels());
        if (inputs[B_INPUT].isConnected())
            channels[0] = std::max(channels[0], inputs[B_INPUT].getChannels());
        outputs[AB_OUTPUT].setChannels(channels[0]);
        
        // set channels "-> A/B" section
        channels[1] = inputs[AB_INPUT].isConnected()
            ? inputs[AB_INPUT].getChannels()
            : inputs[CLOCK_INPUT].isConnected()
				? inputs[CLOCK_INPUT].getChannels()
				: 1;
        outputs[A_OUTPUT].setChannels(channels[1]);
        outputs[B_OUTPUT].setChannels(channels[1]);

        // Update/set lights
        lights[A_IN_LIGHT].setBrightness(bSelected[0] ? 0.f : 1.f);
        lights[B_IN_LIGHT].setBrightness(bSelected[0] ? 1.f : 0.f);
        lights[A_OUT_LIGHT].setBrightness(bSelected[1] ? 0.f : 1.f);
        lights[B_OUT_LIGHT].setBrightness(bSelected[1] ? 1.f : 0.f);

        if (wasJustLoaded) {
            propTrigger.reset();
            if (!inputs[CLOCK_INPUT].isConnected()) {
                float v = clockPhase < 0.5f ? 10.f : 0.f;
                propTrigger.process(v,
                    trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act]);
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

        if (doProcess) {
            // Determine if "coin should be flipped", based on clock-input/knob
            float clockVoltage;
            if (inputs[CLOCK_INPUT].isConnected()) {
                clockVoltage = inputs[CLOCK_INPUT].getVoltage();
			} else {
                clockVoltage = clockPhase < 0.5f ? 10.f : 0.f;
                clockPhase += clockPhaseStep * chaosFactor;
                if (clockPhase >= 1.f) {
                    clockPhase -= std::truncf(clockPhase);  // Remove integer part
                    chaosFactor = rateChaosFactor(chaosAmount); // New factor each clock-cycle
                }
            }
            bool flipCoin = propTrigger.process(clockVoltage,
                trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act]);

            // Deside if A or B should be selected (both sections)
            if (flipCoin) {
                float propB = params[PROB_PARAM].getValue();  // 0..1
                if (inputs[PROB_INPUT].isConnected()) {
                    propB += inputs[PROB_INPUT].getVoltage() / 10.f * params[PROB_TRIM_PARAM].getValue();
                }
                propB = clamp(propB, 0.f, 1.f);
                
                // Update B-selection for both sections
                bSelected[0] = useB(propB, bSelected[0]);
                bSelected[1] = (sectionMode.act == sm_Same)
                    ? bSelected[0]
                    : useB(propB, bSelected[1]);
            }

            if (haveOutputs) {
                float voltage;
                // "A/B ->" section
                if (haveInOutputs) {
                    int inpIdx = bSelected[0] ? B_INPUT : A_INPUT; // which input to use
                    float normVoltage = bSelected[0]  // normalized input voltage   
                        ? voltValues[normBInVolt.act] 
                        : voltValues[normAInVolt.act];
                    bool haveInput = inputs[inpIdx].isConnected();
                    for (int c = 0; c < channels[0]; c++) {
                        voltage = (haveInput)
                            ? inputs[inpIdx].getPolyVoltage(c)
                            : normVoltage; 
                        voltage = clipToVoltRange(voltage, outClipRange.act);
                        outputs[AB_OUTPUT].setVoltage(voltage, c);
                    }
                }

                // "-> A/B" section
                if (haveOutOutputs) {
                    float inpVoltage = clockVoltage;  // Fallback normalized voltage
                    bool haveInput = inputs[AB_INPUT].isConnected();
                    for (int c=0; c<channels[1]; c++) {
                        if (haveInput)
                            inpVoltage= inputs[AB_INPUT].getVoltage(c);

                        // A-output (active when B is not selected)
                        voltage = !bSelected[1]
                            ? inpVoltage 
                            : useLastNonSlctOut.act 
                                ? lastAOut[c] 
                                : voltValues[nonSlctOutVolt.act];
                        voltage = clipToVoltRange(voltage, outClipRange.act);
                        outputs[A_OUTPUT].setVoltage(voltage, c);
                        lastAOut[c] = voltage;
                        
                        // B-output (active when B is selected)
                        voltage = bSelected[1] 
                            ? inpVoltage 
                            : useLastNonSlctOut.act 
                                ? lastBOut[c] 
                                : voltValues[nonSlctOutVolt.act];
                        voltage = clipToVoltRange(voltage, outClipRange.act);
                        outputs[B_OUTPUT].setVoltage(voltage, c);
                        lastBOut[c] = voltage;
                    }
                }
            }
        }

        cycle256++;
    }
};

struct BernoulliSwitchModuleWidget : InfNoiseModuleWidget {
    BernoulliSwitchModuleWidget(BernoulliSwitchModule *module) {
        initializeWidget(module, "res/BernoulliSwitch");

        const float cntrCol = 15.f;
        const float lightCol = 25.284f;
        // Probabillity
        addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(
            Vec(25.198f, 34.027f),
            module, BernoulliSwitchModule::PROB_MODE_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrCol, 47.843f), module, BernoulliSwitchModule::PROB_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(cntrCol, 70.656f), module, BernoulliSwitchModule::PROB_TRIM_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrCol, 93.825f), module, BernoulliSwitchModule::PROB_INPUT));

        // Clock
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrCol, 124.679f), module, BernoulliSwitchModule::CLOCK_FREQ_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrCol, 150.477f), module, BernoulliSwitchModule::CLOCK_INPUT));

        // A/B inputs
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(lightCol, 179.612f), module, BernoulliSwitchModule::A_IN_LIGHT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 189.130f), module, BernoulliSwitchModule::A_INPUT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(lightCol, 204.244f), module, BernoulliSwitchModule::B_IN_LIGHT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 213.763f), module, BernoulliSwitchModule::B_INPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 241.223f), module, BernoulliSwitchModule::AB_OUTPUT));

        // A/B outputs
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 280.601f), module, BernoulliSwitchModule::AB_INPUT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(lightCol, 298.542f), module, BernoulliSwitchModule::A_OUT_LIGHT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 308.061f), module, BernoulliSwitchModule::A_OUTPUT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(lightCol, 323.175f), module, BernoulliSwitchModule::B_OUT_LIGHT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 332.694f), module, BernoulliSwitchModule::B_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        BernoulliSwitchModule* module = dynamic_cast<BernoulliSwitchModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexPtrSubmenuItem("Sections: shared or independent A/B",
            { "Same selection for both sections (A/B -> and -> A/B)", "Independent selection per section" },
            &module->sectionMode.req
        ));

        menu->addChild(new MenuSeparator);

        std::vector<std::string> voltNames = getVoltValuesNames();
        menu->addChild(createIndexPtrSubmenuItem("A-normalized input", voltNames,
            &module->normAInVolt.req));
        menu->addChild(createIndexPtrSubmenuItem("B-normalized input", voltNames,
            &module->normBInVolt.req));

        menu->addChild(createIndexPtrSubmenuItem("Non selected A/B-output (fixed level)", voltNames,
                &module->nonSlctOutVolt.req));    
        menu->addChild(createIndexPtrSubmenuItem("Non selected A/B-output (mode)",
            std::vector<std::string>{ "Fixed level", "Last A/B-value" },
            &module->useLastNonSlctOut.req));

        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexPtrSubmenuItem("Clock rate chaos",
            getRateChaosNames(), &module->clockRateChaos.req));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelBernoulliSwitch = createModel<BernoulliSwitchModule, BernoulliSwitchModuleWidget>("BernoulliSwitch");
