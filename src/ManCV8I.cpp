// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct ManCV8IModule : InfNoiseModule {
    enum ParamId {
        MUTE_PARAM,
        MUTE_LATCH_PARAM,
        MAN_KNOB1_PARAM,
        MAN_KNOB2_PARAM,
        MAN_KNOB3_PARAM,
        MAN_KNOB4_PARAM,
        MAN_KNOB5_PARAM,
        MAN_KNOB6_PARAM,
        MAN_KNOB7_PARAM,
        MAN_KNOB8_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        INPUTS_LEN
    };
    enum OutputsId {
        MAN_POLY_OUTPUT,
        MAN1_OUTPUT,
        MAN2_OUTPUT,
        MAN3_OUTPUT,
        MAN4_OUTPUT,
        MAN5_OUTPUT,
        MAN6_OUTPUT,
        MAN7_OUTPUT,
        MAN8_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        POLY_HINT_LIGHT,
        LIGHTS_LEN
    };

    bool haveOutput = false;
    int firstIdx = -1;
    int lastIdx = -1;
    actReqValue<polyphonyMode> polyOutput = actReqValue<polyphonyMode>(poly_8);
    actReqValue<voltValue> muteLevel = actReqValue<voltValue>(v_zero);
    int polyChannels = 8;
    bool isMuted = false;

    ManCV8IModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
        configLight(POLY_HINT_LIGHT, "Poly output: lit when channel count is not 8");

        configSwitch(MUTE_PARAM, 0.0f, 1.0f, 0.0f, "Mute outputs", { "Unmuted", "Muted" });
        configSwitch(MUTE_LATCH_PARAM, 0.0f, 1.0f, 0.0f, "Latch mute button", { "Unlatched", "Latched" });
        configOutput(MAN_POLY_OUTPUT, "CV-polyphonic");
        for (int i = 0; i < 8; i++) {
            configParam(MAN_KNOB1_PARAM + i, -10.0f, 10.0f, 0.0f, string::f("Manual CV-%d", i + 1), " V");
            configOutput(MAN1_OUTPUT + i, string::f("CV-%d", i + 1));
        }

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
        haveAutoProcQuality = false;
        haveOutQuantize = true;
        haveOutClipRange = true;
        procQuality.setBoth(pq_balancedRate); // Ballanced rate (every 16th cycle)
        haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = false;
		haveTrigHighLow = false;
    }

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);

        procQuality.setBoth(pq_balancedRate); // Ballanced rate (every 16th cycle)
        polyOutput.setBoth(poly_8);
        muteLevel.setBoth(v_zero);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);

        polyOutput.setBoth((polyphonyMode)getJsonInt(rootJ, "polyOutput", (int)polyphonyMode::poly_8));
        muteLevel.setBoth((voltValue)getJsonInt(rootJ, "muteLevel", (int)v_zero));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "polyOutput", json_integer((int)polyOutput.req));
        json_object_set_new(rootJ, "muteLevel", json_integer((int)muteLevel.req));
    }

    void setAllManKnobs(std::vector<float> values) {
        assert(values.size() == 8);
        for (int i = 0; i < 8; i++)
            params[MAN_KNOB1_PARAM + i].setValue(values[i]);    
    }

    void processParams(const ProcessArgs& args)
    {
        preProcessParams(args);
        //--------------------

        haveOutput = outputs[MAN_POLY_OUTPUT].isConnected();
        firstIdx = -1;
        lastIdx = -1;
        for (int i = 0; i < 8; i++) {
            outputs[MAN1_OUTPUT + i].setChannels(1);
            if (outputs[MAN1_OUTPUT + i].isConnected()) {
                haveOutput = true;
                if (firstIdx < 0)
                    firstIdx = i;
                lastIdx = i;
            }
        }

        if (polyOutput.needsUpdate()) {
            polyOutput.updateActual();
            lights[POLY_HINT_LIGHT].setBrightness(polyOutput.req != poly_8 ? 1.f : 0.f);
            if (outputInfos.size() > (unsigned)MAN_POLY_OUTPUT && outputInfos[MAN_POLY_OUTPUT]) {
                outputInfos[MAN_POLY_OUTPUT]->name = polyPortPrefix() + "Poly output: " + getPolyphonyModeName(polyOutput.act);
            }
        }
        muteLevel.updateActual();
        polyChannels = polyphonyModeChannels[polyOutput.act];
        outputs[MAN_POLY_OUTPUT].setChannels(polyChannels);

        isMuted = params[MUTE_PARAM].getValue() > 0.5f;
     
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
            for (int i = 0; i < 8; i++) {
                float voltage = isMuted
                    ? voltValues[muteLevel.act]
                    : params[MAN_KNOB1_PARAM + i].getValue();
                voltage = quantizeToMode(voltage, outQuantize.act);
                voltage = clipToVoltRange(voltage, outClipRange.act);
                outputs[MAN1_OUTPUT + i].setVoltage(voltage);
                if (i < polyChannels)
                    outputs[MAN_POLY_OUTPUT].setVoltage(voltage, i);
            }
        }

        cycle256++;
    }
};

