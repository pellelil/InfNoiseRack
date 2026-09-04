// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"
#include "InfNoiseEnvelope.hpp"

struct ADSDREnvelopeModule : InfNoiseEnvelopeModule {
    enum ParamId {
        PHASE_GATE_TRIG_PARAM,
        A_TRIG_BTN_PARAM,
        DR_TRIG_BTN_PARAM,
        A_RETRIG_PARAM,
        DL_RETRIG_PARAM,
        DL_TIME_PARAM,
        A_TIME_PARAM,
        DC_TIME_PARAM,
        R_TIME_PARAM,
        A_SHAPE_PARAM,
        DC_SHAPE_PARAM,
        R_SHAPE_PARAM,
        A_LEVEL_PARAM,
        SUSTAIN_LEVEL_PARAM,
        R_LEVEL_PARAM,
        DC_TIME_LINK_PARAM,
        DC_SHAPE_LINK_PARAM,
        DL_TIME_LINK_PARAM,
        DL_SHAPE_LINK_PARAM,
        R_TIME_LINK_PARAM,
        R_SHAPE_LINK_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        PHASE_INPUT,
        A_TRIG_INPUT,
        DLR_TRIG_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        BOA_OUTPUT,
        EOA_OUTPUT,
        BOS_OUTPUT,
        EOS_OUTPUT,
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
    float decayTime = 0.f;
    float delayTime = 0.f;
    float releaseTime = 0.f;
    float attackStep = 0.f;   // phasePos increment per process
    float decayStep = 0.f;
    float delayStep = 0.f;
    float releaseStep = 0.f;
    float attackShape = 0.f;
    float decayShape = 0.f;
    float releaseShape = 0.f;
    float oldAttackShape = 2.f;   // Sentinel: first processParams always rebuilds
    float oldDecayShape = 2.f;
    float oldReleaseShape = 2.f;
    Lut1D<256> attackShapeLut{0.f, 1.f};
    Lut1D<256> attackShapeInvLut{0.f, 1.f};
    Lut1D<256> decayShapeLut{0.f, 1.f};
    Lut1D<256> decayShapeInvLut{0.f, 1.f};
    Lut1D<256> releaseShapeLut{0.f, 1.f};
    Lut1D<256> releaseShapeInvLut{0.f, 1.f};
    float attackLevel = 10.f;
    float sustainLevel = 5.f;
    float releaseLevel = 0.f;
    float rampFrom = 0.f;           // Start level of the active ramp
    float rampTo = 10.f;            // Target level of the active ramp
    bool attackRetrig = false;
    bool delayRetrig = false;
    bool havePhaseInput = false;
    bool haveAttTrigInput = false;
    bool haveRelTrigInput = false;
    bool phaseTrigMode = false; // false = gate (default), true = trigger
    /// Set in processParams; used by widget overlay (Dc/Dl/R knobs follow A when linked).
    bool decayTimeLinked = false;
    bool decayShapeLinked = false;
    bool delayTimeLinked = false;
    bool delayShapeLinked = false;
    bool releaseTimeLinked = false;
    bool releaseShapeLinked = false;
    bool haveTriggerOutputs = false;
    bool haveEnvelopeOutput = false;  // ENVELOPE_OUTPUT or INV_ENVELOPE_OUTPUT connected
    actReqValue<rateChaos> attackRateChaos = actReqValue<rateChaos>(rc_default);
    actReqValue<rateChaos> decayRateChaos = actReqValue<rateChaos>(rc_default);
    actReqValue<rateChaos> delayRateChaos = actReqValue<rateChaos>(rc_default);
    actReqValue<rateChaos> releaseRateChaos = actReqValue<rateChaos>(rc_default);
    dsp::SchmittTrigger phaseTrig;
    dsp::SchmittTrigger attTrig;
    dsp::SchmittTrigger relTrig;
    infNoiseInEdgeDetector phaseGate = infNoiseInEdgeDetector(trueDetectValues[td_gateHigh]);
    static const int BOA = 0; // Begin Of Attack
    static const int EOA = 1; // End Of Attack (Begin Of Decay)
    static const int BOS = 2; // Begin Of Sustain (End Of Decay)
    static const int EOS = 3; // End Of Sustain (Begin Of Delay)
    static const int BOR = 4; // Begin Of Release (End Of Delay)
    static const int EOR = 5; // End Of Release
    infNoiseOutTrigger outTrig[6] = {
        infNoiseOutTrigger(), // BOA
        infNoiseOutTrigger(), // EOA
        infNoiseOutTrigger(), // BOS
        infNoiseOutTrigger(), // EOS
        infNoiseOutTrigger(), // BOR
        infNoiseOutTrigger()  // EOR
    };
    float attackChaosFactor = 1.f;
    float decayChaosFactor = 1.f;
    float delayChaosFactor = 1.f;
    float releaseChaosFactor = 1.f;
    float attackChaosAmount = 0.f;
    float decayChaosAmount = 0.f;
    float delayChaosAmount = 0.f;
    float releaseChaosAmount = 0.f;

	ADSDREnvelopeModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        envelope = releaseLevel;
        prevEnvelope = envelope;
        configInput(PHASE_INPUT, "Phase (Gate/Trigger)");
        configSwitch(PHASE_GATE_TRIG_PARAM, 0.f, 1.f, 1.f, "Phase-mode", {"Trigger", "Gate"});
        configInput(A_TRIG_INPUT, "Attack/Decay Trigger");
        configButton(A_TRIG_BTN_PARAM, "Trig Attack/Decay");
        configInput(DLR_TRIG_INPUT, "Delay/Release Trigger");
        configButton(DR_TRIG_BTN_PARAM, "Trig Delay/Release");

        configLight(ATTACK_LIGHT, "Attack/Decay phase (Green=Attack, Yellow=Decay, Red=Sustain)");
        configLight(RELEASE_LIGHT, "Release phase (Yellow=Delay, Green=Release, Red=Hold)");

        configSwitch(A_RETRIG_PARAM, 0.f, 1.f, 0.f, "Attack retrig", {"Disabled", "Enabled"});
        configSwitch(DL_RETRIG_PARAM, 0.f, 1.f, 0.f, "Delay retrig", {"Disabled", "Enabled"});

        const float timeBase = 10000.f;
        const float timeMax = 10.f;
        const float timeMult = timeMax / (timeBase - 1.f);
        configParam(A_TIME_PARAM, 0.f, 1.f, 0.f, "Attack time (0 to 10 s)", " s",
            timeBase, timeMult, -timeMult);
        configParam(DC_TIME_PARAM, 0.f, 1.f, 0.f, "Decay time (0 to 10 s)", " s",
                timeBase, timeMult, -timeMult);
        configParam(DL_TIME_PARAM, 0.f, 1.f, 0.f, "Delay time (0 to 10 s)", " s",
            timeBase, timeMult, -timeMult);
        configParam(R_TIME_PARAM, 0.f, 1.f, 0.f, "Release time (0 to 10 s)", " s",
            timeBase, timeMult, -timeMult);
        configParam(A_SHAPE_PARAM, -1.f, 1.f, 0.f, "Attack shape (Exp/Lin/Log)");
        configParam(DC_SHAPE_PARAM, -1.f, 1.f, 0.f, "Decay shape (Exp/Lin/Log)");
        configParam(R_SHAPE_PARAM, -1.f, 1.f, 0.f, "Release shape (Exp/Lin/Log)");
        configParam(A_LEVEL_PARAM, -10.f, 10.f, 10.f, "Attack level (-10V to +10V)", " V");
        configParam(SUSTAIN_LEVEL_PARAM, -10.f, 10.f, 5.f, "Sustain level (-10V to +10V)", " V");
        configParam(R_LEVEL_PARAM, -10.f, 10.f, 0.f, "Release level (-10V to +10V)", " V");
        configSwitch(DC_TIME_LINK_PARAM, 0.f, 1.f, 0.f, "Decay time link", {"Disabled", "Enabled"});
        configSwitch(DC_SHAPE_LINK_PARAM, 0.f, 2.f, 0.f, "Decay shape link", {"Disabled", "Enabled", "Reversed"});
        configSwitch(DL_TIME_LINK_PARAM, 0.f, 1.f, 0.f, "Delay time link", {"Disabled", "Enabled"});
        configSwitch(DL_SHAPE_LINK_PARAM, 0.f, 2.f, 0.f, "Delay shape link", {"Disabled", "Enabled", "Reversed"});
        configSwitch(R_TIME_LINK_PARAM, 0.f, 1.f, 0.f, "Release time link", {"Disabled", "Enabled"});
        configSwitch(R_SHAPE_LINK_PARAM, 0.f, 2.f, 0.f, "Release shape link", {"Disabled", "Enabled", "Reversed"});

