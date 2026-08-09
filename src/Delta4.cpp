// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct Delta4Module : InfNoiseModule {
    enum ParamId {
        RESET_PUSH_PARAM,
        RESET_TRIGGATE_PARAM,
        REF_MODE_PARAM,
        TOGGLE_OUTPUT_MODE_PARAM,
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
        ENUMS(PROCQUAL_LIGHT, 2),
        ENUMS(CLIP_RANGE_LIGHT, 2),
        OM_SIGN_LIGHT,
        OM_INV_LIGHT,
        OM_ABS_LIGHT,
        LIGHTS_LEN
    };

    enum referenceModeType { rm_individual, rm_aOnly };
    referenceModeType referenceMode = rm_individual;
    dsp::SchmittTrigger outModePress;
    enum outputModeType { om_signed, om_inverted, om_absolute };
    actReqValue<outputModeType> outputMode = actReqValue<outputModeType>(om_signed);
    float outModeSign = 1.f;
    bool outModeAbs = false;

    static constexpr int refCount = 4 * PORT_MAX_CHANNELS; // 4 sections, with up to 16 channels
    float refVal[refCount] = { 0.f }; 
    int channels[4] = { 1, 1, 1, 1 };
    bool inConn[4] = { false, false, false, false};
    bool outConn[4] = { false, false, false, false};
    bool haveConnections = false;
    int firstIdx = -1;
    int lastIdx = -1;
    dsp::SchmittTrigger resetTrigger;
    bool triggerMode = false;
    
	Delta4Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configSwitch(RESET_PUSH_PARAM, 0.0f, 1.0f, 0.0f, "Reset-button" );
        configSwitch(RESET_TRIGGATE_PARAM, 0.0f, 1.0f, 1.0f, "Reset-trigger/gate", { "Gate-mode (continuous)", "Trigger-mode (single)" });
        configInput(RESET_INPUT, "Reset-trigger/gate");

        configSwitch(REF_MODE_PARAM, 0.0f, 1.0f, 0.0f, "Reference-mode", { "Individual", "A-only" });

        configInput(A_INPUT, "A");
        configInput(B_INPUT, "B");
        configInput(C_INPUT, "C");
        configInput(D_INPUT, "D");
        
        configSwitch(TOGGLE_OUTPUT_MODE_PARAM, 0.0f, 1.0f, 0.0f, "Toggle output mode");
        configLight(OM_SIGN_LIGHT, "Signed output when lit");
        configLight(OM_INV_LIGHT, "Inverted output when lit");
        configLight(OM_ABS_LIGHT, "Absolute output when lit");

        configOutput(A_OUTPUT, "A-Delta");
        configOutput(B_OUTPUT, "B-Delta");
        configOutput(C_OUTPUT, "C-Delta");
        configOutput(D_OUTPUT, "D-Delta");

        configBypass(A_INPUT, A_OUTPUT);
        configBypass(B_INPUT, B_OUTPUT);
        configBypass(C_INPUT, C_OUTPUT);
        configBypass(D_INPUT, D_OUTPUT);

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
		haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = true;  
		haveGateDetect = true;
		haveGateHighLow = false;
		haveTrigDetect = true;
		haveTrigHighLow = false;
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        
        params[TOGGLE_OUTPUT_MODE_PARAM].setValue(0.f);
        outputMode.setBoth(om_signed);
        outModePress.reset();

        resetTrigger.reset();
        params[REF_MODE_PARAM].setValue(0.f);
        referenceMode = rm_individual;
        for (int i = 0; i < refCount; i++) {
            refVal[i] = 0.f;
        }
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        getJsonFloatArray(rootJ, "refVal", refVal, refCount, 0.f);
        outputMode.setBoth((outputModeType)getJsonInt(rootJ, "outputMode", (int)om_signed));

        outModePress.reset();
    }

    void dataToJson(json_t* rootJ) override {
        setJsonFloatArray(rootJ, "refVal", refVal, refCount);
        json_object_set_new(rootJ, "outputMode", json_integer((int)outputMode.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        // Connection status/channel count
        haveConnections = false;
        firstIdx = -1;
        lastIdx = -1;
        for (int i = 0; i < 4; i++) {
            inConn[i] = inputs[A_INPUT + i].isConnected();
            channels[i] = inConn[i]
                ? std::max(inputs[A_INPUT + i].getChannels(), 1)
                : 1;
            outputs[A_OUTPUT + i].setChannels(channels[i]);

            outConn[i] = outputs[A_OUTPUT + i].isConnected();
            if (inConn[i] || outConn[i]) {
                if (firstIdx < 0)
                    firstIdx = i;
                lastIdx = i;
                haveConnections = true;
            }
        }

        // Reference mode
        referenceMode = (params[REF_MODE_PARAM].getValue() < 0.5f) ? rm_individual : rm_aOnly;
        if (referenceMode == rm_aOnly) {
            // We must at least be ready to reset to the A-section
            firstIdx = 0;
            if (lastIdx < 0)
                lastIdx = 0;
        }

        // Handle output mode toggle
        if (outModePress.process(params[TOGGLE_OUTPUT_MODE_PARAM].getValue(), 0.1f, 0.5f)) {
            outputMode.setBoth(outputMode.req == om_absolute ? om_signed : static_cast<outputModeType>(outputMode.req + 1 ));
        }
        if (outputMode.needsUpdate()) {
            outputMode.updateActual();
            lights[OM_SIGN_LIGHT].setBrightness(outputMode.act == om_signed ? 1.f : 0.f);
            lights[OM_INV_LIGHT].setBrightness(outputMode.act == om_inverted ? 1.f : 0.f);
            lights[OM_ABS_LIGHT].setBrightness(outputMode.act == om_absolute ? 1.f : 0.f);
            outModeSign = outputMode.act == om_inverted ? -1.f : 1.f;
            outModeAbs = outputMode.act == om_absolute;

            static const char* modeSuffix[3] = { " (signed)", " (inverted)", " (absolute)" };
            static const char* baseNames[4] = { "A-Delta", "B-Delta", "C-Delta", "D-Delta" };
            for (int i = 0; i < 4; i++) {
                if (outputInfos.size() > (unsigned)(A_OUTPUT + i) && outputInfos[A_OUTPUT + i]) {
                    outputInfos[A_OUTPUT + i]->name = polyPortPrefix() + baseNames[i] + modeSuffix[outputMode.act];
                }
            }
        }
        
        // Trigger mode
        bool newTriggerMode = params[RESET_TRIGGATE_PARAM].getValue() > 0.5f;
        if (newTriggerMode != triggerMode) {
            resetTrigger.reset();
            triggerMode = newTriggerMode;
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
            // Track reset activity
            bool resetPressed = params[RESET_PUSH_PARAM].getValue() > 0.5f;
            float resetVoltage = (resetPressed) 
                ? 10.f 
                : (inputs[RESET_INPUT].isConnected()) 
                    ? inputs[RESET_INPUT].getVoltage() 
                    : 0.f;

            bool resetActive = (triggerMode) 
                ? resetTrigger.process(resetVoltage, 
                    trueDetectValues[trigDetHigh.act], trueDetectValues[trigDetLow.act])
                : resetPressed || resetVoltage >= trueDetectValues[gateDetHigh.act];

            // Must obtain reset values for all sections when in individual reference mode
            if (resetActive && referenceMode == rm_individual) { 
                firstIdx = 0;
                lastIdx = 3;
            }

            if (haveConnections || resetActive) {
                for (int i = firstIdx; i <= lastIdx; i++) {
                    bool resetThisSection = resetActive && (i==0 || referenceMode == rm_individual);
                    if (inConn[i] || outConn[i]) {
                        int inChannels = resetThisSection
                            ? 16 
                            : channels[i];
                        for (int c = 0; c < inChannels; c++) {
                            float inVoltage = (inConn[i]) 
                                ? inputs[A_INPUT + i].getPolyVoltage(c) 
                                : 0.f;

                            int refIdx = i * PORT_MAX_CHANNELS + c;
                            if (resetThisSection) 
                                refVal[refIdx] = inVoltage;

                            if (outConn[i] && c < channels[i]) {
                                if (referenceMode == rm_aOnly)
                                    refIdx = refIdx & 0x0F; // Only reference to A-section
                                float delta = (inVoltage - refVal[refIdx]) * outModeSign;
                                if (outModeAbs)
                                    delta = std::abs(delta);
                                delta = clipToVoltRange(delta, outClipRange.act);
                                outputs[A_OUTPUT + i].setVoltage(delta, c);
                            }
                        }
                    }

                    // Reset all 16 referenceValues for this section if input is disconnected and reset is active
                    if (resetActive && !inConn[i]) {
                        int sectionBaseIdx = i * PORT_MAX_CHANNELS;
                        for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
                            refVal[sectionBaseIdx + c] = 0.f;
                        }
                    }
                }
            }
        }

        cycle256++;
    }
};

