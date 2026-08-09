// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct LCMP6x2Module : InfNoiseModule {
    enum ParamId {
        CMP1_MODE_PARAM,
        CMP2_MODE_PARAM,
        CMP3_MODE_PARAM,
        CMP4_MODE_PARAM,
        CMP5_MODE_PARAM,
        CMP6_MODE_PARAM,
        CMP1_INV_PARAM,
        CMP2_INV_PARAM,
        CMP3_INV_PARAM,
        CMP4_INV_PARAM,
        CMP5_INV_PARAM,
        CMP6_INV_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        CMP11_INPUT,
        CMP12_INPUT,
        CMP21_INPUT,
        CMP22_INPUT,
        CMP31_INPUT,
        CMP32_INPUT,
        CMP41_INPUT,
        CMP42_INPUT,
        CMP51_INPUT,
        CMP52_INPUT,
        CMP61_INPUT,
        CMP62_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        CMP1_OUTPUT,
        CMP2_OUTPUT,
        CMP3_OUTPUT,
        CMP4_OUTPUT,
        CMP5_OUTPUT,
        CMP6_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        INV11_LIGHT,
        INV12_LIGHT,
        INV21_LIGHT,
        INV22_LIGHT,
        INV31_LIGHT,
        INV32_LIGHT,
        INV41_LIGHT,
        INV42_LIGHT,
        INV51_LIGHT,
        INV52_LIGHT,
        INV61_LIGHT,
        INV62_LIGHT,
        LIGHTS_LEN
    };

    int channels[6] = { 1, 1, 1, 1, 1, 1 };
    bool haveOutputs = false;
    int lastIdx = -1;
    actReqValue<bool> inpInv[12] = { // Inversion of inputs (6 sections, each with 2 inputs)
        actReqValue<bool>(false),
        actReqValue<bool>(false),
        actReqValue<bool>(false),
        actReqValue<bool>(false),
        actReqValue<bool>(false),
        actReqValue<bool>(false),
        actReqValue<bool>(false),
        actReqValue<bool>(false),
        actReqValue<bool>(false),
        actReqValue<bool>(false),
        actReqValue<bool>(false),
        actReqValue<bool>(false)
    };

	LCMP6x2Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        for (int i = 0; i < 6; i++) {
            std::string normalized = (i == 0) ? " (normalized to 0V)" : " (normalized to previous output)";
            configInput(CMP11_INPUT + i * 2, string::f("Compare %d-1", i + 1) + normalized);
            configInput(CMP12_INPUT + i * 2, string::f("Compare %d-2", i + 1) + " (normalized to 0V)");
            
            configSwitch(CMP1_MODE_PARAM + i, 0.f, 2.f, 0.f, string::f("Operation-mode %d", i + 1), { "AND (NAND)", "OR (NOR)", "XOR (XNOR)" });
            configSwitch(CMP1_INV_PARAM + i, 0.f, 1.f, 0.f, string::f("Operation-logic %d", i + 1), { "Normal (AND/OR/XOR)", "Inverted (NAND/NOR/XNOR)" });

            configLight(INV11_LIGHT + i * 2, string::f("Compare %d-1 input inverted if lit", i + 1));
            configLight(INV12_LIGHT + i * 2, string::f("Compare %d-2 input inverted if lit", i + 1));

            configOutput(CMP1_OUTPUT + i, string::f("Compare %d", i + 1));
		}

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
        
        for (int i = 0; i < 12; i++) {
            inpInv[i].setBoth(false);
        }
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        int inpInvTmp[12];
        getJsonIntArray(rootJ, "inpInv", inpInvTmp, 12, 1);
        for (int i = 0; i < 12; i++)
            inpInv[i].setBoth(inpInvTmp[i] == 1);
    }

    void dataToJson(json_t* rootJ) override {
        int inpInvTmp[12];
        for (int i = 0; i < 12; i++)
            inpInvTmp[i] = inpInv[i].req ? 1 : 0;
        setJsonIntArray(rootJ, "inpInv", inpInvTmp, 12);
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        haveOutputs = false;
        lastIdx = -1;
        int prevChannels = 1;
        for (int i = 0; i < 6; i++) {
            // Get the highest number of channels from the two inputs (or normalized 1st input)
            channels[i] = (inputs[CMP11_INPUT + i * 2].isConnected())
                ? inputs[CMP11_INPUT + i * 2].getChannels() 
                : prevChannels;
            if (inputs[CMP12_INPUT + i * 2].isConnected())
                channels[i] = std::max(channels[i], inputs[CMP12_INPUT + i * 2].getChannels());
            prevChannels = channels[i];

            // Detect if we have any outputs connected and set channels
            if (outputs[CMP1_OUTPUT + i].isConnected()) {
                haveOutputs = true;
                lastIdx = i;
                outputs[CMP1_OUTPUT + i].setChannels(channels[i]);
            }
            else {
                outputs[CMP1_OUTPUT + i].setChannels(1);
                outputs[CMP1_OUTPUT + i].setVoltage(0.f);
            }

            // Update inversion lights if inversion have changed
            for (int j = 0; j < 2; j++) {
                int lgtIdx = i * 2 + j;
                if (inpInv[lgtIdx].needsUpdate()) {
                    inpInv[lgtIdx].updateActual();
                    lights[lgtIdx].setBrightness(inpInv[lgtIdx].req ? 1.f : 0.f);
				}
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
            bool normInp[PORT_MAX_CHANNELS] = { false };
            int prevChannels = 0; // Number of channels in previous section
            for (int i = 0; i <= lastIdx; i++) {
                bool sectHaveOutput = outputs[CMP1_OUTPUT + i].isConnected();
                for (int c = 0; c < channels[i]; c++)
                {
                    // Get input-1 value and invert if applicable
                    bool inp1High = inputs[CMP11_INPUT + i * 2].isConnected()
                        ? inputs[CMP11_INPUT + i * 2].getPolyVoltage(c) >= trueDetectValues[gateDetHigh.act]
                        : prevChannels == 1                        
                            ? normInp[0]
							: c < prevChannels
                                ? normInp[c]
								: false;
                    if (inpInv[i * 2].act)
                        inp1High = !inp1High;

                    // Get input-2 value and invert if applicable
                    bool inp2High = inputs[CMP12_INPUT + i * 2].isConnected()
                        ? inputs[CMP12_INPUT + i * 2].getPolyVoltage(c) >= trueDetectValues[gateDetHigh.act]
                        : false;
                    if (inpInv[i * 2 + 1].act)
                        inp2High = !inp2High;

                    // Evaluate logic-operation
                    switch ((int)params[CMP1_MODE_PARAM + i].getValue()) {
                        case 0: normInp[c] = (inp1High && inp2High); break; // AND
                        case 1: normInp[c] = (inp1High || inp2High); break; // OR
                        case 2: normInp[c] = ((inp1High || inp2High) && !(inp1High && inp2High)); break;  // XOR
                    }
                    // Invert if applicable (AND/OR/XOR -> NAND/NOR/XNOR)
                    if (params[CMP1_INV_PARAM + i].getValue() > 0.5f)  
                        normInp[c] = !normInp[c];

                    // Set output voltage
                    if (sectHaveOutput)
                    {
                        float voltage = normInp[c]
                            ? voltValues[gateOutHigh.act]
                            : voltValues[gateOutLow.act];
                        outputs[CMP1_OUTPUT + i].setVoltage(voltage, c);
                    }
                }

                // Update prevChannels
                prevChannels = channels[i];
            }
        }

        cycle256++;
    }
};