        configOutput(BOA_OUTPUT, "BOA (Begin Of Attack)");
        configOutput(EOA_OUTPUT, "EOA (End Of Attack)/(Begin Of Decay)");
        configOutput(BOS_OUTPUT, "BOS (Begin Of Sustain)/(End of Decay)");
        configOutput(EOS_OUTPUT, "EOS (End Of Sustain)/(Begin Of Delay)");
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

        attackRateChaos.setBoth(rc_default);
        decayRateChaos.setBoth(rc_default);
        delayRateChaos.setBoth(rc_default);
        releaseRateChaos.setBoth(rc_default);
        attackChaosFactor = 1.f;
        decayChaosFactor = 1.f;
        delayChaosFactor = 1.f;
        releaseChaosFactor = 1.f;
        phaseTrig.reset();
        attTrig.reset();
        relTrig.reset();
        phaseGate.reset();
        for (int i = 0; i < 6; i++)
            outTrig[i].reset();
        phaseTrigMode = false;
        envelope = releaseLevel;
        prevEnvelope = envelope;
        envMotion = em_steady;
        rampFrom = releaseLevel;
        rampTo = attackLevel;
        oldAttackShape = 2.f;
        oldDecayShape = 2.f;
        oldReleaseShape = 2.f;
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseEnvelopeModule::dataFromJson(rootJ);

        attackRateChaos.setBoth((rateChaos)getJsonInt(rootJ, "attackRateChaos", (int)rc_default));
        decayRateChaos.setBoth((rateChaos)getJsonInt(rootJ, "decayRateChaos", (int)rc_default));
        delayRateChaos.setBoth((rateChaos)getJsonInt(rootJ, "delayRateChaos", (int)rc_default));
        releaseRateChaos.setBoth((rateChaos)getJsonInt(rootJ, "releaseRateChaos", (int)rc_default));
        rampFrom = getJsonFloat(rootJ, "rampFrom", releaseLevel);
        rampTo = getJsonFloat(rootJ, "rampTo", attackLevel);
    }

    void dataToJson(json_t* rootJ) override {
        InfNoiseEnvelopeModule::dataToJson(rootJ);
        json_object_set_new(rootJ, "attackRateChaos", json_integer((int)attackRateChaos.req));
        json_object_set_new(rootJ, "decayRateChaos", json_integer((int)decayRateChaos.req));
        json_object_set_new(rootJ, "delayRateChaos", json_integer((int)delayRateChaos.req));
        json_object_set_new(rootJ, "releaseRateChaos", json_integer((int)releaseRateChaos.req));
        json_object_set_new(rootJ, "rampFrom", json_real(rampFrom));
        json_object_set_new(rootJ, "rampTo", json_real(rampTo));
    }

    // phasePos along fromLevel → toLevel from the current envelope. Also sets rampFrom/rampTo.
    // On the span: continue along the phase shape. Before the start: ramp from current voltage.
    // At or past the target (or zero span): return 1 so the phase is skipped.
    float phasePosFromEnvelope(float fromLevel, float toLevel, const Lut1D<256>& invLut) {
        float span = toLevel - fromLevel;
        rampTo = toLevel;
        if (std::fabs(span) < 1e-6f) {
            rampFrom = fromLevel;
            return 1.f;
        }
        float envNorm = (envelope - fromLevel) / span;
        if (envNorm < 0.f) {
            rampFrom = envelope;
            return 0.f;
        }
        rampFrom = fromLevel;
        if (envNorm >= 1.f)
            return 1.f;
        return invLut(envNorm);
    }

    void beginAttack() {
        if (attackRetrig) {
            envelope = releaseLevel;
            rampFrom = releaseLevel;
            rampTo = attackLevel;
            phasePos = 0.f;
        }
        else {
            phasePos = phasePosFromEnvelope(releaseLevel, attackLevel, attackShapeInvLut);
        }
        phase = ep_attack;
        outTrig[BOA].trigger();
        attackChaosFactor = rateChaosFactor(attackChaosAmount);
        if (attackTime == 0.f || phasePos >= 1.f) {
            envelope = attackLevel;
            beginDecay();
        }
    }

