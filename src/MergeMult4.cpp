// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct MergeMult4Module : InfNoiseModule {
    enum ParamId {
        MERGEMODE_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        MERGE1_INPUT,
        MERGE2_INPUT,
        MERGE3_INPUT,
        MERGE4_INPUT,
        MULT_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        MERGE_OUTPUT,
        MULT1_OUTPUT,
        MULT2_OUTPUT,
        MULT3_OUTPUT,
        MULT4_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT, 2),
        ENUMS(MODE_LIGHT, 2),
        LIGHTS_LEN
    };

    bool haveOutputs = false;
    int mergeInputCount = 0;
    int mergeChannels = 0;
    int multOutputCount = 0;
    int multChannels = 0;
    bool mixMode = false;
    enum negModeType { nm_neg, nm_zero, nm_absIn, nm_absOut };
    actReqValue<negModeType> negMode = actReqValue<negModeType>(nm_neg);
    float negModeGreen[4] = { 0.f, 1.f, 1.f, 0.f };
    float negModeRed[4] = { 0.f, 0.f, 1.f, 1.f };
    
    MergeMult4Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
        configSwitch(MERGEMODE_PARAM, 0.f, 1.f, 0.f, "Mode", { "Merge/Sum", "Mix/Average" });

        configInput(MERGE1_INPUT, "Merge-1");
        configInput(MERGE2_INPUT, "Merge-2");
        configInput(MERGE3_INPUT, "Merge-3");
        configInput(MERGE4_INPUT, "Merge-4");
        configOutput(MERGE_OUTPUT, "Merge (sum or mix)");
        configLight(MODE_LIGHT, "Negative: Unlit=Signed, Green=Zero, Yellow=Abs-in, Red=Abs-out");

        configInput(MULT_INPUT, "Mult");
        configOutput(MULT1_OUTPUT, "Mult-1");
        configOutput(MULT2_OUTPUT, "Mult-2");
        configOutput(MULT3_OUTPUT, "Mult-3");
        configOutput(MULT4_OUTPUT, "Mult-4");

        configBypass(MERGE1_INPUT, MERGE_OUTPUT);
        configBypass(MULT_INPUT, MULT1_OUTPUT);
        configBypass(MULT_INPUT, MULT2_OUTPUT);
        configBypass(MULT_INPUT, MULT3_OUTPUT);
        configBypass(MULT_INPUT, MULT4_OUTPUT);

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
        negMode.setBoth(nm_neg);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);

        negMode.setBoth((negModeType)getJsonInt(rootJ, "negMode", nm_neg));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "negMode", json_integer((int)negMode.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        haveOutputs = false;
        mergeInputCount = 0;
        mergeChannels = 1;
        multOutputCount = 0;
        multChannels = 0;
        mixMode = params[MERGEMODE_PARAM].getValue() > 0.5f;
        if (negMode.needsUpdate()) {
            negMode.updateActual();
            lights[MODE_LIGHT].setBrightness(negModeGreen[negMode.act]);
            lights[MODE_LIGHT + 1].setBrightness(negModeRed[negMode.act]);
        }

        for (int i = 0; i < 4; i++) {
            if (inputs[MERGE1_INPUT + i].isConnected()) {
                mergeInputCount++;
     			mergeChannels = std::max(mergeChannels, std::max(inputs[MERGE1_INPUT + i].getChannels(), 1));
            }

            if (outputs[MULT1_OUTPUT + i].isConnected())
                multOutputCount++;
        }

        multChannels = (inputs[MULT_INPUT].isConnected())
			? std::max(inputs[MULT_INPUT].getChannels(), 1)
			: mergeChannels; // Normalized to merge-output

        haveOutputs = outputs[MERGE_OUTPUT].isConnected() ||
            multOutputCount > 0;

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
            // both used for merge-out and mult-in (as mult-in is normalized to merge-out)
			float mulIn[PORT_MAX_CHANNELS] = { 0.f };

            // ---------- Merge-section ----------
            float mergeSum[PORT_MAX_CHANNELS] = { 0.f };
            if (mergeInputCount > 0) {
                for (int i = 0; i < 4; i++) {
                    if (inputs[MERGE1_INPUT + i].isConnected()) {
                        for (int c = 0; c < mergeChannels; c++) {
                            float voltage = inputs[MERGE1_INPUT + i].getPolyVoltage(c);
                            if (voltage < 0.f) {
                                if (negMode.act == nm_zero)
                                voltage = 0.f;
                                else if (negMode.act == nm_absIn)
                                    voltage = -voltage;
                            }
                            mergeSum[c] += voltage;
                        }
                    }
				}
            }

            if (negMode.act == nm_absOut) {
                for (int c = 0; c < mergeChannels; c++) {
                    mergeSum[c] = std::abs(mergeSum[c]);
                }
            }

            // set mulIn, as mult-section is normalized to merge-output
            if (mergeInputCount > 0 || multOutputCount > 0) {
                for (int c = 0; c < mergeChannels; c++) {
                    mulIn[c] = (mergeInputCount > 0)
                        ? (!mixMode)
                            ? mergeSum[c]
                            : mergeSum[c] / mergeInputCount
                        : 0.f;
                    mulIn[c] = quantizeToMode(mulIn[c], outQuantize.act);
                    mulIn[c] = clipToVoltRange(mulIn[c], outClipRange.act);
                }
            }

            // Output merge
            if (outputs[MERGE_OUTPUT].isConnected()) {
				outputs[MERGE_OUTPUT].setChannels(mergeChannels);
                for (int c = 0; c < mergeChannels; c++) {
                    outputs[MERGE_OUTPUT].setVoltage(mulIn[c], c);
                }
			}

            // ---------- Mult-section-------------
            if (multOutputCount > 0) {
                // Use mult-input if connected (else merge-output is used)
                if (inputs[MULT_INPUT].isConnected()) {
                    for (int c = 0; c < multChannels; c++) {
                        mulIn[c] = inputs[MULT_INPUT].getVoltage(c);
                        mulIn[c] = quantizeToMode(mulIn[c], outQuantize.act);
                        mulIn[c] = clipToVoltRange(mulIn[c], outClipRange.act);
                    }
                }

                for (int i = 0; i < 4; i++) {
                    if (outputs[MULT1_OUTPUT + i].isConnected()) {
						outputs[MULT1_OUTPUT + i].setChannels(multChannels);
                        for (int c = 0; c < multChannels; c++) {
							outputs[MULT1_OUTPUT + i].setVoltage(mulIn[c], c);
						}
					}
				}
            }
        }

        cycle256++;
    }
};

