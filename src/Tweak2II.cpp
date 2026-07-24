// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inUtil.hpp"

struct Tweak2IIModule : InfNoiseModule {
    enum ParamId {
        A_SCALE_PARAM,
        B_SCALE_PARAM,
        A_OFFSET_PARAM,
        B_OFFSET_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
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
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        ENUMS(A_SCALE_LIGHT,3),
        ENUMS(B_SCALE_LIGHT,3),
        ENUMS(A_EXP_SCALE_LIGHT, 2),
        ENUMS(B_EXP_SCALE_LIGHT, 2),
        A_OFFSET_LIGHT,
        B_OFFSET_LIGHT,
        B_MIX_LIGHT,
        B_LINKED_LIGHT,
        LIGHTS_LEN
    };

    actReqValue<scaleCurve> scaleMode = actReqValue<scaleCurve>(sc_linear);
    actReqValue<infNoiseAttRngQnt::attRange> attRng =
        actReqValue<infNoiseAttRngQnt::attRange>(infNoiseAttRngQnt::attRange::ar_1x);
    float attRngFactor = 1.f;
    enum order { scaleOffset, offsetScale };
    actReqValue<order> orderMode = actReqValue<order>(scaleOffset);
    bool mixMode = true; // Mix unplugged outputs if true
    int outputCount = 0;  // set in processParams, used in process
    int channels[2] = { 1, 1 }; // channels for A and B
    enum bSectionModeType { bsm_Individual, bsm_LinkedToA };
    actReqValue<bSectionModeType> bSectionMode = actReqValue<bSectionModeType>(bsm_Individual);
    /// Set in processParams; used by widget overlay (B knobs follow A when linked).
    bool bSectionLinkedToA = false;

    Tweak2IIModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configParam<infNoiseAttRngQnt>(A_SCALE_PARAM, -1.f, 1.f, 1.f, "A-Scale (-1x to +1x)", "x", 0, 1);
        configParam(A_OFFSET_PARAM, -10.0f, 10.0f, 0.0f, "A-Offset (-10V to +10V)", " V");
        configParam<infNoiseAttRngQnt>(B_SCALE_PARAM, -1.f, 1.f, 1.f, "B-Scale (-1x to +1x)", "x", 0, 1);
        configParam(B_OFFSET_PARAM, -10.0f, 10.0f, 0.0f, "B-Offset (-10V to +10V)", " V");

        configInput(A_INPUT, "A");
        configInput(B_INPUT, "B (normalized to A)");
        configLight(B_LINKED_LIGHT, "B-section linked to A when lit");

        configOutput(A_OUTPUT, "A (can be mixed with B, if not connected)");
        configOutput(B_OUTPUT, "B (can be mixed with A, if A-output is not connected)");

