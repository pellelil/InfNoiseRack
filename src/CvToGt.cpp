// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct CvToGtModule : InfNoiseModule {
    enum ParamId {
        MIN_PARAM,
        MIN_INCL_PARAM,
        MAX_PARAM,
        MAX_INCL_PARAM,
        GATE_DIFF_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        CV_INPUT,
        MIN_INPUT,
        MAX_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        RANGE_OUTPUT,
        NOTRANGE_OUTPUT,
        ABOVE_MAXDIFF_OUTPUT,
        BELOW_MINDIF_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT, 2),
        ENUMS(CLIP_RANGE_LIGHT, 2),
        ENUMS(MIN_MODE_LIGHT, 2),
        ERROR_LIGHT,
        ABOVE_LIGHT,
        MAX_LIGHT,
        BELOW_LIGHT,
        MIN_LIGHT,
        LIGHTS_LEN
    };

    bool haveCvInput = false;
    bool haveMinInput = false;
    bool haveMaxInput = false;
    bool haveOutputs = false;
    bool haveAboveBelowOutputs = false;
    int channels = 1;
    bool inclMin = false;
    bool inclMax = false;
    bool gateMode = true;
    enum minModeType { mm_minMax, mm_lowHigh };
    actReqValue<minModeType> minMode = actReqValue<minModeType>(mm_minMax);
    enum diffModeType { dm_signed, dm_abs };
    actReqValue<diffModeType> diffMode = actReqValue<diffModeType>(dm_signed);
    float redMinMaxLight[2] = { 1.f, 0.f };
    float greenMinMaxLight[2] = { 0.f, 1.f };
    bool errorMode = false;
        
	CvToGtModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configInput(CV_INPUT, "CV");

        configLight(MIN_MODE_LIGHT, "Min>Max (red=Error, Green=auto-swap)");
        configParam(MIN_PARAM, -10.0f, 10.0f, -5.0f, "Min", " V");
        configSwitch(MIN_INCL_PARAM, 0.0f, 1.0f, 1.0f, "Incl. min", { "Excluded", "Included" });
        configInput(MIN_INPUT, "Min-CV");

        configParam(MAX_PARAM, -10.0f, 10.0f, 5.0f, "Max", " V");
        configSwitch(MAX_INCL_PARAM, 0.0f, 1.0f, 1.0f, "Incl. max", { "Excluded", "Included" });
        configInput(MAX_INPUT, "Max-CV");

        configSwitch(GATE_DIFF_PARAM, 0.0f, 1.0f, 0.0f, "Gate/Diff-mode", { "Above/Below-gates", "Min/Max-diff (CV)" });
        configLight(ABOVE_LIGHT, "");
        configLight(MAX_LIGHT, "");
        configLight(BELOW_LIGHT, "");
        configLight(MIN_LIGHT, "");

        configLight(ERROR_LIGHT, "Min>Max error if lit");
        configOutput(RANGE_OUTPUT, "Range-gate");
        configOutput(NOTRANGE_OUTPUT, "Not in-range-gate");
        configOutput(ABOVE_MAXDIFF_OUTPUT, "Above-gate/Max-diff");
        configOutput(BELOW_MINDIF_OUTPUT, "Below-gate/Min-diff");

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
		haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = true;  
		haveGateDetect = false;
		haveGateHighLow = true;
		haveTrigDetect = false;
		haveTrigHighLow = false;
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);

        minMode.setBoth(mm_minMax);
        diffMode.setBoth(dm_signed);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        minMode.setBoth((minModeType)getJsonInt(rootJ, "minMode", (int)mm_minMax));
        diffMode.setBoth((diffModeType)getJsonInt(rootJ, "diffMode", (int)dm_signed));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "minMode", json_integer((int)minMode.req));
        json_object_set_new(rootJ, "diffMode", json_integer((int)diffMode.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        haveMinInput = inputs[MIN_INPUT].isConnected();
        haveMaxInput = inputs[MAX_INPUT].isConnected();
        haveCvInput = inputs[CV_INPUT].isConnected();
        channels = haveCvInput 
            ? inputs[CV_INPUT].getChannels() 
            : 1;
        haveAboveBelowOutputs = outputs[ABOVE_MAXDIFF_OUTPUT].isConnected() || outputs[BELOW_MINDIF_OUTPUT].isConnected();
        haveOutputs = outputs[RANGE_OUTPUT].isConnected() || outputs[NOTRANGE_OUTPUT].isConnected() || 
            haveAboveBelowOutputs;
        outputs[RANGE_OUTPUT].setChannels(channels);
        outputs[NOTRANGE_OUTPUT].setChannels(channels);
        outputs[ABOVE_MAXDIFF_OUTPUT].setChannels(channels);
        outputs[BELOW_MINDIF_OUTPUT].setChannels(channels);

        inclMin = params[MIN_INCL_PARAM].getValue() > 0.5f;
        inclMax = params[MAX_INCL_PARAM].getValue() > 0.5f;

        diffMode.updateActual();
        minMode.updateActual();
        errorMode = false;
        float minVal = (haveMinInput) 
            ? params[MIN_PARAM].getValue() + inputs[MIN_INPUT].getPolyVoltage(0) 
            : params[MIN_PARAM].getValue();
        float maxVal = (haveMaxInput) 
            ? params[MAX_PARAM].getValue() + inputs[MAX_INPUT].getPolyVoltage(0) 
            : params[MAX_PARAM].getValue();
        if (minVal > maxVal) {
            lights[MIN_MODE_LIGHT].setBrightness(greenMinMaxLight[minMode.act]);
            lights[MIN_MODE_LIGHT + 1].setBrightness(redMinMaxLight[minMode.act]);
            if (minMode.act == mm_minMax)
                errorMode = true;
        }
        else {
            lights[MIN_MODE_LIGHT].setBrightness(0.0f);
            lights[MIN_MODE_LIGHT + 1].setBrightness(0.0f);
        }
        lights[ERROR_LIGHT].setBrightness(errorMode ? 1.0f : 0.0f);

        gateMode = params[GATE_DIFF_PARAM].getValue() < 0.5f;
        float gateLight = gateMode ? 1.0f : 0.0f;
        float diffLight = 1.f - gateLight;
        lights[ABOVE_LIGHT].setBrightness(gateLight);
        lights[MAX_LIGHT].setBrightness(diffLight);
        lights[BELOW_LIGHT].setBrightness(gateLight);
        lights[MIN_LIGHT].setBrightness(diffLight);

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
            for (int c = 0; c < channels; c++) {
                // Obtain input, Min, Max values
                float input = haveCvInput 
                    ? inputs[CV_INPUT].getVoltage(c) 
                    : 0.f;
                float minVal = (haveMinInput) 
                    ? params[MIN_PARAM].getValue() + inputs[MIN_INPUT].getPolyVoltage(c) 
                    : params[MIN_PARAM].getValue();
                float maxVal = (haveMaxInput) 
                    ? params[MAX_PARAM].getValue() + inputs[MAX_INPUT].getPolyVoltage(c) 
                    : params[MAX_PARAM].getValue();

                // Swap min/max if min>max and min-mode is low/high
                float lowVal = minVal;
                float highVal = maxVal;
                bool lowIncl = inclMin;
                bool highIncl = inclMax;
                if (minVal > maxVal && minMode.act == mm_lowHigh) {
                    lowVal = maxVal;
                    highVal = minVal;
                    lowIncl = inclMax;
                    highIncl = inclMin;
                }

                bool inRange = (input > lowVal || (lowIncl && input == lowVal)) &&
                    (input < highVal || (highIncl && input == highVal));
    
                // Output In-range-gate
                float voltage = (inRange && !errorMode)
                    ? voltValues[gateOutHigh.act]
                    : voltValues[gateOutLow.act];
                outputs[RANGE_OUTPUT].setVoltage(voltage, c);

                // Output NOT In-range-gate
                voltage = (!inRange || errorMode)
                    ? voltValues[gateOutHigh.act]
                    : voltValues[gateOutLow.act];
                outputs[NOTRANGE_OUTPUT].setVoltage(voltage, c);
            
                if (haveAboveBelowOutputs) {
                    if (gateMode) {
                        bool isAbove = input > highVal || (!highIncl && input == highVal);
                        float vAboveBelow = (isAbove)
                            ? voltValues[gateOutHigh.act]
                            : voltValues[gateOutLow.act];
                        outputs[ABOVE_MAXDIFF_OUTPUT].setVoltage(vAboveBelow, c);

                        bool isBelow = input < lowVal || (!lowIncl && input == lowVal);
                        float vBelowGate = (isBelow)
                            ? voltValues[gateOutHigh.act]
                            : voltValues[gateOutLow.act];
                        outputs[BELOW_MINDIF_OUTPUT].setVoltage(vBelowGate, c);
                    }
                    else {
                        float maxDiff = input - highVal;
                        float minDiff = input - lowVal;
                        if (diffMode.act == dm_abs) {
                            maxDiff = std::fabs(maxDiff);
                            minDiff = std::fabs(minDiff);
                        }
                        outputs[ABOVE_MAXDIFF_OUTPUT].setVoltage(clipToVoltRange(maxDiff, outClipRange.act), c);
                        outputs[BELOW_MINDIF_OUTPUT].setVoltage(clipToVoltRange(minDiff, outClipRange.act), c);    
                    }
                }
            }
        }

        cycle256++;
    }
};

