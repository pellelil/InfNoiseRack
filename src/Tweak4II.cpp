// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inUtil.hpp"
#include "inComponents.hpp"

struct Tweak4IIModule : InfNoiseModule {
    enum ParamId {
        LINK_SCALE_PARAM,
        A_SCALE_PARAM,
        B_SCALE_PARAM,
        C_SCALE_PARAM,
        D_SCALE_PARAM,
        A_SCALE_TRIM_PARAM,
        B_SCALE_TRIM_PARAM,
        C_SCALE_TRIM_PARAM,
        D_SCALE_TRIM_PARAM,
        LINK_OFFSET_PARAM,
        A_OFFSET_PARAM,
        B_OFFSET_PARAM,
        C_OFFSET_PARAM,
        D_OFFSET_PARAM,
        A_OFFSET_TRIM_PARAM,
        B_OFFSET_TRIM_PARAM,
        C_OFFSET_TRIM_PARAM,
        D_OFFSET_TRIM_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        A_INPUT,
        B_INPUT,
        C_INPUT,
        D_INPUT,
        A_SCALE_INPUT,
        B_SCALE_INPUT,
        C_SCALE_INPUT,
        D_SCALE_INPUT,
        A_OFFSET_INPUT,
        B_OFFSET_INPUT,
        C_OFFSET_INPUT,
        D_OFFSET_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        A_OUTPUT,
        B_OUTPUT,
		C_OUTPUT,
        D_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        ENUMS(SCALE_LIGHT, 3),
        ENUMS(EXP_SCALE_LIGHT, 2),
        OFFSET_LIGHT,
        B_MIX_LIGHT,
        C_MIX_LIGHT,
        D_MIX_LIGHT,
        LIGHTS_LEN
    };

    actReqValue<scaleCurve> scaleMode = actReqValue<scaleCurve>(sc_linear);
    actReqValue<infNoiseAttRngQnt::attRange> attRng =
        actReqValue<infNoiseAttRngQnt::attRange>(infNoiseAttRngQnt::attRange::ar_1x);
    float attRngFactor = 1.f;
    enum order { scaleOffset, offsetScale };
    actReqValue<order> orderMode = actReqValue<order>(scaleOffset);
    bool mixMode = true; // Mix unplugged outputs if true
    bool haveOutputs = false;
    int firstIdx = -1;
    int lastIdx = -1;
    bool linkScaleToA = false;
    bool linkOffsetToA = false;
    float scaleKnob[4] = { 1.f, 1.f, 1.f, 1.f };
    float scaleTrim[4] = { 0.f, 0.f, 0.f, 0.f };
    float offsetKnob[4] = { 0.f, 0.f, 0.f, 0.f };
    float offsetTrim[4] = { 0.f, 0.f, 0.f, 0.f };

    Tweak4IIModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configSwitch(LINK_SCALE_PARAM, 0.0, 1.0, 0.0, "Link-scale", { "Individual", "Linked B-D to A" });
        configSwitch(LINK_OFFSET_PARAM, 0.0, 1.0, 0.0, "Link-offset", { "Individual", "Linked B-D to A" });
    
        const std::string letter[]{ "A", "B", "C", "D" };
        for (int i = 0; i < 4; i++) {
			configParam<infNoiseAttRngQnt>(A_SCALE_PARAM + i, -1.f, 1.f, 1.f, letter[i] + "-Scale (-1x to +1x)", " x", 0, 1);
			configParam(A_SCALE_TRIM_PARAM + i, -1.f, 1.f, 0.f, letter[i] + "-Scale CV-trim", "%", 0, 100);
			configParam(A_OFFSET_PARAM + i, -10.0f, 10.0f, 0.0f, letter[i] + "-Offset (-10V to +10V)", " V");
			configParam(A_OFFSET_TRIM_PARAM + i, -1.f, 1.f, 0.f, letter[i] + "-Offset CV-tirm", "%", 0, 100);

			configInput(A_INPUT + i, letter[i]);
			configInput(A_SCALE_INPUT + i, letter[i] + "-Scale CV");
			configInput(A_OFFSET_INPUT + i, letter[i] + "-Offset CV (-10V to +10V)");
			configOutput(A_OUTPUT + i, letter[i]);

            if (i > 0) {
                configLight(B_MIX_LIGHT + i - 1, letter[i] + "-Mix (if lit, mix of non connected outputs)");
            }

            configBypass(A_INPUT + i, A_OUTPUT + i);
		}

        configLight(SCALE_LIGHT, "Scale-range (unlit=1x, green=2x, yellow=5x, red=10x)");
        configLight(EXP_SCALE_LIGHT, "Scale-mode (unlit=Linear, green=Exp, red=Log)");
        configLight(OFFSET_LIGHT, "Order: Scale->Offset if unlit, else Offset->Scale");

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
        mixMode = true;