struct MergeMult4ModuleWidget : InfNoiseModuleWidget {
    MergeMult4ModuleWidget(MergeMult4Module *module) {
        initializeWidget(module, "res/MergeMult4");

        float centerCol = 15.f;  // 14,810
        float rowSpacing = 29.9425f;
        const float modeColOfs = 9.742f;
        const float modeRowOfs = -13.222f;

        // Merge inputs/output
        float row = 52.106f;
        infNoiseLtSmallButton* modeBtn = createParamCentered<infNoiseLtSmallButton>(Vec(centerCol + modeColOfs, row + modeRowOfs), module, MergeMult4Module::MERGEMODE_PARAM);
        modeBtn->setup(bc_green, false);
        addParam(modeBtn);
        for (int i = 0; i < 4; i++) {
			addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, row), module, MergeMult4Module::MERGE1_INPUT + i));
            row += rowSpacing;
		}
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, row), module, MergeMult4Module::MERGE_OUTPUT));
        addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(5.189f, 162.255f), module, MergeMult4Module::MODE_LIGHT));

        // Mult inputs/outputs
        row = 213.026f;
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, row), module, MergeMult4Module::MULT_INPUT));
        for (int i = 0; i < 4; i++) {
            row += rowSpacing;
            addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, row), module, MergeMult4Module::MULT1_OUTPUT + i));
		}
    }

    void appendContextMenu(Menu* menu) override {
        MergeMult4Module* module = dynamic_cast<MergeMult4Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> negModeNames = { "Negative (default)", "Zero", "Absolute (input)", "Absolute (output)" };
        menu->addChild(createIndexPtrSubmenuItem("Negative-mode", negModeNames,
            &module->negMode.req));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelMergeMult4 = createModel<MergeMult4Module, MergeMult4ModuleWidget>("MergeMult4");