struct LCMP6x2ModuleWidget : InfNoiseModuleWidget {
    LCMP6x2ModuleWidget(LCMP6x2Module *module) {
        initializeWidget(module, "res/LCMP6x2");

        const float inpClm = 14.500f;
        const float lgtClm = 24.921f; 
        const float swtClm = 37.913f;
        const float invClm = 50.963f;
        const float outClm = 71.970f;
        const float sctSpacing = 51.097f;
        float sctOfs = 0.f;
        for (int i = 0; i < 6; i++) {
            // Inputs and inv-lights
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(inpClm, 50.679f + sctOfs), module, LCMP6x2Module::CMP11_INPUT + i * 2));
            addChild(createLightCentered<TinyLight<RedLight>>(Vec(lgtClm, 61.101f + sctOfs), module, LCMP6x2Module::INV11_LIGHT + i * 2));
			addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(inpClm, 75.312f + sctOfs), module, LCMP6x2Module::CMP12_INPUT + i * 2));
            addChild(createLightCentered<TinyLight<RedLight>>(Vec(lgtClm, 65.690f + sctOfs), module, LCMP6x2Module::INV12_LIGHT + i * 2));

            // Logic-operation switch and inversion button
            addParam(createParamCentered<CKSSThree>(Vec(swtClm, 62.995f + sctOfs), module, LCMP6x2Module::CMP1_MODE_PARAM + i));
            addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(invClm, 47.671f + sctOfs), module, LCMP6x2Module::CMP1_INV_PARAM + i));

            // Output
            addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(outClm, 62.995f + sctOfs), module, LCMP6x2Module::CMP1_OUTPUT + i));
            sctOfs += sctSpacing;
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        LCMP6x2Module* module = dynamic_cast<LCMP6x2Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        for (int i = 0; i < 6; i++) {
            menu->addChild(createSubmenuItem(string::f("Section %d", i + 1), "",
                [=](Menu* menu) {
                    menu->addChild(createIndexPtrSubmenuItem(string::f("Input %d-1", i + 1), { "Normal", "Inverted" }, &module->inpInv[i * 2].req));
                    menu->addChild(createIndexPtrSubmenuItem(string::f("Input %d-2", i + 1), { "Normal", "Inverted" }, &module->inpInv[i * 2 + 1].req));
                }
            ));
        }
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelLCMP6x2 = createModel<LCMP6x2Module, LCMP6x2ModuleWidget>("LCMP6x2");