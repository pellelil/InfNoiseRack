// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct CombineModule : InfNoiseModule {
    enum ParamId {
        PARAM_PARAM,
        PARAM_TRIM_PARAM,
        MODE_PARAM,
        RANGE_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        PARAM_INPUT,
        A_INPUT,
        B_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        AB_OUTPUT,
        GATE_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT, 2),
        ENUMS(CLIP_RANGE_LIGHT, 2),
        GATE_MODE_LIGHT,
        LIGHTS_LEN
    };

    int channels = 1;
    bool haveOutputs = false;
    bool haveInputs = false;
    bool haveInput[2] = { false, false };
    bool haveOutput[2] = { false, false };
    enum modeType { m_UpperLower, m_AgtB, m_RiseFall };
    const std::string paramNames[3] = { "Breakpoint", "Bias", "Threshold" };
    modeType mode = m_UpperLower;
    modeType prevMode = m_UpperLower;
    bool modeChanged = true;
    enum rangeType { r_Bipolar, r_Unipolar };
    rangeType range = r_Bipolar;
    enum paramObtainType { po_knobOnly, po_monoCV, po_polyCV };
    paramObtainType paramObtain = po_knobOnly;
    float paramKnob = 0.5f;
    float paramTrim = 0.f;
    float rangeMin = -5.f;
    float rangeMax = 5.f;
    float ulThreshold = 0.f; // Threshold for Upper/Lower mode
    float aGtBias = 0.f; // Bias for A > B mode
    float rfDelta = 0.f; // Delta for Rise/Fall mode
    float rfPrevVoltage[PORT_MAX_CHANNELS] = { 0.f };
    bool rfUseFirst[PORT_MAX_CHANNELS] = { false };
    enum highOnModeType { hom_A, hom_B };
    actReqValue<highOnModeType> highOnMode = actReqValue<highOnModeType>(hom_A);
    
	CombineModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
        
        configParam(PARAM_PARAM, 0.f, 1.f, 0.5f, paramNames[0]+"-param", "", 0, 1);
        configParam(PARAM_TRIM_PARAM, -1.f, 1.f, 0.f, paramNames[0]+"-param CV-trim", "%", 0, 100);
        configInput(PARAM_INPUT, paramNames[0]+"-paramCV");

        configSwitch(MODE_PARAM, 0.f, 2.f, 0.f, "Mode", { "Upper/Lower", "A > B", "Rise/Fall" });
        configSwitch(RANGE_PARAM, 0.f, 1.f, 0.f, "Range", { "Bipolar (-5 to +5)", "Unipolar (0 to 10)" });

        configInput(A_INPUT, "A");
        configInput(B_INPUT, "B");

        configOutput(AB_OUTPUT, "A/B");
        configOutput(GATE_OUTPUT, "Gate");
        configLight(GATE_MODE_LIGHT, "Dim: High on A (default), Red: High on B");

        configBypass(A_INPUT, AB_OUTPUT);

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
		haveAutoProcQuality = false;
        haveOutQuantize = true;
        haveOutClipRange = true;  
		haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = false;
		haveTrigHighLow = false;

        ensureFiveSineExpLogLuts();
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        
        highOnMode.setBoth(hom_A);
        for (int i = 0; i < PORT_MAX_CHANNELS; i++) {
            rfPrevVoltage[i] = 0.f;
            rfUseFirst[i] = true;
        }

        modeChanged = true;  // Force update of param-names
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        highOnMode.setBoth((highOnModeType)getJsonInt(rootJ, "highOnMode", (int)hom_A));
        getJsonFloatArray(rootJ, "rfPrevVoltage", rfPrevVoltage, PORT_MAX_CHANNELS, 0.f);
        getJsonBoolArray(rootJ, "rfUseFirst", rfUseFirst, PORT_MAX_CHANNELS, true);

        modeChanged = true;  // Force update of param-names
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "highOnMode", json_integer((int)highOnMode.req));
        setJsonFloatArray(rootJ, "rfPrevVoltage", rfPrevVoltage, PORT_MAX_CHANNELS);
        setJsonBoolArray(rootJ, "rfUseFirst", rfUseFirst, PORT_MAX_CHANNELS);
    }


    void setParamValue(int channel) {
        float paramValue = paramKnob;
        if (inputs[PARAM_INPUT].isConnected()) {
            paramValue += (inputs[PARAM_INPUT].getPolyVoltage(channel) / 10.f) * paramTrim;
            paramValue = clamp(paramValue, 0.f, 1.f);
        }

        switch (mode) {
            case m_UpperLower:
                ulThreshold = rangeMin + (rangeMax - rangeMin) * paramValue;
                break;
            case m_AgtB:
                aGtBias = (paramValue - 0.5f) * 10.f;
                break;
            case m_RiseFall:
                if (paramValue <= 0.5f) {
                    paramValue = paramValue / 0.5f; // scale as 0.0 to 1.0
                    paramValue = fiveSineLogIsh(paramValue) * 0.0005f;
                    rfDelta = paramValue * processQualityCycles[procQuality.act]; 
                }
                else {
                    paramValue = (paramValue - 0.5f) / 0.5f; // scale as 0.0 to 1.0
                    paramValue = 0.0005f + fiveSineExpIsh(paramValue) * (0.01f - 0.0005f);
                    rfDelta = paramValue * processQualityCycles[procQuality.act]; 
                }
                break;
         }
    }

    bool useAInput(float aVoltage, float bVoltage, int channel) {
        switch (mode) {
            case m_UpperLower:
                return (aVoltage >= ulThreshold) ? true : false;
            case m_AgtB:
                return (aVoltage >= bVoltage + aGtBias) ? true : false;
            case m_RiseFall:
                float delta = aVoltage - rfPrevVoltage[channel];
                if (delta > rfDelta) {
                    rfUseFirst[channel] = true;     // rising → use first
                }
                else if (delta < -rfDelta) {
                    rfUseFirst[channel] = false;    // falling → use second
                }
                // else: hold previous state
            
                rfPrevVoltage[channel] = aVoltage;           
                return rfUseFirst[channel] ? true : false;
        }
        return true;
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        channels = 1;
        for (int i = 0; i < 2; i++) {
            haveInput[i] = inputs[A_INPUT + i].isConnected();
            if (haveInput[i]) {
                channels = std::max(channels, inputs[A_INPUT + i].getChannels());
            }
            haveOutput[i] = outputs[AB_OUTPUT + i].isConnected();
        }       
        haveInputs = haveInput[0] || haveInput[1];
        haveOutputs = haveOutput[0] || haveOutput[1];

        outputs[AB_OUTPUT].setChannels(channels);
        outputs[GATE_OUTPUT].setChannels(channels);

        mode = (modeType)params[MODE_PARAM].getValue();
        modeChanged = modeChanged || mode != prevMode;
        if (modeChanged) {
            prevMode = mode;
            modeChanged = false;

            const std::string& name = paramNames[(int)mode];
            paramQuantities[PARAM_PARAM]->name = name + "-param";
            paramQuantities[PARAM_TRIM_PARAM]->name = name + "-param CV-trim";
            inputInfos[PARAM_INPUT]->name = polyPortPrefix() + name + "-paramCV";
        }

        range = (rangeType)params[RANGE_PARAM].getValue();
        if (range == r_Bipolar) {
            rangeMin = -5.f;
            rangeMax = 5.f;
        }
        else {
            rangeMin = 0.f;
            rangeMax = 10.f;
        }

        paramObtain = po_knobOnly;
        if (inputs[PARAM_INPUT].isConnected()) {
            paramObtain = inputs[PARAM_INPUT].getChannels() > 1 
                ? po_polyCV 
                : po_monoCV;
        }

        paramKnob = params[PARAM_PARAM].getValue();
        paramTrim = params[PARAM_TRIM_PARAM].getValue();
        if (paramObtain == po_knobOnly)  // Set every 256th cycle
            setParamValue(0);

        if (highOnMode.needsUpdate()) {
            highOnMode.updateActual();
            lights[GATE_MODE_LIGHT].setBrightness(highOnMode.req == hom_A ? 0.f : 1.f);
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
            if (paramObtain == po_monoCV) // Read param-CV once for all channels
                setParamValue(0);

            float voltage = 0.f;
            float aVoltage = 0.f;
            float bVoltage = 0.f;
            for (int c = 0; c < channels; c++) {
                if (paramObtain == po_polyCV) // Read param-CV for each channel
                    setParamValue(c);

                aVoltage = haveInput[0] ? inputs[A_INPUT].getPolyVoltage(c) : 0.f;
                bVoltage = haveInput[1] ? inputs[B_INPUT].getPolyVoltage(c) : 0.f;

                // A/B output
                bool useA = useAInput(aVoltage, bVoltage, c);
                voltage = useA ? aVoltage : bVoltage;
                voltage = quantizeToMode(voltage, outQuantize.act);
                outputs[AB_OUTPUT].setVoltage(clipToVoltRange(voltage, outClipRange.act), c);

                // Gate output
                voltage = (useA && highOnMode.req == hom_A) || (!useA && highOnMode.req == hom_B)
                    ? voltValues[gateOutHigh.act]
                    : voltValues[gateOutLow.act];
                outputs[GATE_OUTPUT].setVoltage(voltage, c);
            }
        }

        cycle256++;
    }
};

