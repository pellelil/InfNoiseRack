// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct SLFO4ssModule : InfNoiseModule {
    enum ParamId {
        FREQKNOB1_PARAM,
        FREQKNOB2_PARAM,
        FREQKNOB3_PARAM,
        FREQKNOB4_PARAM,
        RANGE1_TOGGLE_PARAM,
        RANGE2_TOGGLE_PARAM,
        RANGE3_TOGGLE_PARAM,
        RANGE4_TOGGLE_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        INPUTS_LEN
    };
    enum OutputsId {
        WAVE1_1ST_OUTPUT,
        WAVE1_2ND_OUTPUT,
        WAVE1_3RD_OUTPUT,
        WAVE1_4TH_OUTPUT,
        WAVE2_1ST_OUTPUT,
        WAVE2_2ND_OUTPUT,
        WAVE2_3RD_OUTPUT,
        WAVE2_4TH_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        FREQ1_LIGHT,
        FREQ2_LIGHT,
        FREQ3_LIGHT,
        FREQ4_LIGHT,
        ENUMS(SYNC2_LIGHT, 2),  // Indicate LFO-2 is synced
        ENUMS(SYNC3_LIGHT, 2),  // Indicate LFO-3 is synced
        ENUMS(SYNC4_LIGHT, 2),  // Indicate LFO-4 is synced
        INV1_LIGHT,
        INV2_LIGHT,
        INV3_LIGHT,
        INV4_LIGHT,
        LIGHTS_LEN
    };

    bool haveOutputs = false; // True if any LFO is in use
    bool lfoInUse[4] = { false, false, false, false }; // LFO's in use (have outputs, or is sync-master)
    float sampleRate = 44100.f;  // Re-obtained in processParams
    float sampleTime = 1.f / 44100.f;  // Re-obtained in processParams
    float phase[4] = { 0.f, 0.f, 0.f, 0.f }; // Phase of each LFO (0-1)
    float freq[4] = { 2.f, 2.f, 2.f, 2.f }; // Frequency of each LFO (Hz)
    float phaseStep[4] = { sampleTime, sampleTime, sampleTime, sampleTime }; // Phase-step of each LFO (per process)
    float waveScale[4] = { 10.f, 10.f, 10.f, 10.f }; // Scale of each LFO (based on range)
    float waveOffset[4] = { -5.f, -5.f, -5.f, -5.f };  // Offset of each LFO (based on range)
    float prevRangeToggle[4] = { -1.f, -1.f, -1.f, -1.f };  // Detects range-toggle change (to update lfoRange)
    float syncSign[4] = { 1.f, 1.f, 1.f, 1.f }; // Sign for soft-sync (1 or -1), [0] is LFO1 hence always 1
    float chaosAmount[4] = { 0.f, 0.f, 0.f, 0.f }; // Cached rate-chaos amount (0-1) per LFO
    float chaosFactor[4] = { 1.f, 1.f, 1.f, 1.f }; // Current phase-step factor per LFO (new each cycle)

    actReqValue<bool> phaseLights = actReqValue<bool>(true);
    enum lfoSyncMode { slm_off, slm_2to1, slm_4to3, slm_2to1_4to3, slm_234To1, slm_34to12, slm_4to123 };
    actReqValue<lfoSyncMode> syncLfoMode = actReqValue<lfoSyncMode>(slm_off);
    enum syncModeType { sm_hard, sm_soft };
    actReqValue<syncModeType> syncMode = actReqValue<syncModeType>(sm_hard);
    actReqValue<voltRange> lfoRange[4] = {
        actReqValue<voltRange>((voltRange)vr_Bipolar),
        actReqValue<voltRange>((voltRange)vr_Bipolar),
        actReqValue<voltRange>((voltRange)vr_Bipolar),
        actReqValue<voltRange>((voltRange)vr_Bipolar)
    };
    actReqValue<bool> lfoInvert[4] = {
        actReqValue<bool>(false),
        actReqValue<bool>(false),
        actReqValue<bool>(false),
        actReqValue<bool>(false)
    };
    actReqValue<rateChaos> lfoRateChaos[4] = {
        actReqValue<rateChaos>(rc_default),
        actReqValue<rateChaos>(rc_default),
        actReqValue<rateChaos>(rc_default),
        actReqValue<rateChaos>(rc_default)
    };


    SLFO4ssModule() : InfNoiseModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        for (int i = 0; i < 4; i++) {
            configParam<infNoiseLfoFreqQnt>(FREQKNOB1_PARAM + i, -8.f, 10.f, 1.f, string::f("LFO-%d Frequency", i + 1), " Hz", 2, 1);
            configSwitch(RANGE1_TOGGLE_PARAM + i, 0.0, 2.0, 2.0, string::f("LFO-%d Output-range", i + 1), { "Manual (via menu)", "Bipolar (-5 to 5)", "Unipolar (0 to 10)" });

            configOutput(WAVE1_1ST_OUTPUT + i, string::f("LFO-%d Saw-waveform", i + 1));
            configOutput(WAVE2_1ST_OUTPUT + i, string::f("LFO-%d Sine-waveform", i + 1));

            configLight(FREQ1_LIGHT + i, string::f("LFO-%d Phase", i + 1));
            configLight(INV1_LIGHT + i, string::f("LFO-%d inverted (if lit)", i + 1));
        }

        configLight(SYNC2_LIGHT, "LFO-2 synced if lit");
        configLight(SYNC3_LIGHT, "LFO-3 synced if lit");
        configLight(SYNC4_LIGHT, "LFO-4 synced if lit");

		// Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
        haveAutoProcQuality = true;
        haveOutClipRange = true;  // But user might want to choose other ranges?
        haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = false;
		haveTrigHighLow = false;
        autoProcQuality.setBoth(true);
        outClipRange.setBoth(vr_off);  // By default on, but output can't exceed -+12V
    }

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);

        outClipRange.setBoth(vr_off);  // Override default (can't exceed -+12V)

        syncLfoMode.setBoth(slm_off);
        syncMode.setBoth(sm_hard);

        phaseLights.setBoth(true);
        for (int i = 0; i < 4; i++) {
            lfoInUse[i] = false;
            prevRangeToggle[i] = -1.f;  // Force update according to actual toggle-value

            freq[i] = 2.f;
            phase[i] = 0.f;
            phaseStep[i] = sampleTime;
            syncSign[i] = 1.f;
            chaosAmount[i] = 0.f;
            chaosFactor[i] = 1.f;
            lfoRange[i].setBoth((voltRange)vr_Bipolar);
            waveScale[i] = voltRangeNormScale[lfoRange[i].req];
            waveOffset[i] = voltRangeNormOffset[lfoRange[i].req];
            lfoInvert[i].setBoth(false);
            lfoRateChaos[i].setBoth(rc_default);
        }
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        syncLfoMode.setBoth((lfoSyncMode) getJsonInt(rootJ, "syncLfoMode", (int)lfoSyncMode::slm_off));
        syncMode.setBoth((syncModeType)getJsonInt(rootJ, "syncMode", (int)sm_hard));
        phaseLights.setBoth(getJsonBool(rootJ, "phaseLights", true));

        int lfoRangeTmp[4];
        bool lfoInvertTmp[4];
        int lfoRateChaosTmp[4];
        getJsonIntArray(rootJ, "lfoRange", lfoRangeTmp, 4, (int)vr_Bipolar);
        getJsonBoolArray(rootJ, "lfoInvert", lfoInvertTmp, 4, false);
        getJsonIntArray(rootJ, "lfoRateChaos", lfoRateChaosTmp, 4, (int)rc_default);
        for (int i = 0; i < 4; i++) {
            lfoRange[i].setBoth((voltRange)lfoRangeTmp[i]);
            lfoInvert[i].setBoth(lfoInvertTmp[i]);
            lfoRateChaos[i].setBoth((rateChaos)lfoRateChaosTmp[i]);
        }
        getJsonFloatArray(rootJ, "phase", phase, 4, 0.f);
        getJsonFloatArray(rootJ, "syncSign", syncSign, 4, 1.f);
        getJsonFloatArray(rootJ, "chaosFactor", chaosFactor, 4, 1.f);
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "syncLfoMode", json_integer((int)syncLfoMode.req));
        json_object_set_new(rootJ, "syncMode", json_integer((int)syncMode.req));
        json_object_set_new(rootJ, "phaseLights", json_boolean(phaseLights.req));

        int lfoRangeTmp[4];
        bool lfoInvertTmp[4];
        int lfoRateChaosTmp[4];
        for (int i = 0; i < 4; i++) {
            lfoRangeTmp[i] = (int)lfoRange[i].req;
            lfoInvertTmp[i] = lfoInvert[i].req;
            lfoRateChaosTmp[i] = (int)lfoRateChaos[i].req;
        }
        setJsonIntArray(rootJ, "lfoRange", lfoRangeTmp, 4);
        setJsonBoolArray(rootJ, "lfoInvert", lfoInvertTmp, 4);
        setJsonIntArray(rootJ, "lfoRateChaos", lfoRateChaosTmp, 4);
        setJsonFloatArray(rootJ, "phase", phase, 4);
        setJsonFloatArray(rootJ, "syncSign", syncSign, 4);
        setJsonFloatArray(rootJ, "chaosFactor", chaosFactor, 4);
    }

    void setLightColor(int lightId, bool red, bool green)
    {
        lights[lightId].value = green ? 1.0f : 0.f;
        lights[lightId + 1].value = red ? 1.0f : 0.f;
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        if (syncLfoMode.needsUpdate()) {
            syncLfoMode.updateActual();

            setLightColor(SYNC2_LIGHT, false, false);
            setLightColor(SYNC3_LIGHT, false, false);
            setLightColor(SYNC4_LIGHT, false, false);

            // Set green light for LFO-2, LFO-3, LFO-4 if synced
            if (syncLfoMode.act == slm_2to1 || syncLfoMode.act == slm_2to1_4to3 || syncLfoMode.act == slm_234To1)
                setLightColor(SYNC2_LIGHT, false, true);
            if (syncLfoMode.act == slm_234To1)
                setLightColor(SYNC3_LIGHT, false, true);
            if (syncLfoMode.act == slm_4to3 || syncLfoMode.act == slm_2to1_4to3 || syncLfoMode.act == slm_234To1)
                setLightColor(SYNC4_LIGHT, false, true);

            // Set red light for LFO-3, LFO-4 if synced
            if (syncLfoMode.act == slm_34to12)
                setLightColor(SYNC3_LIGHT, true, false);
            if ((syncLfoMode.act == slm_34to12) || (syncLfoMode.act == slm_4to123))
                setLightColor(SYNC4_LIGHT, true, false);
        }

        if (syncMode.needsUpdate()) {
			syncMode.updateActual();
            if (syncMode.act == sm_hard) {
                // syncSign[0] is LFO1, hence always 1
                syncSign[1] = 1.f;  // LFO 2 not synced
                syncSign[2] = 1.f;  // LFO 3 not synced
                syncSign[3] = 1.f;  // LFO 4 not synced
            }
            else { // Soft-sync
                //enum lfoSyncMode { slm_off, slm_2to1, slm_4to3, slm_2to1_4to3, slm_234To1 };
                if (syncLfoMode.act == slm_off || syncLfoMode.act == slm_4to3)
                    syncSign[1] = 1.f;  // LFO 2 not synced
                if (syncLfoMode.act != slm_234To1)
                    syncSign[2] = 1.f;  // LFO 3 not synced
                if (syncLfoMode.act != slm_off && syncLfoMode.act != slm_2to1)
                    syncSign[3] = 1.f;  // LFO 4 is not synced
            }
		}

        sampleRate = safeSampleRate(args.sampleRate);
        sampleTime = 1.f / sampleRate;
        haveOutputs = false;
        
        for (int i = 0; i < 4; i++) {
            // Set booleans for output-usage
            lfoInUse[i] = outputs[WAVE1_1ST_OUTPUT + i].isConnected() ||
                outputs[WAVE2_1ST_OUTPUT + i].isConnected();
            if (lfoInUse[i])
                haveOutputs = true;

            // Update frequencies to knobs
            float clockFreq = 2.f;  // 2 Hz
            float pitch = params[FREQKNOB1_PARAM + i].getValue();
            freq[i] = clockFreq / 2.f * dsp::exp2_taylor5(pitch);

            // Updates toggle-values and wave-Scale/Offset
            float toggleValue = params[RANGE1_TOGGLE_PARAM + i].getValue();
            if (!lfoRange[i].needsUpdate() && toggleValue != prevRangeToggle[i]) {
                if (toggleValue == 0) { // Manual
                    // Cannot be manually selected
                    lfoRange[i].setBoth((voltRange)vr_mp5);
                }
                else if (toggleValue == 1) { // Bipolar
                    lfoRange[i].setBoth((voltRange)vr_mp5);
                }
                else if (toggleValue == 2) { // Unipolar
                    lfoRange[i].setBoth((voltRange)vr_zt10);
                }
            }
            
            if (lfoRange[i].needsUpdate()) {
                lfoRange[i].updateActual();
                
                voltRange rangeIdx = (voltRange)lfoRange[i].act;
                waveScale[i] = voltRangeNormScale[rangeIdx];
                waveOffset[i] = voltRangeNormOffset[rangeIdx];

                // Update range-toggle
                if (rangeIdx == vr_zt10) {
                    params[RANGE1_TOGGLE_PARAM + i].setValue(2);
                    prevRangeToggle[i] = 2;
                }
                else if (rangeIdx == vr_mp5) {
                    params[RANGE1_TOGGLE_PARAM + i].setValue(1);
                    prevRangeToggle[i] = 1;
                }
                else {
                    params[RANGE1_TOGGLE_PARAM + i].setValue(0);
                    prevRangeToggle[i] = 0;
                }
            }
            
            // Update invert
            lfoInvert[i].updateActual();
            lights[INV1_LIGHT + i].setBrightness(lfoInvert[i].act ? 1.0f : 0.f);

            // Rate-chaos (cache amount for the per-sample path)
            lfoRateChaos[i].updateActual();
            chaosAmount[i] = rateChaosValues[lfoRateChaos[i].act];
        }

        // Update output-usage according to sync-mode
        if (haveOutputs) {
            switch (syncLfoMode.act) {
                case slm_off:
                    break;
                case slm_2to1:
                    lfoInUse[0] = lfoInUse[0] || lfoInUse[1];
                    break;
                case slm_4to3:
                    lfoInUse[2] = lfoInUse[2] || lfoInUse[3];
                    break;
                case slm_2to1_4to3:
                    lfoInUse[0] = lfoInUse[0] || lfoInUse[1];
                    lfoInUse[2] = lfoInUse[2] || lfoInUse[3];
                    break;
                case slm_234To1:
                    lfoInUse[0] = lfoInUse[0] || lfoInUse[1] || lfoInUse[2] || lfoInUse[3];
                    break;
                case slm_34to12:
                    lfoInUse[0] = lfoInUse[0] || lfoInUse[2] || lfoInUse[3];
                    lfoInUse[1] = lfoInUse[1] || lfoInUse[2] || lfoInUse[3];
                    break;
                case slm_4to123:
                    lfoInUse[0] = lfoInUse[0] || lfoInUse[3];
                    lfoInUse[1] = lfoInUse[1] || lfoInUse[3];
                    lfoInUse[2] = lfoInUse[2] || lfoInUse[3];
                    break;
            }
        }

        // Ensure freq-lights go off for unused LFO's, or if disabled
        phaseLights.updateActual();
        for (int i = 0; i < 4; i++) {
            if (!lfoInUse[i] || !phaseLights.act)
                lights[FREQ1_LIGHT + i].setBrightness(0.f);
        }

        // Highest Freq if auto-quality
        if (autoProcQuality.act) {
            if (haveOutputs) {
                float highestFreq = 0.f;
                if (lfoInUse[0]) highestFreq = freq[0] * rateChaosMaxFactor[lfoRateChaos[0].act];
                if (lfoInUse[1]) highestFreq = std::max(highestFreq, freq[1] * rateChaosMaxFactor[lfoRateChaos[1].act]);
                if (lfoInUse[2]) highestFreq = std::max(highestFreq, freq[2] * rateChaosMaxFactor[lfoRateChaos[2].act]);
                if (lfoInUse[3]) highestFreq = std::max(highestFreq, freq[3] * rateChaosMaxFactor[lfoRateChaos[3].act]);
                procQuality.setBoth(getEstimatedLfoProcessQuality(sampleRate, highestFreq), false);
            }
            else
                procQuality.setBoth(pq_veryLowRate, false); // No outputs
        }

        float cycleStep = processQualityCycles[procQuality.act];
        phaseStep[0] = (freq[0] * cycleStep) / sampleRate;
        phaseStep[1] = (freq[1] * cycleStep) / sampleRate;
        phaseStep[2] = (freq[2] * cycleStep) / sampleRate;
        phaseStep[3] = (freq[3] * cycleStep) / sampleRate;

        //--------------------
        postProcessParams(args);
    }

    inline void syncLfo(float& phase, float& sign)
    {
        if (syncMode.act == sm_hard) // Hard-sync
            phase = 0.f;
        else  // Soft-sync
            sign *= -1.f;
    }

    void process(const ProcessArgs& args) override {
        bool doProcessParams = mustProcessParams || 
            ((cycle256 & patternProcessParams) == patternProcessParams);
        if (doProcessParams)
            processParams(args);

        bool doProcess = (doProcessParams ||
            ((cycle256 & processQualityPatterns[procQuality.act]) == processQualityPatterns[procQuality.act]));

        if (doProcess && haveOutputs) {
            float output;
            bool doSync[4] = { false, false, false, false };  // Sync LFO's 2, 3, 4 (1 is always false)
            for (int i = 0; i < 4; i++) {
                if (lfoInUse[i]) {
                    // Update freq-lights accoring to phase and frequency
                    if (phaseLights.act) {
                        lights[FREQ1_LIGHT + i].setBrightness(
                            freq[i] >= 60.f
                                ? 1.0f
                                : phase[i] < 0.75f
                                    ? 1.f - phase[i]
                                    : 0.f);
                    }

                    // Output Waveform-1: Saw (down)
                    if (outputs[WAVE1_1ST_OUTPUT + i].isConnected()) {
                        output = (lfoInvert[i].act)
                            ? phase[i] * waveScale[i] + waveOffset[i]
                            : (1.f - phase[i]) * waveScale[i] + waveOffset[i];
                        output = clipToVoltRange(output, outClipRange.act);
                        outputs[WAVE1_1ST_OUTPUT + i].setVoltage(output);
                    }

                    // Output Waveform-2: Sine
                    if (outputs[WAVE2_1ST_OUTPUT + i].isConnected()) {
                        output = (lfoInvert[i].act)
                            ? bSinNorm(1.f - phase[i]) * waveScale[i] + waveOffset[i]
                            : bSinNorm(phase[i]) * waveScale[i] + waveOffset[i];
                        output = clipToVoltRange(output, outClipRange.act);
                        outputs[WAVE2_1ST_OUTPUT + i].setVoltage(output);
                    }

                    // Increment phase, and handle sync-mode
                    phase[i] += phaseStep[i] * syncSign[i] * chaosFactor[i];
                    if (phase[i] < 0.f || phase[i] >= 1.f) {
                        phase[i] -= std::floor(phase[i]);
                        chaosFactor[i] = rateChaosFactor(chaosAmount[i]); // New factor each completed cycle
                        if (i == 0) {
                            if (syncLfoMode.act == slm_234To1) {
                                doSync[1] = true;
                                doSync[2] = true;
                                doSync[3] = true;
                            }
                            else if (syncLfoMode.act == slm_2to1 || syncLfoMode.act == slm_2to1_4to3) {
                                doSync[1] = true;
                            }
                            else if (syncLfoMode.act == slm_34to12) {
								doSync[2] = true;
								doSync[3] = true;
                            }
                        }
                        else if (i == 1 && (syncLfoMode.act == slm_34to12)) {
                            doSync[2] = true;
                            doSync[3] = true;
                        }
                        else if (i == 2 && (syncLfoMode.act == slm_4to3 || syncLfoMode.act == slm_2to1_4to3)) {
                            doSync[3] = true;
                        }
                        if (syncLfoMode.act == slm_4to123 && (i < 3))  // no else here !!!
                            doSync[3] = true;
                    }

                    if (doSync[i]) {
                        syncLfo(phase[i], syncSign[i]);
                        if (syncMode.act == sm_hard) // Hard-sync resets phase -> treat as new cycle
                            chaosFactor[i] = rateChaosFactor(chaosAmount[i]);
                    }
                }
            }
        }

        cycle256++;
    }
};

