// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inUtil.hpp"

struct AutoScale4Module : InfNoiseModule {
    enum ParamId {
        MIN_PARAM,
        MAX_PARAM,
        LINK_PARAM,
        RESET_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        RESET_INPUT,
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
        MIN_LIGHT,
        A_LIGHT,
        B_LIGHT,
        C_LIGHT,
        D_LIGHT,
        LIGHTS_LEN
    };

    autoScaleData autoScaleSection[5] = { autoScaleData(), autoScaleData(),
        autoScaleData(), autoScaleData(), autoScaleData() };  // autoScaleSection[4] used for common
    enum sectionSelectionType { ss_individual, ss_common };
    actReqValue<sectionSelectionType> sectSelection = actReqValue<sectionSelectionType>(ss_individual); 
    enum updateScaleOffsetType { uso_disabled, uso_enabled };
    actReqValue<updateScaleOffsetType> updateScaleOffset = actReqValue<updateScaleOffsetType>(uso_enabled);
    float minRange = 0.0f;
    float maxRange = 0.0f;
    float rangeCenter = 0.0f;
    bool invRange = false;
    bool maxLinkedToMin = false;
    bool haveConnections = false;
    bool inConn[4] = { false, false, false, false};
    bool outConn[4] = { false, false, false, false};
    int firstIdx = -1;
    int lastIdx = -1;    
    int channels[4] = { 1, 1, 1, 1 };
    dsp::TSchmittTrigger<float> resetTrigger;
    infNoiseDecayValue lightDecay[4] = {
        infNoiseDecayValue(2.f, 30.f),
        infNoiseDecayValue(2.f, 30.f),
        infNoiseDecayValue(2.f, 30.f),
        infNoiseDecayValue(2.f, 30.f)
    };


    AutoScale4Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
        
        configParam(MIN_PARAM, -10.0f, 10.0f, -5.0f, "Min-range", " V");
        configParam(MAX_PARAM, -10.0f, 10.0f, 5.0f, "Max-range", " V");
        configSwitch(LINK_PARAM, 0.0, 1.0, 0.0, "Link max to min (mirrored)", { "Off", "On" });
        configSwitch(RESET_PARAM, 0.0, 1.0, 0.0, "Reset");

        configInput(RESET_INPUT, "Reset");
        configInput(A_INPUT, "A");
        configInput(B_INPUT, "B");
        configInput(C_INPUT, "C");
        configInput(D_INPUT, "D");
        
        configOutput(A_OUTPUT, "AutoScaled-A");
        configOutput(B_OUTPUT, "AutoScaled-B");
        configOutput(C_OUTPUT, "AutoScaled-C");
        configOutput(D_OUTPUT, "AutoScaled-D");

        configBypass(A_INPUT, A_OUTPUT);
        configBypass(B_INPUT, B_OUTPUT);
        configBypass(C_INPUT, C_OUTPUT);
        configBypass(D_INPUT, D_OUTPUT);