    void beginDecay() {
        phasePos = 0;  // Always start from where Attack ended (full decay, unless interrupted by release)
        phase = ep_decay;
        outTrig[EOA].trigger();
        decayChaosFactor = rateChaosFactor(decayChaosAmount);
        if (decayTime == 0.f) {
            envelope = sustainLevel;
            phase = ep_sustain;
            phasePos = 1.f;
            outTrig[BOS].trigger();
        }
    }

    void beginRelease() {
        // Always start from the current voltage so Delay (hold) and interrupts
        // do not jump. phasePos 0→1 covers this remaining span over R.time.
        rampFrom = envelope;
        rampTo = releaseLevel;
        phasePos = 0.f;
        phase = ep_release;
        outTrig[BOR].trigger();
        releaseChaosFactor = rateChaosFactor(releaseChaosAmount);
        if (releaseTime == 0.f || std::fabs(rampTo - rampFrom) < 1e-6f) {
            envelope = releaseLevel;
            phase = ep_idle;
            outTrig[EOR].trigger();
        }
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        attackRateChaos.updateActual();
        attackChaosAmount = rateChaosValues[attackRateChaos.act];
        decayRateChaos.updateActual();
        decayChaosAmount = rateChaosValues[decayRateChaos.act];
        delayRateChaos.updateActual();
        delayChaosAmount = rateChaosValues[delayRateChaos.act];
        releaseRateChaos.updateActual();
        releaseChaosAmount = rateChaosValues[releaseRateChaos.act];

        // Attack, Delay and Release times
        attackTime = readTimeParam(A_TIME_PARAM, args.sampleTime);
        decayTime = readTimeParam(DC_TIME_PARAM, args.sampleTime);
        delayTime = readTimeParam(DL_TIME_PARAM, args.sampleTime);
        releaseTime = readTimeParam(R_TIME_PARAM, args.sampleTime);
        decayTimeLinked = params[DC_TIME_LINK_PARAM].getValue() > 0.5f;
        if (decayTimeLinked) {
            params[DC_TIME_PARAM].setValue(params[A_TIME_PARAM].getValue());
            decayTime = attackTime;
        }
        delayTimeLinked = params[DL_TIME_LINK_PARAM].getValue() > 0.5f;
        if (delayTimeLinked) {
            params[DL_TIME_PARAM].setValue(params[A_TIME_PARAM].getValue());
            delayTime = attackTime;
        }
        releaseTimeLinked = params[R_TIME_LINK_PARAM].getValue() > 0.5f;
        if (releaseTimeLinked) {
            params[R_TIME_PARAM].setValue(params[A_TIME_PARAM].getValue());
            releaseTime = attackTime;
        }

        // Attack and Release levels
        attackLevel = params[A_LEVEL_PARAM].getValue();
        sustainLevel = params[SUSTAIN_LEVEL_PARAM].getValue();
        releaseLevel = params[R_LEVEL_PARAM].getValue();

        // Attack, Delay and Release steps (phasePos 0→1 over the phase time)
        attackStep = (attackTime > 0.f) ? procSampleTime / attackTime : 1.f;
        decayStep = (decayTime > 0.f) ? procSampleTime / decayTime : 1.f;
        delayStep = (delayTime > 0.f) ? procSampleTime / delayTime : 1.f;
        releaseStep = (releaseTime > 0.f) ? procSampleTime / releaseTime : 1.f;

        // Attack, Decay and Release shapes
        attackShape = params[A_SHAPE_PARAM].getValue();
        float decayShapeLink = params[DC_SHAPE_LINK_PARAM].getValue();
        decayShapeLinked = decayShapeLink > 0.5f;
        if (decayShapeLinked) {
            float linkedShape = (decayShapeLink > 1.5f) ? -attackShape : attackShape;
            params[DC_SHAPE_PARAM].setValue(linkedShape);
            decayShape = linkedShape;
        }
        else {
            decayShape = params[DC_SHAPE_PARAM].getValue();
        }
        float releaseShapeLink = params[R_SHAPE_LINK_PARAM].getValue();
        releaseShapeLinked = releaseShapeLink > 0.5f;
        if (releaseShapeLinked) {
            float linkedShape = (releaseShapeLink > 1.5f) ? -attackShape : attackShape;
            params[R_SHAPE_PARAM].setValue(linkedShape);
            releaseShape = linkedShape;
        }
        else {
            releaseShape = params[R_SHAPE_PARAM].getValue();
        }
        float delayShapeLink = params[DL_SHAPE_LINK_PARAM].getValue();
        delayShapeLinked = delayShapeLink > 0.5f;

        // Rebuild shape LUTs as needed
        if (attackShape != oldAttackShape) {
            rebuildShapeLuts(attackShape, attackShapeLut, attackShapeInvLut);
            oldAttackShape = attackShape;
        }
        if (decayShape != oldDecayShape) {
            rebuildShapeLuts(decayShape, decayShapeLut, decayShapeInvLut);
            oldDecayShape = decayShape;
        }
        if (releaseShape != oldReleaseShape) {
            rebuildShapeLuts(releaseShape, releaseShapeLut, releaseShapeInvLut);
            oldReleaseShape = releaseShape;
        }

        // Attack and Release retrig
        attackRetrig = params[A_RETRIG_PARAM].getValue() > 0.5f;
        delayRetrig = params[DL_RETRIG_PARAM].getValue() > 0.5f;

        // Check for input connections
        havePhaseInput = inputs[PHASE_INPUT].isConnected();
        haveAttTrigInput = inputs[A_TRIG_INPUT].isConnected();
        haveRelTrigInput = inputs[DLR_TRIG_INPUT].isConnected();

        // Check for output connections
        haveTriggerOutputs = outputs[BOA_OUTPUT].isConnected() || outputs[EOA_OUTPUT].isConnected() ||
            outputs[BOS_OUTPUT].isConnected() || outputs[EOS_OUTPUT].isConnected() ||
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
            float relVolt = haveRelTrigInput ? inputs[DLR_TRIG_INPUT].getVoltage() : 0.f;
            if (params[DR_TRIG_BTN_PARAM].getValue() > 0.5f)
                relVolt = 10.f;
            bool relTriggered = relTrig.process(relVolt, 
                trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act]);

