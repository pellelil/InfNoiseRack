// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inUtil.hpp"

struct Tweak2IModule : InfNoiseModule {
    enum ParamId {
        SCALE_PARAM,
        SCALE_TRIM_PARAM,
        OFFSET_PARAM,
        OFFSET_TRIM_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        A_INPUT,
        B_INPUT,
        SCALE_INPUT,
        OFFSET_INPUT,
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
        ENUMS(SCALE_LIGHT,3),
        OFFSET_LIGHT,
        ENUMS(EXP_SCALE_LIGHT, 2),
        LIGHTS_LEN
    };

    actReqValue<scaleCurve> scaleMode = actReqValue<scaleCurve>(sc_linear);
    actReqValue<infNoiseAttRngQnt::attRange> attRng =
        actReqValue<infNoiseAttRngQnt::attRange>(infNoiseAttRngQnt::attRange::ar_1x);
    float attRngFactor = 1.f;
    enum order { scaleOffset, offsetScale };
    actReqValue<order> orderMode = actReqValue<order>(scaleOffset);
    bool haveAOutput = false;
    bool haveAInput = false;
    bool haveBOutput = false;
    bool haveBInput = false;
    bool haveScaleInput = false;
    bool haveOffsetInput = false;
    bool haveOutputs = false;
    float scaleParam = 0.f;
    float offsetParam = 0.f;
    float scaleTrim = 0.f;
    float offsetTrim = 0.f;
    int aChannels = 1;
    int bChannels = 1;
    int maxChannels = 1;

    Tweak2IModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configParam<infNoiseAttRngQnt>(SCALE_PARAM, -1.f, 1.f, 1.f, "Scale (-1x to +1x)", " x", 0, 1);
        configParam(SCALE_TRIM_PARAM, -1.f, 1.f, 0.f, "Scale CV-trim", "%", 0, 100);
        configParam(OFFSET_PARAM, -10.0f, 10.0f, 0.0f, "Offset (-10V to +10V)", " V");
        configParam(OFFSET_TRIM_PARAM, -1.f, 1.f, 0.f, "Offset CV-tirm", "%", 0, 100);

        configInput(A_INPUT, "A");
        configInput(B_INPUT, "B");
        configInput(SCALE_INPUT, "Scale CV");
        configInput(OFFSET_INPUT, "Offset CV (-10V to +10V)");

        configOutput(A_OUTPUT, "A");
        configOutput(B_OUTPUT, "B");

        configLight(SCALE_LIGHT, "Scale-range (unlit=1x, green=2x, yellow=5x, red=10x)");
        configLight(EXP_SCALE_LIGHT, "Scale-mode (unlit=Linear, green=Exp, red=Log)");
        configLight(OFFSET_LIGHT, "Order: Scale->Offset if unlit, else Offset->Scale");

        configBypass(A_INPUT, A_OUTPUT);

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
            const float rangeFactors[4] = { 1.f, 2.f, 5.f, 10.f };
            attRngFactor = rangeFactors[(int)attRng.act];

