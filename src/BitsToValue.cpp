// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct BitsToValueModule : InfNoiseModule {
    enum ParamId {
        BIT_WGT1_PARAM,
        BIT_WGT2_PARAM,
        BIT_WGT3_PARAM,
        BIT_WGT4_PARAM,
        BIT_WGT5_PARAM,
        BIT_WGT6_PARAM,
        BIT_WGT7_PARAM,
        BIT_WGT8_PARAM,
        RANGE_PARAM,
        RANGE_TRIM_PARAM,
        MINCNTRMAX_PARAM,
        MINCNTRMAX_TRIM_PARAM,
        MINCNTRMAX_BTN_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        BITS_INPUT,
        BIT1_INPUT,
        BIT2_INPUT,
        BIT3_INPUT,
        BIT4_INPUT,
        BIT5_INPUT,
        BIT6_INPUT,
        BIT7_INPUT,
        BIT8_INPUT,
        RANGE_INPUT,
        MINCNTRMAX_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        VALUE_OUTPUT,
        INV_VALUE_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        MIN_LIGHT,
        CNTR_LIGHT,
        MAX_LIGHT,
        LIGHTS_LEN
    };

    int polyChannels = 0;
    int inpChannels = 0;
    int maxBit = 0;
    float knobSum = 0.0f;
    bool haveOutputs = false;
    dsp::SchmittTrigger minCntrMaxTrigger;
    enum minCntrMaxType { mcm_Min, mcm_Center, mcm_Max };
    actReqValue<minCntrMaxType> minCntrMax = actReqValue<minCntrMaxType>(minCntrMaxType::mcm_Center);
    
	BitsToValueModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        // Bit input/knobs
        configInput(BITS_INPUT, "Bits (channel 1-8)");
        float weight[8] = { 1.f / 128.f, 1.f / 64.f, 1.f / 32.f, 1.f / 16.f, 1.f / 8.f, 1.f / 4.f, 1.f / 2.f, 1.f };
        for (int i = 0; i < 8; i++) {
            configInput(BIT1_INPUT + i, string::f("Bit-%d", i + 1));
            configParam(BIT_WGT1_PARAM + i, 0.f, 1.f, weight[i], string::f("Bit-%d weight", i + 1), "", 0, 1);
        }

        // Range controls
        configParam(RANGE_PARAM, 0.f, 10.f, 10.0f, "Range", " V", 0, 1);
        configParam(RANGE_TRIM_PARAM, -1.f, 1.f, 0.f, "Range CV-trim", "%", 0, 100);
        configInput(RANGE_INPUT, "Range");

        // Min/Center/Max controls
        configParam(MINCNTRMAX_PARAM, -10.f, 10.f, 0.0f, "Min/Center/Max", " V", 0, 1);
        configParam(MINCNTRMAX_TRIM_PARAM, -1.f, 1.f, 0.f, "Min/Center/Max CV-trim", "%", 0, 100);
        configInput(MINCNTRMAX_INPUT, "Min/Center/Max (-10V to +10V)");
        configSwitch(MINCNTRMAX_BTN_PARAM, 0.0f, 2.0f, 1.0f, "Min/Center/Max-mode", { "Min", "Center", "Max" });
        configLight(MIN_LIGHT, "Minimum when lit");
        configLight(CNTR_LIGHT, "Center when lit");
        configLight(MAX_LIGHT, "Maximum when lit");
        
        // Outputs
        configOutput(VALUE_OUTPUT, "Value");
        configOutput(INV_VALUE_OUTPUT, "Inverted value");

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

        minCntrMax.setBoth(minCntrMaxType::mcm_Center);
        params[MINCNTRMAX_BTN_PARAM].setValue(1.f);
        minCntrMaxTrigger.reset();
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        minCntrMax.setBoth((minCntrMaxType)getJsonInt(rootJ, "minCntrMax", (int)minCntrMaxType::mcm_Center));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "minCntrMax", json_integer((int)minCntrMax.req));
    }

    void setAllWgtKnobs(std::vector<float> wgts) {
		params[BIT_WGT1_PARAM].setValue(wgts[0]);
		params[BIT_WGT2_PARAM].setValue(wgts[1]);
		params[BIT_WGT3_PARAM].setValue(wgts[2]);
		params[BIT_WGT4_PARAM].setValue(wgts[3]);
		params[BIT_WGT5_PARAM].setValue(wgts[4]);
		params[BIT_WGT6_PARAM].setValue(wgts[5]);
		params[BIT_WGT7_PARAM].setValue(wgts[6]);
		params[BIT_WGT8_PARAM].setValue(wgts[7]);
        mustProcessParams = true;  // force knobSum to be updated
	}

    void setRandomWgtKnobs()
    {
        for (int i = 0; i < 8; i++) {
            params[BIT_WGT1_PARAM + i].setValue(randomNorm());
        }
        mustProcessParams = true;  // force knobSum to be updated
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        polyChannels = inputs[BITS_INPUT].isConnected()
            ? std::max(inputs[BITS_INPUT].getChannels(), 8) // max 8 input-channels
            : 0;
        inpChannels = 0;
        for (int i = 0; i < 8; i++) {
            if (inputs[BIT1_INPUT + i].isConnected())
				inpChannels = std::max(inpChannels, i+1);
		}

        knobSum = 0.f;
        for (int i = 0; i < 8; i++) {
            knobSum += params[BIT_WGT1_PARAM + i].getValue();
        }

        haveOutputs = outputs[VALUE_OUTPUT].isConnected() || 
            outputs[INV_VALUE_OUTPUT].isConnected();

        // Update Min/Center/Max lights
        if (minCntrMax.needsUpdate()) {
            minCntrMax.updateActual();
            lights[MIN_LIGHT].setBrightness(minCntrMax.act == minCntrMaxType::mcm_Min ? 1.f : 0.f);
            lights[CNTR_LIGHT].setBrightness(minCntrMax.act == minCntrMaxType::mcm_Center ? 1.f : 0.f);
            lights[MAX_LIGHT].setBrightness(minCntrMax.act == minCntrMaxType::mcm_Max ? 1.f : 0.f);
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

            // Handle Min/Center/Max-button (switch between Min, Center and Max)
            // Button idles at 1 (center), so map to 0-based before edge detection.
            if (minCntrMaxTrigger.process(params[MINCNTRMAX_BTN_PARAM].getValue() - 1.f, 0.1f, 0.9f)) {
                minCntrMax.setBoth(minCntrMax.act == mcm_Max ? mcm_Min : static_cast<minCntrMaxType>(minCntrMax.act + 1));
            }

            // Get range and min/center/max values
            float rangeVolt = params[RANGE_PARAM].getValue();
            if (inputs[RANGE_INPUT].isConnected())
                rangeVolt += params[RANGE_TRIM_PARAM].getValue() * inputs[RANGE_INPUT].getVoltage();
            float minCntrMaxVolt = params[MINCNTRMAX_PARAM].getValue();
            if (inputs[MINCNTRMAX_INPUT].isConnected())
                minCntrMaxVolt += params[MINCNTRMAX_TRIM_PARAM].getValue() * inputs[MINCNTRMAX_INPUT].getVoltage();

            // Set minValue and maxValue
            float minValue;
            if (minCntrMax.act == mcm_Min) {
                minValue = minCntrMaxVolt;
            }
            else if (minCntrMax.act == mcm_Center) {
                float halfRange = rangeVolt / 2.f;
                minValue = minCntrMaxVolt - halfRange;
            }
            else { // mcm_Max
                minValue = minCntrMaxVolt - rangeVolt;
            }
            float maxValue = minValue + rangeVolt;

            if (knobSum > 0.f && (inpChannels > 0 || polyChannels > 0)) {
                float chnlSum = 0.f;
                for (int c = 0; c < 8; c++) {
                    float voltage = inputs[BIT1_INPUT + c].isConnected() 
                        ? inputs[BIT1_INPUT + c].getVoltage() 
                        : (c < polyChannels && inputs[BITS_INPUT].isConnected())
							? inputs[BITS_INPUT].getPolyVoltage(c) 
							: 0.f;
                    if (voltage >= trueDetectValues[gateDetHigh.act])
					    chnlSum += params[BIT_WGT1_PARAM + c].getValue();
				}
				chnlSum /= knobSum;

                float rangeSum = rangeVolt * chnlSum;
                outputs[VALUE_OUTPUT].setVoltage(clipToVoltRange(quantizeToMode(minValue + rangeSum, outQuantize.act), outClipRange.act));
                outputs[INV_VALUE_OUTPUT].setVoltage(clipToVoltRange(quantizeToMode(maxValue - rangeSum, outQuantize.act), outClipRange.act));
            } 
            else {
				outputs[VALUE_OUTPUT].setVoltage(clipToVoltRange(quantizeToMode(minValue, outQuantize.act), outClipRange.act));
				outputs[INV_VALUE_OUTPUT].setVoltage(clipToVoltRange(quantizeToMode(maxValue, outQuantize.act), outClipRange.act));
			}
        }

        cycle256++;
    }
};

