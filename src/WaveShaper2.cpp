// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"
#include "dsp/resampler.hpp"

struct WaveShaper2Module : InfNoiseModule {
    enum ParamId {
        MOD_PARAM,
        MOD_TRIM_PARAM,
        SHAPER_MODE_PARAM,
        VAL_RANGE_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        MOD_INPUT,
        RESET_INPUT,
        A_INPUT,
        B_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        A_OUTPUT,
        B_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT, 2),
        ENUMS(CLIP_RANGE_LIGHT, 2),
        LIGHTS_LEN
    };

    enum valueRangeType { vr_auto, vr_bipolar, vr_unipolar };
    valueRangeType valRange = valueRangeType::vr_auto;
    enum shaperModeType { sm_fiveSine, sm_knee, sm_clmpFld };
    shaperModeType shaperMode = sm_fiveSine;
    enum oversampModeType { os_single, os_2x };
    actReqValue<oversampModeType> oversampMode = actReqValue<oversampModeType>(os_single);
    bool haveOutput = false;
    int channels[2] = { 0, 0 };
    autoScaleData scaleData[3] = { autoScaleData(), autoScaleData(), autoScaleData() }; // scaleData[2] used for common, 0 and 1 for A and B
    enum sectionSelectionType { ss_individual, ss_common };
    actReqValue<sectionSelectionType> sectSelection = actReqValue<sectionSelectionType>(ss_individual);
    dsp::TSchmittTrigger<float> resetTrigger;
    bool resetUnused = false; // processParams; widget overlay (Reset unused except Automatic range)
    static constexpr int maxChannels = PORT_MAX_CHANNELS;
    dsp::Upsampler<2, 8> upsampler[2][maxChannels];
    dsp::Decimator<2, 8> decimator[2][maxChannels];

    static inline float applyModulation(float normValue, float mod, float modAbs, shaperModeType shaperMode) {
        float absNormValue = std::fabs(normValue);
        if (modAbs < 0.00001f || absNormValue < 0.00001f)
            return normValue;

        float signNormValue = (normValue >= 0.f) ? 1.f : -1.f;
        if (shaperMode == sm_fiveSine) {
            float sinValue = (mod > 0.f)
                ? fiveSineLogIsh(absNormValue)
                : fiveSineExpIsh(absNormValue);
            float valueFactor = 1.f - modAbs;
            return ((absNormValue * valueFactor) + (sinValue * modAbs)) * signNormValue;
        }
        else if (shaperMode == sm_knee) {
            float x = absNormValue;
            const float low = 0.15f;
            const float high = 1 - low;
            float effectValue = (mod > 0.f)
                ? (x <= low) ? (x * (high / low)) : (high + (x - low) * (low / high))  
                : (x <= high) ? (x * (low / high)) : (low + (x - high) * (high / low));   
            float valueFactor = 1.f - modAbs;
            return ((absNormValue * valueFactor) + (effectValue * modAbs)) * signNormValue;
        }
        else if (shaperMode == sm_clmpFld) {
            float result = absNormValue;
            if (mod > 0.f) { // Fold
                result = absNormValue * (1.f + (modAbs * 5.f));
                float intPart;
                float frac = std::modf(result, &intPart);
                bool isOdd = ((int)intPart % 2) == 1;
                result = (isOdd)
                    ? (1.f - frac) * signNormValue
                    : frac * signNormValue;
                return result;
            }
            else { // Clamp 
                result = clamp(normValue * (1.f + (modAbs * 5.f)), -1.f, 1.f);
                return result;
            }
        }

        return normValue;
    }
    
	WaveShaper2Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configParam(MOD_PARAM, -1.f, 1.f, 0.f, "Modulation", "", 0, 1);
        configParam(MOD_TRIM_PARAM, -1.f, 1.f, 0.f, "Modulaiton CV-trim (-100% to +100%)", " %", 0, 100);
        configInput(MOD_INPUT, "Modulaiton CV");

        configSwitch(SHAPER_MODE_PARAM, 0.0, 2.0, 0.0, "Shaper-mode", { "5Sine", "Knee", "Clamp/Fold" });

        configSwitch(VAL_RANGE_PARAM, 0.0, 2.0, 0.0, "Value-range", { "Automatic", "Bipolar (-5V/+5V)", "Unipolar (0V/+10V)" });

        configInput(RESET_INPUT, "Reset automatic value-range (trigger)");

        configInput(A_INPUT, "A");
        configInput(B_INPUT, "B");

        configOutput(A_OUTPUT, "A modulated");
        configOutput(B_OUTPUT, "B modulated");

        configBypass(A_INPUT, A_OUTPUT);
        configBypass(B_INPUT, B_OUTPUT);

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
		haveAutoProcQuality = false;
        haveOutQuantize = true;
        haveOutClipRange = true;  
		haveGateDetect = false;
		haveGateHighLow = false;
        haveTrigDetect = false;
		haveTrigHighLow = false;

        ensureFiveSineExpLogLuts();
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        
        resetTrigger.reset();
        sectSelection.setBoth(sectionSelectionType::ss_individual);
        scaleData[0].reset();
        scaleData[1].reset();
        scaleData[2].reset();
        oversampMode.setBoth(os_single);
        for (int i = 0; i < 2; i++) {
            for (int c = 0; c < maxChannels; c++) {
                upsampler[i][c].reset();
                decimator[i][c].reset();
            }
        }
    }

    void onSampleRateChange() override {
        InfNoiseModule::onSampleRateChange();
        for (int i = 0; i < 2; i++) {
            for (int c = 0; c < maxChannels; c++) {
                upsampler[i][c].reset();
                decimator[i][c].reset();
            }
        }
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        sectSelection.setBoth((sectionSelectionType)getJsonInt(rootJ, "sectSelection", (int)ss_individual));
        oversampMode.setBoth((oversampModeType)getJsonInt(rootJ, "oversampMode", (int)os_single));
        scaleData[0].Load(rootJ, "scaleA");
        scaleData[1].Load(rootJ, "scaleB");
        scaleData[2].Load(rootJ, "scaleC");
        resetTrigger.reset();
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "sectSelection", json_integer((int)sectSelection.req));
        json_object_set_new(rootJ, "oversampMode", json_integer((int)oversampMode.req));
        scaleData[0].Save(rootJ, "scaleA");
        scaleData[1].Save(rootJ, "scaleB");
        scaleData[2].Save(rootJ, "scaleC");
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        if (oversampMode.needsUpdate()) {
            oversampMode.updateActual();
            for (int i = 0; i < 2; i++) {
                for (int c = 0; c < maxChannels; c++) {
                    upsampler[i][c].reset();
                    decimator[i][c].reset();
                }
            }
        }

        // Shaper-mode
        shaperMode = sm_knee;
        if (params[SHAPER_MODE_PARAM].getValue() < 0.5f)
			shaperMode = sm_fiveSine;
		else if (params[SHAPER_MODE_PARAM].getValue() > 1.5)
			shaperMode = sm_clmpFld;

        // Value-range
        valueRangeType newValRange = vr_bipolar;
        if (params[VAL_RANGE_PARAM].getValue() < 0.5f)
            newValRange = vr_auto;
        else if (params[VAL_RANGE_PARAM].getValue() > 1.5)
            newValRange = vr_unipolar;

        if (!wasJustLoaded && valRange == vr_auto && newValRange != vr_auto) {
            scaleData[0].reset();
            scaleData[1].reset();
            scaleData[2].reset();
        }
        valRange = newValRange;
        resetUnused = (valRange != vr_auto); 

        // Outputs
        haveOutput = outputs[A_OUTPUT].isConnected() || outputs[B_OUTPUT].isConnected();
        channels[0] = inputs[A_INPUT].isConnected() 
            ? inputs[A_INPUT].getChannels() 
            : 1;
        outputs[A_OUTPUT].setChannels(channels[0]);
        channels[1] = inputs[B_INPUT].isConnected() 
            ? inputs[B_INPUT].getChannels() 
            : 1;
        outputs[B_OUTPUT].setChannels(channels[1]);

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

        if (doProcess && haveOutput) {
            // check for reset
            float voltage = (inputs[RESET_INPUT].isConnected())
                ? inputs[RESET_INPUT].getVoltage()
                : 0.f;
            if (resetTrigger.process(voltage,
                trueDetectValues[td_triggerLow], trueDetectValues[td_triggerHigh])) {
                scaleData[0].reset(); // A-section
                scaleData[1].reset(); // B-section
                scaleData[2].reset(); // Common section
            }

            // Get initial modulation value (if it does not change during the loop)
            bool updateMod = false;
            float mod = params[MOD_PARAM].getValue();
            if (inputs[MOD_INPUT].isConnected()) {
                mod += params[MOD_TRIM_PARAM].getValue() * inputs[MOD_INPUT].getVoltage() / 5.f;
                mod = clamp(mod, -1.f, 1.f);
                updateMod = inputs[MOD_INPUT].getChannels() > 1;
            }
            float modAbs = fabs(mod);

            // Loop through A and B sections (and channels for each
            for (int i = 0; i < 2; i++) {
                int sectIdx = (sectSelection.req == ss_common) ? 2 : i;
                if (outputs[A_OUTPUT + i].isConnected()) {
                    for (int c = 0; c < channels[i]; c++) {
                        // Get modulation value for all channels
                        if (updateMod) {
                            mod = params[MOD_PARAM].getValue();
                            if (inputs[MOD_INPUT].isConnected()) {
                                mod += params[MOD_TRIM_PARAM].getValue() * inputs[MOD_INPUT].getPolyVoltage(c) / 5.f;
                                mod = clamp(mod, -1.f, 1.f);
                            }
                            modAbs = fabs(mod);
                        }

                        // Get value and clamp/scale, and normalized to range -1 to +1
                        float voltage = inputs[A_INPUT + i].getPolyVoltage(c);
                        float normValue; // Voltage normalized to -1 to +1
                        if (valRange == vr_bipolar)
                            normValue = clamp(voltage, -5.f, 5.f) / 5.f;
                        else if (valRange == vr_unipolar)
                            normValue = (clamp(voltage, 0.f, 10.f) - 5.f) / 5.f;
                        else {
                            int sectIdx = (sectSelection.req == ss_common) ? 2 : i;
                            scaleData[sectIdx].updateScaleOffset(voltage, -1, +1);
                            normValue = scaleData[sectIdx].getScaledValue(voltage);
                        }

                        if (oversampMode.act == os_single) {
                            normValue = applyModulation(normValue, mod, modAbs, shaperMode);
                        }
                        else {
                            float buf[2];
                            upsampler[i][c].process(normValue, buf);
                            buf[0] = applyModulation(buf[0], mod, modAbs, shaperMode);
                            buf[1] = applyModulation(buf[1], mod, modAbs, shaperMode);
                            normValue = decimator[i][c].process(buf);
                        }

                        // Get voltage from normalized value
                        if (valRange == vr_bipolar)
							voltage = normValue * 5.f;
						else if (valRange == vr_unipolar)
                            voltage = (normValue * 5.f) + 5.f;
                        else 
                            voltage = scaleData[sectIdx].getRawValue(normValue);

                        voltage = quantizeToMode(voltage, outQuantize.act);
                        voltage = clipToVoltRange(voltage, outClipRange.act);
						outputs[A_OUTPUT + i].setVoltage(voltage, c);
                    }
                }
            }
        }

        cycle256++;
    }
};