struct ManCV8IModuleWidget : InfNoiseModuleWidget {
    infNoiseSmallButton<bc_red, true>* muteBtn;

    ManCV8IModuleWidget(ManCV8IModule *module) {
        initializeWidget(module, "res/ManCV8I");

        const float knobCol = 14.810f;
        const float latchClm = 25.980f;
        const float latchOffset = 11.078f;
        const float outCol = 43.545f;
        const float attRow = 52.220f;
        const float lgtOfs = 10.021f;
        muteBtn = createParamCentered<infNoiseSmallButton<bc_red, true>>(Vec(knobCol, attRow), module, ManCV8IModule::MUTE_PARAM);
        addParam(muteBtn);
        addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(latchClm, attRow + latchOffset), module, ManCV8IModule::MUTE_LATCH_PARAM));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(outCol, attRow), module, ManCV8IModule::MAN_POLY_OUTPUT));
        addChild(createLightCentered<TinyLight<RedLight>>(Vec(outCol - lgtOfs, attRow - lgtOfs), module, ManCV8IModule::POLY_HINT_LIGHT));

        const float knobSpacing = 35.0735f;
        float knobRow = 87.179f;
        for (int i = 0; i < 8; i++) {
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(knobCol, knobRow), module, ManCV8IModule::MAN_KNOB1_PARAM + i));
            addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(outCol, knobRow), module, ManCV8IModule::MAN1_OUTPUT + i));
            knobRow += knobSpacing;
        }
    }

    void step() override {
        if (module) {
            muteBtn->momentary = module->params[ManCV8IModule::MUTE_LATCH_PARAM].getValue() < 0.5f;
        }
        InfNoiseModuleWidget::step();
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        ManCV8IModule* module = dynamic_cast<ManCV8IModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> voltNames = getVoltValuesNames();
        menu->addChild(createIndexPtrSubmenuItem("Mute voltage", voltNames,
            &module->muteLevel.req));

        menu->addChild(createSubmenuItem("Set knobs 1-8", "", [=](Menu* menu) {
            menu->addChild(createMenuItem("To zero", "(0, 0, 0, 0, 0, 0, 0, 0)", [=]() {
                module->setAllManKnobs(std::vector<float>({ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}));
                }));
            menu->addChild(createMenuItem("To Interval", "(0, 1, 2, 3, 4, 5, 7, 12 semitones)", [=]() {
                    module->setAllManKnobs(std::vector<float>({ 0.f, 1.f/12.f, 2.f/12.f, 3.f/12.f, 4.f/12.f, 5.f/12.f, 7.f/12.f, 1.f}));
                    }));
            menu->addChild(createMenuItem("To positive", "(0, 1, 2, 3, 4, 5, 8, 10)", [=]() {
                module->setAllManKnobs(std::vector<float>({ 0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 8.f, 10.f}));
                }));
            menu->addChild(createMenuItem("To negative", "(0, -1, -2, -3, -4, -5, -8, -10)", [=]() {
                module->setAllManKnobs(std::vector<float>({ 0.f, -1.f, -2.f, -3.f, -4.f, -5.f, -8.f, -10.f}));
                }));
            menu->addChild(createMenuItem("To negative/positive", "(-10, -5, -2, -1, 1, 2, 5, 10)", [=]() {
                module->setAllManKnobs(std::vector<float>({ -10.f, -5.f, -2.f, -1.f, 1.f, 2.f, 5.f, 10.f}));
                }));
            menu->addChild(createMenuItem("To 5V", "(5, 5, 5, 5, 5, 5, 5, 5)", [=]() {
                module->setAllManKnobs(std::vector<float>({ 5.f, 5.f, 5.f, 5.f, 5.f, 5.f, 5.f, 5.f}));
                }));
            menu->addChild(createMenuItem("To -5V", "(-5, -5, -5, -5, -5, -5, -5, -5)", [=]() {
                module->setAllManKnobs(std::vector<float>({ -5.f, -5.f, -5.f, -5.f, -5.f, -5.f, -5.f, -5.f}));
                }));
            menu->addChild(createMenuItem("To 10V", "(10, 10, 10, 10, 10, 10, 10, 10)", [=]() {
                module->setAllManKnobs(std::vector<float>({ 10.f, 10.f, 10.f, 10.f, 10.f, 10.f, 10.f, 10.f }));
                }));
            menu->addChild(createMenuItem("To -10V", "(-10, -10, -10, -10, -10, -10, -10, -10)", [=]() {
                module->setAllManKnobs(std::vector<float>({ -10.f, -10.f, -10.f, -10.f, -10.f, -10.f, -10.f, -10.f }));
                }));
            appendScaleChordSetMenuItems(menu, module, ManCV8IModule::MAN_KNOB1_PARAM, 8);
            }));

        std::vector<std::string> polyNames = getPolyphonyModeNames(false);
        polyNames.resize(8);
        menu->addChild(createIndexPtrSubmenuItem("Poly output channels", polyNames,
            &module->polyOutput.req));
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelManCV8I = createModel<ManCV8IModule, ManCV8IModuleWidget>("ManCV8I");