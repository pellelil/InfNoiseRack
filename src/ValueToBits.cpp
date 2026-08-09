// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct ValueToBitsModule : InfNoiseModule {
    enum ParamId {
        RANGE_PARAM,
        RANGE_TRIM_PARAM,
        MINCNTRMAX_PARAM,
        MINCNTRMAX_TRIM_PARAM,
        MINCNTRMAX_BTN_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        VALUE_INPUT,
        RANGE_INPUT,
        MINCNTRMAX_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        BIT1_OUTPUT,
        BIT2_OUTPUT,
        BIT3_OUTPUT,
        BIT4_OUTPUT,
        BIT5_OUTPUT,
        BIT6_OUTPUT,
        BIT7_OUTPUT,
        BIT8_OUTPUT,
        BITS_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        MIN_LIGHT,
        CNTR_LIGHT,
        MAX_LIGHT,
        BITS_LIGHT,
        LIGHTS_LEN
    };

    actReqValue<int> bitsInUse = actReqValue<int>(8); // Number of bits in use (3-16, but defaults to 8)
    int maxBitValue = 255; // Higest value for bitsInUse bits (e.g. 8 bits = 255)
    int firstIdx = -1;
    int lastIdx = -1;
    bool haveOutputs = false;
    dsp::SchmittTrigger minCntrMaxTrigger;
    enum minCntrMaxType { mcm_Min, mcm_Center, mcm_Max };
    actReqValue<minCntrMaxType> minCntrMax = actReqValue<minCntrMaxType>(minCntrMaxType::mcm_Center);
    bool bitsConnected = false;
    bool bitConnected[8] = { false, false, false, false, false, false, false, false };
    
	ValueToBitsModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        // Value input
        configInput(VALUE_INPUT, "Value (to be clipped/converted to bits)");

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

        // Bit outputs
        for (int i = 0; i < 8; i++) {
			configOutput(BIT1_OUTPUT + i, string::f("Bit-%d", i + 1));
		}
        configOutput(BITS_OUTPUT, "Bits (2-16 channels, but defaults to 8)");
        configLight(BITS_LIGHT, "Lit if bits-in-use is not 8");

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
		haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = false;
		haveGateDetect = false;
		haveGateHighLow = true;
		haveTrigDetect = false;
		haveTrigHighLow = false;
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        
        bitsInUse.setBoth(8);
        minCntrMax.setBoth(minCntrMaxType::mcm_Center);
        params[MINCNTRMAX_BTN_PARAM].setValue(1.f);
        minCntrMaxTrigger.reset();
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        bitsInUse.setBoth(getJsonInt(rootJ, "bitsInUse", 8));
        minCntrMax.setBoth((minCntrMaxType)getJsonInt(rootJ, "minCntrMax", (int)minCntrMaxType::mcm_Center));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "bitsInUse", json_integer(bitsInUse.req));
        json_object_set_new(rootJ, "minCntrMax", json_integer((int)minCntrMax.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        // Update bitsInUse/maxBitValue and bits-lights
        if (bitsInUse.needsUpdate()) {
            bitsInUse.updateActual();
            lights[BITS_LIGHT].setBrightness(bitsInUse.act == 8 ? 0.f : 1.f);

            if (outputInfos.size() > (unsigned)BITS_OUTPUT && outputInfos[BITS_OUTPUT]) {
                if (bitsInUse.act == 8)
                    outputInfos[BITS_OUTPUT]->name = polyPortPrefix() + "Bits (8 channels, default)";
                else
                    outputInfos[BITS_OUTPUT]->name = polyPortPrefix() + string::f("Bits (%d channels)", bitsInUse.act);
            }

            const int maxBitValues[17] = { -1, -1, 3, 7, 15, 31, 63, 127, 255, 511, 1023, 2047, 4095, 8191, 16383, 32767, 65535 };
            maxBitValue = maxBitValues[bitsInUse.act];
        }
        outputs[BITS_OUTPUT].setChannels(bitsInUse.act);

        // Update Min/Center/Max lights
        if (minCntrMax.needsUpdate()) {
            minCntrMax.updateActual();
            lights[MIN_LIGHT].setBrightness(minCntrMax.act == minCntrMaxType::mcm_Min ? 1.f : 0.f);
            lights[CNTR_LIGHT].setBrightness(minCntrMax.act == minCntrMaxType::mcm_Center ? 1.f : 0.f);
            lights[MAX_LIGHT].setBrightness(minCntrMax.act == minCntrMaxType::mcm_Max ? 1.f : 0.f);
        }

        // Check outputs in use
        haveOutputs = false;
        firstIdx = -1;
        lastIdx = -1;
        bitsConnected = false;
        if (outputs[BITS_OUTPUT].isConnected())
        {
			haveOutputs = true;
            bitsConnected = true;
			firstIdx = 0;
			lastIdx = bitsInUse.act-1;
		}

        for (int i = 0; i < 8; i++) {
            bitConnected[i] = outputs[BIT1_OUTPUT + i].isConnected();
            if (bitConnected[i])
            {
                haveOutputs = true;
                if (firstIdx < 0)
                    firstIdx = i;
                if (i > lastIdx)
                    lastIdx = i;
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
            // Handle Min/Center/Max-button (switch between Min, Center and Max)
            // Button idles at 1 (center), so map to 0-based before edge detection.
            if (minCntrMaxTrigger.process(params[MINCNTRMAX_BTN_PARAM].getValue() - 1.f, 0.1f, 0.9f)) {
                minCntrMax.setBoth(minCntrMax.req == mcm_Max ? mcm_Min : static_cast<minCntrMaxType>(minCntrMax.req + 1));
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

            // Get input value, clip it to range and offset by minValue
            float value = inputs[VALUE_INPUT].isConnected()
                ? inputs[VALUE_INPUT].getVoltage()
                : 0.f;
            value = clamp(value, minValue, maxValue) - minValue;

            // Convert value to bits
            int bit8Value = (value > 0.f && rangeVolt > 0.f) // Individual/monophic bit-outputs
                ? (int)round(value / rangeVolt * 255) // Always 8 bits
                : 0;
            int bitsValue = (value > 0.f && rangeVolt > 0.f) // Polyphonic bits-output
                ? (int)round(value / rangeVolt * maxBitValue) // Variable number of bits (3-16)
                : 0;

            // Set outputs
            float voltage = 0.f;
            bool bitSet = false;
            for (int i = firstIdx; i <= lastIdx; i++) {
                if (i < 8 && bitConnected[i])
                {
                    bitSet = (bit8Value & (1 << i));
                    voltage = bitSet
                        ? voltValues[gateOutHigh.act]
                        : voltValues[gateOutLow.act];
                    outputs[BIT1_OUTPUT + i].setVoltage(voltage);
                }
                if (bitsConnected) {
                    bitSet = (bitsValue & (1 << i));
                    voltage = bitSet
                        ? voltValues[gateOutHigh.act]
                        : voltValues[gateOutLow.act];
                    outputs[BITS_OUTPUT].setVoltage(voltage, i);
                }
			}
        }

        cycle256++;
    }
};

struct ValueToBitsModuleWidget : InfNoiseModuleWidget {
    ValueToBitsModuleWidget(ValueToBitsModule *module) {
        initializeWidget(module, "res/ValueToBits");

        // Value input
        const float ctrCol = 29.500f;
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(ctrCol, 52.883f), module, ValueToBitsModule::VALUE_INPUT));

        // Range controls
        const float lftClm = 15.500f;
        const float rgtClm = 45.500f;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(rgtClm, 90.099f), module, ValueToBitsModule::RANGE_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(lftClm, 77.553f), module, ValueToBitsModule::RANGE_TRIM_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(lftClm, 104.482), module, ValueToBitsModule::RANGE_INPUT));

        // Min/Center/Max controls
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(rgtClm, 145.137f), module, ValueToBitsModule::MINCNTRMAX_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(lftClm, 132.091f), module, ValueToBitsModule::MINCNTRMAX_TRIM_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(lftClm, 159.021f), module, ValueToBitsModule::MINCNTRMAX_INPUT));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(30.880f, 120.393f), module, ValueToBitsModule::MIN_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(42.225f, 120.393f), module, ValueToBitsModule::CNTR_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(54.538f, 120.393f), module, ValueToBitsModule::MAX_LIGHT));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, true>>(Vec(27.891f, 145.973f), module, ValueToBitsModule::MINCNTRMAX_BTN_PARAM));

        // Individual bit-outputs and lights
        const float rowSpacing = 35.3386f;
        float row = 193.274f;
        for (int i = 0; i < 4; i++) {
            addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(lftClm, row), module, ValueToBitsModule::BIT1_OUTPUT + i));
            addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(rgtClm, row), module, ValueToBitsModule::BIT5_OUTPUT + i));

			row += rowSpacing;
        }

        // Polyphonic bits-output and light
        const float lgtOfs = -10.021f;
        row = 334.447f;
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(ctrCol, row), module, ValueToBitsModule::BITS_OUTPUT));
        addChild(createLightCentered<SmallLight<RedLight>>(Vec(ctrCol + lgtOfs, row + lgtOfs), module, ValueToBitsModule::BITS_LIGHT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        ValueToBitsModule* module = dynamic_cast<ValueToBitsModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> bitNames = { "2", "3", "4", "5", "6", "7", "8 (default)", "9", "10", "11", "12", "13", "14", "15", "16"};
        const int bitOffset = 2; // 0 and 1 bit are not possible, so offset index by 2
        menu->addChild(createIndexSubmenuItem("Polyphonic bits-in-use", bitNames,
            [=]() {
                return (int)(module->bitsInUse.req - bitOffset);
            },
            [=](int bits) {
                module->bitsInUse.req = (bits + bitOffset);
            }
        ));
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelValueToBits = createModel<ValueToBitsModule, ValueToBitsModuleWidget>("ValueToBits");