struct SLFO4ssModuleWidget : InfNoiseModuleWidget {
    SLFO4ssModuleWidget(SLFO4ssModule *module) {
        initializeWidget(module, "res/SLFO4ss");

        float freqKnobCol = 39.985f;
        float freqKnobRow = 57.587f;
        float freqKnobSpacing = 45.524f;
        float rangeToggleCol = 7.744f;
        float rangeToggleRow = 57.587f;

        float wave1OutputCol = 14.912f;
        float wave2OutputCol = 43.443f;
        float waveOutputRow = 242.866f;
        float waveOutputSpacing = 29.943f;

        float freqLightCol = 26.183f;  // 4 Freq lights 
        float freqLightRow = 39.669f;
        float syncLightCol = 55.390f;  // 3 Sync lights (LFO-1 is not synced to anything)
        float syncLightRow = 78.031f;

        for (int i = 0; i < 4; i++)	{
            // Freq-knobs
            addParam(createParamCentered<RoundLargeBlackKnob>(Vec(freqKnobCol, freqKnobRow), module, SLFO4ssModule::FREQKNOB1_PARAM + i));
            freqKnobRow += freqKnobSpacing;

            // Output-range toggles
            addParam(createParamCentered<CKSSThree>(Vec(rangeToggleCol, rangeToggleRow), module, SLFO4ssModule::RANGE1_TOGGLE_PARAM + i));
            rangeToggleRow += freqKnobSpacing;

            // Waveform outputs
			addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(wave1OutputCol, waveOutputRow), module, SLFO4ssModule::WAVE1_1ST_OUTPUT + i));
			addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(wave2OutputCol, waveOutputRow), module, SLFO4ssModule::WAVE2_1ST_OUTPUT + i));
            addChild(createLightCentered<TinyLight<RedLight>>(Vec(29.578f, waveOutputRow + 10.421f), module, SLFO4ssModule::INV1_LIGHT + i));
            waveOutputRow += waveOutputSpacing;

            // Freq-lights
            addChild(createLightCentered<SmallLight<GreenLight>>(Vec(freqLightCol, freqLightRow), module, SLFO4ssModule::FREQ1_LIGHT + i));
            freqLightRow += freqKnobSpacing;

            // Sync-lights
            if (i < 3) {  // Only 3 sync-lights
                int syncLightIdx = i * 2;
                addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(syncLightCol, syncLightRow), module, SLFO4ssModule::SYNC2_LIGHT + syncLightIdx));
                syncLightRow += freqKnobSpacing;
            }
		}
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        SLFO4ssModule* module = dynamic_cast<SLFO4ssModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> outputRangeNames = getVoltRangesNames(false);
        std::vector<std::string> rateChaosNames = getRateChaosNames();
        for (int i = 0; i < 4; i++) {
            const int vrOffOffset = 1; // vr[0] ("off") excluded from lfo-range menu
            menu->addChild(createIndexSubmenuItem(string::f("LFO-%d Range", i + 1), outputRangeNames,
                [=]() {
                    return (int)(module->lfoRange[i].req - vrOffOffset);
                },
                [=](int range) {
                    module->lfoRange[i].req = (voltRange)(range + vrOffOffset);
                }
            ));
            menu->addChild(createBoolPtrMenuItem(string::f("LFO-%d Invert", i + 1), "", &module->lfoInvert[i].req));
            menu->addChild(createIndexPtrSubmenuItem(string::f("LFO-%d Rate chaos", i + 1), rateChaosNames,
                &module->lfoRateChaos[i].req));
        }

        menu->addChild(new MenuSeparator);

        std::vector<std::string> syncLfoModeNames = { "Off", "Sync 2 to 1", "Sync 4 to 3",
            "Sync 2 to 1, and 4 to 3", "Sync 2, 3, 4 to 1",
            "Sync 3 and 4 to 1 and 2", "Sync 4 to 1, 2 and 3" };
        menu->addChild(createIndexPtrSubmenuItem("Sync LFO-mode", syncLfoModeNames,
            &module->syncLfoMode.req));

        std::vector<std::string> syncModeNames = { "Hard-sync", "Soft-sync" };
        menu->addChild(createIndexPtrSubmenuItem("Sync-mode", syncModeNames,
            &module->syncMode.req));

        menu->addChild(createIndexPtrSubmenuItem("Phase/frequency-lights",
            {"Disabled", "Enabled"},
            &module->phaseLights.req));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelSLFO4ss = createModel<SLFO4ssModule, SLFO4ssModuleWidget>("SLFO4ss");
