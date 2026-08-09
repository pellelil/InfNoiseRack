// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inUtil.hpp"
#include "dsp/resampler.hpp"

struct Clamp4Module : InfNoiseModule {
    enum ParamId {
        MIN_PARAM,
        MAX_PARAM,
        LINK_PARAM,
        CLAMP_MODE_PARAM,
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
        MIN_LIGHT,
        A_LIGHT,
        B_LIGHT,
        C_LIGHT,
        D_LIGHT,
        LIGHTS_LEN
    };

    enum clampModeType { cm_clamp, cm_diff, cm_gate };
    clampModeType clampMode = cm_clamp;
    enum diffModeType { dm_relDiff, dm_absDiff };
    actReqValue<diffModeType> diffMode = actReqValue<diffModeType>(dm_relDiff);
    enum gateModeType { gm_outOfRange, gm_inRange };
    actReqValue<gateModeType> gateMode = actReqValue<gateModeType>(gm_outOfRange);
    float minRange = 0.0f;
    float maxRange = 0.0f;
    float rangeCenter = 0.0f;
    bool invRange = false;
    bool maxLinkedToMin = false;
    bool haveConnections = false;
    int firstIdx = -1;
    int lastIdx = -1;    
    int channels[4] = { 1, 1, 1, 1 };
    infNoiseDecayValue lightDecay[4] = { 
        infNoiseDecayValue(15.f, 60.f), 
        infNoiseDecayValue(15.f, 60.f),
        infNoiseDecayValue(15.f, 60.f),
        infNoiseDecayValue(15.f, 60.f)
    };
    enum oversampModeType { os_single, os_2x };
    actReqValue<oversampModeType> oversampMode = actReqValue<oversampModeType>(os_single);
    static constexpr int maxChannels = PORT_MAX_CHANNELS;
    dsp::Upsampler<2, 8> upsampler[4][maxChannels];
    dsp::Decimator<2, 8, float> decimator[4][maxChannels];

    static inline float processSample(float inVoltage,
        clampModeType clampMode,
        bool invRange,
        float rangeCenter,
        float minRange,
        float maxRange,
        diffModeType diffModeAct,
        gateModeType gateModeAct,
        const float* voltValues,
        voltValue gateOutHighAct,
        voltValue gateOutLowAct,
        bool& outOfRange)
    {
        float voltage = inVoltage;

        switch (clampMode) {
        case cm_clamp: {
            if (invRange)
                voltage = (voltage - rangeCenter) * -1.f + rangeCenter;

            if (voltage < minRange || voltage > maxRange) {
                voltage = clamp(voltage, minRange, maxRange);
                outOfRange = true;
            }
            break;
        }
        case cm_diff: {
            if (voltage >= minRange && voltage <= maxRange) {
                voltage = 0.0f;
            }
            else if (voltage < minRange) {
                voltage = std::abs(voltage - minRange) * -1.f;
                outOfRange = true;
            }
            else if (voltage > maxRange) {
                voltage = voltage - maxRange;
                outOfRange = true;
            }

            if (diffModeAct == dm_absDiff)
                voltage = std::abs(voltage);
            break;
        }
        case cm_gate: {
            bool gate = (voltage < minRange || voltage > maxRange);
            if (gate)
                outOfRange = true;
            if (gateModeAct == gm_inRange)
                gate = !gate;

            voltage = gate
                ? voltValues[gateOutHighAct]
                : voltValues[gateOutLowAct];
            break;
        }
        }

        return voltage;
    }
    Clamp4Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configParam(MIN_PARAM, -10.0f, 10.0f, -5.0f, "Min-range", " V");
        configParam(MAX_PARAM, -10.0f, 10.0f, 5.0f, "Max-range", " V");
        configSwitch(LINK_PARAM, 0.0, 1.0, 0.0, "Link max to min (mirrored)", { "Off", "On" });
        configSwitch(CLAMP_MODE_PARAM, 0.0, 2.0, 0.0, "Clamp-mode", { "Clamp/Clip", "Diff", "Gate" });