            // Handle Phase input
            // Dedicated A.trig / DR.trig take priority; Phase fills unpatched sides.
            bool phaseIsAttack = phase == ep_attack || phase == ep_decay || phase == ep_sustain;
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
            outTrig[BOS].process(procSampleTime);
            outTrig[EOS].process(procSampleTime);
            outTrig[BOR].process(procSampleTime);
            outTrig[EOR].process(procSampleTime);
    
            // Handle triggered phase change/retrig or transition/delay/hold
            if (attTriggered) {
                beginAttack();
            } else if (relTriggered) {
                if (phase == ep_sustain)
                    outTrig[EOS].trigger();
                if (delayTime > 0.f) {
                    phase = ep_delay;
                    phasePos = 0.f;
                    delayChaosFactor = rateChaosFactor(delayChaosAmount);
                }
                else {
                    beginRelease();
                }
            }
            else { // Handle transition/delay/hold
                if (phase == ep_attack) {
                    phasePos += attackStep * attackChaosFactor;
                    if (phasePos >= 1.f) {
                        envelope = attackLevel;
                        beginDecay();
                    }
                }
                else if (phase == ep_decay) {
                    phasePos += decayStep * decayChaosFactor;
                    if (phasePos >= 1.f) {
                        phase = ep_sustain;
                        phasePos = 1.f;
                        outTrig[BOS].trigger();
                    }
                }
                else if (phase == ep_delay) {
                    phasePos += delayStep * delayChaosFactor;
                    if (phasePos >= 1.f) {
                        beginRelease();
                    }
                }
                else if (phase == ep_release) {
                    phasePos += releaseStep * releaseChaosFactor;
                    if (phasePos >= 1.f) {
                        phase = ep_idle;
                        phasePos = 0.f;
                        outTrig[EOR].trigger();
                    }
                }
            }