struct Delta4ModuleWidget : InfNoiseModuleWidget {
    infNoiseSmallButton<bc_green, true>* resetPushBtn = nullptr;

    Delta4ModuleWidget(Delta4Module *module) {
        initializeWidget(module, "res/Delta4");

        // Basic layout columns (exact positions will be fine-tuned later)
        const float switchCol = 9.126f;   // Diff-mode switch column
        const float centerCol = 15.f;
        const float trigGateBtnCol  = 24.298f;  // Trigger/gate-btn column

        // Reset button (momentary / latched via RESET_LATCH_PARAM)
        resetPushBtn = createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(centerCol, 49.689f), module, Delta4Module::RESET_PUSH_PARAM);
        addParam(resetPushBtn);

        // Reset trigger/gate input and mode switch
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(centerCol, 78.600f), module, Delta4Module::RESET_INPUT));

        // Trigger/gate switch
        addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(trigGateBtnCol, 64.265f), module, Delta4Module::RESET_TRIGGATE_PARAM));

        // Ref-mode switch (Individual / A-only)
        addParam(createParamCentered<CKSS>(Vec(switchCol, 111.057f), module, Delta4Module::REF_MODE_PARAM));

        // Reference mode (toggle and lights)
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, true>>(Vec(centerCol, 228.777f), module, Delta4Module::TOGGLE_OUTPUT_MODE_PARAM));
        const float lightRow = 241.171f;
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(7.390f, lightRow), module, Delta4Module::OM_SIGN_LIGHT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(14.671f, lightRow), module, Delta4Module::OM_INV_LIGHT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(22.015f, lightRow), module, Delta4Module::OM_ABS_LIGHT));

        // Channel inputs (A–D) and outputs (A–D)
        const float rowSpacing = 24.665f;
        const float outputOffset = 122.689f; // Offset between input and output rows
        float row = 136.861f;
        for (int i = 0; i < 4; ++i) {
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, row), module, Delta4Module::A_INPUT + i));
            addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, row + outputOffset), module, Delta4Module::A_OUTPUT + i));
            row += rowSpacing;
        }
    }

    void step() override {
        Delta4Module* m = dynamic_cast<Delta4Module*>(module);
        if (m && resetPushBtn) {
            // RESET_LATCH_PARAM: 0 = Latched, 1 = Momentary
            resetPushBtn->momentary = m->params[Delta4Module::RESET_TRIGGATE_PARAM].getValue() > 0.5f;
        }

        InfNoiseModuleWidget::step();
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        Delta4Module* module = dynamic_cast<Delta4Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> voltNames = getVoltValuesNames();
        auto addUpdateRefMenu = [=](const std::string& label, int section) {
            // section < 0 updates all sections; otherwise only that section (0=A .. 3=D)
            menu->addChild(createSubmenuItem(label, "", [=](Menu* submenu) {
                for (int i = 0; i < voltValueCount; i++) {
                    submenu->addChild(createMenuItem(voltNames[i], "", [=]() {
                        float v = voltValues[i];
                        if (section < 0) {
                            for (int r = 0; r < Delta4Module::refCount; r++)
                                module->refVal[r] = v;
                        }
                        else {
                            int base = section * PORT_MAX_CHANNELS;
                            for (int c = 0; c < PORT_MAX_CHANNELS; c++)
                                module->refVal[base + c] = v;
                        }
                    }));
                }
            }));
        };
        static const char* sectionNames[4] = { "A", "B", "C", "D" };
        addUpdateRefMenu("Update A-D reference values", -1);
        for (int s = 0; s < 4; s++)
            addUpdateRefMenu(std::string("Update ") + sectionNames[s] + " reference values", s);

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelDelta4 = createModel<Delta4Module, Delta4ModuleWidget>("Delta4");