        configLight(MIN_LIGHT, "Inverted range (when min > max)");
        configLight(A_LIGHT, "A-input exceeds range");
        configLight(B_LIGHT, "B-input exceeds range");
        configLight(C_LIGHT, "C-input exceeds range");
        configLight(D_LIGHT, "D-input exceeds range");

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
        haveOutQuantize = false;
        haveOutClipRange = false;
        haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = true;
        haveTrigHighLow = false;
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);

        resetTrigger.reset();
        updateScaleOffset.setBoth(uso_enabled);
        sectSelection.setBoth(ss_individual);
        autoScaleSection[0].reset();
        autoScaleSection[1].reset();
        autoScaleSection[2].reset();
        autoScaleSection[3].reset();
        autoScaleSection[4].reset(); // Common section

        for (int i = 0; i < 4; i++) {
            lightDecay[i].reset();
        }
    }

    void setMinMaxRange(float minValue, float maxValue) {
        params[MIN_PARAM].setValue(minValue);
        params[MAX_PARAM].setValue(maxValue);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);

        sectSelection.setBoth((sectionSelectionType)getJsonInt(rootJ, "sectSelection", (int)ss_individual));
        updateScaleOffset.setBoth((updateScaleOffsetType)getJsonInt(rootJ, "updateScaleOffset", (int)uso_enabled));
        for (int i = 0; i < 5; i++)
            autoScaleSection[i].Load(rootJ, string::f("sd%d", i));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "sectSelection", json_integer((int)sectSelection.req));
        json_object_set_new(rootJ, "updateScaleOffset", json_integer((int)updateScaleOffset.req));
        for (int i = 0; i < 5; i++)
            autoScaleSection[i].Save(rootJ, string::f("sd%d", i));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        updateScaleOffset.updateActual();
        if (sectSelection.act != sectSelection.req)  // User changed selection
        {
            autoScaleSection[0].reset();
            autoScaleSection[1].reset();
            autoScaleSection[2].reset();
            autoScaleSection[3].reset();
            autoScaleSection[4].reset(); // Common section
        }
        sectSelection.updateActual();

        maxLinkedToMin = params[LINK_PARAM].getValue() > 0.5f;
        if (maxLinkedToMin) {
            params[MAX_PARAM].setValue(params[MIN_PARAM].getValue() * -1.f);
        }

        invRange = params[MIN_PARAM].getValue() > params[MAX_PARAM].getValue();
        minRange = std::min(params[MIN_PARAM].getValue(), params[MAX_PARAM].getValue());
        maxRange = std::max(params[MIN_PARAM].getValue(), params[MAX_PARAM].getValue());
        float rangeSpan = maxRange - minRange;
        float halfRangeSpan = rangeSpan / 2.f;
        rangeCenter = minRange + halfRangeSpan;

        lights[MIN_LIGHT].setBrightness(invRange ? 1.f : 0.f);

        haveConnections = false;
        firstIdx = -1;
        lastIdx = -1;
        for (int i = 0; i < 4; i++) {
            inConn[i] = inputs[A_INPUT + i].isConnected();
			outConn[i] = outputs[A_OUTPUT + i].isConnected();
            channels[i] = (inConn[i])
                ? std::max(inputs[A_INPUT + i].getChannels(), 1)
                : 1;
            outputs[A_OUTPUT + i].setChannels(channels[i]);
            if (!outConn[i] &&
                autoScaleSection[i].scale != 0.f)
                autoScaleSection[i].reset();  // Reset if not connected

            if (inConn[i] || outConn[i]) {
                haveConnections = true;
                if (firstIdx < 0)
                    firstIdx = i;
                lastIdx = i;
            }

            if (!inputs[A_INPUT + i].isConnected()) {
                lights[A_LIGHT + i].setBrightness(0.f);
                lightDecay[i].reset();
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

        if (doProcess) {
            // check for reset
            float voltage = params[RESET_PARAM].getValue() > 0.5
                ? 10.f
                : (inputs[RESET_INPUT].isConnected())
                    ? inputs[RESET_INPUT].getVoltage()
                    : 0.f;
            if (resetTrigger.process(voltage,
                trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act])) {
                updateScaleOffset.setBoth(uso_enabled);
                autoScaleSection[0].reset();
                autoScaleSection[1].reset();
                autoScaleSection[2].reset();
                autoScaleSection[3].reset();
                autoScaleSection[4].reset(); // Common section
            }

            if (haveConnections) {
                // First update all section-scale/offset (before extracting scaled values)
                int sectIdx = 0;
                if (updateScaleOffset.act == uso_enabled) {
                    for (int i = firstIdx; i <= lastIdx; i++) {
                        if (inConn[i]) {
                            sectIdx = (sectSelection.act == ss_common) ? 4 : i;
                            for (int c = 0; c < channels[i]; c++) {
                                voltage = inputs[A_INPUT + i].getVoltage(c);
                                autoScaleSection[sectIdx].updateScaleOffset(voltage, minRange, maxRange);
                            }
                        }
                    }
                }

                // Extract scaled values
                for (int i=firstIdx; i<=lastIdx; i++) {
                    if (!(inConn[i] || outConn[i]))
                        continue;

                    bool outOfRange = false;
                    if (outputs[A_OUTPUT + i].isConnected()) {
                        sectIdx = (sectSelection.act == ss_common) ? 4 : i;
                        for (int c = 0; c < channels[i]; c++) {
                            voltage = 0.f;
                            if (inputs[A_INPUT + i].isConnected()) {
                                voltage = inputs[A_INPUT + i].getVoltage(c);
                                if (voltage < minRange || voltage > maxRange)
                                    outOfRange = true;
                                if (!autoScaleSection[sectIdx].wasReset)
                                    voltage = voltage *
                                    autoScaleSection[sectIdx].scale + autoScaleSection[sectIdx].offset;
                            }

                            if (invRange)
                                voltage = (voltage - rangeCenter) * -1.f + rangeCenter;

                            voltage = clamp(voltage, minRange, maxRange);
                            outputs[A_OUTPUT + i].setVoltage(voltage, c);
                        }
                    }

                    if (outOfRange)
                        lightDecay[i].setDecayValue(1.f);
                    if (lightDecay[i].process(procSampleTime))
                    {
                        lights[A_LIGHT + i].setBrightness(lightDecay[i].decayValue);
                    }
                }
            }
        }

        cycle256++;
    }
};