struct CvToGtModuleWidget : InfNoiseModuleWidget {
    CvToGtModuleWidget(CvToGtModule *module) {
        initializeWidget(module, "res/CvToGt");
        
        const float cntrClm = 15.0f;
        const float inclClm = 25.546;
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 52.283f), module, CvToGtModule::CV_INPUT));

        addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(4.981f, 69.377f), module, CvToGtModule::MIN_MODE_LIGHT));
        infNoiseLtSmallButton* inclBtn = createParamCentered<infNoiseLtSmallButton>(Vec(inclClm, 72.679f), module, CvToGtModule::MIN_INCL_PARAM);
        inclBtn->setup(bc_green, false);
        addParam(inclBtn);
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrClm, 84.723f), module, CvToGtModule::MIN_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 111.913f), module, CvToGtModule::MIN_INPUT));

        inclBtn = createParamCentered<infNoiseLtSmallButton>(Vec(inclClm, 131.851f), module, CvToGtModule::MAX_INCL_PARAM);
        inclBtn->setup(bc_green, false);
        addParam(inclBtn);
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrClm, 143.695f), module, CvToGtModule::MAX_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 170.886f), module, CvToGtModule::MAX_INPUT));

        addParam(createParamCentered<CKSS>(Vec(8.482f, 194.950f), module, CvToGtModule::GATE_DIFF_PARAM));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(7.345f, 277.191f), module, CvToGtModule::ABOVE_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(22.682f, 277.191f), module, CvToGtModule::MAX_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(7.345f, 311.186f), module, CvToGtModule::BELOW_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(22.682f, 311.186f), module, CvToGtModule::MIN_LIGHT));

        addChild(createLightCentered<SmallLight<RedLight>>(Vec(5.698f, 247.543f), module, CvToGtModule::ERROR_LIGHT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 227.517f), module, CvToGtModule::RANGE_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 262.591f), module, CvToGtModule::NOTRANGE_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 297.664f), module, CvToGtModule::ABOVE_MAXDIFF_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 332.738f), module, CvToGtModule::BELOW_MINDIF_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        CvToGtModule* module = dynamic_cast<CvToGtModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexPtrSubmenuItem("Min-mode", {"Min/Max", "Auto-swap"},
		 	&module->minMode.req
        ));

        menu->addChild(createIndexPtrSubmenuItem("Diff-mode", {"Signed", "Absolute"},
		 	&module->diffMode.req
        ));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelCvToGt = createModel<CvToGtModule, CvToGtModuleWidget>("CvToGt");