        // paramQuantity.defaultValue might not be correct yet, hence manually set value
        for (int i = 0; i < 4; i++)
            params[A_SCALE_PARAM + i].setValue(1.f);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        attRng.setBoth((infNoiseAttRngQnt::attRange)getJsonInt(rootJ, "attRng", (int)infNoiseAttRngQnt::attRange::ar_1x));
        scaleMode.setBoth((scaleCurve)getJsonInt(rootJ, "scaleMode", (int)sc_linear));
        orderMode.setBoth((order)getJsonInt(rootJ, "orderMode", (int)order::scaleOffset));
        mixMode = getJsonInt(rootJ, "mixMode", 1) == 1;
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "attRng", json_integer((int)attRng.req));
        json_object_set_new(rootJ, "scaleMode", json_integer((int)scaleMode.req));
        json_object_set_new(rootJ, "orderMode", json_integer((int)orderMode.req));
        json_object_set_new(rootJ, "mixMode", json_integer(mixMode ? 1 : 0));
    }
    
    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        scaleKnob[0] = params[A_SCALE_PARAM].getValue();
        scaleTrim[0] = params[A_SCALE_TRIM_PARAM].getValue();
        linkScaleToA = params[LINK_SCALE_PARAM].getValue() > 0.5f;
        if (linkScaleToA) {
            for (int i = 1; i < 4; i++) {
                scaleKnob[i] = scaleKnob[0];
                scaleTrim[i] = scaleTrim[0];
                params[A_SCALE_PARAM + i].setValue(scaleKnob[i-1]);
                params[A_SCALE_TRIM_PARAM + i].setValue(scaleTrim[i-1]);
            }
        } else {
            for (int i = 1; i < 4; i++) {
                scaleKnob[i] = params[A_SCALE_PARAM + i].getValue();
                scaleTrim[i] = params[A_SCALE_TRIM_PARAM + i].getValue();
            }
        }

        offsetKnob[0] = params[A_OFFSET_PARAM].getValue();
        offsetTrim[0] = params[A_OFFSET_TRIM_PARAM].getValue();
        linkOffsetToA = params[LINK_OFFSET_PARAM].getValue() > 0.5f;
        if (linkOffsetToA) {
            for (int i = 1; i < 4; i++) {
                offsetKnob[i] = offsetKnob[0];
                offsetTrim[i] = offsetTrim[0];
                params[A_OFFSET_PARAM + i].setValue(offsetKnob[0]);
                params[A_OFFSET_TRIM_PARAM + i].setValue(offsetTrim[0]);
            }
        } else {
            for (int i = 1; i < 4; i++) {
                offsetKnob[i] = params[A_OFFSET_PARAM + i].getValue();
                offsetTrim[i] = params[A_OFFSET_TRIM_PARAM + i].getValue();
            }
        }
        
        if (scaleMode.needsUpdate()) {
            scaleMode.updateActual();
            setScaleModeLight(this, EXP_SCALE_LIGHT, scaleMode.act);
        }

        if (attRng.needsUpdate()) {
            attRng.updateActual();
            const float rangeFactor[4] = { 1.f, 2.f, 5.f, 10.f };
            attRngFactor = rangeFactor[(int)attRng.act];

            const std::string letters[]{ "A", "B", "C", "D" };
            for (int i = 0; i < 4; i++) {
                infNoiseAttRngQnt* attQty = dynamic_cast<infNoiseAttRngQnt*>(paramQuantities[A_SCALE_PARAM + i]);
                attQty->setRange(attRng.act, letters[i] + "-Scale");
                if (i == 0)
                {
                    attQty->setRangeLights(this, SCALE_LIGHT);
                }
            }
        }

        haveOutputs = false;
        firstIdx = -1;
        lastIdx = -1;
        for (int i=0; i<4; i++)
        {
            bool inUse = inputs[A_INPUT + i].isConnected() || outputs[A_OUTPUT + i].isConnected() ||
                inputs[A_SCALE_INPUT + i].isConnected() || inputs[A_OFFSET_INPUT + i].isConnected();
            if (inUse) {
                if (firstIdx < 0)
					firstIdx = i;
				lastIdx = i;
				haveOutputs = haveOutputs || outputs[A_OUTPUT + i].isConnected();
            }
        }
        
        // Update lights (indicating scale/offset order)
        orderMode.updateActual();
        lights[OFFSET_LIGHT].value = (orderMode.act == offsetScale) ? 1.0f : 0.f;

        // Clear mix-lights (set in process if applicable)
        lights[B_MIX_LIGHT].setBrightness(0.f);
        lights[C_MIX_LIGHT].setBrightness(0.f);
        lights[D_MIX_LIGHT].setBrightness(0.f);

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

        if (doProcess && haveOutputs) {
            float input = 0.f;
            float outputSum = 0.f;
            int outputCount = 0;
            float normSclIn = 0.f;
            float normOfsIn = 0.f;
            for (int i = firstIdx; i <= lastIdx; i++) {
                bool outputsMix = false; // True if outputting a mix (multiple sections)
                bool haveInput = inputs[A_INPUT + i].isConnected();
                bool haveOutput = outputs[A_OUTPUT + i].isConnected();
                if (haveInput)
                    input = inputs[A_INPUT + i].getVoltage();
                else {
                    outputSum = 0.f;
                    outputCount = 0;
                }

                if (inputs[A_SCALE_INPUT + i].isConnected())
					normSclIn = inputs[A_SCALE_INPUT + i].getVoltage() * scaleTrim[i] / 5.f;
                float scale = scaleKnob[i] + normSclIn;
                scale = clamp(scale, -1.f, 1.f);
                scale = applyScaleCurveSigned(scale, scaleMode.act);
                scale *= attRngFactor;

                if (inputs[A_OFFSET_INPUT + i].isConnected())
                    normOfsIn = inputs[A_OFFSET_INPUT + i].getVoltage() * offsetTrim[i];
                float offset = offsetKnob[i] + normOfsIn;

                outputSum += (orderMode.act == scaleOffset)
                    ? input * scale + offset
                    : (input + offset) * scale;
                outputCount++;

                if (haveOutput) {
                    outputsMix = outputCount > 1;
                    float output = (outputCount > 0)
                        ? outputSum / outputCount
                        : 0.f;
                    output = quantizeToMode(output, outQuantize.act);
                    output = clipToVoltRange(output, outClipRange.act);
                    outputs[A_OUTPUT + i].setVoltage(output);
                    outputSum = 0.f;
                    outputCount = 0;
                }
                else if (!mixMode) {
                    outputSum = 0.f;
                    outputCount = 0;
                }

                // Set mix-light
                if (i > 0) {
                    lights[B_MIX_LIGHT + i - 1].setBrightness(outputsMix ? 1.f : 0.f);
                }
            }
        }

        cycle256++;
    }
};

