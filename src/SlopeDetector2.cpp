// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

#include <limits>

namespace {

// Index 0 = Off (no steady detection); other entries are hold times in ms.
const float steadyHoldMs[] = {
    std::numeric_limits<float>::max(),
    1.f, 2.f, 5.f, 6.f, 10.f, 25.f, 50.f, 100.f, 250.f, 500.f, 1000.f,
    2000.f, 5000.f, 10000.f
};
const int steadyHoldCount = 15;
const int steadyHoldOffIndex = 0;
const int steadyHoldDefaultIndex = 4; // 6 ms

std::vector<std::string> getSteadyHoldMenuNames(float sampleRate) {
    sampleRate = sampleRate > 0.f ? sampleRate : 44100.f;
    std::vector<std::string> names;
    names.reserve(steadyHoldCount);
    for (int i = 0; i < steadyHoldCount; i++) {
        if (i == steadyHoldOffIndex) {
            names.push_back("Off (no steady detection)");
            continue;
        }
        int cycles = (int)(steadyHoldMs[i] * 0.001f * sampleRate + 0.5f);
        if (i == steadyHoldDefaultIndex)
            names.push_back(string::f("%g ms (%d cycles, default)", steadyHoldMs[i], cycles));
        else if (steadyHoldMs[i] >= 1000.f)
            names.push_back(string::f("%g sec (%d cycles)", steadyHoldMs[i] * 0.001f, cycles));
        else
            names.push_back(string::f("%g ms (%d cycles)", steadyHoldMs[i], cycles));
    }
    return names;
}

// Index 0 = Off (no smoothing); other entries are one-pole lowpass time constants.
const float inputSmoothMs[] = {
    0.f, 0.5f, 1.f, 2.f, 5.f, 6.f, 10.f, 25.f, 50.f, 100.f
};
const int inputSmoothCount = 10;
const int inputSmoothDefaultIndex = 0; // Off

std::vector<std::string> getInputSmoothMenuNames(float sampleRate) {
    sampleRate = sampleRate > 0.f ? sampleRate : 44100.f;
    std::vector<std::string> names;
    names.reserve(inputSmoothCount);
    for (int i = 0; i < inputSmoothCount; i++) {
        if (i == inputSmoothDefaultIndex)
            names.push_back("Off (default)");
        else {
            int cycles = (int)(inputSmoothMs[i] * 0.001f * sampleRate + 0.5f);
            names.push_back(string::f("%g ms (%d cycles)", inputSmoothMs[i], cycles));
        }
    }
    return names;
}

// Index 0 = Off (1x, immediate flip); others require a larger delta to reverse an active slope.
const float reverseFactorVals[] = { 1.f, 1.5f, 2.f, 3.f, 5.f };
const int reverseFactorCount = 5;
const int reverseFactorDefaultIndex = 0; // Off (1x)

std::vector<std::string> getReverseFactorMenuNames() {
    return { "Off (1x, default)", "1.5x", "2x", "3x", "5x" };
}

} // namespace

struct SlopeDetector2Module : InfNoiseModule {
    enum slopePhase { sp_rise, sp_steady, sp_fall };

