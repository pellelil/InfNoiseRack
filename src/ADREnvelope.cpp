// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"
#include "InfNoiseEnvelope.hpp"

struct ADREnvelopeModule : InfNoiseEnvelopeModule {
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
        R_TIME_LINK_PARAM,
        R_SHAPE_LINK_PARAM,
        DELAY_TIME_LINK_PARAM,
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
        INV_ENVELOPE_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT, 2),
        ENUMS(CLIP_RANGE_LIGHT, 2),
        ENUMS(ATTACK_LIGHT, 2),
        ENUMS(RELEASE_LIGHT, 2),
        LIGHTS_LEN
    };

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
    Lut1D<256> attackShapeLut{0.f, 1.f};
    Lut1D<256> attackShapeInvLut{0.f, 1.f};
    Lut1D<256> releaseShapeLut{0.f, 1.f};
    Lut1D<256> releaseShapeInvLut{0.f, 1.f};
    float attackLevel = 10.f;
    float releaseLevel = 0.f;
    bool attackRetrig = false;
    bool delayRetrig = false;
    bool havePhaseInput = false;
    bool haveAttTrigInput = false;
    bool haveRelTrigInput = false;
    bool phaseTrigMode = false; // false = gate (default), true = trigger
    /// Set in processParams; used by widget overlay (D/R knobs follow A when linked).
    bool delayTimeLinked = false;
    bool releaseTimeLinked = false;
    bool releaseShapeLinked = false;
    bool haveTriggerOutputs = false;
    bool haveEnvelopeOutput = false;  // ENVELOPE_OUTPUT or INV_ENVELOPE_OUTPUT connected
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

        envelope = releaseLevel;
        prevEnvelope = envelope;
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
        configParam(A_SHAPE_PARAM, -1.f, 1.f, 0.f, "Attack shape (Exp/Lin/Log)");
        configParam(R_SHAPE_PARAM, -1.f, 1.f, 0.f, "Release shape (Exp/Lin/Log)");
        configParam(A_LEVEL_PARAM, -10.f, 10.f, 10.f, "Attack level (-10V to +10V)", " V");
        configParam(R_LEVEL_PARAM, -10.f, 10.f, 0.f, "Release level (-10V to +10V)", " V");
        configSwitch(R_TIME_LINK_PARAM, 0.f, 1.f, 0.f, "Release time link", {"Disabled", "Enabled"});
        configSwitch(R_SHAPE_LINK_PARAM, 0.f, 2.f, 0.f, "Release shape link", {"Disabled", "Enabled", "Reversed"});
        configSwitch(DELAY_TIME_LINK_PARAM, 0.f, 1.f, 0.f, "Delay time link", {"Disabled", "Enabled"});

        configOutput(BOA_OUTPUT, "BOA (Begin Of Attack)");
        configOutput(EOA_OUTPUT, "EOA (End Of Attack)");
        configOutput(BOR_OUTPUT, "BOR (Begin Of Release)/(End Of Delay)");
        configOutput(EOR_OUTPUT, "EOR (End Of Release)");
        configOutput(ENVELOPE_OUTPUT, "Envelope");
        configOutput(INV_ENVELOPE_OUTPUT, "Inverted Envelope");

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
		haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = false;  
		haveGateDetect = true;
		haveGateHighLow = false;
        haveTrigDetect = true;
		haveTrigHighLow = true;

        ensureNormExpLogLuts();
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseEnvelopeModule::onReset(e);

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
        prevEnvelope = envelope;
        envMotion = em_steady;
        oldAttackShape = 2.f;
        oldReleaseShape = 2.f;
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseEnvelopeModule::dataFromJson(rootJ);

        delayRateChaos.setBoth((rateChaos)getJsonInt(rootJ, "delayRateChaos", (int)rc_default));
        attackRateChaos.setBoth((rateChaos)getJsonInt(rootJ, "attackRateChaos", (int)rc_default));
        releaseRateChaos.setBoth((rateChaos)getJsonInt(rootJ, "releaseRateChaos", (int)rc_default));
        // Pre-v3 ADR phase enum had no ap_decay (holdA was 1). Shift values >= ap_decay up by 1.
        if (jsonVersion < 3) {
            int p = getJsonInt(rootJ, "phase", (int)ap_holdR);
            if (p >= (int)ap_decay)
                p += 1;
            phase = (adrPhase)clamp(p, (int)ap_attack, (int)ap_holdR);
        }
    }

    void dataToJson(json_t* rootJ) override {
        InfNoiseEnvelopeModule::dataToJson(rootJ);
        json_object_set_new(rootJ, "delayRateChaos", json_integer((int)delayRateChaos.req));
        json_object_set_new(rootJ, "attackRateChaos", json_integer((int)attackRateChaos.req));
        json_object_set_new(rootJ, "releaseRateChaos", json_integer((int)releaseRateChaos.req));
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
            phase = ap_sustain;
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
        delayTimeLinked = params[DELAY_TIME_LINK_PARAM].getValue() > 0.5f;
        if (delayTimeLinked) {
            params[DELAY_TIME_PARAM].setValue(params[A_TIME_PARAM].getValue());
            delayTime = attackTime;
        }
        releaseTimeLinked = params[R_TIME_LINK_PARAM].getValue() > 0.5f;
        if (releaseTimeLinked) {
            params[R_TIME_PARAM].setValue(params[A_TIME_PARAM].getValue());
            releaseTime = attackTime;
        }
        else {
            releaseTime = readTimeParam(R_TIME_PARAM, args.sampleTime);
        }

        // Attack and Release levels
        attackLevel = params[A_LEVEL_PARAM].getValue();
        releaseLevel = params[R_LEVEL_PARAM].getValue();

        // Attack, Delay and Release steps (phasePos 0→1 over the phase time)
        attackStep = (attackTime > 0.f) ? procSampleTime / attackTime : 1.f;
        delayStep = (delayTime > 0.f) ? procSampleTime / delayTime : 1.f;
        releaseStep = (releaseTime > 0.f) ? procSampleTime / releaseTime : 1.f;

        // Attack and Release shapes
        attackShape = params[A_SHAPE_PARAM].getValue();
        float shapeLink = params[R_SHAPE_LINK_PARAM].getValue();
        releaseShapeLinked = shapeLink > 0.5f;
        if (releaseShapeLinked) {
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
        haveEnvelopeOutput = outputs[ENVELOPE_OUTPUT].isConnected() ||
            outputs[INV_ENVELOPE_OUTPUT].isConnected();

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
            prevEnvelope = envelope;

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
            bool phaseIsAttack = phase == ap_attack || phase == ap_sustain;
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
                        phase = ap_sustain;
                        phasePos = 0.f;
                        outTrig[EOA].trigger();       
                    }
                }
                else if (phase == ap_delay) {
                    phasePos += delayStep * delayChaosFactor;
                    if (phasePos >= 1.f) {
                        beginRelease();
                        outTrig[BOR].trigger();
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
                        ? voltValues[trigOutHigh.act] : voltValues[trigOutLow.act]);
                }
            }

            // Generate envelope (always, so interrupts map from the current voltage
            // even when Env/!Env are unpatched — same rule as ADSDR Envelope).
            if (phase == ap_sustain)
                envelope = attackLevel;
            else if (phase == ap_holdR)
                envelope = releaseLevel;
            else if (phase == ap_attack)
                envelope = releaseLevel + (attackLevel - releaseLevel) * attackShapeLut(phasePos);
            else if (phase == ap_release)
                envelope = attackLevel + (releaseLevel - attackLevel) * releaseShapeLut(phasePos);

            if (haveEnvelopeOutput) {
                outputs[ENVELOPE_OUTPUT].setVoltage(envelope);
                outputs[INV_ENVELOPE_OUTPUT].setVoltage(attackLevel + releaseLevel - envelope);
            }
            updateEnvMotion();
            pushToExpanders();
        }

        cycle256++;
    }
};

