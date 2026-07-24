// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inUtil.hpp"

struct Tweak4IModule : InfNoiseModule {
    enum ParamId {
        SCALE_PARAM,
        OFFSET_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        A_INPUT,
        B_INPUT,
        C_INPUT,
        D_INPUT,
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
        ENUMS(SCALE_LIGHT,3),
        ENUMS(EXP_SCALE_LIGHT, 2),
        OFFSET_LIGHT,
        LIGHTS_LEN
    };

    actReqValue<scaleCurve> scaleMode = actReqValue<scaleCurve>(sc_linear);
    actReqValue<infNoiseAttRngQnt::attRange> attRng =
        actReqValue<infNoiseAttRngQnt::attRange>(infNoiseAttRngQnt::attRange::ar_1x);
    float attRngFactor = 1.f;
    enum order { scaleOffset, offsetScale };
    actReqValue<order> orderMode = actReqValue<order>(scaleOffset);
    int outputCount = 0;  // set in processParams, used in process
    int channels[4] = { 1, 1, 1, 1 }; // channels for A, B, C and D
    bool haveOutputs = false;
    int firstIdx = -1;
    int lastIdx = -1;

    Tweak4IModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configParam<infNoiseAttRngQnt>(SCALE_PARAM, -1.f, 1.f, 1.f, "Scale (-1x to +1x)", "x", 0, 1);
        configParam(OFFSET_PARAM, -10.0f, 10.0f, 0.0f, "Offset (-10V to +10V)", " V");

        configInput(A_INPUT, "A");
        configInput(B_INPUT, "B");
        configInput(C_INPUT, "C");
		configInput(D_INPUT, "D");

        configOutput(A_OUTPUT, "A");
        configOutput(B_OUTPUT, "B");
        configOutput(C_OUTPUT, "C");
        configOutput(D_OUTPUT, "D");

        configLight(SCALE_LIGHT, "Scale-range (unlit=1x, green=2x, yellow=5x, red=10x)");
        configLight(EXP_SCALE_LIGHT, "Scale-mode (unlit=Linear, green=Exp, red=Log)");
        configLight(OFFSET_LIGHT, "Order: Scale->Offset if unlit, else Offset->Scale");

        configBypass(A_INPUT, A_OUTPUT);
        configBypass(B_INPUT, B_OUTPUT);
        configBypass(C_INPUT, C_OUTPUT);
        configBypass(D_INPUT, D_OUTPUT);

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

        // paramQuantity.defaultValue might not be correct yet, hence manually set value
        params[SCALE_PARAM].setValue(1.f);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);

        attRng.setBoth((infNoiseAttRngQnt::attRange)getJsonInt(rootJ, "attRng", (int)infNoiseAttRngQnt::attRange::ar_1x));
        scaleMode.setBoth((scaleCurve)getJsonInt(rootJ, "scaleMode", (int)sc_linear));
        orderMode.setBoth((order)getJsonInt(rootJ, "orderMode", (int)order::scaleOffset));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "attRng", json_integer((int)attRng.req));
        json_object_set_new(rootJ, "scaleMode", json_integer((int)scaleMode.req));
        json_object_set_new(rootJ, "orderMode", json_integer((int)orderMode.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        if (scaleMode.needsUpdate()) {
            scaleMode.updateActual();
            setScaleModeLight(this, EXP_SCALE_LIGHT, scaleMode.act);
        }

        if (attRng.needsUpdate()) {
            attRng.updateActual();
            const float rangeFactor[4] = { 1.f, 2.f, 5.f, 10.f };
            attRngFactor = rangeFactor[attRng.act];

            infNoiseAttRngQnt* attQty = dynamic_cast<infNoiseAttRngQnt*>(paramQuantities[SCALE_PARAM]);
            attQty->setRange(attRng.act, "Scale");
            attQty->setRangeLights(this, SCALE_LIGHT);
        }

        haveOutputs = false;
        firstIdx = -1;
        lastIdx = -1;
        for (int i = 0; i < 4; i++) {
            channels[i] = (inputs[A_INPUT + i].isConnected())
                ? std::max(inputs[A_INPUT + i].getChannels(), 1)
                : 1;
            outputs[A_OUTPUT + i].setChannels(channels[i]);

            if (outputs[A_OUTPUT + i].isConnected()) {
                if (firstIdx < 0)
                    firstIdx = i;
                lastIdx = i;
                haveOutputs = true;
            }
        }

        // Update order-lights (indicating scale/offset order)
        orderMode.updateActual();
        lights[OFFSET_LIGHT].value = (orderMode.act == offsetScale) ? 1.0f : 0.f;

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
            float scale = params[SCALE_PARAM].getValue();
            scale = applyScaleCurveSigned(scale, scaleMode.act);
            scale *= attRngFactor;
            float offset = params[OFFSET_PARAM].getValue();
            for (int i = firstIdx; i <= lastIdx; i++) {
                if (outputs[A_OUTPUT + i].isConnected()) {
                    bool haveInput = inputs[A_INPUT + i].isConnected();
                    for (int c = 0; c < channels[i]; c++) {
                        float output = (haveInput)
                            ? (orderMode.act == scaleOffset)
                                ? inputs[A_INPUT + i].getVoltage(c) * scale + offset
                                : (inputs[A_INPUT + i].getVoltage(c) + offset) * scale
                            : 0.f;
                        output = quantizeToMode(output, outQuantize.act);
                        output = clipToVoltRange(output, outClipRange.act);
                        outputs[A_OUTPUT + i].setVoltage(output, c);
                    }
                }
            }
        }

        cycle256++;
    }
};

