// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct ManMix4stModule : InfNoiseModule {
    enum ParamId {
        MASTER_MIX_PARAM,
        MIX1_PARAM,
        MIX2_PARAM,
        MIX3_PARAM,
        MIX4_PARAM,
        MIX2_NORM_PARAM,
        MIX3_NORM_PARAM,
        MIX4_NORM_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        MASTER_MIX_CV_INPUT,
        MIX1_CV_INPUT,
        MIX2_CV_INPUT,
        MIX3_CV_INPUT,
        MIX4_CV_INPUT,
        CV1_LEFT_INPUT,
        CV2_LEFT_INPUT,
        CV3_LEFT_INPUT,
        CV4_LEFT_INPUT,
        CV1_RIGHT_INPUT,
        CV2_RIGHT_INPUT,
        CV3_RIGHT_INPUT,
        CV4_RIGHT_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        MIX_LEFT_OUTPUT,
        MIX_RIGHT_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        INV1_LIGHT,
        INV2_LIGHT,
        INV3_LIGHT,
        INV4_LIGHT,
        ENUMS(MONO_TO_STEREO1_LIGHT,2),
        ENUMS(MONO_TO_STEREO2_LIGHT,2),
        ENUMS(MONO_TO_STEREO3_LIGHT,2),
        ENUMS(MONO_TO_STEREO4_LIGHT,2),
        MIX_MODE_LIGHT,
        LIGHTS_LEN
    };

    enum mixModeType { mm_averaging, mm_unity };
    enum mixKnobInvert { mki_normal, mki_inverted };
    actReqValue<mixModeType> mixMode = actReqValue<mixModeType>(mm_averaging);
    actReqValue<mixKnobInvert> mixInv[4] = {
        actReqValue<mixKnobInvert>(mki_normal),
        actReqValue<mixKnobInvert>(mki_normal),
        actReqValue<mixKnobInvert>(mki_normal),
        actReqValue<mixKnobInvert>(mki_normal)
    };

    int inputsInUse = 0;
    bool haveOutputs = false;
    bool mixNormEnabled[3] = { true, true, true };
    int channels = 1;
    int firstIdx = -1;
    int lastIdx = -1;
    float masterKnob = 1.f;
    float mixKnob[4] = { 1.f, 1.f, 1.f, 1.f };
    float knobSigns[4] = { 1.f, 1.f, 1.f, 1.f };
    float mixScale = 1.f;
    actReqValue<bool> monoToStereo = actReqValue<bool>(true);

	ManMix4stModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
        
        configLight(MONO_TO_STEREO1_LIGHT, "Mono-to-Stereo-1");
        configLight(MONO_TO_STEREO2_LIGHT, "Mono-to-Stereo-2");
        configLight(MONO_TO_STEREO3_LIGHT, "Mono-to-Stereo-3");
        configLight(MONO_TO_STEREO4_LIGHT, "Mono-to-Stereo-4");

        configParam(MASTER_MIX_PARAM, 0.f, 2.f, 1.f, "Master-amplification (0% to 200%)", " %", 0, 100);
        configParam(MIX1_PARAM, 0.f, 2.f, 1.f, "A-amplification (0% to 200%)", " %", 0, 100);
        configParam(MIX2_PARAM, 0.f, 2.f, 1.f, "B-amplification (0% to 200%)", " %", 0, 100);
        configParam(MIX3_PARAM, 0.f, 2.f, 1.f, "C-amplification (0% to 200%)", " %", 0, 100);
        configParam(MIX4_PARAM, 0.f, 2.f, 1.f, "D-amplification (0% to 200%)", " %", 0, 100);

        configSwitch(MIX2_NORM_PARAM, 0.f, 1.f, 1.f, "B-amp CV normalized to A", { "Disabled", "Enabled" });
        configSwitch(MIX3_NORM_PARAM, 0.f, 1.f, 1.f, "C-amp CV normalized to B", { "Disabled", "Enabled" });
        configSwitch(MIX4_NORM_PARAM, 0.f, 1.f, 1.f, "D-amp CV normalized to C", { "Disabled", "Enabled" });

        const std::string letters[]{ "A", "B", "C", "D" };
        for (int i = 0; i < 4; i++)
            configLight(INV1_LIGHT + i, letters[i] + "-amplification inverted (if lit)");
        configLight(MIX_MODE_LIGHT, "Dim: Averaging mix (default), Red: Unity mix");

        configInput(MASTER_MIX_CV_INPUT, "Master-amplification CV");
        configInput(MIX1_CV_INPUT, "A-amplification CV (normalized to 0V)");
        configInput(MIX2_CV_INPUT, "B-amplification CV (normalized to A)");
        configInput(MIX3_CV_INPUT, "C-amplification CV (normalized to B)");
        configInput(MIX4_CV_INPUT, "D-amplification CV (normalized to C)");
        
        configInput(CV1_LEFT_INPUT, "A-Left CV");
        configInput(CV2_LEFT_INPUT, "B-Left CV");
        configInput(CV3_LEFT_INPUT, "C-Left CV");
        configInput(CV4_LEFT_INPUT, "D-Left CV");
        configInput(CV1_RIGHT_INPUT, "A-Right CV");
        configInput(CV2_RIGHT_INPUT, "B-Right CV");
        configInput(CV3_RIGHT_INPUT, "C-Right CV");
        configInput(CV4_RIGHT_INPUT, "D-Right CV");

        configOutput(MIX_LEFT_OUTPUT, "Mix-Left");
        configOutput(MIX_RIGHT_OUTPUT, "Mix-Right");

        configBypass(CV1_LEFT_INPUT, MIX_LEFT_OUTPUT);
        configBypass(CV1_RIGHT_INPUT, MIX_RIGHT_OUTPUT);

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
		haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = true;  
		haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = false;
		haveTrigHighLow = false;
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        mixMode.setBoth(mm_averaging);
        monoToStereo.setBoth(true);
        for (int i = 0; i < 4; i++) {
            mixInv[i].setBoth(mki_normal);
            knobSigns[i] = 1.f;
        }
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        mixMode.setBoth((mixModeType)getJsonInt(rootJ, "mixMode", (int)mm_averaging));
        monoToStereo.setBoth(getJsonBool(rootJ, "monoToStereo", true));
        int mixInvTmp[4];
        getJsonIntArray(rootJ, "mixInv", mixInvTmp, 4, (int)mki_normal);
        for (int i = 0; i < 4; i++)
            mixInv[i].setBoth((mixKnobInvert)mixInvTmp[i]);
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "mixMode", json_integer((int)mixMode.req));
        json_object_set_new(rootJ, "monoToStereo", json_boolean(monoToStereo.req));
        int mixInvTmp[4];
        for (int i = 0; i < 4; i++)
            mixInvTmp[i] = (int)mixInv[i].req;
        setJsonIntArray(rootJ, "mixInv", mixInvTmp, 4);
    }

    void updateMixCvPortNames() {
        static const char* prevLetter[]{ "", "A", "B", "C" };
        const std::string letters[]{ "A", "B", "C", "D" };
        for (int i = 0; i < 4; i++) {
            if (inputInfos.size() <= (unsigned)(MIX1_CV_INPUT + i) || !inputInfos[MIX1_CV_INPUT + i])
                continue;
            std::string name = letters[i] + "-amplification CV";
            if (i == 0)
                name += " (normalized to 0V)";
            else if (mixNormEnabled[i - 1])
                name += string::f(" (normalized to %s)", prevLetter[i]);
            inputInfos[MIX1_CV_INPUT + i]->name = monoPortPrefix() + name;
        }
    }

    float resolveMixCv(int i) {
        float v = 0.f;
        for (int j = 0; j <= i; j++) {
            if (inputs[MIX1_CV_INPUT + j].isConnected())
                v = inputs[MIX1_CV_INPUT + j].getVoltage();
            else if (j == 0 || !mixNormEnabled[j - 1])
                v = 0.f;
        }
        return v;
    }

    float effectiveChannelGain(int i) {
        float g = rack::math::clamp(mixKnob[i] + resolveMixCv(i) / 5.f, 0.f, 2.f);
        return g * knobSigns[i];
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        for (int i = 0; i < 3; i++)
            mixNormEnabled[i] = params[MIX2_NORM_PARAM + i].getValue() > 0.5f;
        updateMixCvPortNames();

        masterKnob = params[MASTER_MIX_PARAM].getValue();
        for (int i = 0; i < 4; i++) {
            mixKnob[i] = params[MIX1_PARAM + i].getValue();
            if (mixInv[i].needsUpdate()) {
                mixInv[i].updateActual();
                knobSigns[i] = (mixInv[i].act == mki_inverted) ? -1.f : 1.f;
                lights[INV1_LIGHT + i].setBrightness(mixInv[i].act == mki_inverted ? 1.f : 0.f);
            }
        }

        haveOutputs = outputs[MIX_LEFT_OUTPUT].isConnected() || outputs[MIX_RIGHT_OUTPUT].isConnected();
        firstIdx = -1;
        lastIdx = -1;
        inputsInUse = 0;
        channels = 1;
        if (haveOutputs) {
            for (int i = 0; i < 4; i++) {
                if (inputs[CV1_LEFT_INPUT + i].isConnected() || inputs[CV1_RIGHT_INPUT + i].isConnected()) {
                    int sectionInput = std::max(inputs[CV1_LEFT_INPUT + i].getChannels(), inputs[CV1_RIGHT_INPUT + i].getChannels());
                    channels = (inputsInUse == 0)
                        ? sectionInput
                        : std::max(channels, sectionInput);
                    inputsInUse++;
                    if (firstIdx < 0)
						firstIdx = i;
					lastIdx = i;
				}
			}
		}
        outputs[MIX_LEFT_OUTPUT].setChannels(channels);
        outputs[MIX_RIGHT_OUTPUT].setChannels(channels);

        if (monoToStereo.needsUpdate()) {
            monoToStereo.updateActual();

            const std::string letters[]{ "A", "B", "C", "D" };
            const std::string norm = (monoToStereo.act) ? " (normalized to left)" : "";
            float greenLight = monoToStereo.act ? 1.f : 0.f;
            float redLight = monoToStereo.act ? 0.f : 1.f;
            for (int i = 0; i < 4; i++) {
                lights[MONO_TO_STEREO1_LIGHT + i * 2].setBrightness(greenLight);
                lights[MONO_TO_STEREO1_LIGHT + i * 2 + 1].setBrightness(redLight);
                if (inputInfos.size() > (unsigned)CV1_RIGHT_INPUT+i && inputInfos[CV1_RIGHT_INPUT+i]) {
                    inputInfos[CV1_RIGHT_INPUT+i]->name = polyPortPrefix() + letters[i]+"-Right CV" + norm;
                }
            }
        }

        if (mixMode.needsUpdate()) {
            mixMode.updateActual();
            lights[MIX_MODE_LIGHT].setBrightness(mixMode.act == mm_unity ? 1.f : 0.f);
        }
        mixScale = (mixMode.act == mm_averaging && inputsInUse > 0) 
            ? (1.f / (float)inputsInUse) 
            : 1.f;

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
            float masterGain = masterKnob;
            if (inputs[MASTER_MIX_CV_INPUT].isConnected())
                masterGain = rack::math::clamp(masterKnob + inputs[MASTER_MIX_CV_INPUT].getVoltage() / 5.f, 0.f, 2.f);

            float channelGain[4];
            for (int i = 0; i < 4; i++)
                channelGain[i] = effectiveChannelGain(i);

            if (inputsInUse > 0) {
                for (int c = 0; c < channels; c++) {
                    float leftVoltage = 0.f;
                    float rightVoltage = 0.f;
                    for (int i = firstIdx; i <= lastIdx; i++) {
                        float sectionLeft = 0.f;
                        if (inputs[CV1_LEFT_INPUT + i].isConnected()) {
                            sectionLeft = inputs[CV1_LEFT_INPUT + i].getPolyVoltage(c) * channelGain[i];
                            leftVoltage += sectionLeft;
                        }
                        if (inputs[CV1_RIGHT_INPUT + i].isConnected()) {
                            rightVoltage += inputs[CV1_RIGHT_INPUT + i].getPolyVoltage(c) * channelGain[i];
                        } else if (monoToStereo.act) {
                            rightVoltage += sectionLeft;
                        }
                    }

                    leftVoltage = leftVoltage * mixScale * masterGain;
                    leftVoltage = clipToVoltRange(leftVoltage, outClipRange.act);
                    outputs[MIX_LEFT_OUTPUT].setVoltage(leftVoltage, c);

                    rightVoltage = rightVoltage * mixScale * masterGain;
                    rightVoltage = clipToVoltRange(rightVoltage, outClipRange.act);
                    outputs[MIX_RIGHT_OUTPUT].setVoltage(rightVoltage, c);
                }
            }
            else
            {
                // Nothing was mixed (no inputs connected)
				outputs[MIX_LEFT_OUTPUT].setVoltage(0.f);
                outputs[MIX_RIGHT_OUTPUT].setVoltage(0.f);
			}
        }

        cycle256++;
    }
};