struct AutoScale4ModuleWidget : InfNoiseModuleWidget {
    InfNoiseDisableOverlayGroup* linkMaxOverlayGroup = nullptr;
    bool maxLinkedToMin = false;

    AutoScale4ModuleWidget(AutoScale4Module *module) {
        initializeWidget(module, "res/AutoScale4");

        const float centerClmn = 15.f;
        const float lightClm = 24.819f;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerClmn, 49.999f), module, AutoScale4Module::MIN_PARAM));
        addChild(createLightCentered<TinyLight<RedLight>>(Vec(lightClm, 40.577f), module, AutoScale4Module::MIN_LIGHT));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerClmn, 86.884f), module, AutoScale4Module::MAX_PARAM));
        infNoiseLtSmallButton* linkBtn = createParamCentered<infNoiseLtSmallButton>(Vec(4.287f, 69.287f),
            module, AutoScale4Module::LINK_PARAM);
        linkBtn->setup(bc_green, false);
        addParam(linkBtn);

        InfNoiseDisableOverlayManager& overlayManager = getDisableOverlayManager();
        linkMaxOverlayGroup = overlayManager.addGroup("Max linked to min (mirrored)");
        linkMaxOverlayGroup->addTarget(InfNoiseOverlayTargetType::param, AutoScale4Module::MAX_PARAM);

        addInput(createInputCentered<ThemedPJ301MPort>(Vec(centerClmn, 124.914f), module, AutoScale4Module::RESET_INPUT));
        infNoiseLtSmallButton* resetBtn = createParamCentered<infNoiseLtSmallButton>(Vec(4.015f, 113.244f), 
            module, AutoScale4Module::RESET_PARAM);
        resetBtn->setup(bc_green, true);
        addParam(resetBtn);

        const float rowSpacing = 24.6323f;
        const float lightPortOffset = -9.518;
        float row = 155.769f;
        for (int i=0; i<4; i++)
        {
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerClmn, row), module, AutoScale4Module::A_INPUT + i));
            addChild(createLightCentered<TinyLight<RedLight>>(Vec(lightClm, row+lightPortOffset), module, AutoScale4Module::A_LIGHT + i));
            row += rowSpacing;
        }

        row = 258.797f;
        for (int i=0; i<4; i++)
        {
            addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerClmn, row), module, AutoScale4Module::A_OUTPUT + i));
            row += rowSpacing;
        }
    }

    void step() override {
        InfNoiseModuleWidget::step();

        if (!module)
            return;

        auto* m = static_cast<AutoScale4Module*>(module);
        if (linkMaxOverlayGroup) {
            if (m->maxLinkedToMin != maxLinkedToMin) {
                maxLinkedToMin = m->maxLinkedToMin;
                linkMaxOverlayGroup->setActive(maxLinkedToMin);
            }
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        AutoScale4Module* module = dynamic_cast<AutoScale4Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexPtrSubmenuItem("Update Scale/offset-values",
            { "Disabled (locked)", "Enabled" }, &module->updateScaleOffset.req
        ));

        menu->addChild(createIndexPtrSubmenuItem("Scale/offset-usage",
            { "Individual (per section)", "Common (all sections)" }, &module->sectSelection.req
        ));

        menu->addChild(new MenuSeparator);

		menu->addChild(createSubmenuItem("Set min/max-range", "",
			[=](Menu* menu) {
				menu->addChild(createMenuItem("-1 to +1", "", [=]() {
                  module->setMinMaxRange(-1.f, 1.f);
                  }));
				menu->addChild(createMenuItem("+1 to -1 (inverted)", "", [=]() {
                  module->setMinMaxRange(1.f, -1.f);
                  }));
				menu->addChild(createMenuItem("Bipolar -5 to +5", "", [=]() {
                  module->setMinMaxRange(-5.f, 5.f);
                  }));
				menu->addChild(createMenuItem("Bipolar +5 to -5 (inverted)", "", [=]() {
                  module->setMinMaxRange(5.f, -5.f);
                  }));
				menu->addChild(createMenuItem("0 to +1", "", [=]() {
                  module->setMinMaxRange(0.f, 1.f);
                  }));
				menu->addChild(createMenuItem("+1 to 0 (inverted)", "", [=]() {
                  module->setMinMaxRange(1.f, 0.f);
                  }));
				menu->addChild(createMenuItem("Unipolar 0 to +10", "", [=]()  {
                  module->setMinMaxRange(0.f, 10.f);
                  }));
				menu->addChild(createMenuItem("Unipolar +10 to 0 (inverted)", "", [=]()  {
                  module->setMinMaxRange(10.f, 0.f);
                  }));
			}
		));
       
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelAutoScale4 = createModel<AutoScale4Module, AutoScale4ModuleWidget>("AutoScale4");