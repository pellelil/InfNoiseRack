// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct Arm3XYModule : InfNoiseModule {
    enum ParamId {
        ROT1_PARAM,
        ROT2_PARAM,
        ROT3_PARAM,
        ROT_TRIM1_PARAM,
        ROT_TRIM2_PARAM,
        ROT_TRIM3_PARAM,
        LEN1_PARAM,
        LEN2_PARAM,
        LEN3_PARAM,
        LEN_TRIM1_PARAM,
        LEN_TRIM2_PARAM,
        LEN_TRIM3_PARAM,
        RNG_PARAM,
        RNG_TRIM_PARAM,
        MIN_CNTR_MAX_PARAM,
        MIN_CNTR_MAX_TRIM_PARAM,
        MIN_CNTR_MAX_BTN_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        ROT1_INPUT,
        ROT2_INPUT,
        ROT3_INPUT,
        LEN1_INPUT,
        LEN2_INPUT,
        LEN3_INPUT,
        RNG_INPUT,
        MIN_CNTR_MAX_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        X_OUTPUT,
        Y_OUTPUT,
        XY_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        ENUMS(ROT1_MODE_LIGHT, 2),
        ENUMS(ROT2_MODE_LIGHT, 2),
        ENUMS(ROT3_MODE_LIGHT, 2),
        MIN_LIGHT,
        CNTR_LIGHT,
        MAX_LIGHT,
        LIGHTS_LEN
    };

    struct RotArm {  // Rotational-arm where angle is persisted (the rest is calculated)
        int controlIdx = 0; // Arm index, set in constructor (0, 1, or 2)
        float angle = 0.f; // Unified angle (0 to 1)
        float length = 0.f; // Magnitude of arm as polar vector
        float rotVel = 0.f;
        float x = 0.f;
        float y = 0.f;
        float chaosRotFactor = 1.f; // Current rotation-step factor (new each 0° crossing)
        actReqValue<rateChaos> chaosRot = actReqValue<rateChaos>(rc_default);
        float chaosLenFactor = 1.f; // Current length multiplier (new each 0° crossing)
        actReqValue<rateChaos> chaosLen = actReqValue<rateChaos>(rc_default);

        RotArm(int controlIdx)
        {
            this->controlIdx = controlIdx;
        }

        inline void reset() {
            angle = 0.f;
            length = 0.f;
            rotVel = 0.f;
            x = 0.f;
            y = 0.f;
            chaosRotFactor = 1.f;
            chaosRot.setBoth(rc_default);
            chaosLenFactor = 1.f;
            chaosLen.setBoth(rc_default);
		}
    };

    actReqValue<scaleCurve> scaleMode = actReqValue<scaleCurve>(sc_linear);
    bool haveOutputs = false;
    RotArm arms[3] {
        RotArm(0),
		RotArm(1),
		RotArm(2)
	};
    const float maxRotSpeed = 50.f; // Max rotation speed in rotations per second
    float cycleRot = 0.f; // Max rotation per cycle (based on process-quality)
    enum minCntrMaxType { mcm_Min, mcm_Center, mcm_Max };
    actReqValue<minCntrMaxType> minCntrMax = actReqValue<minCntrMaxType>(minCntrMaxType::mcm_Center);
    std::string minCntrMaxTooltip[3] = { "Minimum", "Center", "Maximum" };
    dsp::SchmittTrigger minCntrMaxBtnPress;

	Arm3XYModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        // Arm 1, 2, 3 knobs, trim-pots and inputs
        const float defRotVel[3] = { 0.04f, 0.0f, -0.0f }; // Default rotational-velocities
        const float defLength[3] = { 1.0f, 0.0f, 0.0f }; // Default lengths
        //const float defRotVel[3] = { 0.068f, 0.352f, -0.116f }; // Default rotational-velocities
        //const float defLength[3] = { 1.0f, 0.184f, 0.333f }; // Default lengths
        for (int i = 0; i < 3; i++) {
            int lgtIdx = i * 2;
            configParam(ROT1_PARAM + i, -1.f, 1.f, defRotVel[i], string::f("Arm-%d Rotational velocity", i + 1), " Hz", 0, 50);
            configLight(ROT1_MODE_LIGHT + lgtIdx, string::f("Arm-%d rot. scale-mode (unlit=linear, green=exp, red=log)", i + 1));
            configParam(ROT_TRIM1_PARAM + i, -1.f, 1.f, 0.f, string::f("Arm-%d Rotational velocity trim", i + 1), "%", 0, 100);
            configInput(ROT1_INPUT + i, string::f("Arm-%d -Rotational velocity", i + 1));

            configParam(LEN1_PARAM + i, 0.f, 1.f, defLength[i], string::f("Arm-%d Length", i + 1), "", 0, 1);
            configParam(LEN_TRIM1_PARAM + i, -1.f, 1.f, 0.f, string::f("Arm-%d Length trim", i + 1), "%", 0, 100);
            configInput(LEN1_INPUT + i, string::f("Arm-%d Length", i + 1));
        }

        configParam(RNG_PARAM, 0.f, 10.f, 10.f, "Range", "", 0, 1);
        configParam(RNG_TRIM_PARAM, -1.f, 1.f, 0.f, "Range trim", "%", 0, 100);
        configInput(RNG_INPUT, "Range");

        configLight(MIN_LIGHT, "Minimum when lit");
        configLight(CNTR_LIGHT, "Center when lit");
        configLight(MAX_LIGHT, "Maximum when lit");
        configSwitch(MIN_CNTR_MAX_BTN_PARAM, 0.0f, 2.0f, 1.0f, "Min/Center/Max-mode", { "Min", "Center", "Max" });
        configParam(MIN_CNTR_MAX_PARAM, -10.f, 10.f, 0.f, "Center (-10V to 10V)", " v", 0, 1);
        configParam(MIN_CNTR_MAX_TRIM_PARAM, -1.f, 1.f, 0.f, "Center CV-trim", "%", 0, 100);
        configInput(MIN_CNTR_MAX_INPUT, "Center-CV");

        getParamQuantity(RNG_PARAM)->randomizeEnabled = false;
        getParamQuantity(MIN_CNTR_MAX_PARAM)->randomizeEnabled = false;

        configOutput(X_OUTPUT, "X");
        configOutput(Y_OUTPUT, "Y");
        configOutput(XY_OUTPUT, "XY (mix)");

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
		haveAutoProcQuality = true;
        haveOutQuantize = false;
        haveOutClipRange = true;  
		haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = false;
		haveTrigHighLow = false;
        autoProcQuality.setBoth(true);
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);

        minCntrMax.setBoth(minCntrMaxType::mcm_Center);
        params[MIN_CNTR_MAX_BTN_PARAM].setValue(1.f);
        minCntrMaxBtnPress.reset();
        scaleMode.setBoth(sc_linear);
        arms[0].reset();
        arms[1].reset();
        arms[2].reset();
    }

    // 80% off; 30% random rc_5..rc_50 (5%–50% chaos)
    static rateChaos randomizeArmChaos() {
        if (random::uniform() >= 0.3f)
            return rc_0;
        int level = 1 + (int)(random::uniform() * 10.f); // rc_5 .. rc_50
        if (level > 10)
            level = 10;
        return (rateChaos)level;
    }

    void onRandomize(const RandomizeEvent& e) override {
        InfNoiseModule::onRandomize(e);

        params[RNG_PARAM].setValue(getParamQuantity(RNG_PARAM)->getDefaultValue());
        params[MIN_CNTR_MAX_PARAM].setValue(getParamQuantity(MIN_CNTR_MAX_PARAM)->getDefaultValue());
        minCntrMax.setBoth(minCntrMaxType::mcm_Center);

        for (int i = 0; i < 3; i++) {
            arms[i].chaosRot.setBoth(randomizeArmChaos());
            arms[i].chaosLen.setBoth(randomizeArmChaos());
        }
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        minCntrMax.setBoth((minCntrMaxType)getJsonInt(rootJ, "minCntrMax", (int)minCntrMaxType::mcm_Center));
        scaleMode.setBoth((scaleCurve)getJsonInt(rootJ, "scaleMode", (int)sc_linear));
        float armAngles[3];
        getJsonFloatArray(rootJ, "armAngles", armAngles, 3, 0.f);
        int armChaosRotTmp[3];
        getJsonIntArray(rootJ, "armChaosRot", armChaosRotTmp, 3, (int)rc_default);
        float armChaosRotFactor[3];
        getJsonFloatArray(rootJ, "armChaosRotFactor", armChaosRotFactor, 3, 1.f);
        int armChaosLenTmp[3];
        getJsonIntArray(rootJ, "armChaosLen", armChaosLenTmp, 3, (int)rc_default);
        float armChaosLenFactor[3];
        getJsonFloatArray(rootJ, "armChaosLenFactor", armChaosLenFactor, 3, 1.f);
        for (int i = 0; i < 3; i++) {
            arms[i].angle = armAngles[i];
            arms[i].chaosRot.setBoth((rateChaos)armChaosRotTmp[i]);
            arms[i].chaosRotFactor = armChaosRotFactor[i];
            arms[i].chaosLen.setBoth((rateChaos)armChaosLenTmp[i]);
            arms[i].chaosLenFactor = armChaosLenFactor[i];
        }
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "minCntrMax", json_integer((int)minCntrMax.req));
        json_object_set_new(rootJ, "scaleMode", json_integer((int)scaleMode.req));
        float armAngles[3];
        int armChaosRotTmp[3];
        float armChaosRotFactor[3];
        int armChaosLenTmp[3];
        float armChaosLenFactor[3];
        for (int i = 0; i < 3; i++) {
            armAngles[i] = arms[i].angle;
            armChaosRotTmp[i] = (int)arms[i].chaosRot.req;
            armChaosRotFactor[i] = arms[i].chaosRotFactor;
            armChaosLenTmp[i] = (int)arms[i].chaosLen.req;
            armChaosLenFactor[i] = arms[i].chaosLenFactor;
        }
        setJsonFloatArray(rootJ, "armAngles", armAngles, 3);
        setJsonIntArray(rootJ, "armChaosRot", armChaosRotTmp, 3);
        setJsonFloatArray(rootJ, "armChaosRotFactor", armChaosRotFactor, 3);
        setJsonIntArray(rootJ, "armChaosLen", armChaosLenTmp, 3);
        setJsonFloatArray(rootJ, "armChaosLenFactor", armChaosLenFactor, 3);
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        // Handle min/cntr/max mode-change (update lights and param/input-names)
        // Button idles at 1 (center), so map to 0-based before edge detection.
        if (minCntrMaxBtnPress.process(params[MIN_CNTR_MAX_BTN_PARAM].getValue() - 1.f, 0.5f, 0.1f)) {
            minCntrMax.setBoth(minCntrMax.act == mcm_Max ? mcm_Min : static_cast<minCntrMaxType>(minCntrMax.act + 1));
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

        if (scaleMode.needsUpdate()) {
            scaleMode.updateActual();
            setScaleModeLight(this, ROT1_MODE_LIGHT, scaleMode.act);
            setScaleModeLight(this, ROT2_MODE_LIGHT, scaleMode.act);
            setScaleModeLight(this, ROT3_MODE_LIGHT, scaleMode.act);
        }

        for (int i = 0; i < 3; i++) {
            arms[i].chaosRot.updateActual();
            arms[i].chaosLen.updateActual();
        }

        haveOutputs = outputs[X_OUTPUT].isConnected() || outputs[Y_OUTPUT].isConnected() || outputs[XY_OUTPUT].isConnected();

        // Set Auto process-quality
        float sampleRate = safeSampleRate(args.sampleRate);
        if (autoProcQuality.act) {
            if (haveOutputs) {
                bool haveInputs = inputs[RNG_INPUT].isConnected() || inputs[MIN_CNTR_MAX_INPUT].isConnected() ||
                    inputs[ROT1_INPUT].isConnected() || inputs[ROT2_INPUT].isConnected() || inputs[ROT3_INPUT].isConnected() ||
                    inputs[LEN1_INPUT].isConnected() || inputs[LEN2_INPUT].isConnected() || inputs[LEN3_INPUT].isConnected();
                if (haveInputs)
                    procQuality.setBoth(pq_audioRate, false);
                else
                {
                    float maxFreq = 0.f;
                    for (int i = 0; i < 3; i++) {
                        if (params[LEN1_PARAM + i].getValue() > 0.f)
                            maxFreq = std::max(maxFreq, fabsf(params[ROT1_PARAM + i].getValue()) * maxRotSpeed
                                * rateChaosMaxFactor[arms[i].chaosRot.act]);
                    }
                    procQuality.setBoth(getEstimatedLfoProcessQuality(sampleRate, maxFreq), false);
                }
            }
            else
                procQuality.setBoth(pq_veryLowRate, false); // No outputs
        }

        cycleRot = processQualityCycles[procQuality.act] * maxRotSpeed / sampleRate;

        //--------------------
        postProcessParams(args);
    }

    float processArm(RotArm& arm) {
        // Get- and scale rotational-velocity
        arm.rotVel = params[ROT1_PARAM + arm.controlIdx].getValue();
        if (inputs[ROT1_INPUT + arm.controlIdx].isConnected()) {
            arm.rotVel += (inputs[ROT1_INPUT + arm.controlIdx].getVoltage() / 5.f) * params[ROT_TRIM1_PARAM + arm.controlIdx].getValue();
            arm.rotVel = clamp(arm.rotVel, -1.f, 1.f); // Can't be outside -1 to +1
        }
        arm.rotVel = applyScaleCurveSigned(arm.rotVel, scaleMode.act);

        // Perform cycle-rotation based on rotVel (update angle)
        if (fabs(arm.rotVel) > 1e-8) {
            arm.angle += cycleRot * arm.rotVel * arm.chaosRotFactor;
            if (arm.angle < 0.f) {
                arm.angle += 1.f;
                arm.chaosRotFactor = rateChaosFactor(rateChaosValues[arm.chaosRot.act]);
                arm.chaosLenFactor = rateChaosFactor(rateChaosValues[arm.chaosLen.act]);
            } else if (arm.angle >= 1.f) {
                arm.angle -= std::truncf(arm.angle);
                arm.chaosRotFactor = rateChaosFactor(rateChaosValues[arm.chaosRot.act]);
                arm.chaosLenFactor = rateChaosFactor(rateChaosValues[arm.chaosLen.act]);
            }
        }

        // Get length-setting (Knob + CV-input), then apply length chaos
        float baseLength = params[LEN1_PARAM + arm.controlIdx].getValue();
        if (inputs[LEN1_INPUT + arm.controlIdx].isConnected()) {
            baseLength += (inputs[LEN1_INPUT + arm.controlIdx].getVoltage() / 10.f) * params[LEN_TRIM1_PARAM + arm.controlIdx].getValue();
            baseLength = clamp(baseLength, 0.f, 10.f); // Can't be negative
        }

        // Angle-weight length chaos: 0% at 0°/360°, 100% at 180° (avoids abrupt jumps at 0° crossings)
        float chaosLenScale = 1.f - fabsf(2.f * arm.angle - 1.f);
        float effectiveChaosLenFactor = 1.f + chaosLenScale * (arm.chaosLenFactor - 1.f);
        arm.length = baseLength * effectiveChaosLenFactor;
        if (arm.length < 1e-5) {  // Length < 1e-5 treaded as zero
            arm.length = 0.f;
            arm.x = 0.f;
            arm.y = 0.f;
            return arm.length;
        }

        // Calculate X and Y according to angle and length
        arm.x = arm.length * bSin(arm.angle + 0.25f); // Consine is 90 degrees "ahead" of sine
        arm.y = arm.length * bSin(arm.angle);
        return arm.length;
    }

    void process(const ProcessArgs& args) override {
        bool doProcessParams = mustProcessParams || 
            ((cycle256 & patternProcessParams) == patternProcessParams);
        if (doProcessParams)
            processParams(args);

        bool doProcess = (doProcessParams ||
            ((cycle256 & processQualityPatterns[procQuality.act]) == processQualityPatterns[procQuality.act]));

        if (doProcess && haveOutputs) {
            // Get range and min/center/max values (knob + trim * CV-input)
            float rangeVolt = params[RNG_PARAM].getValue();
            if (inputs[RNG_INPUT].isConnected()) {
                rangeVolt += params[RNG_TRIM_PARAM].getValue() * inputs[RNG_INPUT].getVoltage();
                rangeVolt = clamp(rangeVolt, 0.f, 20.f);
            }

            float minCntrMaxVolt = params[MIN_CNTR_MAX_PARAM].getValue();
            if (inputs[MIN_CNTR_MAX_INPUT].isConnected())
                minCntrMaxVolt += params[MIN_CNTR_MAX_TRIM_PARAM].getValue() * inputs[MIN_CNTR_MAX_INPUT].getVoltage();

            float minValue, centerValue, maxValue;
            if (minCntrMax.act == mcm_Min) {
                minValue = minCntrMaxVolt;
                maxValue = minCntrMaxVolt + rangeVolt;
                centerValue = minCntrMaxVolt + rangeVolt / 2.f;
            }
            else if (minCntrMax.act == mcm_Center) {
                float halfRange = rangeVolt / 2.f;
                centerValue = minCntrMaxVolt;
                minValue = minCntrMaxVolt - halfRange;
                maxValue = minCntrMaxVolt + halfRange;
            }
            else { // mcm_Max
                maxValue = minCntrMaxVolt;
                minValue = minCntrMaxVolt - rangeVolt;
                centerValue = minCntrMaxVolt - rangeVolt / 2.f;
            }
            float halfRange = (maxValue - minValue) / 2.f;

            // Process the 3 arms (add as polar vectors)
            float x=0.f, y=0.f;
            float radius = 0.f;
            for (int i = 0; i < 3; i++)
            {
				radius += processArm(arms[i]);
				x += arms[i].x;
				y += arms[i].y;
			}

            // Unify X and Y (-1 to +1)
            if (radius > 0.f)
            {
				x /= radius;
				y /= radius;
			}

            // Scale and output X and Y
            x = centerValue + x * halfRange;
            outputs[X_OUTPUT].setVoltage(clipToVoltRange(x, outClipRange.act));

            y = centerValue + y * halfRange;
            outputs[Y_OUTPUT].setVoltage(clipToVoltRange(y, outClipRange.act));

            // Output averaging mix of X and Y
            outputs[XY_OUTPUT].setVoltage(clipToVoltRange((x + y) / 2.f, outClipRange.act));
        }

        cycle256++;
    }
};

