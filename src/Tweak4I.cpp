// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inUtil.hpp"

struct Tweak4IModule : InfNoiseModule {
    enum ParamId {
        SCALE_PARAM,
        OFFSET_PARAM,
        ATT_RNG_PARAM,
        ORDER_PARAM,
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
        ENUMS(EXP_SCALE_LIGHT, 2),
        LIGHTS_LEN
    };

    actReqValue<scaleCurve> scaleMode = actReqValue<scaleCurve>(sc_linear);
    infNoiseAttRngQnt::attRange attRng = infNoiseAttRngQnt::attRange::ar_1x;
    float attRngFactor = 1.f;
    enum order { scaleOffset, offsetScale };
    order orderMode = scaleOffset;
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
        configSwitch(ATT_RNG_PARAM, 0.f, 3.f, 0.f, "Scale-range", { "1x", "2x", "5x", "10x" });
        configSwitch(ORDER_PARAM, 0.f, 1.f, 0.f, "Order", { "Scale->Offset", "Offset->Scale" });

        configInput(A_INPUT, "A");
        configInput(B_INPUT, "B");
        configInput(C_INPUT, "C");
		configInput(D_INPUT, "D");

        configOutput(A_OUTPUT, "A");
        configOutput(B_OUTPUT, "B");
        configOutput(C_OUTPUT, "C");
        configOutput(D_OUTPUT, "D");

        configLight(EXP_SCALE_LIGHT, "Scale-mode (unlit=Linear, green=Exp, red=Log)");

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

        attRng = infNoiseAttRngQnt::attRange::ar_1x;
        scaleMode.setBoth(sc_linear);
        orderMode = scaleOffset;

        // paramQuantity.defaultValue might not be correct yet, hence manually set value
        params[SCALE_PARAM].setValue(1.f);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);

        if (jsonVersion < 4) { // previous versions used context-menu items for Scale-range and Order-mode
            params[ATT_RNG_PARAM].setValue((float)getJsonInt(rootJ, "attRng", (int)infNoiseAttRngQnt::attRange::ar_1x));
            params[ORDER_PARAM].setValue((float)getJsonInt(rootJ, "orderMode", (int)order::scaleOffset));
        }
        scaleMode.setBoth((scaleCurve)getJsonInt(rootJ, "scaleMode", (int)sc_linear));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "scaleMode", json_integer((int)scaleMode.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        if (scaleMode.needsUpdate()) {
            scaleMode.updateActual();
            setScaleModeLight(this, EXP_SCALE_LIGHT, scaleMode.act);
        }

        int rngIdx = (int)(params[ATT_RNG_PARAM].getValue() + 0.5f);
        if (rngIdx < 0)
            rngIdx = 0;
        if (rngIdx > 3)
            rngIdx = 3;
        infNoiseAttRngQnt::attRange newRng = (infNoiseAttRngQnt::attRange)rngIdx;
        if (newRng != attRng || mustProcessParams) {
            attRng = newRng;
            const float rangeFactor[4] = { 1.f, 2.f, 5.f, 10.f };
            attRngFactor = rangeFactor[(int)attRng];
            infNoiseAttRngQnt* attQty = dynamic_cast<infNoiseAttRngQnt*>(paramQuantities[SCALE_PARAM]);
            if (attQty)
                attQty->setRange(attRng, "Scale");
        }

        orderMode = (params[ORDER_PARAM].getValue() > 0.5f) ? offsetScale : scaleOffset;

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
                            ? (orderMode == scaleOffset)
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
        addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_green, bc_yellow, bc_red>>(
            Vec(sclOfsLgtCol, 38.933f), module, Tweak4IModule::ATT_RNG_PARAM));
        addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(26.903f, 39.030f), module, Tweak4IModule::EXP_SCALE_LIGHT));
        addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_blue>>(
            Vec(sclOfsLgtCol, 82.203f), module, Tweak4IModule::ORDER_PARAM));
    }

    void appendContextMenu(Menu* menu) override {
        Tweak4IModule* module = dynamic_cast<Tweak4IModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

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

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelTweak4I = createModel<Tweak4IModule, Tweak4IModuleWidget>("Tweak4I");