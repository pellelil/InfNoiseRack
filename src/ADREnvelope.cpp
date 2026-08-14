// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"
#include <cstdlib>

struct ADREnvelopeModule : InfNoiseModule {
    enum ParamId {
        PHASE_GATE_TRIG_PARAM,
        A_TRIG_BTN_PARAM,
        DR_TRIG_BTN_PARAM,
        A_RETRIG_PARAM,
        D_RETRIG_PARAM,
        DELAY_TIME_PARAM,
        A_TIME_PARAM,
        R_TIME_PARAM,
        A_SHAPE_PARAM,
        R_SHAPE_PARAM,
        A_LEVEL_PARAM,
        R_LEVEL_PARAM,
        TIME_LINK_PARAM,
        SHAPE_LINK_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        PHASE_INPUT,
        A_TRIG_INPUT,
        DR_TRIG_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        BOA_OUTPUT,
        EOA_OUTPUT,
        BOR_OUTPUT,
        EOR_OUTPUT,
        ENVELOPE_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT, 2),
        ENUMS(CLIP_RANGE_LIGHT, 2),
        ENUMS(ATTACK_LIGHT, 2),
        ENUMS(RELEASE_LIGHT, 2),
        LIGHTS_LEN
    };

    // Five sub-phases: Attack/Release each have a ramp and a hold; Delay is hold-only.
    // Default is ap_holdR (idle at R.level, waiting for attack).
    enum adrPhase {
        ap_attack,   // ramping to A.level  (A light green)
        ap_holdA,    // held at A.level     (A light red)
        ap_delay,    // delay hold          (DR light yellow)
        ap_release,  // ramping to R.level  (DR light green)
        ap_holdR,    // held at R.level     (DR light red)
        ap_len
    };
    adrPhase phase = ap_holdR;  // Current/active phase
    float phasePos = 0.f;        // Position within current phase (0 to 1)
    float attackLightGreen[ap_len] = { 1.f, 0.f, 0.f, 0.f, 0.f };
    float attackLightRed[ap_len]   = { 0.f, 1.f, 0.f, 0.f, 0.f };
    float releaseLightGreen[ap_len] = { 0.f, 0.f, 1.f, 1.f, 0.f };
    float releaseLightRed[ap_len]   = { 0.f, 0.f, 1.f, 0.f, 1.f };
    
    float attackTime = 0.f;
    float delayTime = 0.f;
    float releaseTime = 0.f;
    float attackStep = 0.f;   // phasePos increment per process
    float delayStep = 0.f;
    float releaseStep = 0.f;
    float attackShape = 0.f;
    float releaseShape = 0.f;
    float oldAttackShape = 2.f;   // Sentinel: first processParams always rebuilds
    float oldReleaseShape = 2.f;
    Lut1D<256> attackShapeLut;
    Lut1D<256> attackShapeInvLut;
    Lut1D<256> releaseShapeLut;
    Lut1D<256> releaseShapeInvLut;
    float attackLevel = 10.f;
    float releaseLevel = 0.f;
    float deltaLevel = 10.f;  // Abs.diff between A.level and R.level
    float envelope = releaseLevel;  // Current Envelope voltage
    bool attackRetrig = false;
    bool delayRetrig = false;
    bool havePhaseInput = false;
    bool haveAttTrigInput = false;
    bool haveRelTrigInput = false;
    bool phaseTrigMode = false; // false = gate (default), true = trigger
    /// Set in processParams; used by widget overlay (R knobs follow A when linked).
    bool timeLinked = false;
    bool shapeLinked = false;
    bool haveTriggerOutputs = false;
    bool haveEnvelopeOutput = false;
    actReqValue<rateChaos> delayRateChaos = actReqValue<rateChaos>(rc_default);
    actReqValue<rateChaos> attackRateChaos = actReqValue<rateChaos>(rc_default);
    actReqValue<rateChaos> releaseRateChaos = actReqValue<rateChaos>(rc_default);
    dsp::SchmittTrigger phaseTrig;
    dsp::SchmittTrigger attTrig;
    dsp::SchmittTrigger relTrig;
    infNoiseInEdgeDetector phaseGate = infNoiseInEdgeDetector(trueDetectValues[td_gateHigh]);
    static const int BOA = 0;
    static const int EOA = 1;
    static const int BOR = 2;
    static const int EOR = 3;
    infNoiseOutTrigger outTrig[4] = {
        infNoiseOutTrigger(), // BOA
        infNoiseOutTrigger(), // EOA
        infNoiseOutTrigger(), // BOR
        infNoiseOutTrigger()  // EOR
    };
    float attackChaosFactor = 1.f;
    float delayChaosFactor = 1.f;
    float releaseChaosFactor = 1.f;
    float attackChaosAmount = 0.f;
    float delayChaosAmount = 0.f;
    float releaseChaosAmount = 0.f;

	ADREnvelopeModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
        
        configInput(PHASE_INPUT, "Phase (Gate/Trigger)");
        configSwitch(PHASE_GATE_TRIG_PARAM, 0.f, 1.f, 1.f, "Phase-mode", {"Trigger", "Gate"});
        configInput(A_TRIG_INPUT, "Attack Trigger");
        configButton(A_TRIG_BTN_PARAM, "Trig Attack");
        configInput(DR_TRIG_INPUT, "Delay/Release Trigger");
        configButton(DR_TRIG_BTN_PARAM, "Trig Delay/Release");

        configLight(ATTACK_LIGHT, "Attack phase (Green=Attack, Red=Hold)");
        configLight(RELEASE_LIGHT, "Release phase (Yellow=Delay, Green=Release, Red=Hold)");

        configSwitch(A_RETRIG_PARAM, 0.f, 1.f, 0.f, "Attack retrig", {"Disabled", "Enabled"});
        configSwitch(D_RETRIG_PARAM, 0.f, 1.f, 0.f, "Delay retrig", {"Disabled", "Enabled"});

        const float timeBase = 10000.f;
        const float timeMax = 10.f;
        const float timeMult = timeMax / (timeBase - 1.f);
        configParam(A_TIME_PARAM, 0.f, 1.f, 0.f, "Attack time (0 to 10 s)", " s",
            timeBase, timeMult, -timeMult);
        configParam(DELAY_TIME_PARAM, 0.f, 1.f, 0.f, "Delay time (0 to 10 s)", " s",
            timeBase, timeMult, -timeMult);
        configParam(R_TIME_PARAM, 0.f, 1.f, 0.f, "Release time (0 to 10 s)", " s",
            timeBase, timeMult, -timeMult);
        configParam(A_SHAPE_PARAM, -1.f, 1.f, 0.f, "Attack shape (-1 to +1)");
        configParam(R_SHAPE_PARAM, -1.f, 1.f, 0.f, "Release shape (-1 to +1)");
        configParam(A_LEVEL_PARAM, -10.f, 10.f, 10.f, "Attack level (-10V to +10V)", " V");
        configParam(R_LEVEL_PARAM, -10.f, 10.f, 0.f, "Release level (-10V to +10V)", " V");
        configSwitch(TIME_LINK_PARAM, 0.f, 1.f, 0.f, "Time link", {"Disabled", "Enabled"});
        configSwitch(SHAPE_LINK_PARAM, 0.f, 2.f, 0.f, "Shape link", {"Disabled", "Enabled", "Reversed"});

        configOutput(BOA_OUTPUT, "BOA (Begin Of Attack)");
        configOutput(EOA_OUTPUT, "EOA (End Of Attack)");
        configOutput(BOR_OUTPUT, "BOR (Begin Of Release)");
        configOutput(EOR_OUTPUT, "EOR (End Of Release)");
        configOutput(ENVELOPE_OUTPUT, "Envelope");
        
        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
		haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = false;  
		haveGateDetect = true;
		haveGateHighLow = true;
        haveTrigDetect = true;
		haveTrigHighLow = true;

        ensureNormExpLogLuts();
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);

        phase = ap_holdR;
        phasePos = 0.f;
        delayRateChaos.setBoth(rc_default);
        attackRateChaos.setBoth(rc_default);
        releaseRateChaos.setBoth(rc_default);
        delayChaosFactor = 1.f;
        attackChaosFactor = 1.f;
        releaseChaosFactor = 1.f;
        phaseTrig.reset();
        attTrig.reset();
        relTrig.reset();
        phaseGate.reset();
        for (int i = 0; i < 4; i++)
            outTrig[i].reset();
        phaseTrigMode = false;
        envelope = releaseLevel;
        oldAttackShape = 2.f;
        oldReleaseShape = 2.f;
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);

        delayRateChaos.setBoth((rateChaos)getJsonInt(rootJ, "delayRateChaos", (int)rc_default));
        attackRateChaos.setBoth((rateChaos)getJsonInt(rootJ, "attackRateChaos", (int)rc_default));
        releaseRateChaos.setBoth((rateChaos)getJsonInt(rootJ, "releaseRateChaos", (int)rc_default));
        phase = (adrPhase)getJsonInt(rootJ, "phase", (int)ap_holdR);
        phasePos = getJsonFloat(rootJ, "phasePos", 0.f);
        envelope = getJsonFloat(rootJ, "envelope", releaseLevel);
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "delayRateChaos", json_integer((int)delayRateChaos.req));
        json_object_set_new(rootJ, "attackRateChaos", json_integer((int)attackRateChaos.req));
        json_object_set_new(rootJ, "releaseRateChaos", json_integer((int)releaseRateChaos.req));
        json_object_set_new(rootJ, "phase", json_integer((int)phase));
        json_object_set_new(rootJ, "phasePos", json_real(phasePos));
        json_object_set_new(rootJ, "envelope", json_real(envelope));
    }

    float readTimeParam(int paramId, float sampleTime) {
        float t = getParamQuantity(paramId)->getDisplayValue();
        return (t < sampleTime) ? 0.f : t;
    }

    // Mix linear with normExp (shape < 0) or normLog (shape > 0). Fill-time only.
    static float applyShape(float x, float shape) {
        if (shape < 0.f)
            return x + (-shape) * (normExp(x) - x);
        if (shape > 0.f)
            return x + shape * (normLog(x) - x);
        return x;
    }

    // Forward LUT: shaped 0→1 vs phasePos. Inverse: phasePos vs shaped (interrupt).
    static void rebuildShapeLuts(float shape, Lut1D<256>& fwd, Lut1D<256>& inv) {
        const int n = Lut1D<256>::Size;
        for (int i = 0; i < n; i++) {
            float x = (float)i / (float)(n - 1);
            fwd.data[i] = applyShape(x, shape);
        }
        fwd.data[0] = 0.f;
        fwd.data[n - 1] = 1.f;

        int lo = 0;
        for (int j = 0; j < n; j++) {
            float y = (float)j / (float)(n - 1);
            while (lo < n - 1 && fwd.data[lo + 1] < y)
                lo++;
            float y0 = fwd.data[lo];
            float y1 = fwd.data[lo + 1];
            float x0 = (float)lo / (float)(n - 1);
            float x1 = (float)(lo + 1) / (float)(n - 1);
            float denom = y1 - y0;
            float frac = (denom > 1e-12f) ? (y - y0) / denom : 0.f;
            inv.data[j] = x0 + frac * (x1 - x0);
        }
        inv.data[0] = 0.f;
        inv.data[n - 1] = 1.f;
    }

    // phasePos along the full R.level ↔ A.level span from the current envelope.
    // forAttack: 0 at R.level, 1 at A.level. Release: 0 at A.level, 1 at R.level.
    float phasePosFromEnvelope(bool forAttack) {
        float span = attackLevel - releaseLevel;
        if (std::fabs(span) < 1e-6f)
            return 1.f;
        float envNorm = (envelope - releaseLevel) / span;
        return forAttack
            ? attackShapeInvLut(envNorm)
            : releaseShapeInvLut(1.f - envNorm);
    }

    void beginAttack() {
        if (attackRetrig) {
            envelope = releaseLevel;
            phasePos = 0.f;
        }
        else {
            phasePos = phasePosFromEnvelope(true);
        }
        phase = ap_attack;
        outTrig[BOA].trigger();
        if (attackTime == 0.f || phasePos >= 1.f) {
            phase = ap_holdA;
            phasePos = 1.f;
            outTrig[EOA].trigger();
        }
        attackChaosFactor = rateChaosFactor(attackChaosAmount);
    }

    void beginRelease() {
        phase = ap_release;
        phasePos = phasePosFromEnvelope(false);
        outTrig[BOR].trigger();
        if (releaseTime == 0.f || phasePos >= 1.f) {
            phase = ap_holdR;
            outTrig[EOR].trigger();
        }
        releaseChaosFactor = rateChaosFactor(releaseChaosAmount);
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        delayRateChaos.updateActual();
        delayChaosAmount = rateChaosValues[delayRateChaos.act];
        attackRateChaos.updateActual();
        attackChaosAmount = rateChaosValues[attackRateChaos.act];
        releaseRateChaos.updateActual();
        releaseChaosAmount = rateChaosValues[releaseRateChaos.act];

        // Attack, Delay and Release times
        attackTime = readTimeParam(A_TIME_PARAM, args.sampleTime);
        delayTime = readTimeParam(DELAY_TIME_PARAM, args.sampleTime);
        timeLinked = params[TIME_LINK_PARAM].getValue() > 0.5f;
        if (timeLinked) {
            params[R_TIME_PARAM].setValue(params[A_TIME_PARAM].getValue());
            releaseTime = attackTime;
        }
        else {
            releaseTime = readTimeParam(R_TIME_PARAM, args.sampleTime);
        }

        // Attack and Release levels
        attackLevel = params[A_LEVEL_PARAM].getValue();
        releaseLevel = params[R_LEVEL_PARAM].getValue();
        deltaLevel = std::abs(releaseLevel - attackLevel);

        // Attack, Delay and Release steps (phasePos 0→1 over the phase time)
        attackStep = (attackTime > 0.f) ? procSampleTime / attackTime : 1.f;
        delayStep = (delayTime > 0.f) ? procSampleTime / delayTime : 1.f;
        releaseStep = (releaseTime > 0.f) ? procSampleTime / releaseTime : 1.f;

        // Attack and Release shapes
        attackShape = params[A_SHAPE_PARAM].getValue();
        float shapeLink = params[SHAPE_LINK_PARAM].getValue();
        shapeLinked = shapeLink > 0.5f;
        if (shapeLinked) {
            float linkedShape = (shapeLink > 1.5f) ? -attackShape : attackShape;
            params[R_SHAPE_PARAM].setValue(linkedShape);
            releaseShape = linkedShape;
        }
        else {
            releaseShape = params[R_SHAPE_PARAM].getValue();
        }
        if (attackShape != oldAttackShape) {
            rebuildShapeLuts(attackShape, attackShapeLut, attackShapeInvLut);
            oldAttackShape = attackShape;
        }
        if (releaseShape != oldReleaseShape) {
            rebuildShapeLuts(releaseShape, releaseShapeLut, releaseShapeInvLut);
            oldReleaseShape = releaseShape;
        }

        // Attack and Release retrig
        attackRetrig = params[A_RETRIG_PARAM].getValue() > 0.5f;
        delayRetrig = params[D_RETRIG_PARAM].getValue() > 0.5f;

        // Check for input connections
        havePhaseInput = inputs[PHASE_INPUT].isConnected();
        haveAttTrigInput = inputs[A_TRIG_INPUT].isConnected();
        haveRelTrigInput = inputs[DR_TRIG_INPUT].isConnected();

        // Check for output connections
        haveTriggerOutputs = outputs[BOA_OUTPUT].isConnected() || outputs[EOA_OUTPUT].isConnected() ||
            outputs[BOR_OUTPUT].isConnected() || outputs[EOR_OUTPUT].isConnected();
        haveEnvelopeOutput = outputs[ENVELOPE_OUTPUT].isConnected();

        bool newPhaseTrigMode = params[PHASE_GATE_TRIG_PARAM].getValue() < 0.5f;
        if (newPhaseTrigMode != phaseTrigMode) {
            phaseTrigMode = newPhaseTrigMode;
            phaseTrig.reset();
            phaseGate.reset();
        }
        phaseGate.setThreshold(trueDetectValues[gateDetHigh.act]);

        // Update phase lights
        lights[ATTACK_LIGHT].setBrightness(attackLightGreen[phase]);
        lights[ATTACK_LIGHT + 1].setBrightness(attackLightRed[phase]);
        lights[RELEASE_LIGHT].setBrightness(releaseLightGreen[phase]);
        lights[RELEASE_LIGHT + 1].setBrightness(releaseLightRed[phase]);

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
            // Handle direct A.trig
            float attVolt = haveAttTrigInput ? inputs[A_TRIG_INPUT].getVoltage() : 0.f;
            if (params[A_TRIG_BTN_PARAM].getValue() > 0.5f)
                attVolt = 10.f;
            bool attTriggered = attTrig.process(attVolt, 
                trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act]);

            // Handle direct DR.trig
            float relVolt = haveRelTrigInput ? inputs[DR_TRIG_INPUT].getVoltage() : 0.f;
            if (params[DR_TRIG_BTN_PARAM].getValue() > 0.5f)
                relVolt = 10.f;
            bool relTriggered = relTrig.process(relVolt, 
                trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act]);

            // Handle Phase input
            // Dedicated A.trig / DR.trig take priority; Phase fills unpatched sides.
            bool phaseIsAttack = phase == ap_attack || phase == ap_holdA;
            if (havePhaseInput) {
                bool phaseTrigToAttack = !phaseIsAttack;
                float phaseVolt = inputs[PHASE_INPUT].getVoltage();
                if (phaseTrigMode) { // Trigger-mode
                    if (phaseTrig.process(phaseVolt, 
                        trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act])) {
                        if (phaseTrigToAttack) {
                            if (!haveAttTrigInput)
                                attTriggered = true;
                        }
                        else if (!haveRelTrigInput) {
                            relTriggered = true;
                        }
                    }
                }
                else { // Gate-mode
                    if (phaseGate.process(phaseVolt)) {
                        if (phaseGate.isHigh()) {
                            if (!haveAttTrigInput)
                                attTriggered = true;
                        }
                        else if (!haveRelTrigInput) {
                            relTriggered = true;
                        }
                    }
                }
            }

            // Check for retrig
            if (attTriggered && phaseIsAttack) 
                attTriggered = attackRetrig;
            else if (relTriggered && !phaseIsAttack)
                relTriggered = delayRetrig;

            // Clear trig flags if both triggered in same cycle
            if (attTriggered && relTriggered) {
                attTriggered = false;
                relTriggered = false;
            }

            // Process trigger outputs
            outTrig[BOA].process(procSampleTime);
            outTrig[EOA].process(procSampleTime);
            outTrig[BOR].process(procSampleTime);
            outTrig[EOR].process(procSampleTime);
    
            // Handle triggered phase change/retrig or transition/delay/hold
            if (attTriggered) {
                beginAttack();
            } else if (relTriggered) {
                if (delayTime > 0.f) {
                    phase = ap_delay;
                    phasePos = 0.f;
                    delayChaosFactor = rateChaosFactor(delayChaosAmount);
                }
                else {
                    beginRelease();
                }
            }
            else { // Handle transition/delay/hold
                if (phase == ap_attack) {
                    phasePos += attackStep * attackChaosFactor;
                    if (phasePos >= 1.f) {
                        phase = ap_holdA;
                        phasePos = 0.f;
                        outTrig[EOA].trigger();       
                    }
                }
                else if (phase == ap_delay) {
                    phasePos += delayStep * delayChaosFactor;
                    if (phasePos >= 1.f) {
                        beginRelease();
                    }
                }
                else if (phase == ap_release) {
                    phasePos += releaseStep * releaseChaosFactor;
                    if (phasePos >= 1.f) {
                        phase = ap_holdR;
                        phasePos = 0.f;
                        outTrig[EOR].trigger();
                    }
                }
            }

            // Generate trigger outputs
            if (haveTriggerOutputs) {
                for (int i = 0; i < 4; i++) {
                    outputs[BOA_OUTPUT + i].setVoltage(outTrig[i].running() 
                        ? voltValues[gateOutHigh.act] : voltValues[gateOutLow.act]);
                }
            }

            // Envelope: holds pin to level knobs; ramps lerp along phasePos; delay keeps last voltage
            if (phase == ap_holdA)
                envelope = attackLevel;  // Must be set whether or not connected
            else if (phase == ap_holdR)
                envelope = releaseLevel;  // Must be set whether or not connected
            if (haveEnvelopeOutput) {
                if (phase == ap_attack)
                    envelope = releaseLevel + (attackLevel - releaseLevel) * attackShapeLut(phasePos);
                else if (phase == ap_release)
                    envelope = attackLevel + (releaseLevel - attackLevel) * releaseShapeLut(phasePos);

                // Output Envelope voltage
                outputs[ENVELOPE_OUTPUT].setVoltage(envelope);
            }
        }

        cycle256++;
    }
};