        configLight(A_SCALE_LIGHT, "Scale-range (unlit=1x, green=2x, yellow=5x, red=10x)");
        configLight(A_EXP_SCALE_LIGHT, "Scale-mode (unlit=Linear, green=Exp, red=Log)");
        configLight(A_OFFSET_LIGHT, "A-Order: Scale->Offset if unlit, else Offset->Scale");
        configLight(B_SCALE_LIGHT, "Scale-range (unlit=1x, green=2x, yellow=5x, red=10x)");
        configLight(B_EXP_SCALE_LIGHT, "Scale-mode (unlit=Linear, green=Exp, red=Log)");
        configLight(B_OFFSET_LIGHT, "B-Order: Scale->Offset if unlit, else Offset->Scale");
        configLight(B_MIX_LIGHT, "B-Mix (if lit, mix of A/B)");

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
    }

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);

        attRng.setBoth(infNoiseAttRngQnt::attRange::ar_1x);
        scaleMode.setBoth(sc_linear);
        orderMode.setBoth(scaleOffset);
        bSectionMode.setBoth(bsm_Individual);
        mixMode = true;

        // paramQuantity.defaultValue might not be correct yet, hence manually set value
        params[A_SCALE_PARAM].setValue(1.f);
		params[B_SCALE_PARAM].setValue(1.f);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);

        attRng.setBoth((infNoiseAttRngQnt::attRange)getJsonInt(rootJ, "attRng", (int)infNoiseAttRngQnt::attRange::ar_1x));
        scaleMode.setBoth((scaleCurve)getJsonInt(rootJ, "scaleMode", (int)sc_linear));
        orderMode.setBoth((order)getJsonInt(rootJ, "orderMode", (int)order::scaleOffset));
        bSectionMode.setBoth((bSectionModeType)getJsonInt(rootJ, "bSectionMode", (int)bSectionModeType::bsm_Individual));
        mixMode = getJsonInt(rootJ, "mixMode", 1) == 1;
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "attRng", json_integer((int)attRng.req));
        json_object_set_new(rootJ, "scaleMode", json_integer((int)scaleMode.req));
        json_object_set_new(rootJ, "orderMode", json_integer((int)orderMode.req));
        json_object_set_new(rootJ, "bSectionMode", json_integer((int)bSectionMode.req));
        json_object_set_new(rootJ, "mixMode", json_integer(mixMode ? 1 : 0));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        if (bSectionMode.needsUpdate()) {
            bSectionMode.updateActual();
            lights[B_LINKED_LIGHT].setBrightness(bSectionMode.act == bsm_LinkedToA ? 1.f : 0.f);
        }
        if (bSectionMode.act == bsm_LinkedToA) {
            params[B_SCALE_PARAM].setValue(params[A_SCALE_PARAM].getValue());
            params[B_OFFSET_PARAM].setValue(params[A_OFFSET_PARAM].getValue());
        }
        bSectionLinkedToA = (bSectionMode.act == bsm_LinkedToA);

        if (scaleMode.needsUpdate()) {
            scaleMode.updateActual();
            setScaleModeLight(this, A_EXP_SCALE_LIGHT, scaleMode.act);
            setScaleModeLight(this, B_EXP_SCALE_LIGHT, scaleMode.act);
        }

        if (attRng.needsUpdate()) {
            attRng.updateActual();
            const float rangeFactor[4] = { 1.f, 2.f, 5.f, 10.f };
            attRngFactor = rangeFactor[attRng.act];

            const std::string letters[]{ "A", "B"};
            for (int i = 0; i < 2; i++) {
                infNoiseAttRngQnt* attQty = dynamic_cast<infNoiseAttRngQnt*>(paramQuantities[A_SCALE_PARAM + i]);
                attQty->setRange(attRng.act, letters[i] + "-Scale");
                attQty->setRangeLights(this, A_SCALE_LIGHT + i * 3);
            }
        }

        outputCount = 0;       
        for (int i = 0; i < 2; i++) {
            channels[i] = (i == 0 || inputs[A_INPUT + i].isConnected())
                ? std::max(inputs[A_INPUT + i].getChannels(), 1)
                : channels[i - 1];  // Normalized to previous input
            outputs[A_OUTPUT + i].setChannels(channels[i]);

            if (outputs[A_OUTPUT + i].isConnected())
                outputCount++;
        }

        // Update mix-light, based on if mix is enabled/applicable
        bool outputsMix = mixMode && inputs[A_INPUT].isConnected() &&
            !outputs[A_OUTPUT].isConnected() && inputs[B_INPUT].isConnected() &&
            outputs[B_OUTPUT].isConnected();
        lights[B_MIX_LIGHT].setBrightness(outputsMix ? 1.f : 0.f);

        // Increase channels if A and B are to be mixed (only B-output connected)
        if (outputsMix) {
            int maxChannels = std::max(channels[0], channels[1]);
            channels[0] = maxChannels;
            channels[1] = maxChannels;
            outputs[B_OUTPUT].setChannels(maxChannels);
        }

        // Update order-lights (indicating scale/offset order)
        orderMode.updateActual();
        lights[A_OFFSET_LIGHT].setBrightness((orderMode.act == offsetScale) ? 1.0f : 0.f);
        lights[B_OFFSET_LIGHT].setBrightness((orderMode.act == offsetScale) ? 1.0f : 0.f);

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

        if (doProcess && outputCount > 0) {
            float input[PORT_MAX_CHANNELS] = { 0.f };
            float outputSum[PORT_MAX_CHANNELS] = { 0.f };
            int outputCount[PORT_MAX_CHANNELS] = { 0 };
            for (int i = 0; i < 2; i++) {
                bool haveInput = inputs[A_INPUT + i].isConnected();
                bool haveOutput = outputs[A_OUTPUT + i].isConnected();
                float scale = params[A_SCALE_PARAM + i].getValue();

                scale = applyScaleCurveSigned(scale, scaleMode.act);
                scale *= attRngFactor;
                float offset = params[A_OFFSET_PARAM + i].getValue();

                for (int c = 0; c < channels[i]; c++) {
                    if (haveInput)
                        input[c] = inputs[A_INPUT + i].getPolyVoltage(c);

                    outputSum[c] += (orderMode.act == scaleOffset)
                        ? input[c] * scale + offset
                        : (input[c] + offset) * scale;
                    outputCount[c]++;

                    if (haveOutput) {
                        float output = (outputCount[c] > 0)
                            ? outputSum[c] / outputCount[c]
                            : 0.f;
                        output = quantizeToMode(output, outQuantize.act);
                        output = clipToVoltRange(output, outClipRange.act);
                        outputs[A_OUTPUT + i].setVoltage(output, c);
                    }

                    if (!mixMode || !haveInput || haveOutput) { // "clear sum"
                        outputSum[c] = 0.f;
                        outputCount[c] = 0;
                    }
				}
            }
        }

        cycle256++;
    }
};

