// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct PolyLCMPModule : InfNoiseModule {
    enum ParamId {
        //SOME_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        POLY_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        AND_OUTPUT,
        OR_OUTPUT,
        XOR_OUTPUT,
        NAND_OUTPUT,
        NOR_OUTPUT,
        XNOR_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        ENUMS(CHNL1_LIGHT,2),
        ENUMS(CHNL2_LIGHT,2),
        ENUMS(CHNL3_LIGHT,2),
        ENUMS(CHNL4_LIGHT,2),
        ENUMS(CHNL5_LIGHT,2),
        ENUMS(CHNL6_LIGHT,2),
        ENUMS(CHNL7_LIGHT,2),
        ENUMS(CHNL8_LIGHT,2),
        ENUMS(CHNL9_LIGHT,2),
        ENUMS(CHNL10_LIGHT,2),
        ENUMS(CHNL11_LIGHT,2),
        ENUMS(CHNL12_LIGHT,2),
        ENUMS(CHNL13_LIGHT,2),
        ENUMS(CHNL14_LIGHT,2),
        ENUMS(CHNL15_LIGHT,2),  
        ENUMS(CHNL16_LIGHT,2),
        OR_COUNT_LIGHT,
        XOR_COUNT_LIGHT,
        LIGHTS_LEN
    };

    bool haveOutputs = false;
    enum countValueType { cv_one, cv_two, cv_three, cv_four, cv_five, cv_six, cv_seven, cv_eight, 
        cv_nine, cv_ten, cv_eleven, cv_twelve, cv_thirteen, cv_fourteen, cv_fifteen, cv_sixteen };
    actReqValue<countValueType> orCountValue = actReqValue<countValueType>(cv_one);
    actReqValue<countValueType> xorCountValue = actReqValue<countValueType>(cv_one);
    
	PolyLCMPModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configInput(POLY_INPUT, "Polyphonic (up to 16 channels)");
        
        configOutput(AND_OUTPUT, "AND of all channels");
        configOutput(OR_OUTPUT, "OR of all channels");
        configOutput(XOR_OUTPUT, "XOR of all channels");
        configOutput(NAND_OUTPUT, "NAND of all channels");
        configOutput(NOR_OUTPUT, "NOR of all channels");
        configOutput(XNOR_OUTPUT, "XNOR of all channels");

        for (int c=0; c<16; c++) {
            int lgtIdx = c * 2;
			configLight(CHNL1_LIGHT + lgtIdx, string::f("Channel-%d (high=green, low=red)", c + 1));
        }

        configLight(OR_COUNT_LIGHT, "OR-count not 1 when lit");
        configLight(XOR_COUNT_LIGHT, "XOR-count not 1 when lit");

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
		haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = false;
		haveGateDetect = true;
		haveGateHighLow = true;
		haveTrigDetect = false;
		haveTrigHighLow = false;
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        orCountValue.setBoth(cv_one);
        xorCountValue.setBoth(cv_one);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        orCountValue.setBoth((countValueType)getJsonInt(rootJ, "orCount", (int)countValueType::cv_one));
        xorCountValue.setBoth((countValueType)getJsonInt(rootJ, "xorCount", (int)countValueType::cv_one));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "orCount", json_integer((int)orCountValue.req));
        json_object_set_new(rootJ, "xorCount", json_integer((int)xorCountValue.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        haveOutputs= outputs[AND_OUTPUT].isConnected() || outputs[OR_OUTPUT].isConnected() || outputs[XOR_OUTPUT].isConnected() ||
			outputs[NAND_OUTPUT].isConnected() || outputs[NOR_OUTPUT].isConnected() || outputs[XNOR_OUTPUT].isConnected();
        if (!haveOutputs) {
            outputs[AND_OUTPUT].setVoltage(0);
            outputs[OR_OUTPUT].setVoltage(0); 
            outputs[XOR_OUTPUT].setVoltage(0);
            outputs[NAND_OUTPUT].setVoltage(0); 
            outputs[NOR_OUTPUT].setVoltage(0); 
            outputs[XNOR_OUTPUT].setVoltage(0);
        }

        // Update channel-high lights 
        bool haveInput = inputs[POLY_INPUT].isConnected();
        int channels = haveInput ? inputs[POLY_INPUT].getChannels() : 0;
        for (int c = 0; c < 16; c++) {
            float green = 0.f;
            float red = 0.f;
            if (c < channels) {
                bool isHigh = inputs[POLY_INPUT].getVoltage(c) >= trueDetectValues[gateDetHigh.act];
                green = isHigh ? 1.f : 0.f;
                red = isHigh ? 0.f : 1.f;
            }
            int lgtIdx = c * 2;
            lights[CHNL1_LIGHT + lgtIdx].setBrightness(green);
            lights[CHNL1_LIGHT + lgtIdx + 1].setBrightness(red);
        }

        // Update OR-count light
        if (orCountValue.needsUpdate())
        {
            orCountValue.updateActual();
            lights[OR_COUNT_LIGHT].setBrightness(orCountValue.act != cv_one ? 1.f : 0.f);
        }

        // Update XOR-count light
        if (xorCountValue.needsUpdate())
        {
            xorCountValue.updateActual();
            lights[XOR_COUNT_LIGHT].setBrightness(xorCountValue.act != cv_one ? 1.f : 0.f);
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
            int channels = inputs[POLY_INPUT].isConnected()
                ? inputs[POLY_INPUT].getChannels()
                : 0;
            if (channels == 0)
            {
				outputs[AND_OUTPUT].setVoltage(voltValues[gateOutLow.act]);
				outputs[OR_OUTPUT].setVoltage(voltValues[gateOutLow.act]);
				outputs[XOR_OUTPUT].setVoltage(voltValues[gateOutLow.act]);
				outputs[NAND_OUTPUT].setVoltage(voltValues[gateOutHigh.act]);
				outputs[NOR_OUTPUT].setVoltage(voltValues[gateOutHigh.act]);
				outputs[XNOR_OUTPUT].setVoltage(voltValues[gateOutHigh.act]);
			}
			else
			{
                // Count high channels
                int highCount = 0;
				for (int i = 0; i < channels; i++) {
                    if (inputs[POLY_INPUT].getVoltage(i) >= trueDetectValues[gateDetHigh.act])
						highCount++;
				}

                // Set logical outputs
                bool andOut = highCount == channels;  // all channels (in use) high
                int orCount = (int)orCountValue.act + 1;
                bool orOut = highCount >= orCount;  // at least one channel high (or whatever the OR-count is set to)
                int xorCount = (int)xorCountValue.act + 1;
                bool xorOut = highCount == xorCount;  // exactly one channel high (or whatever the XOR-count is set to)
                outputs[AND_OUTPUT].setVoltage(andOut ? voltValues[gateOutHigh.act] : voltValues[gateOutLow.act]);
				outputs[OR_OUTPUT].setVoltage(orOut ? voltValues[gateOutHigh.act] : voltValues[gateOutLow.act]);
				outputs[XOR_OUTPUT].setVoltage(xorOut ? voltValues[gateOutHigh.act] : voltValues[gateOutLow.act]);
				outputs[NAND_OUTPUT].setVoltage(!andOut ? voltValues[gateOutHigh.act] : voltValues[gateOutLow.act]);
				outputs[NOR_OUTPUT].setVoltage(!orOut ? voltValues[gateOutHigh.act] : voltValues[gateOutLow.act]);
				outputs[XNOR_OUTPUT].setVoltage(!xorOut ? voltValues[gateOutHigh.act] : voltValues[gateOutLow.act]);
            }
        }

        cycle256++;
    }
};