struct Tweak4IModuleWidget : InfNoiseModuleWidget {
    Tweak4IModuleWidget(Tweak4IModule *module) {
        initializeWidget(module, "res/Tweak4I");

        const float centerCol = 15.f;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, 56.070f), module, Tweak4IModule::SCALE_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, 100.084f), module, Tweak4IModule::OFFSET_PARAM));

        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 155.677f), module, Tweak4IModule::A_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 180.309f), module, Tweak4IModule::B_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 204.941f), module, Tweak4IModule::C_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 229.574f), module, Tweak4IModule::D_INPUT));

        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 258.797f), module, Tweak4IModule::A_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 283.429f), module, Tweak4IModule::B_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 308.061f), module, Tweak4IModule::C_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 332.694f), module, Tweak4IModule::D_OUTPUT));

        const float sclOfsLgtCol = 5.488;
        addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(Vec(sclOfsLgtCol, 38.933f), module, Tweak4IModule::SCALE_LIGHT));
        addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(26.903f, 39.030f), module, Tweak4IModule::EXP_SCALE_LIGHT));
        addChild(createLightCentered<SmallLight<BlueLight>>(Vec(sclOfsLgtCol, 82.203f), module, Tweak4IModule::OFFSET_LIGHT));
    }

    void appendContextMenu(Menu* menu) override {
        Tweak4IModule* module = dynamic_cast<Tweak4IModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> scaleRangeNames = { "1x (-100\% to +100\%)",
            "2x (-200\% to +200\%)", "5x (-500\% to +500\%)", "10x (-1000\% to +1000\%)" };
        menu->addChild(createIndexPtrSubmenuItem("Scale-range mode", scaleRangeNames,
            &module->attRng.req));

        menu->addChild(createIndexPtrSubmenuItem("Scale-mode", getScaleCurveMenuNames(),
            &module->scaleMode.req));

        std::vector<std::string> intervalNames = getVoltIntervalValuesNames();
        menu->addChild(createSubmenuItem("Set offset (semitone steps)", "", [=](Menu* submenu) {
            for (int i = 0; i < voltIntervalValueCount; i++) {
                submenu->addChild(createMenuItem(intervalNames[i], "", [=]() {
                    module->params[Tweak4IModule::OFFSET_PARAM].setValue(
                        voltIntervalValues[(voltIntervalValue)i]);
                }));
            }
        }));

        menu->addChild(createIndexPtrSubmenuItem("Operation-order",
		 	{"Scale->Offset", "Offset->Scale"},
		 	&module->orderMode.req
        ));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelTweak4I = createModel<Tweak4IModule, Tweak4IModuleWidget>("Tweak4I");