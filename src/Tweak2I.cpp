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
        ATT_RNG_PARAM,
        ORDER_PARAM,
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
        ENUMS(EXP_SCALE_LIGHT, 2),
        LIGHTS_LEN
    };

    actReqValue<scaleCurve> scaleMode = actReqValue<scaleCurve>(sc_linear);
    infNoiseAttRngQnt::attRange attRng = infNoiseAttRngQnt::attRange::ar_1x;
    float attRngFactor = 1.f;
    enum order { scaleOffset, offsetScale };
    order orderMode = scaleOffset;
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
        configSwitch(ATT_RNG_PARAM, 0.f, 3.f, 0.f, "Scale-range", { "1x", "2x", "5x", "10x" });
        configSwitch(ORDER_PARAM, 0.f, 1.f, 0.f, "Order", { "Scale->Offset", "Offset->Scale" });

        configInput(A_INPUT, "A");
        configInput(B_INPUT, "B");
        configInput(SCALE_INPUT, "Scale CV");
        configInput(OFFSET_INPUT, "Offset CV (-10V to +10V)");

        configOutput(A_OUTPUT, "A");
        configOutput(B_OUTPUT, "B");

        configLight(EXP_SCALE_LIGHT, "Scale-mode (unlit=Linear, green=Exp, red=Log)");

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
            const float rangeFactors[4] = { 1.f, 2.f, 5.f, 10.f };
            attRngFactor = rangeFactors[(int)attRng];
            infNoiseAttRngQnt* attQty = dynamic_cast<infNoiseAttRngQnt*>(paramQuantities[SCALE_PARAM]);
            if (attQty)
                attQty->setRange(attRng, "Scale");
        }

        orderMode = (params[ORDER_PARAM].getValue() > 0.5f) ? offsetScale : scaleOffset;

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
                    float aOutput = (orderMode == scaleOffset) 
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
                    float bOutput = (orderMode == scaleOffset) 
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
        addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_green, bc_yellow, bc_red>>(
            Vec(lightCol, 39.155f), module, Tweak2IModule::ATT_RNG_PARAM));
        addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(27.159f, 39.155f), module, Tweak2IModule::EXP_SCALE_LIGHT));
        addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_blue>>(
            Vec(lightCol, 138.486f), module, Tweak2IModule::ORDER_PARAM));
    }

    void appendContextMenu(Menu* menu) override {
        Tweak2IModule* module = dynamic_cast<Tweak2IModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

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

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelTweak2I = createModel<Tweak2IModule, Tweak2IModuleWidget>("Tweak2I");