            // Generate trigger outputs
            if (haveTriggerOutputs) {
                for (int i = 0; i < 6; i++) {
                    outputs[BOA_OUTPUT + i].setVoltage(outTrig[i].running() 
                        ? voltValues[trigOutHigh.act] : voltValues[trigOutLow.act]);
                }
            }

            // Generate envelope (always, so interrupts map from the current voltage)
            if (phase == ep_sustain)
                envelope = sustainLevel;
            else if (phase == ep_idle)
                envelope = releaseLevel;
            else if (phase == ep_attack)
                envelope = rampFrom + (rampTo - rampFrom) * attackShapeLut(phasePos);
            else if (phase == ep_decay)
                envelope = rampFrom + (rampTo - rampFrom) * decayShapeLut(phasePos);
            else if (phase == ep_release)
                envelope = rampFrom + (rampTo - rampFrom) * releaseShapeLut(phasePos);

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

struct ADSDREnvelopeModuleWidget : InfNoiseModuleWidget {
    InfNoiseDisableOverlayGroup* decayLinkTimeOverlayGroup = nullptr;
    InfNoiseDisableOverlayGroup* decayLinkShapeOverlayGroup = nullptr;
    InfNoiseDisableOverlayGroup* delayLinkTimeOverlayGroup = nullptr;
    InfNoiseDisableOverlayGroup* releaseLinkTimeOverlayGroup = nullptr;
    InfNoiseDisableOverlayGroup* releaseLinkShapeOverlayGroup = nullptr;
    bool decayTimeLinked = false;
    bool decayShapeLinked = false;
    bool delayTimeLinked = false;
    bool releaseTimeLinked = false;
    bool releaseShapeLinked = false;

    ADSDREnvelopeModuleWidget(ADSDREnvelopeModule *module) {
        initializeWidget(module, "res/ADSDREnvelope");

        const float leftClm = 15.f;
        const float centerClm = 45.f;
        const float rightClm = 75.f;

        const float centerLeftClm = 30.f;
        const float centerRightClm = 60.f;

        addInput(createInputCentered<ThemedPJ301MPort>(Vec(centerClm, 47.127f), module, ADSDREnvelopeModule::PHASE_INPUT));
        addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_red, bc_green>>(
            Vec(60.924f, 47.127f), module, ADSDREnvelopeModule::PHASE_GATE_TRIG_PARAM));

        addInput(createInputCentered<ThemedPJ301MPort>(Vec(centerLeftClm, 84.901f), module, ADSDREnvelopeModule::A_TRIG_INPUT));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_red, true>>(Vec(19.861f, 97.773f), module, ADSDREnvelopeModule::A_TRIG_BTN_PARAM));