    enum ParamId {
        THRESHOLD_PARAM,
        RISE_MODE1_PARAM,
        RISE_MODE2_PARAM,
        STEADY_MODE1_PARAM,
        STEADY_MODE2_PARAM,
        FALL_MODE1_PARAM,
        FALL_MODE2_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        CV1_INPUT,
        CV2_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        RISE1_OUTPUT,
        RISE2_OUTPUT,
        STEADY1_OUTPUT,
        STEADY2_OUTPUT,
        FALL1_OUTPUT,
        FALL2_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT, 2),
        ENUMS(CLIP_RANGE_LIGHT, 2),
        TIMING_WARN_LIGHT,
        LIGHTS_LEN
    };

    float threshold = 0.0005f;
    float sampleRate = 44100.f;
    float steadyHoldSec = steadyHoldMs[steadyHoldDefaultIndex] * 0.001f;
    bool steadyDetectionEnabled = true;
    actReqValue<int> steadyHoldIndex = actReqValue<int>(steadyHoldDefaultIndex);
    actReqValue<int> inputSmoothIndex = actReqValue<int>(inputSmoothDefaultIndex);
    actReqValue<int> reverseHystIndex = actReqValue<int>(reverseFactorDefaultIndex);
    float smoothCoef = 1.f; // one-pole coefficient (1 = passthrough/Off)
    float reverseFactor = 1.f; // slope-reversal threshold multiplier (1 = Off)
    bool haveInputs[2] = { false, false };
    bool haveOutputs[2] = { false, false };
    float lastInput[2] = { 0.f, 0.f };
    float smooth[2] = { 0.f, 0.f };
    slopePhase activePhase[2] = { sp_steady, sp_steady };
    float flatTime[2] = { 0.f, 0.f };
    infNoiseOutTrigger outTrig[6] = {
        infNoiseOutTrigger(), // Rise1
        infNoiseOutTrigger(), // Steady1
        infNoiseOutTrigger(), // Fall1
        infNoiseOutTrigger(), // Rise2
        infNoiseOutTrigger(), // Steady2
        infNoiseOutTrigger()  // Fall2
    };

    SlopeDetector2Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
        configLight(TIMING_WARN_LIGHT, "Timing warning (if lit)");

        configParam(THRESHOLD_PARAM, 0.f, 1.f, 0.5f, "Threshold", "", 0, 1);

        for (int i = 0; i < 2; i++) {
            std::string letter = i == 0 ? "A" : "B";
            configInput(CV1_INPUT + i, letter + "-CV");

            configOutput(RISE1_OUTPUT + i, letter + "-Rise");
            configSwitch(RISE_MODE1_PARAM + i, 0.0f, 1.0f, 1.0f, letter + "-Rise output-type",
                { "Trigger when red", "Gate when green" });

            configOutput(STEADY1_OUTPUT + i, letter + "-Steady");
            configSwitch(STEADY_MODE1_PARAM + i, 0.0f, 1.0f, 1.0f, letter + "-Steady output-type",
                { "Trigger when red", "Gate when green" });

            configOutput(FALL1_OUTPUT + i, letter + "-Fall");
            configSwitch(FALL_MODE1_PARAM + i, 0.0f, 1.0f, 1.0f, letter + "-Fall output-type",
                { "Trigger when red", "Gate when green" });
        }

        // Set InfNoise features (e.g. menu-items)
        haveProcQuality = true;
        haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = false;
        haveGateDetect = false;
        haveGateHighLow = true;
        haveTrigDetect = false;
        haveTrigHighLow = true;

        ensureFiveSineExpLogLuts();
    }

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);

        steadyHoldIndex.setBoth(steadyHoldDefaultIndex);
        inputSmoothIndex.setBoth(inputSmoothDefaultIndex);
        reverseHystIndex.setBoth(reverseFactorDefaultIndex);

        threshold = 0.0005f;
        steadyHoldSec = steadyHoldMs[steadyHoldDefaultIndex] * 0.001f;
        steadyDetectionEnabled = true;

        for (int i = 0; i < 2; i++) {
            lastInput[i] = 0.f;
            smooth[i] = 0.f;
            activePhase[i] = sp_steady;
            flatTime[i] = 0.f;

            for (int j = 0; j < 3; j++) {
                int idx = i * 3 + j;
                outTrig[idx].reset();
            }
        }
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);

        int holdIdx = getJsonInt(rootJ, "steadyHoldIndex", steadyHoldDefaultIndex);
        if (holdIdx < 0)
            holdIdx = 0;
        if (holdIdx >= steadyHoldCount)
            holdIdx = steadyHoldCount - 1;
        steadyHoldIndex.setBoth(holdIdx);

        int smoothIdx = getJsonInt(rootJ, "inputSmoothIndex", inputSmoothDefaultIndex);
        if (smoothIdx < 0)
            smoothIdx = 0;
        if (smoothIdx >= inputSmoothCount)
            smoothIdx = inputSmoothCount - 1;
        inputSmoothIndex.setBoth(smoothIdx);

        int reverseIdx = getJsonInt(rootJ, "reverseHystIndex", reverseFactorDefaultIndex);
        if (reverseIdx < 0)
            reverseIdx = 0;
        if (reverseIdx >= reverseFactorCount)
            reverseIdx = reverseFactorCount - 1;
        reverseHystIndex.setBoth(reverseIdx);

        getJsonFloatArray(rootJ, "lastInput", lastInput, 2, 0.f);
        for (int i = 0; i < 2; i++)
            smooth[i] = lastInput[i]; // avoid a startup jump when smoothing is active
        for (int i = 0; i < 6; i++)
            outTrig[i].reset();
    }

    void dataToJson(json_t* rootJ) override {
        InfNoiseModule::dataToJson(rootJ);
        json_object_set_new(rootJ, "steadyHoldIndex", json_integer(steadyHoldIndex.req));
        json_object_set_new(rootJ, "inputSmoothIndex", json_integer(inputSmoothIndex.req));
        json_object_set_new(rootJ, "reverseHystIndex", json_integer(reverseHystIndex.req));
        setJsonFloatArray(rootJ, "lastInput", lastInput, 2);
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        float newSampleRate = safeSampleRate(args.sampleRate);
        bool updateTimingWarning = steadyHoldIndex.needsUpdate()
            || inputSmoothIndex.needsUpdate()
            || reverseHystIndex.needsUpdate()
            || prevProcessQuality != procQuality.act
            || newSampleRate != sampleRate;
        sampleRate = newSampleRate;

        steadyHoldIndex.updateActual();
        int holdIdx = steadyHoldIndex.act;
        if (holdIdx < 0)
            holdIdx = 0;
        if (holdIdx >= steadyHoldCount)
            holdIdx = steadyHoldCount - 1;
        steadyHoldSec = steadyHoldMs[holdIdx] * 0.001f;
        steadyDetectionEnabled = holdIdx != steadyHoldOffIndex;

        inputSmoothIndex.updateActual();
        int smoothIdx = inputSmoothIndex.act;
        if (smoothIdx < 0)
            smoothIdx = 0;
        if (smoothIdx >= inputSmoothCount)
            smoothIdx = inputSmoothCount - 1;
        float smoothTau = inputSmoothMs[smoothIdx] * 0.001f;
        // procSampleTime = seconds per processed evaluation (adapts to process quality).
        smoothCoef = (smoothTau > 0.f) ? (procSampleTime / (smoothTau + procSampleTime)) : 1.f;

        reverseHystIndex.updateActual();
        int reverseIdx = reverseHystIndex.act;
        if (reverseIdx < 0)
            reverseIdx = 0;
        if (reverseIdx >= reverseFactorCount)
            reverseIdx = reverseFactorCount - 1;
        reverseFactor = reverseFactorVals[reverseIdx];

        if (updateTimingWarning) {
            float processCycles = processQualityCycles[procQuality.act];
            float holdSamples = steadyDetectionEnabled ? steadyHoldSec * sampleRate : 0.f;
            float smoothSamples = inputSmoothMs[smoothIdx] * 0.001f * sampleRate;
            bool steadyHoldWarn = steadyDetectionEnabled && holdSamples < processCycles;
            bool inputSmoothWarn = smoothIdx != inputSmoothDefaultIndex
                && smoothSamples < processCycles;

            std::string warningCauses;
            if (steadyHoldWarn)
                warningCauses = "Hold";
            if (inputSmoothWarn)
                warningCauses += warningCauses.empty() ? "Smoothing" : ", Smoothing";

            lights[TIMING_WARN_LIGHT].setBrightness(warningCauses.empty() ? 0.f : 1.f);
            lightInfos[TIMING_WARN_LIGHT]->name = warningCauses.empty()
                ? "Timing warning (if lit)"
                : "Warning: " + warningCauses;
        }

        float thresholdVal = params[THRESHOLD_PARAM].getValue();
        if (thresholdVal <= 0.5f) {
            thresholdVal = thresholdVal / 0.5f; // scale as 0.0 to 1.0
            thresholdVal = fiveSineLogIsh(thresholdVal) * 0.0005f;
        }
        else {
            thresholdVal = (thresholdVal - 0.5f) / 0.5f; // scale as 0.0 to 1.0
            thresholdVal = 0.0005f + fiveSineExpIsh(thresholdVal) * (0.01f - 0.0005f);
        }
        threshold = thresholdVal * processQualityCycles[procQuality.req];

        // Check inputs (CV2 is normalized to CV1)
        haveInputs[0] = inputs[CV1_INPUT].isConnected();
        haveInputs[1] = inputs[CV2_INPUT].isConnected() || haveInputs[0];
        if (!wasJustLoaded) {
            for (int i = 0; i < 2; i++) {
                if (!haveInputs[i]) {
                    lastInput[i] = 0.f;
                    smooth[i] = 0.f;
                    activePhase[i] = sp_steady;
                    flatTime[i] = 0.f;
                }
            }
        }

        // Check outputs
        for (int i = 0; i < 2; i++) {
            bool haveRSF[3]; // Rise/Steady/Fall
            haveRSF[0] = outputs[RISE1_OUTPUT + i].isConnected();
            haveRSF[1] = outputs[STEADY1_OUTPUT + i].isConnected();
            haveRSF[2] = outputs[FALL1_OUTPUT + i].isConnected();
            haveOutputs[i] = haveRSF[0] || haveRSF[1] || haveRSF[2];

            for (int j = 0; j < 3; j++) {
                int triggerIdx = i * 3 + j;
                int prmOutIdx = j * 2 + i;
                if (!haveRSF[j] || params[RISE_MODE1_PARAM + prmOutIdx].getValue() > 0.5) {
                    outTrig[triggerIdx].reset();
                    outputs[RISE1_OUTPUT + prmOutIdx].setVoltage(0.f);
                }
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

        if (doProcess && (haveOutputs[0] || haveOutputs[1])) {
            float normInput = 0.f;
            for (int i = 0; i < 2; i++) {
                if (inputs[CV1_INPUT + i].isConnected())
                    normInput = inputs[CV1_INPUT + i].getVoltage();

                bool phaseBegun[3] = { false, false, false };
                if (haveInputs[i]) {
                    // One-pole smoothing; smoothCoef == 1 is passthrough (Off).
                    smooth[i] += smoothCoef * (normInput - smooth[i]);
                    float delta = smooth[i] - lastInput[i];

                    // Reversing an active slope needs a larger delta (reverseFactor);
                    // starting from Steady always uses the base threshold.
                    float riseThr = (activePhase[i] == sp_fall) ? threshold * reverseFactor : threshold;
                    float fallThr = (activePhase[i] == sp_rise) ? threshold * reverseFactor : threshold;
                    bool riseBeg = delta > 0.f && delta >= riseThr;
                    bool fallBeg = delta < 0.f && (-delta) >= fallThr;

                    slopePhase prevPhase = activePhase[i];
                    if (riseBeg) {
                        activePhase[i] = sp_rise;
                        flatTime[i] = 0.f;
                    }
                    else if (fallBeg) {
                        activePhase[i] = sp_fall;
                        flatTime[i] = 0.f;
                    }
                    else {
                        flatTime[i] += procSampleTime;
                        if (steadyDetectionEnabled && flatTime[i] >= steadyHoldSec
                            && activePhase[i] != sp_steady)

                            activePhase[i] = sp_steady;
                    }

                    phaseBegun[0] = activePhase[i] == sp_rise && prevPhase != sp_rise;
                    phaseBegun[1] = activePhase[i] == sp_steady && prevPhase != sp_steady;
                    phaseBegun[2] = activePhase[i] == sp_fall && prevPhase != sp_fall;

                    lastInput[i] = smooth[i];
                }

                // Generate all connected outputs even when this section has no input.
                if (haveOutputs[i]) {
                    for (int j = 0; j < 3; j++) {  // 0=Rise/1=Steady/2=Fall
                        int prmOutIdx = j * 2 + i;
                        if (outputs[RISE1_OUTPUT + prmOutIdx].isConnected()) {
                            float voltage = 0.f;
                            if (params[RISE_MODE1_PARAM + prmOutIdx].getValue() < 0.5) { // trigger
                                int triggerIdx = i * 3 + j;
                                if (phaseBegun[j])
                                    outTrig[triggerIdx].trigger();
                                outTrig[triggerIdx].process(procSampleTime);
                                voltage = outTrig[triggerIdx].isHigh()
                                    ? voltValues[trigOutHigh.act]
                                    : voltValues[trigOutLow.act];
                            }
                            else { // gate
                                bool active = (j == 0 && activePhase[i] == sp_rise)
                                    || (j == 1 && activePhase[i] == sp_steady)
                                    || (j == 2 && activePhase[i] == sp_fall);
                                voltage = active
                                    ? voltValues[gateOutHigh.act]
                                    : voltValues[gateOutLow.act];
                            }

                            outputs[RISE1_OUTPUT + prmOutIdx].setVoltage(voltage);
                        }
                    }
                }
            }
        }

        cycle256++;
    }
};

struct SlopeDetector2ModuleWidget : InfNoiseModuleWidget {
    SlopeDetector2ModuleWidget(SlopeDetector2Module* module) {
        initializeWidget(module, "res/SlopeDetector2");

        const float cntrClm = 15.f;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrClm, 52.106f), module, SlopeDetector2Module::THRESHOLD_PARAM));
        addChild(createLightCentered<TinyLight<RedLight>>(Vec(cntrClm, 32.875f), module, SlopeDetector2Module::TIMING_WARN_LIGHT));

        const float outOfs = 35.0735f;
        const float typeClmOfs = -8.804f;
        const float typeRowOfs = -13.039f;
        for (int i = 0; i < 2; i++)
        {
            float secOfs = i * 140.421f;
            float row = 87.679f + secOfs;
            addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrClm, row), module, SlopeDetector2Module::CV1_INPUT + i));

            row = 122.753f + secOfs;
            addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(cntrClm, row), module, SlopeDetector2Module::RISE1_OUTPUT + i));
            addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_red, bc_green>>(
                Vec(cntrClm + typeClmOfs, row + typeRowOfs), module, SlopeDetector2Module::RISE_MODE1_PARAM + i));

            row += outOfs;
            addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(cntrClm, row), module, SlopeDetector2Module::STEADY1_OUTPUT + i));
            addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_red, bc_green>>(
                Vec(cntrClm + typeClmOfs, row + typeRowOfs), module, SlopeDetector2Module::STEADY_MODE1_PARAM + i));

            row += outOfs;
            addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(cntrClm, row), module, SlopeDetector2Module::FALL1_OUTPUT + i));
            addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_red, bc_green>>(
                Vec(cntrClm + typeClmOfs, row + typeRowOfs), module, SlopeDetector2Module::FALL_MODE1_PARAM + i));
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        SlopeDetector2Module* module = dynamic_cast<SlopeDetector2Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexPtrSubmenuItem("Input smoothing",
            getInputSmoothMenuNames(module->sampleRate),
            &module->inputSmoothIndex.req));
        menu->addChild(createIndexPtrSubmenuItem("Slope reversal hysteresis",
            getReverseFactorMenuNames(),
            &module->reverseHystIndex.req));
            
        menu->addChild(createIndexPtrSubmenuItem("Steady hold time",
            getSteadyHoldMenuNames(module->sampleRate),
            &module->steadyHoldIndex.req));

        appendInfNoiseMenuItems(menu);
    }
};

Model* modelSlopeDetector2 = createModel<SlopeDetector2Module, SlopeDetector2ModuleWidget>("SlopeDetector2");