struct CombineModuleWidget : InfNoiseModuleWidget {
    CombineModuleWidget(CombineModule *module) {
        initializeWidget(module, "res/Combine");

        // Parameter
        const float centerCol = 15.f;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, 50.029f), module, CombineModule::PARAM_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(centerCol, 77.768f), module, CombineModule::PARAM_TRIM_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 105.857f), module, CombineModule::PARAM_INPUT));
     
        // Mode and range
        const float switchCol = 8.858f; 
        addParam(createParamCentered<CKSSThree>(Vec(switchCol, 148.649f), module, CombineModule::MODE_PARAM));
        addParam(createParamCentered<CKSS>(Vec(switchCol, 190.043f), module, CombineModule::RANGE_PARAM));

        // A and B inputs
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 227.673f), module, CombineModule::A_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 262.931f), module, CombineModule::B_INPUT));

        // A/B and Gate outputs
        addChild(createLightCentered<TinyLight<RedLight>>(Vec(5.969f, 317.487f), module, CombineModule::GATE_MODE_LIGHT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 298.189f), module, CombineModule::AB_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 333.447f), module, CombineModule::GATE_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        CombineModule* module = dynamic_cast<CombineModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexPtrSubmenuItem("High gate", {"On A selected (default)", "On B selected"},
		 	&module->highOnMode.req
        ));
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelCombine = createModel<CombineModule, CombineModuleWidget>("Combine");