        addInput(createInputCentered<ThemedPJ301MPort>(Vec(centerRightClm, 84.901f), module, ADSDREnvelopeModule::DLR_TRIG_INPUT));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_red, true>>(Vec(49.861f, 97.773f), module, ADSDREnvelopeModule::DR_TRIG_BTN_PARAM));

        addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(centerLeftClm, 65.332f), module, ADSDREnvelopeModule::ATTACK_LIGHT));  
        addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(centerRightClm, 65.332f), module, ADSDREnvelopeModule::RELEASE_LIGHT));  

        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, false>>(Vec(6.133f, 114.109f), module, ADSDREnvelopeModule::A_RETRIG_PARAM));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, false>>(Vec(6.133f, 123.334f), module, ADSDREnvelopeModule::DL_RETRIG_PARAM));

        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(rightClm, 121.126f), module, ADSDREnvelopeModule::DL_TIME_PARAM));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, false>>(Vec(centerRightClm, 106.091f), module, ADSDREnvelopeModule::DL_TIME_LINK_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(leftClm, 154.524f), module, ADSDREnvelopeModule::A_TIME_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerClm, 154.524f), module, ADSDREnvelopeModule::DC_TIME_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(rightClm, 154.524f), module, ADSDREnvelopeModule::R_TIME_PARAM));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, false>>(Vec(centerLeftClm, 139.489f), module, ADSDREnvelopeModule::DC_TIME_LINK_PARAM));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, false>>(Vec(centerRightClm, 139.489f), module, ADSDREnvelopeModule::R_TIME_LINK_PARAM));
        
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(leftClm, 186.869f), module, ADSDREnvelopeModule::A_SHAPE_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerClm, 186.869f), module, ADSDREnvelopeModule::DC_SHAPE_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(rightClm, 186.869f), module, ADSDREnvelopeModule::R_SHAPE_PARAM));
        addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_green, bc_red>>(
            Vec(centerLeftClm, 172.888f), module, ADSDREnvelopeModule::DC_SHAPE_LINK_PARAM));
        addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_green, bc_red>>(
            Vec(centerRightClm, 172.888f), module, ADSDREnvelopeModule::R_SHAPE_LINK_PARAM));

        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(leftClm, 223.274f), module, ADSDREnvelopeModule::A_LEVEL_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerClm, 223.274f), module, ADSDREnvelopeModule::SUSTAIN_LEVEL_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(rightClm, 223.274f), module, ADSDREnvelopeModule::R_LEVEL_PARAM));

        InfNoiseDisableOverlayManager& overlayManager = getDisableOverlayManager();
        decayLinkTimeOverlayGroup = overlayManager.addGroup("Dc.time linked to A.time");
        decayLinkTimeOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
            ADSDREnvelopeModule::DC_TIME_PARAM
        });
        decayLinkShapeOverlayGroup = overlayManager.addGroup("Dc.shape linked to A.shape");
        decayLinkShapeOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
            ADSDREnvelopeModule::DC_SHAPE_PARAM
        });

        delayLinkTimeOverlayGroup = overlayManager.addGroup("Dl.time linked to A.time");
        delayLinkTimeOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
            ADSDREnvelopeModule::DL_TIME_PARAM
        });

        releaseLinkTimeOverlayGroup = overlayManager.addGroup("R.time linked to A.time");
        releaseLinkTimeOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
            ADSDREnvelopeModule::R_TIME_PARAM
        });
        releaseLinkShapeOverlayGroup = overlayManager.addGroup("R.shape linked to A.shape");
        releaseLinkShapeOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
            ADSDREnvelopeModule::R_SHAPE_PARAM
        });

        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(leftClm, 262.931f), module, ADSDREnvelopeModule::BOA_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(centerClm, 262.931f), module, ADSDREnvelopeModule::BOS_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(rightClm, 262.931f), module, ADSDREnvelopeModule::BOR_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(leftClm, 298.189f), module, ADSDREnvelopeModule::EOA_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(centerClm, 298.189f), module, ADSDREnvelopeModule::EOS_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(rightClm, 298.189f), module, ADSDREnvelopeModule::EOR_OUTPUT));

        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(centerLeftClm, 333.447f), module, ADSDREnvelopeModule::ENVELOPE_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(centerRightClm, 333.447f), module, ADSDREnvelopeModule::INV_ENVELOPE_OUTPUT));
    }

    void step() override {
        InfNoiseModuleWidget::step();

        if (!module)
            return;

        auto* m = static_cast<ADSDREnvelopeModule*>(module);
        if (decayLinkTimeOverlayGroup) {
            if (m->decayTimeLinked != decayTimeLinked) {
                decayTimeLinked = m->decayTimeLinked;
                decayLinkTimeOverlayGroup->setActive(decayTimeLinked);
            }
        }
        if (decayLinkShapeOverlayGroup) {
            if (m->decayShapeLinked != decayShapeLinked) {
                decayShapeLinked = m->decayShapeLinked;
                decayLinkShapeOverlayGroup->setActive(decayShapeLinked);
            }
        }
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
        ADSDREnvelopeModule* module = dynamic_cast<ADSDREnvelopeModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> rateChaosNames = getRateChaosNames();
        menu->addChild(createIndexPtrSubmenuItem("Attack rate chaos", rateChaosNames,
            &module->attackRateChaos.req));
        menu->addChild(createIndexPtrSubmenuItem("Decay rate chaos", rateChaosNames,
            &module->decayRateChaos.req));
        menu->addChild(createIndexPtrSubmenuItem("Delay rate chaos", rateChaosNames,
            &module->delayRateChaos.req));
        menu->addChild(createIndexPtrSubmenuItem("Release rate chaos", rateChaosNames,
            &module->releaseRateChaos.req));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelADSDREnvelope = createModel<ADSDREnvelopeModule, ADSDREnvelopeModuleWidget>("ADSDREnvelope");