struct BitsToValueModuleWidget : InfNoiseModuleWidget {
    BitsToValueModuleWidget(BitsToValueModule *module) {
        initializeWidget(module, "res/BitsToValue");

        // Bits input and weight-knobs
        const float ctrClm = 59.325f;
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(ctrClm, 56.248f), module, BitsToValueModule::BITS_INPUT));

        const float lftInpClm = 14.304f;
        const float lftKnobClm = 44.500f;
        const float rgtKnobClm = 74.501f;
        const float rgtInpClm = 104.305f;
        float row = 89.737f;
        const float rowSpacing = 35.4383f;
        for (int i = 0; i < 4; i++) {
            addInput(createInputCentered<ThemedPJ301MPort>(Vec(lftInpClm, row), module, BitsToValueModule::BIT1_INPUT + i));
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(lftKnobClm, row), module, BitsToValueModule::BIT_WGT1_PARAM + i));

            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(rgtKnobClm, row), module, BitsToValueModule::BIT_WGT5_PARAM + i));
            addInput(createInputCentered<ThemedPJ301MPort>(Vec(rgtInpClm, row), module, BitsToValueModule::BIT5_INPUT + i));
            row += rowSpacing;
        }

        // Range controls
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(29.325f, 236.561f), module, BitsToValueModule::RANGE_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(59.325f, 236.561f), module, BitsToValueModule::RANGE_TRIM_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(89.325f, 236.561f), module, BitsToValueModule::RANGE_INPUT));

        // Min/Center/Max controls
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(29.325f, 288.792f), module, BitsToValueModule::MINCNTRMAX_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(59.325f, 288.792f), module, BitsToValueModule::MINCNTRMAX_TRIM_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(89.325f, 288.792f), module, BitsToValueModule::MINCNTRMAX_INPUT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(38.435f, 264.538f), module, BitsToValueModule::MIN_LIGHT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(60.319f, 264.538f), module, BitsToValueModule::CNTR_LIGHT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(82.713f, 264.538f), module, BitsToValueModule::MAX_LIGHT));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, true>>(Vec(94.450f, 269.019f), module, BitsToValueModule::MINCNTRMAX_BTN_PARAM));

        // Outputs
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(lftKnobClm, 334.447f), module, BitsToValueModule::VALUE_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(rgtKnobClm, 334.447f), module, BitsToValueModule::INV_VALUE_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        BitsToValueModule* module = dynamic_cast<BitsToValueModule*>(this->module);
        assert(module);
        
        menu->addChild(new MenuSeparator);

        menu->addChild(createSubmenuItem("Set weight-knobs 1-8 to", "", [=](Menu* menu) {
            menu->addChild(createMenuItem("Binary (default)", "(1/128, 1/64, 1/32, 1/16, 1/8, 1/4, 1/2, 1/1)", [=]() {
                module->setAllWgtKnobs(std::vector<float>({ 1.f / 128.f, 1.f / 64.f, 1.f / 32.f, 1.f / 16.f, 1.f / 8.f, 1.f / 4.f, 1.f / 2.f, 1.f }));
                }));
            menu->addChild(createMenuItem("Binary reversed", "(1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/64, 1/128)", [=]() {
                module->setAllWgtKnobs(std::vector<float>({ 1.f, 1.f / 2.f, 1.f / 4.f, 1.f / 8.f, 1.f / 16.f, 1.f / 32.f, 1.f / 64.f, 1.f / 128.f }));
                }));
            menu->addChild(createMenuItem("Linear inc", "(1/8, 2/8, 3/8, 4/8, 5/8, 6/8, 7/8, 8/8)", [=]() {
                module->setAllWgtKnobs(std::vector<float>({ 1.f / 8.f, 2.f / 8.f, 3.f / 8.f, 4.f / 8.f, 5.f / 8.f, 6.f / 8.f, 7.f / 8.f, 1.f }));
                }));
            menu->addChild(createMenuItem("Fixed/same", "(0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5)", [=]() {
                module->setAllWgtKnobs(std::vector<float>({ 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f }));
                }));
            menu->addChild(new MenuSeparator);
            menu->addChild(createMenuItem("Random weights", "", [=]() {
                module->setRandomWgtKnobs();
                }));
            }));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelBitsToValue = createModel<BitsToValueModule, BitsToValueModuleWidget>("BitsToValue");