struct Tweak4IIModuleWidget : InfNoiseModuleWidget {
    InfNoiseDisableOverlayGroup* linkScaleOverlayGroup = nullptr;
    InfNoiseDisableOverlayGroup* linkOffsetOverlayGroup = nullptr;
    bool linkScaleToA = false;
    bool linkOffsetToA = false;

    Tweak4IIModuleWidget(Tweak4IIModule* module) {
        initializeWidget(module, "res/Tweak4II");

        addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(Vec(99.076f, 38.941f), module, Tweak4IIModule::LINK_SCALE_PARAM));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(Vec(99.076f, 161.541f), module, Tweak4IIModule::LINK_OFFSET_PARAM));

        float col = 14.033f; //14.848f;
        const float colSpacing = 25.644f; //25.645f;
        for (int i = 0; i < 4; i++) {
            // Scale
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(col, 56.070f), module, Tweak4IIModule::A_SCALE_PARAM + i));
            addParam(createParamCentered<Trimpot>(Vec(col, 93.155f), module, Tweak4IIModule::A_SCALE_TRIM_PARAM + i));
            addInput(createInputCentered<ThemedPJ301MPort>(Vec(col, 127.965f), module, Tweak4IIModule::A_SCALE_INPUT +i));

            // Offset
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(col, 178.697f), module, Tweak4IIModule::A_OFFSET_PARAM + i));
            addParam(createParamCentered<Trimpot>(Vec(col, 215.804f), module, Tweak4IIModule::A_OFFSET_TRIM_PARAM + i));
            addInput(createInputCentered<ThemedPJ301MPort>(Vec(col, 250.614f), module, Tweak4IIModule::A_OFFSET_INPUT +i));

            // Input/Output
            addInput(createInputCentered<ThemedPJ301MPort>(Vec(col, 297.938f), module, Tweak4IIModule::A_INPUT + i));
            addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(col, 332.694f), module, Tweak4IIModule::A_OUTPUT + i));
            if (i > 0) { // A-output does not have a mix-light (can't output a mix)
                addChild(createLightCentered<TinyLight<GreenLight>>(Vec(col - 9.621f, 343.115f),
                    module, Tweak4IIModule::B_MIX_LIGHT + i - 1));
            }

            col += colSpacing;
        }

        InfNoiseDisableOverlayManager& overlayManager = getDisableOverlayManager();
        linkScaleOverlayGroup = overlayManager.addGroup("Scale linked to A");
        linkScaleOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
            Tweak4IIModule::B_SCALE_PARAM,
            Tweak4IIModule::C_SCALE_PARAM,
            Tweak4IIModule::D_SCALE_PARAM
        });
        linkScaleOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
            Tweak4IIModule::B_SCALE_TRIM_PARAM,
            Tweak4IIModule::C_SCALE_TRIM_PARAM,
            Tweak4IIModule::D_SCALE_TRIM_PARAM
        });

        linkOffsetOverlayGroup = overlayManager.addGroup("Offset linked to A");
        linkOffsetOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
            Tweak4IIModule::B_OFFSET_PARAM,
            Tweak4IIModule::C_OFFSET_PARAM,
            Tweak4IIModule::D_OFFSET_PARAM
        });
        linkOffsetOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
            Tweak4IIModule::B_OFFSET_TRIM_PARAM,
            Tweak4IIModule::C_OFFSET_TRIM_PARAM,
            Tweak4IIModule::D_OFFSET_TRIM_PARAM
        });

        const float lightCol = 7.298f;
        addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(Vec(lightCol, 38.941f), module, Tweak4IIModule::SCALE_LIGHT));
        addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(40.668f, 38.941f), module, Tweak4IIModule::EXP_SCALE_LIGHT));
        addChild(createLightCentered<SmallLight<BlueLight>>(Vec(lightCol, 161.541f), module, Tweak4IIModule::OFFSET_LIGHT));
    }

    void step() override {
        InfNoiseModuleWidget::step();
    
        if (!module)
            return;
    
        auto* m = static_cast<Tweak4IIModule*>(module);
        if (linkScaleOverlayGroup){
            if (m->linkScaleToA != linkScaleToA) {
                linkScaleToA = m->linkScaleToA;
                linkScaleOverlayGroup->setActive(linkScaleToA);
            }
        }
        if (linkOffsetOverlayGroup) {
            if (m->linkOffsetToA != linkOffsetToA) {
                linkOffsetToA = m->linkOffsetToA;
                linkOffsetOverlayGroup->setActive(linkOffsetToA);
            }
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        Tweak4IIModule* module = dynamic_cast<Tweak4IIModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> scaleRangeNames = { "1x (-100\% to +100\%)",
            "2x (-200\% to +200\%)", "5x (-500\% to +500\%)", "10x (-1000\% to +1000\%)" };
        menu->addChild(createIndexPtrSubmenuItem("Scale-range mode", scaleRangeNames,
            &module->attRng.req));

        menu->addChild(createIndexPtrSubmenuItem("Scale-mode", getScaleCurveMenuNames(),
            &module->scaleMode.req));

        std::vector<std::string> intervalNames = getVoltIntervalValuesNames();
        menu->addChild(createSubmenuItem("Set offset", "", [=](Menu* setOffsetMenu) {
            setOffsetMenu->addChild(createSubmenuItem("A (semitone steps)", "", [=](Menu* submenu) {
                for (int i = 0; i < voltIntervalValueCount; i++) {
                    submenu->addChild(createMenuItem(intervalNames[i], "", [=]() {
                        module->params[Tweak4IIModule::A_OFFSET_PARAM].setValue(
                            voltIntervalValues[(voltIntervalValue)i]);
                    }));
                }
            }));
            setOffsetMenu->addChild(createSubmenuItem("B (semitone steps)", "", [=](Menu* submenu) {
                for (int i = 0; i < voltIntervalValueCount; i++) {
                    submenu->addChild(createMenuItem(intervalNames[i], "", [=]() {
                        module->params[Tweak4IIModule::B_OFFSET_PARAM].setValue(
                            voltIntervalValues[(voltIntervalValue)i]);
                    }));
                }
            }));
            setOffsetMenu->addChild(createSubmenuItem("C (semitone steps)", "", [=](Menu* submenu) {
                for (int i = 0; i < voltIntervalValueCount; i++) {
                    submenu->addChild(createMenuItem(intervalNames[i], "", [=]() {
                        module->params[Tweak4IIModule::C_OFFSET_PARAM].setValue(
                            voltIntervalValues[(voltIntervalValue)i]);
                    }));
                }
            }));
            setOffsetMenu->addChild(createSubmenuItem("D (semitone steps)", "", [=](Menu* submenu) {
                for (int i = 0; i < voltIntervalValueCount; i++) {
                    submenu->addChild(createMenuItem(intervalNames[i], "", [=]() {
                        module->params[Tweak4IIModule::D_OFFSET_PARAM].setValue(
                            voltIntervalValues[(voltIntervalValue)i]);
                    }));
                }
            }));
        }));

        menu->addChild(createIndexPtrSubmenuItem("Order",
            { "Scale->Offset", "Offset->Scale" },
            &module->orderMode.req
        ));
        
        menu->addChild(createBoolPtrMenuItem("Mix-mode (mix unplugged outputs)", "", &module->mixMode));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelTweak4II = createModel<Tweak4IIModule, Tweak4IIModuleWidget>("Tweak4II");