struct ManMix4stModuleWidget : InfNoiseModuleWidget {
    ManMix4stModuleWidget(ManMix4stModule *module) {
        initializeWidget(module, "res/ManMix4st");

        const float inpClm = 15.f;
        const float knobClm = 45.f;
        const float lgtOfs = 10.021f;
        const float masterRow = 52.106f;
        const float mixKnobRows[]{ 88.993f, 120.250f, 152.007f, 183.263f };
        const float normBtnRows[]{ 104.924f, 136.181f, 167.338f };
        const float leftClm = 15.f;
        const float rightClm = 45.f;
        const float monoToStereoClm = 30.f;
        const float signalRows[]{ 220.849f, 245.481f, 270.114f, 294.746f };
        const float monoToStereoLightRows[]{ 207.673f, 232.272f, 256.871f, 281.470f };

        addInput(createInputCentered<ThemedPJ301MPort>(Vec(inpClm, masterRow), module, ManMix4stModule::MASTER_MIX_CV_INPUT));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(knobClm, masterRow), module, ManMix4stModule::MASTER_MIX_PARAM));

        for (int i = 0; i < 4; i++) {
            addInput(createInputCentered<ThemedPJ301MPort>(Vec(inpClm, mixKnobRows[i]), module, ManMix4stModule::MIX1_CV_INPUT + i));
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(knobClm, mixKnobRows[i]), module, ManMix4stModule::MIX1_PARAM + i));
            addChild(createLightCentered<TinyLight<RedLight>>(Vec(knobClm + lgtOfs, mixKnobRows[i] - lgtOfs), module, ManMix4stModule::INV1_LIGHT + i));
        }

        for (int i = 0; i < 3; i++) {
            infNoiseLtSmallButton* normBtn = createParamCentered<infNoiseLtSmallButton>(
                Vec(22.553f, normBtnRows[i]), module, ManMix4stModule::MIX2_NORM_PARAM + i);
            normBtn->setup(bc_redGreen, false);
            addParam(normBtn);
        }

        for (int i = 0; i < 4; i++) {
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftClm, signalRows[i]), module, ManMix4stModule::CV1_LEFT_INPUT + i));
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(rightClm, signalRows[i]), module, ManMix4stModule::CV1_RIGHT_INPUT + i));
            addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(monoToStereoClm, monoToStereoLightRows[i]), module, ManMix4stModule::MONO_TO_STEREO1_LIGHT + (i * 2)));
        }

        addChild(createLightCentered<TinyLight<RedLight>>(Vec(30.f, 318.01f), module, ManMix4stModule::MIX_MODE_LIGHT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftClm, 333.194f), module, ManMix4stModule::MIX_LEFT_OUTPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightClm, 333.194f), module, ManMix4stModule::MIX_RIGHT_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        ManMix4stModule* module = dynamic_cast<ManMix4stModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexPtrSubmenuItem("Mix mode",
            { "Averaging mix", "Unity mix" },
            &module->mixMode.req));

        menu->addChild(createBoolPtrMenuItem("Mono-to-Stereo (L>R)", "", &module->monoToStereo.req));

        const std::string letters[]{ "A", "B", "C", "D" };
        for (int i = 0; i < 4; i++) {
            menu->addChild(createIndexPtrSubmenuItem(
                letters[i] + "-amplification",
                { "Normal", "Inverted" },
                &module->mixInv[i].req));
        }
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelManMix4st = createModel<ManMix4stModule, ManMix4stModuleWidget>("ManMix4st");