        configInput(A_INPUT, "A");
        configInput(B_INPUT, "B");
        configInput(C_INPUT, "C");
        configInput(D_INPUT, "D");
        
        configOutput(A_OUTPUT, "A-Clamped/clipped");
        configOutput(B_OUTPUT, "B-Clamped/clipped");
        configOutput(C_OUTPUT, "C-Clamped/clipped");
        configOutput(D_OUTPUT, "D-Clamped/clipped");

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
		haveTrigDetect = false;
		haveTrigHighLow = false;
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        
        diffMode.setBoth(dm_relDiff);
        gateMode.setBoth(gm_outOfRange);

        for (int i = 0; i < 4; i++) {
			lightDecay[i].reset();
		}
        oversampMode.setBoth(os_single);
        for (int i = 0; i < 4; i++) {
            for (int c = 0; c < maxChannels; c++) {
                upsampler[i][c].reset();
                decimator[i][c].reset();
            }
        }
    }

    void onSampleRateChange() override {
        InfNoiseModule::onSampleRateChange();
        for (int i = 0; i < 4; i++) {
            for (int c = 0; c < maxChannels; c++) {
                upsampler[i][c].reset();
                decimator[i][c].reset();
            }
        }
    }

    void setMinMaxRange(float minValue, float maxValue) {
        params[MIN_PARAM].setValue(minValue);
        params[MAX_PARAM].setValue(maxValue);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        diffMode.setBoth((diffModeType)getJsonInt(rootJ, "diffMode", (int)diffModeType::dm_relDiff));
        gateMode.setBoth((gateModeType)getJsonInt(rootJ, "gateMode", (int)gateModeType::gm_outOfRange));
        oversampMode.setBoth((oversampModeType)getJsonInt(rootJ, "oversampMode", (int)os_single));
    }

    void
     dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "diffMode", json_integer((int)diffMode.req));
        json_object_set_new(rootJ, "gateMode", json_integer((int)gateMode.req));
        json_object_set_new(rootJ, "oversampMode", json_integer((int)oversampMode.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        diffMode.updateActual();
        gateMode.updateActual();
        if (oversampMode.needsUpdate()) {
            oversampMode.updateActual();
            for (int i = 0; i < 4; i++) {
                for (int c = 0; c < maxChannels; c++) {
                    upsampler[i][c].reset();
                    decimator[i][c].reset();
                }
            }
        }

        clampMode = cm_clamp;
        if (params[CLAMP_MODE_PARAM].getValue() > 0.5f) {
            clampMode = cm_diff;
            if (params[CLAMP_MODE_PARAM].getValue() > 1.5f)
                clampMode = cm_gate;
        }

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

        lights[MIN_LIGHT].setBrightness(invRange && clampMode == cm_clamp 
            ? 1.f 
            : 0.f);

        haveConnections = false;
        firstIdx = -1;
        lastIdx = -1;
        for (int i = 0; i < 4; i++) {
            channels[i] = inputs[A_INPUT + i].isConnected()
                ? std::max(inputs[A_INPUT + i].getChannels(), 1)
                : 1;
            outputs[A_OUTPUT + i].setChannels(channels[i]);

            if (inputs[A_INPUT + i].isConnected() || outputs[A_OUTPUT +i].isConnected()) {
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

        if (doProcess && haveConnections) {
            float voltage = 0.0f;
            for (int i=firstIdx; i<=lastIdx; i++) {
                bool outOfRange = false;
                for (int c=0; c<channels[i]; c++) {
                    float inVoltage = (inputs[A_INPUT + i].isConnected())
                        ? inputs[A_INPUT + i].getVoltage(c)
                        : 0.f;

                    if (oversampMode.act == os_single) {
                        voltage = processSample(inVoltage,
                            clampMode,
                            invRange,
                            rangeCenter,
                            minRange,
                            maxRange,
                            diffMode.act,
                            gateMode.act,
                            voltValues,
                            gateOutHigh.act,
                            gateOutLow.act,
                            outOfRange);
                    }
                    else {
                        float buf[2];
                        upsampler[i][c].process(inVoltage, buf);
                        buf[0] = processSample(buf[0],
                            clampMode,
                            invRange,
                            rangeCenter,
                            minRange,
                            maxRange,
                            diffMode.act,
                            gateMode.act,
                            voltValues,
                            gateOutHigh.act,
                            gateOutLow.act,
                            outOfRange);
                        buf[1] = processSample(buf[1],
                            clampMode,
                            invRange,
                            rangeCenter,
                            minRange,
                            maxRange,
                            diffMode.act,
                            gateMode.act,
                            voltValues,
                            gateOutHigh.act,
                            gateOutLow.act,
                            outOfRange);
                        voltage = decimator[i][c].process(buf);
                    }

                    if (outputs[A_OUTPUT + i].isConnected())
                        outputs[A_OUTPUT + i].setVoltage(voltage, c);
                }

                if (outOfRange)
                    lightDecay[i].setDecayValue(1.f);
                if (lightDecay[i].process(procSampleTime))
                {
                    lights[A_LIGHT + i].setBrightness(lightDecay[i].decayValue);
                }
            }
        }

        cycle256++;
    }
};

struct Clamp4ModuleWidget : InfNoiseModuleWidget {
    InfNoiseDisableOverlayGroup* linkMaxOverlayGroup = nullptr;
    bool maxLinkedToMin = false;

    Clamp4ModuleWidget(Clamp4Module *module) {
        initializeWidget(module, "res/Clamp4");

        const float centerClmn = 15.f;
        const float lightClm = 24.819f;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerClmn, 49.999f), module, Clamp4Module::MIN_PARAM));
        addChild(createLightCentered<TinyLight<RedLight>>(Vec(lightClm, 40.577f), module, Clamp4Module::MIN_LIGHT));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerClmn, 86.884f), module, Clamp4Module::MAX_PARAM));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(
            Vec(4.287f, 69.287f),
            module, Clamp4Module::LINK_PARAM));

        InfNoiseDisableOverlayManager& overlayManager = getDisableOverlayManager();
        linkMaxOverlayGroup = overlayManager.addGroup("Max linked to min (mirrored)");
        linkMaxOverlayGroup->addTarget(InfNoiseOverlayTargetType::param, Clamp4Module::MAX_PARAM);

        addParam(createParamCentered<CKSSThree>(Vec(8.489f, 122.435f), module, Clamp4Module::CLAMP_MODE_PARAM));

        const float rowSpacing = 24.6323f;
        const float lightPortOffset = -9.518;
        float row = 155.769f;
        for (int i=0; i<4; i++)
        {
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerClmn, row), module, Clamp4Module::A_INPUT + i));
            addChild(createLightCentered<TinyLight<RedLight>>(Vec(lightClm, row+lightPortOffset), module, Clamp4Module::A_LIGHT + i));
            row += rowSpacing;
        }

        row = 258.797f;
        for (int i=0; i<4; i++)
        {
            addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerClmn, row), module, Clamp4Module::A_OUTPUT + i));
            row += rowSpacing;
        }
    }

    void step() override {
        InfNoiseModuleWidget::step();

        if (!module)
            return;

        auto* m = static_cast<Clamp4Module*>(module);
        if (linkMaxOverlayGroup) {
            if (m->maxLinkedToMin != maxLinkedToMin) {
                maxLinkedToMin = m->maxLinkedToMin;
                linkMaxOverlayGroup->setActive(maxLinkedToMin);
            }
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        Clamp4Module* module = dynamic_cast<Clamp4Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

		menu->addChild(createIndexPtrSubmenuItem("Diff-mode",
		 	{"Relative", "Absolute"},
		 	&module->diffMode.req
        ));

		menu->addChild(createIndexPtrSubmenuItem("Gate-mode",
		 	{"High when out of range", "High when in range"},
		 	&module->gateMode.req
        ));

        menu->addChild(new MenuSeparator);

        std::vector<std::string> oversampNames = { "Single sample", "2x oversampling" };
        menu->addChild(createIndexPtrSubmenuItem("Oversampling mode", oversampNames,
            &module->oversampMode.req));

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

Model *modelClamp4 = createModel<Clamp4Module, Clamp4ModuleWidget>("Clamp4");