struct PolyLCMPModuleWidget : InfNoiseModuleWidget {
    PolyLCMPModuleWidget(PolyLCMPModule *module) {
        initializeWidget(module, "res/PolyLCMP");

        const float cntrCol = 15.f;
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 51.428f), module, PolyLCMPModule::POLY_INPUT));

        const float lftLgtCol = 12.244f;
        const float rgtLgtCol = 17.556f;
        const float lgtSpacing = 7.003f;
        float ltgRow = 67.623f;
        for (int i = 0; i < 8; i++) {
            int lgtIdx = i * 2;
			addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(lftLgtCol, ltgRow), module, PolyLCMPModule::CHNL1_LIGHT + lgtIdx));
			addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(rgtLgtCol, ltgRow), module, PolyLCMPModule::CHNL9_LIGHT + lgtIdx));
			ltgRow += lgtSpacing;
		}

        const float rowSpacing = 35.0736f; //29.228f;
        float row = 157.326f;
        for (int i = 0; i < 6; i++) {
			addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(cntrCol, row), module, PolyLCMPModule::AND_OUTPUT + i));
			row += rowSpacing;
		}

        addChild(createLightCentered<TinyLight<RedLight>>(Vec(5.228f, 175.519f), module, PolyLCMPModule::OR_COUNT_LIGHT));
        addChild(createLightCentered<TinyLight<RedLight>>(Vec(5.228f, 211.228f), module, PolyLCMPModule::XOR_COUNT_LIGHT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        PolyLCMPModule* module = dynamic_cast<PolyLCMPModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> countNames = { "1 (default)", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" };
        menu->addChild(createIndexPtrSubmenuItem("OR min count",
            countNames, &module->orCountValue.req));

        menu->addChild(createIndexPtrSubmenuItem("XOR exact count",
            countNames, &module->xorCountValue.req));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelPolyLCMP = createModel<PolyLCMPModule, PolyLCMPModuleWidget>("PolyLCMP");