struct Tweak2IIModuleWidget : InfNoiseModuleWidget {
    InfNoiseDisableOverlayGroup* linkBOverlayGroup = nullptr;
    bool bSectionLinkedToA = false;

    Tweak2IIModuleWidget(Tweak2IIModule *module) {
        initializeWidget(module, "res/Tweak2II");

        const float centerCol = 15.f;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, 56.070f), module, Tweak2IIModule::A_SCALE_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, 100.084f), module, Tweak2IIModule::A_OFFSET_PARAM));

        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, 214.562f), module, Tweak2IIModule::B_SCALE_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, 258.577f), module, Tweak2IIModule::B_OFFSET_PARAM));

        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 139.465f), module, Tweak2IIModule::A_INPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 174.220f), module, Tweak2IIModule::A_OUTPUT));

        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 297.938f), module, Tweak2IIModule::B_INPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 332.694f), module, Tweak2IIModule::B_OUTPUT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(4.981f, 343.012f),
            module, Tweak2IIModule::B_MIX_LIGHT));

        const float lightCol = 5.588f;
        addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(Vec(lightCol, 38.993f), module, Tweak2IIModule::A_SCALE_LIGHT));
        addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(26.668f, 38.993f), module, Tweak2IIModule::A_EXP_SCALE_LIGHT));
        addChild(createLightCentered<SmallLight<BlueLight>>(Vec(lightCol, 83.835f), module, Tweak2IIModule::A_OFFSET_LIGHT));
        addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(Vec(lightCol, 197.679f), module, Tweak2IIModule::B_SCALE_LIGHT));
        addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(26.668f, 197.679f), module, Tweak2IIModule::B_EXP_SCALE_LIGHT));
        addChild(createLightCentered<SmallLight<BlueLight>>(Vec(lightCol, 241.520f), module, Tweak2IIModule::B_OFFSET_LIGHT));

        InfNoiseDisableOverlayManager& overlayManager = getDisableOverlayManager();
        linkBOverlayGroup = overlayManager.addGroup("B-section linked to A");
        linkBOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
            Tweak2IIModule::B_SCALE_PARAM,
            Tweak2IIModule::B_OFFSET_PARAM
        });
    }

    void step() override {
        InfNoiseModuleWidget::step();

        if (!module)
            return;

        auto* m = static_cast<Tweak2IIModule*>(module);
        if (linkBOverlayGroup) {
            if (m->bSectionLinkedToA != bSectionLinkedToA) {
                bSectionLinkedToA = m->bSectionLinkedToA;
                linkBOverlayGroup->setActive(bSectionLinkedToA);
            }
        }
    }

    void appendContextMenu(Menu* menu) override {
        Tweak2IIModule* module = dynamic_cast<Tweak2IIModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> scaleRangeNames = { "1x (-100\% to +100\%)",
            "2x (-200\% to +200\%)", "5x (-500\% to +500\%)", "10x (-1000\% to +1000\%)" };
        menu->addChild(createIndexPtrSubmenuItem("Scale-range mode", scaleRangeNames,
            &module->attRng.req));

        menu->addChild(createIndexPtrSubmenuItem("Scale-mode", getScaleCurveMenuNames(),
            &module->scaleMode.req));

        std::vector<std::string> intervalNames = getVoltIntervalValuesNames();
        menu->addChild(createSubmenuItem("Set A-offset (semitone steps)", "", [=](Menu* submenu) {
            for (int i = 0; i < voltIntervalValueCount; i++) {
                submenu->addChild(createMenuItem(intervalNames[i], "", [=]() {
                    module->params[Tweak2IIModule::A_OFFSET_PARAM].setValue(
                        voltIntervalValues[(voltIntervalValue)i]);
                }));
            }
        }));

        menu->addChild(createSubmenuItem("Set B-offset (semitone steps)", "", [=](Menu* submenu) {
            for (int i = 0; i < voltIntervalValueCount; i++) {
                submenu->addChild(createMenuItem(intervalNames[i], "", [=]() {
                    module->params[Tweak2IIModule::B_OFFSET_PARAM].setValue(
                        voltIntervalValues[(voltIntervalValue)i]);
                }));
            }
        }));

        menu->addChild(createIndexPtrSubmenuItem("Operation-order",
		 	{"Scale->Offset", "Offset->Scale"},
		 	&module->orderMode.req
        ));

        std::vector<std::string> bSectionModeNames = { "Individual", "Linked to A-section" };
        menu->addChild(createIndexPtrSubmenuItem("B-section mode", bSectionModeNames,
            &module->bSectionMode.req));

        menu->addChild(createBoolPtrMenuItem("Mix-mode (mix unplugged outputs)", "", &module->mixMode));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelTweak2II = createModel<Tweak2IIModule, Tweak2IIModuleWidget>("Tweak2II");