            infNoiseAttRngQnt* attQty = dynamic_cast<infNoiseAttRngQnt*>(paramQuantities[SCALE_PARAM]);
            attQty->setRange(attRng.act, "Scale");
            attQty->setRangeLights(this, SCALE_LIGHT);
        }

        haveAInput = inputs[A_INPUT].isConnected();
        haveAOutput = outputs[A_OUTPUT].isConnected();
        aChannels = (haveAInput) ? inputs[A_INPUT].getChannels() : 1;
        outputs[A_OUTPUT].setChannels(aChannels);
        
        haveBInput = inputs[B_INPUT].isConnected();
        haveBOutput = outputs[B_OUTPUT].isConnected();
        bChannels = (haveBInput) ? inputs[B_INPUT].getChannels() : 1;
        outputs[B_OUTPUT].setChannels(bChannels);

        haveScaleInput = inputs[SCALE_INPUT].isConnected();
        haveOffsetInput = inputs[OFFSET_INPUT].isConnected();
        scaleParam = params[SCALE_PARAM].getValue();
        offsetParam = params[OFFSET_PARAM].getValue();
        scaleTrim = params[SCALE_TRIM_PARAM].getValue();
        offsetTrim = params[OFFSET_TRIM_PARAM].getValue();

        maxChannels = std::max(aChannels, bChannels);

        // Update order-light (indicating scale/offset order)
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

        if (doProcess && (haveAOutput || haveBOutput)) {
            for (int c = 0; c < maxChannels; c++) {
                float scale = scaleParam;
                if (haveScaleInput) {
                    scale += scaleTrim * (inputs[SCALE_INPUT].getPolyVoltage(c) / 5.f);
                    scale = clamp(scale, -1.f, 1.f);
                }
                scale = applyScaleCurveSigned(scale, scaleMode.act);
                scale *= attRngFactor;

                float offset = offsetParam;
                if (haveOffsetInput) {
                    offset += offsetTrim * inputs[OFFSET_INPUT].getPolyVoltage(c);
                }

                if (haveAOutput && c < aChannels) {
                    float aInput = (haveAInput)
                        ? inputs[A_INPUT].getVoltage(c)
                        :0.f; 
                    float aOutput = (orderMode.act == scaleOffset) 
                        ? aInput * scale + offset
                        : (aInput + offset) * scale;
                    aOutput = quantizeToMode(aOutput, outQuantize.act);
                    aOutput = clipToVoltRange(aOutput, outClipRange.act);
                    outputs[A_OUTPUT].setVoltage(aOutput, c);
                }
                if (haveBOutput && c < bChannels) {
                    float bInput = (haveBInput)
                        ? inputs[B_INPUT].getVoltage(c)
                        :0.f; 
                    float bOutput = (orderMode.act == scaleOffset) 
                        ? bInput * scale + offset
                        : (bInput + offset) * scale;
                    bOutput = quantizeToMode(bOutput, outQuantize.act);
                    bOutput = clipToVoltRange(bOutput, outClipRange.act);
                    outputs[B_OUTPUT].setVoltage(bOutput, c);
                }
            }
        }

        cycle256++;
    }
};

struct Tweak2IModuleWidget : InfNoiseModuleWidget {
    Tweak2IModuleWidget(Tweak2IModule *module) {
        initializeWidget(module, "res/Tweak2I");

        const float centerCol = 15.f;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, 56.070f), module, Tweak2IModule::SCALE_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(centerCol, 83.809f), module, Tweak2IModule::SCALE_TRIM_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 111.898f), module, Tweak2IModule::SCALE_INPUT));

        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, 155.498f), module, Tweak2IModule::OFFSET_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(centerCol, 183.237f), module, Tweak2IModule::OFFSET_TRIM_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 211.326f), module, Tweak2IModule::OFFSET_INPUT));

        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 246.911f), module, Tweak2IModule::A_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 271.543f), module, Tweak2IModule::B_INPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 308.061f), module, Tweak2IModule::A_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 332.694f), module, Tweak2IModule::B_OUTPUT));

        const float lightCol = 5.454f;
        addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(Vec(lightCol, 39.155f), module, Tweak2IModule::SCALE_LIGHT));
        addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(27.159f, 39.155f), module, Tweak2IModule::EXP_SCALE_LIGHT));
        addChild(createLightCentered<SmallLight<BlueLight>>(Vec(lightCol, 138.486f), module, Tweak2IModule::OFFSET_LIGHT));
    }

    void appendContextMenu(Menu* menu) override {
        Tweak2IModule* module = dynamic_cast<Tweak2IModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> scaleRangeNames = { "1x (-100\% to +100\%)",
            "2x (-200\% to +200\%)", "5x (-500\% to +500\%)", "10x (-1000\% to +1000\%)" };
        menu->addChild(createIndexPtrSubmenuItem("Scale-range", scaleRangeNames,
            &module->attRng.req));

        menu->addChild(createIndexPtrSubmenuItem("Scale-mode", getScaleCurveMenuNames(),
            &module->scaleMode.req));

        std::vector<std::string> intervalNames = getVoltIntervalValuesNames();
        menu->addChild(createSubmenuItem("Set offset (semitone steps)", "", [=](Menu* submenu) {
            for (int i = 0; i < voltIntervalValueCount; i++) {
                submenu->addChild(createMenuItem(intervalNames[i], "", [=]() {
                    module->params[Tweak2IModule::OFFSET_PARAM].setValue(
                        voltIntervalValues[(voltIntervalValue)i]);
                }));
            }
        }));
    
        menu->addChild(createIndexPtrSubmenuItem("Order",
		 	{"Scale->Offset", "Offset->Scale"},
		 	&module->orderMode.req));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelTweak2I = createModel<Tweak2IModule, Tweak2IModuleWidget>("Tweak2I");