struct ADREnvelopeModuleWidget : InfNoiseModuleWidget {
    InfNoiseDisableOverlayGroup* delayLinkTimeOverlayGroup = nullptr;
    InfNoiseDisableOverlayGroup* releaseLinkTimeOverlayGroup = nullptr;
    InfNoiseDisableOverlayGroup* releaseLinkShapeOverlayGroup = nullptr;
    bool delayTimeLinked = false;
    bool releaseTimeLinked = false;
    bool releaseShapeLinked = false;

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
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, false>>(Vec(centerClm, 106.091f), module, ADREnvelopeModule::DELAY_TIME_LINK_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(leftClm, 154.524f), module, ADREnvelopeModule::A_TIME_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(rightClm, 154.524f), module, ADREnvelopeModule::R_TIME_PARAM));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, false>>(Vec(centerClm, 139.489f), module, ADREnvelopeModule::R_TIME_LINK_PARAM));
        
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(leftClm, 186.869f), module, ADREnvelopeModule::A_SHAPE_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(rightClm, 186.869f), module, ADREnvelopeModule::R_SHAPE_PARAM));
        addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_green, bc_red>>(
            Vec(centerClm, 172.888f), module, ADREnvelopeModule::R_SHAPE_LINK_PARAM));
        
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(leftClm, 223.274f), module, ADREnvelopeModule::A_LEVEL_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(rightClm, 223.274f), module, ADREnvelopeModule::R_LEVEL_PARAM));

        InfNoiseDisableOverlayManager& overlayManager = getDisableOverlayManager();
        delayLinkTimeOverlayGroup = overlayManager.addGroup("D.time linked to A.time");
        delayLinkTimeOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
            ADREnvelopeModule::DELAY_TIME_PARAM
        });
        releaseLinkTimeOverlayGroup = overlayManager.addGroup("R.time linked to A.time");
        releaseLinkTimeOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
            ADREnvelopeModule::R_TIME_PARAM
        });
        releaseLinkShapeOverlayGroup = overlayManager.addGroup("R.shape linked to A.shape");
        releaseLinkShapeOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
            ADREnvelopeModule::R_SHAPE_PARAM
        });

        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(leftClm, 262.931f), module, ADREnvelopeModule::BOA_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(rightClm, 262.931f), module, ADREnvelopeModule::BOR_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(leftClm, 298.189f), module, ADREnvelopeModule::EOA_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(rightClm, 298.189f), module, ADREnvelopeModule::EOR_OUTPUT));

        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(leftClm, 333.447f), module, ADREnvelopeModule::ENVELOPE_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(rightClm, 333.447f), module, ADREnvelopeModule::INV_ENVELOPE_OUTPUT));
    }

    void step() override {
        InfNoiseModuleWidget::step();

        if (!module)
            return;

        auto* m = static_cast<ADREnvelopeModule*>(module);
        if (delayLinkTimeOverlayGroup) {
            if (m->delayTimeLinked != delayTimeLinked) {
                delayTimeLinked = m->delayTimeLinked;
                delayLinkTimeOverlayGroup->setActive(delayTimeLinked);
            }
        }
        if (releaseLinkTimeOverlayGroup) {
            if (m->releaseTimeLinked != releaseTimeLinked) {
                releaseTimeLinked = m->releaseTimeLinked;
                releaseLinkTimeOverlayGroup->setActive(releaseTimeLinked);
            }
        }
        if (releaseLinkShapeOverlayGroup) {
            if (m->releaseShapeLinked != releaseShapeLinked) {
                releaseShapeLinked = m->releaseShapeLinked;
                releaseLinkShapeOverlayGroup->setActive(releaseShapeLinked);
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