struct WaveShaper2ModuleWidget : InfNoiseModuleWidget {
    InfNoiseDisableOverlayGroup* resetOverlayGroup = nullptr;
    bool resetUnused = false;

    WaveShaper2ModuleWidget(WaveShaper2Module *module) {
        initializeWidget(module, "res/WaveShaper2");

        // Mod-value
        const float cntrClm = 15.f;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrClm, 50.016f), module, WaveShaper2Module::MOD_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(cntrClm, 77.755f), module, WaveShaper2Module::MOD_TRIM_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 105.844f), module, WaveShaper2Module::MOD_INPUT));

        // Shaper-mode
        const float switchClm = 9.277f;
        addParam(createParamCentered<CKSSThree>(Vec(switchClm, 144.049f), module, WaveShaper2Module::SHAPER_MODE_PARAM));

        // Modulation-range
        addParam(createParamCentered<CKSSThree>(Vec(switchClm, 184.740f), module, WaveShaper2Module::VAL_RANGE_PARAM));

        // Reset automatic value-range
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrClm, 222.638f), module, WaveShaper2Module::RESET_INPUT));

        InfNoiseDisableOverlayManager& overlayManager = getDisableOverlayManager();
        resetOverlayGroup = overlayManager.addGroup("Reset only in Automatic range");
        resetOverlayGroup->addTargets(InfNoiseOverlayTargetType::input, {
            WaveShaper2Module::RESET_INPUT
        });

        // A/B inputs
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 253.823f), module, WaveShaper2Module::A_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 278.455f), module, WaveShaper2Module::B_INPUT));

        // A/B outputs
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 308.106f), module, WaveShaper2Module::A_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 332.738f), module, WaveShaper2Module::B_OUTPUT));
    }

    void step() override {
        InfNoiseModuleWidget::step();

        if (!module)
            return;

        auto* m = static_cast<WaveShaper2Module*>(module);
        if (m->resetUnused != resetUnused) {
            resetUnused = m->resetUnused;
            if (resetOverlayGroup)
                resetOverlayGroup->setActive(resetUnused);
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        WaveShaper2Module* module = dynamic_cast<WaveShaper2Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexPtrSubmenuItem("Auto Scale/offset-mode",
            { "Individual (per section)", "Common (both sections)" }, &module->sectSelection.req
        ));

        std::vector<std::string> oversampNames = { "Single sample", "2x oversampling" };
        menu->addChild(createIndexPtrSubmenuItem("Oversampling mode", oversampNames,
            &module->oversampMode.req));
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelWaveShaper2 = createModel<WaveShaper2Module, WaveShaper2ModuleWidget>("WaveShaper2");