struct Arm3XYModuleWidget : InfNoiseModuleWidget {
    Arm3XYModuleWidget(Arm3XYModule *module) {
        initializeWidget(module, "res/Arm3XY");

        // Arm 1, 2, 3 knob, trim-pot and input
        const float knobClm = 18.128f;
        const float modeLgtClm = 31.018f;
        const float modeLgtOffset = -12.091f;
        const float trimClm = 60.731f;
        const float inputClm = 100.848f;
        const float rowSpacing = 35.409f;
        float row = 50.679f;
        for (int i = 0; i < 3; i++) {
            // Rotational-velocity
            addParam(createParamCentered<RoundBlackKnob>(Vec(knobClm, row), module, Arm3XYModule::ROT1_PARAM + i));
            addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(modeLgtClm, row + modeLgtOffset), module, Arm3XYModule::ROT1_MODE_LIGHT + i));
            addParam(createParamCentered<Trimpot>(Vec(trimClm, row), module, Arm3XYModule::ROT_TRIM1_PARAM + i));
            addInput(createInputCentered<ThemedPJ301MPort>(Vec(inputClm, row), module, Arm3XYModule::ROT1_INPUT + i));
            row += rowSpacing;

            // Length
            addParam(createParamCentered<RoundBlackKnob>(Vec(knobClm, row), module, Arm3XYModule::LEN1_PARAM + i));
            addParam(createParamCentered<Trimpot>(Vec(trimClm, row), module, Arm3XYModule::LEN_TRIM1_PARAM + i));
            addInput(createInputCentered<ThemedPJ301MPort>(Vec(inputClm, row), module, Arm3XYModule::LEN1_INPUT + i));
            row += rowSpacing;
        }

        // Range knob, trim-pot and input
        addParam(createParamCentered<RoundBlackKnob>(Vec(knobClm, row), module, Arm3XYModule::RNG_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(trimClm, row), module, Arm3XYModule::RNG_TRIM_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(inputClm, row), module, Arm3XYModule::RNG_INPUT));
        row += rowSpacing;

        // Min/Center/Max knob, trim-pot and input
        addParam(createParamCentered<RoundBlackKnob>(Vec(knobClm, row), module, Arm3XYModule::MIN_CNTR_MAX_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(trimClm, row), module, Arm3XYModule::MIN_CNTR_MAX_TRIM_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(inputClm, row), module, Arm3XYModule::MIN_CNTR_MAX_INPUT));

        // Min/Center/Max toggle-button
        infNoiseLtSmallButton* mnCnMxBtn = createParamCentered<infNoiseLtSmallButton>(Vec(89.472f, 285.633f), module, Arm3XYModule::MIN_CNTR_MAX_BTN_PARAM);
        mnCnMxBtn->setup(bc_green, true);
        addParam(mnCnMxBtn);

        // Min/Center/Max lights
        const float lightRow = 279.757f;
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(42.648f, lightRow), module, Arm3XYModule::MIN_LIGHT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(60.632f, lightRow), module, Arm3XYModule::CNTR_LIGHT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(78.615f, lightRow), module, Arm3XYModule::MAX_LIGHT));

        // X/Y/XY outputs
        const float outputRow = 333.801f;
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(45.299f, outputRow), module, Arm3XYModule::X_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(73.701f, outputRow), module, Arm3XYModule::Y_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(100.848f, outputRow), module, Arm3XYModule::XY_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        Arm3XYModule* module = dynamic_cast<Arm3XYModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexPtrSubmenuItem("Rot.vel. scale-mode", getScaleCurveMenuNames(),
            &module->scaleMode.req));

        std::vector<std::string> rateChaosNames = getRateChaosNames();
        for (int i = 0; i < 3; i++) {
            menu->addChild(createIndexPtrSubmenuItem(string::f("Arm-%d rot. chaos", i + 1),
                rateChaosNames, &module->arms[i].chaosRot.req));
            menu->addChild(createIndexPtrSubmenuItem(string::f("Arm-%d length chaos", i + 1),
                rateChaosNames, &module->arms[i].chaosLen.req));
        }
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelArm3XY = createModel<Arm3XYModule, Arm3XYModuleWidget>("Arm3XY");