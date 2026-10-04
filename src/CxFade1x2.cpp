// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct CxFade1x2Module : InfNoiseModule {
    enum ParamId {
        CROSSFADE_PARAM,
        CROSSFADE_TRIM_PARAM,
        CROSSFADE_TOGGLE_PARAM,
        CROSSFADE_TRIG_PARAM,
        TOGGLE_CX_MODE_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        CROSSFADE_INPUT,
        A1_INPUT,
        B1_INPUT,
        A2_INPUT,
        B2_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        LEFT1_OUTPUT,
        RIGHT2_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        CF_LIGHT,
        BL_LIGHT,
        PN_LIGHT,
        LIGHTS_LEN
    };

    enum fadeKnobMode { fm_log, fm_linear, fm_exp, fm_len };
    actReqValue<fadeKnobMode> fadeMode = actReqValue<fadeKnobMode>(fm_linear);
    enum crossFadeMode { cxm_crossFade, cxm_Balance, cxm_Pan, cxm_len };
    actReqValue<crossFadeMode> cxMode = actReqValue<crossFadeMode>(cxm_crossFade);
    int channels[2] = { 1, 1 };  // [0] = left, [1] = right
    dsp::SchmittTrigger toggleTrig;
    dsp::SchmittTrigger cxModePress;
    bool triggerMode = false; // processParams; widget overlay (trim unused in trigger)
    actReqValue<fixedSlewTimes> fadeTime = actReqValue<fixedSlewTimes>(fst_0);
    actReqValue<bool> fadeSCurve = actReqValue<bool>(false);
    infNoiseUnitFade fade;
    bool doFade = false;
    float lastSwitchKnob = 0.f; // knob written by trigger; override drops ramp

    CxFade1x2Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configParam(CROSSFADE_PARAM, -1.f, 1.f, 0.0f, "Cross-fade (A to B)", " %", 0, 100);
        configParam(CROSSFADE_TRIM_PARAM, -1.f, 1.f, 0.f, "Cross-fade CV trim", " %", 0, 100);
        configSwitch(CROSSFADE_TOGGLE_PARAM, 0.0f, 1.0f, 0.0f, "Manual A/B-toggle");

        configInput(CROSSFADE_INPUT, "Cross-fade CV/Switch-Trigger");
        configSwitch(CROSSFADE_TRIG_PARAM, 0.0f, 1.0f, 0.0f, "Cross-fade CV/Switch-trigger", { "CV-Mode", "Trigger-mode" });
        configSwitch(TOGGLE_CX_MODE_PARAM, 0.0f, 1.0f, 0.0f, "Toggle cross-fade mode");
        configLight(CF_LIGHT, "Cross-fade (Cf) when lit");
        configLight(BL_LIGHT, "Balance (Bl) when lit");
        configLight(PN_LIGHT, "Pan (Pn) when lit");

        configInput(A1_INPUT, "A1");
        configInput(B1_INPUT, "B1 (normalized to A1)");
        configInput(A2_INPUT, "A2 (normalized to B1)");
        configInput(B2_INPUT, "B2 (normalized to A1)");

        configOutput(LEFT1_OUTPUT, "A1/B1");
        configOutput(RIGHT2_OUTPUT, "A2/B2");

        configBypass(A1_INPUT, LEFT1_OUTPUT);
        configBypass(A2_INPUT, RIGHT2_OUTPUT);

        // Set InfNoise features (e.g. menu-items) 
		haveProcQuality = true;
        haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = true;
        haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = true;
		haveTrigHighLow = false;
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        fadeMode.setBoth(fadeKnobMode::fm_linear);
        cxMode.setBoth(cxm_crossFade);
        toggleTrig.reset();
        cxModePress.reset();
        fadeTime.setBoth(fst_0);
        fadeSCurve.setBoth(false);
        doFade = false;
        fade.reset(0.5f);
        lastSwitchKnob = 0.f;
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);

        fadeMode.setBoth((fadeKnobMode)getJsonInt(rootJ, "fadeMode", (int)fadeKnobMode::fm_linear, (int)fm_len - 1));
        cxMode.setBoth((crossFadeMode)getJsonInt(rootJ, "cxMode", (int)cxm_crossFade, (int)cxm_len - 1));
        fadeTime.setBoth((fixedSlewTimes)getJsonInt(rootJ, "fadeTime", (int)fst_0, (int)fst_len - 2));
        fadeSCurve.setBoth(getJsonBool(rootJ, "fadeSCurve", false));
        cxModePress.reset();
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "fadeMode", json_integer((int)fadeMode.req));
        json_object_set_new(rootJ, "cxMode", json_integer((int)cxMode.req));
        json_object_set_new(rootJ, "fadeTime", json_integer((int)fadeTime.req));
        json_object_set_new(rootJ, "fadeSCurve", json_boolean(fadeSCurve.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        fadeMode.updateActual();
        triggerMode = params[CROSSFADE_TRIG_PARAM].getValue() > 0.5f;

        if (fadeSCurve.needsUpdate() || wasJustLoaded) {
            fadeSCurve.updateActual();
            fade.setUseSCurve(fadeSCurve.act);
        }
        if (fadeTime.needsUpdate() || wasJustLoaded) {
            bool wasOn = doFade;
            fadeTime.updateActual();
            doFade = fadeTime.act != fst_0;
            fade.setTime(doFade ? fixedSlewTimesValues[(int)fadeTime.act] : 0.f);
            if (wasJustLoaded || !doFade || !wasOn)
                fade.snap((params[CROSSFADE_PARAM].getValue() + 1.f) * 0.5f);
        }

        // 1(Left)-channels count
        int channelsA1 = inputs[A1_INPUT].isConnected()
            ? std::max(inputs[A1_INPUT].getChannels(), 1)
            : 1;
        int channelsB1 = inputs[B1_INPUT].isConnected()
            ? std::max(inputs[B1_INPUT].getChannels(), 1)
            : channelsA1;
        channels[0] = std::max(channelsA1, channelsB1);
        outputs[LEFT1_OUTPUT].setChannels(channels[0]);

        // 2(Right)-channels count (normalized to 1-channels)
        int channelsA2 = inputs[A2_INPUT].isConnected()
            ? std::max(inputs[A2_INPUT].getChannels(), 1)
            : channelsB1;
        int channelsB2 = inputs[B2_INPUT].isConnected()
            ? std::max(inputs[B2_INPUT].getChannels(), 1)
            : channelsA1;
        channels[1] = std::max(channelsA2, channelsB2);
        outputs[RIGHT2_OUTPUT].setChannels(channels[1]);

        // process Toggle A/B
        if (!inputs[CROSSFADE_INPUT].isConnected()) {
            if (toggleTrig.process(params[CROSSFADE_TOGGLE_PARAM].getValue())) {
                params[CROSSFADE_PARAM].setValue(params[CROSSFADE_PARAM].getValue() >= 0.f ? -1.f : 1.f);
            }
        }

        // Cross-fade / Balance / Pan mode toggle
        if (cxModePress.process(params[TOGGLE_CX_MODE_PARAM].getValue(), 0.1f, 0.5f)) {
            cxMode.setBoth(cxMode.req == cxm_Pan ? cxm_crossFade : static_cast<crossFadeMode>(cxMode.req + 1));
        }
        if (cxMode.needsUpdate()) {
            cxMode.updateActual();
            lights[CF_LIGHT].setBrightness(cxMode.act == cxm_crossFade ? 1.f : 0.f);
            lights[BL_LIGHT].setBrightness(cxMode.act == cxm_Balance ? 1.f : 0.f);
            lights[PN_LIGHT].setBrightness(cxMode.act == cxm_Pan ? 1.f : 0.f);
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

        if (doProcess && (outputs[LEFT1_OUTPUT].isConnected() ||
            outputs[RIGHT2_OUTPUT].isConnected())) {
            float a1Input[PORT_MAX_CHANNELS] = { 0.f };
            float b1Input[PORT_MAX_CHANNELS] = { 0.f };
            float a2Input[PORT_MAX_CHANNELS] = { 0.f };
            float b2Input[PORT_MAX_CHANNELS] = { 0.f };

            // Grab 1-inputs (B1 normalizes to A1; 2-inputs normalize from these)
            for (int c = 0; c < channels[0]; c++) {
                a1Input[c] = (inputs[A1_INPUT].isConnected())
                    ? inputs[A1_INPUT].getPolyVoltage(c)
                    : 0.f;
                b1Input[c] = (inputs[B1_INPUT].isConnected())
                    ? inputs[B1_INPUT].getPolyVoltage(c)
                    : a1Input[c];
            }

            // Grab 2-inputs, or use 1-inputs
            if (outputs[RIGHT2_OUTPUT].isConnected()) {
                for (int c = 0; c < channels[1]; c++) {
                    a2Input[c] = (inputs[A2_INPUT].isConnected())
                        ? inputs[A2_INPUT].getPolyVoltage(c)
                        : b1Input[c];
                    b2Input[c] = (inputs[B2_INPUT].isConnected())
                        ? inputs[B2_INPUT].getPolyVoltage(c)
                        : a1Input[c];
                }
            }

            // Calc cross-fade
            float crossFade = params[CROSSFADE_PARAM].getValue();
            bool switchNow = false;
            if (inputs[CROSSFADE_INPUT].isConnected())
            {
                if (params[CROSSFADE_TRIG_PARAM].getValue() < 0.5) { // CV-mode
                    crossFade += inputs[CROSSFADE_INPUT].getVoltage() / 5.f * params[CROSSFADE_TRIM_PARAM].getValue();
                    crossFade = clamp(crossFade, -1.f, 1.f);
                }
    			else { // Trigger-mode
                    if (toggleTrig.process(inputs[CROSSFADE_INPUT].getVoltage(),
                        trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act])) {
                        crossFade = crossFade >= 0.f ? -1.f : 1.f;
                        params[CROSSFADE_PARAM].setValue(crossFade);
                        switchNow = true;
                    }
                }
            }
        
            if (fadeMode.act == fm_exp) {
                float sign = (crossFade < 0.f) ? -1.f : 1.f;
                float flipped = 1.f - std::abs(crossFade);
                crossFade = (1.f - (flipped * flipped)) * sign;
            }
            else if (fadeMode.act == fm_log) {
                float sign = (crossFade < 0.f) ? -1.f : 1.f;
                crossFade = crossFade * crossFade * sign;
            }
            
            float mixFromKnob = (crossFade + 1.f) / 2.f;  // Normalize to 0-1
            bool trigMode = params[CROSSFADE_TRIG_PARAM].getValue() > 0.5f;
            if (switchNow && doFade && trigMode) {
                lastSwitchKnob = params[CROSSFADE_PARAM].getValue();
                fade.setTarget(mixFromKnob);
            }
            else {
                bool ramping = std::fabs(fade.amount - fade.target) > 1e-6f;
                bool knobOverride = std::fabs(params[CROSSFADE_PARAM].getValue() - lastSwitchKnob) > 1e-4f;
                if (!(trigMode && doFade && ramping && !knobOverride))
                    fade.snap(mixFromKnob);
            }
            if (trigMode && doFade && std::fabs(fade.amount - fade.target) > 1e-6f)
                crossFade = fade.next(procSampleTime);
            else
                crossFade = mixFromKnob;

            if (cxMode.act == cxm_crossFade) {
                float revCrossFade = 1.f - crossFade;
                // Cross-fade between A1/B1 (Left)
                if (outputs[LEFT1_OUTPUT].isConnected()) {
                    for (int c = 0; c < channels[0]; c++) {
                        float out1 = revCrossFade * a1Input[c] + crossFade * b1Input[c];
                        out1 = clipToVoltRange(out1, outClipRange.act);
                        outputs[LEFT1_OUTPUT].setVoltage(out1, c);
                    }
                }
                // Cross-fade between A2/B2 (Right)
                if (outputs[RIGHT2_OUTPUT].isConnected()) {
                    for (int c = 0; c < channels[1]; c++) {
                        float out2 = revCrossFade * a2Input[c] + crossFade * b2Input[c];
                        out2 = clipToVoltRange(out2, outClipRange.act);
                        outputs[RIGHT2_OUTPUT].setVoltage(out2, c);
                    }
                }
            }
            else {
                // Balance / Pan: out1 = A1 * aGain, out2 = A2 * bGain (A2 normalizes from B1)
                float aGain, bGain;
                if (cxMode.act == cxm_Balance) {
                    if (crossFade <= 0.5f) {
                        aGain = 1.f;
                        bGain = crossFade * 2.f;
                    }
                    else {
                        aGain = (1.f - crossFade) * 2.f;
                        bGain = 1.f;
                    }
                }
                else { // cxm_Pan
                    aGain = std::sqrt(1.f - crossFade);
                    bGain = std::sqrt(crossFade);
                }

                if (outputs[LEFT1_OUTPUT].isConnected()) {
                    for (int c = 0; c < channels[0]; c++) {
                        float out1 = clipToVoltRange(aGain * a1Input[c], outClipRange.act);
                        outputs[LEFT1_OUTPUT].setVoltage(out1, c);
                    }
                }
                if (outputs[RIGHT2_OUTPUT].isConnected()) {
                    for (int c = 0; c < channels[1]; c++) {
                        float out2 = clipToVoltRange(bGain * a2Input[c], outClipRange.act);
                        outputs[RIGHT2_OUTPUT].setVoltage(out2, c);
                    }
                }
            }
        }

        cycle256++;
    }
};