struct ADREnvelopeModuleWidget : InfNoiseModuleWidget {
    InfNoiseDisableOverlayGroup* linkTimeOverlayGroup = nullptr;
    InfNoiseDisableOverlayGroup* linkShapeOverlayGroup = nullptr;
    bool timeLinked = false;
    bool shapeLinked = false;

    ADREnvelopeModuleWidget(ADREnvelopeModule *module) {
        initializeWidget(module, "res/ADREnvelope");

        const float leftClm = 15.f;
        const float centerClm = 30.f;
        const float rightClm = 45.f;

        addInput(createInputCentered<ThemedPJ301MPort>(Vec(centerClm, 47.127f), module, ADREnvelopeModule::PHASE_INPUT));
        addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_red, bc_green>>(
            Vec(45.924f, 47.127f), module, ADREnvelopeModule::PHASE_GATE_TRIG_PARAM));

        addInput(createInputCentered<ThemedPJ301MPort>(Vec(leftClm, 84.901f), module, ADREnvelopeModule::A_TRIG_INPUT));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_red, true>>(Vec(4.861f, 97.773f), module, ADREnvelopeModule::A_TRIG_BTN_PARAM));

        addInput(createInputCentered<ThemedPJ301MPort>(Vec(rightClm, 84.901f), module, ADREnvelopeModule::DR_TRIG_INPUT));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_red, true>>(Vec(34.861f, 97.773f), module, ADREnvelopeModule::DR_TRIG_BTN_PARAM));

        addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(4.764f, 70.636f), module, ADREnvelopeModule::ATTACK_LIGHT));  
        addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(56.363f, 70.636f), module, ADREnvelopeModule::RELEASE_LIGHT));  

        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, false>>(Vec(6.133f, 114.109f), module, ADREnvelopeModule::A_RETRIG_PARAM));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, false>>(Vec(6.133f, 123.334f), module, ADREnvelopeModule::D_RETRIG_PARAM));

        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(44.500f, 121.126f), module, ADREnvelopeModule::DELAY_TIME_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(leftClm, 154.524f), module, ADREnvelopeModule::A_TIME_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(rightClm, 154.524f), module, ADREnvelopeModule::R_TIME_PARAM));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, false>>(Vec(centerClm, 139.489f), module, ADREnvelopeModule::TIME_LINK_PARAM));
        
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(leftClm, 186.869f), module, ADREnvelopeModule::A_SHAPE_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(rightClm, 186.869f), module, ADREnvelopeModule::R_SHAPE_PARAM));
        addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_green, bc_red>>(
            Vec(centerClm, 172.888f), module, ADREnvelopeModule::SHAPE_LINK_PARAM));
        
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(leftClm, 223.274f), module, ADREnvelopeModule::A_LEVEL_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(rightClm, 223.274f), module, ADREnvelopeModule::R_LEVEL_PARAM));

        InfNoiseDisableOverlayManager& overlayManager = getDisableOverlayManager();
        linkTimeOverlayGroup = overlayManager.addGroup("R.time linked to A.time");
        linkTimeOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
            ADREnvelopeModule::R_TIME_PARAM
        });
        linkShapeOverlayGroup = overlayManager.addGroup("R.shape linked to A.shape");
        linkShapeOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
            ADREnvelopeModule::R_SHAPE_PARAM
        });

        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(leftClm, 262.931f), module, ADREnvelopeModule::BOA_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(rightClm, 262.931f), module, ADREnvelopeModule::BOR_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(leftClm, 298.189f), module, ADREnvelopeModule::EOA_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(rightClm, 298.189f), module, ADREnvelopeModule::EOR_OUTPUT));

        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(centerClm, 333.447f), module, ADREnvelopeModule::ENVELOPE_OUTPUT));
    }

    void step() override {
        InfNoiseModuleWidget::step();

        if (!module)
            return;

        auto* m = static_cast<ADREnvelopeModule*>(module);
        if (linkTimeOverlayGroup) {
            if (m->timeLinked != timeLinked) {
                timeLinked = m->timeLinked;
                linkTimeOverlayGroup->setActive(timeLinked);
            }
        }
        if (linkShapeOverlayGroup) {
            if (m->shapeLinked != shapeLinked) {
                shapeLinked = m->shapeLinked;
                linkShapeOverlayGroup->setActive(shapeLinked);
            }
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        ADREnvelopeModule* module = dynamic_cast<ADREnvelopeModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> rateChaosNames = getRateChaosNames();
        menu->addChild(createIndexPtrSubmenuItem("Attack rate chaos", rateChaosNames,
            &module->attackRateChaos.req));
        menu->addChild(createIndexPtrSubmenuItem("Delay rate chaos", rateChaosNames,
            &module->delayRateChaos.req));
        menu->addChild(createIndexPtrSubmenuItem("Release rate chaos", rateChaosNames,
            &module->releaseRateChaos.req));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelADREnvelope = createModel<ADREnvelopeModule, ADREnvelopeModuleWidget>("ADREnvelope");