struct CxFade1x2ModuleWidget : InfNoiseModuleWidget {
    InfNoiseDisableOverlayGroup* trimOverlayGroup = nullptr;
    bool triggerMode = false;

    CxFade1x2ModuleWidget(CxFade1x2Module *module) {
        initializeWidget(module, "res/CxFade1x2");

        float centerCol = 15.f;
        // Cross-fade knob, trim-pot and CV-input
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, 48.343f), module, CxFade1x2Module::CROSSFADE_PARAM));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, true>>(Vec(25.556f, 37.309f), module, CxFade1x2Module::CROSSFADE_TOGGLE_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(centerCol, 76.352f), module, CxFade1x2Module::CROSSFADE_TRIM_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(centerCol, 104.865f), module, CxFade1x2Module::CROSSFADE_INPUT));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(25.556f, 92.900f), module, CxFade1x2Module::CROSSFADE_TRIG_PARAM));

        InfNoiseDisableOverlayManager& overlayManager = getDisableOverlayManager();
        trimOverlayGroup = overlayManager.addGroup("Trim ignored in trigger mode");
        trimOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
            CxFade1x2Module::CROSSFADE_TRIM_PARAM
        });

        // Cross-fade mode (Cf / Bl / Pn)
        const float modeLightRow = 127.774f;
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(4.398f, modeLightRow), module, CxFade1x2Module::CF_LIGHT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(centerCol, modeLightRow), module, CxFade1x2Module::BL_LIGHT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(25.328f, modeLightRow), module, CxFade1x2Module::PN_LIGHT));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, true>>(Vec(centerCol, 134.277f), module, CxFade1x2Module::TOGGLE_CX_MODE_PARAM));

        // 1(Left) Inputs/output
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 161.700f), module, CxFade1x2Module::A1_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 193.439f), module, CxFade1x2Module::B1_INPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 228.713f), module, CxFade1x2Module::LEFT1_OUTPUT));

        // 2(Right) Inputs/output
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 266.381f), module, CxFade1x2Module::A2_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 298.120f), module, CxFade1x2Module::B2_INPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 333.395f), module, CxFade1x2Module::RIGHT2_OUTPUT));
    }

    void step() override {
        InfNoiseModuleWidget::step();

        if (!module)
            return;

        auto* m = static_cast<CxFade1x2Module*>(module);
        if (m->triggerMode != triggerMode) {
            triggerMode = m->triggerMode;
            if (trimOverlayGroup)
                trimOverlayGroup->setActive(triggerMode);
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        CxFade1x2Module* module = dynamic_cast<CxFade1x2Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

		menu->addChild(createIndexPtrSubmenuItem("Fade-mode (scaling of fade-knob/input)",
		 	{"Logarithmic", "Linear", "Exponential"},
		 	&module->fadeMode.req
        ));
        menu->addChild(createIndexPtrSubmenuItem("Fade time", getFixedSlewTimesNames(false),
            &module->fadeTime.req));
        menu->addChild(createBoolPtrMenuItem("S-curve fade", "", &module->fadeSCurve.req));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelCxFade1x2 = createModel<CxFade1x2Module, CxFade1x2ModuleWidget>("CxFade1x2");