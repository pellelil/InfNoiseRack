// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct ManMix4IIModule : InfNoiseModule {
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
        CV1_INPUT,
        CV2_INPUT,
        CV3_INPUT,
        CV4_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        OUT1_OUTPUT,
        OUT2_OUTPUT,
        OUT3_OUTPUT,
        OUT4_OUTPUT,
        MIX_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        INV1_LIGHT,
        INV2_LIGHT,
        INV3_LIGHT,
        INV4_LIGHT,
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

    bool haveAnyOutput = false;
    bool haveMixOutput = false;
    bool haveIndivOutput[4] = { false, false, false, false };
    bool haveSignalIn[4] = { false, false, false, false };
    bool mixNormEnabled[3] = { true, true, true };
    int inputsInUse = 0;
    int channels[4] = { 1, 1, 1, 1 };
    int mixChannels = 1;
    int firstIdx = -1;
    int lastIdx = -1;
    float masterKnob = 1.f;
    float mixKnob[4] = { 1.f, 1.f, 1.f, 1.f };
    float knobSigns[4] = { 1.f, 1.f, 1.f, 1.f };
    float mixScale = 1.f;

	ManMix4IIModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

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

        configInput(CV1_INPUT, "A-CV");
        configInput(CV2_INPUT, "B-CV");
        configInput(CV3_INPUT, "C-CV");
        configInput(CV4_INPUT, "D-CV");

        configOutput(OUT1_OUTPUT, "A-out");
        configOutput(OUT2_OUTPUT, "B-out");
        configOutput(OUT3_OUTPUT, "C-out");
        configOutput(OUT4_OUTPUT, "D-out");
        configOutput(MIX_OUTPUT, "Mix");

        configBypass(CV1_INPUT, OUT1_OUTPUT);
        configBypass(CV1_INPUT, MIX_OUTPUT);

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
        for (int i = 0; i < 4; i++) {
            mixInv[i].setBoth(mki_normal);
            knobSigns[i] = 1.f;
        }
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        mixMode.setBoth((mixModeType)getJsonInt(rootJ, "mixMode", (int)mm_averaging));
        int mixInvTmp[4];
        getJsonIntArray(rootJ, "mixInv", mixInvTmp, 4, (int)mki_normal);
        for (int i = 0; i < 4; i++)
            mixInv[i].setBoth((mixKnobInvert)mixInvTmp[i]);
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "mixMode", json_integer((int)mixMode.req));
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

        haveAnyOutput = false;
        haveMixOutput = outputs[MIX_OUTPUT].isConnected();
        if (haveMixOutput)
            haveAnyOutput = true;

        firstIdx = -1;
        lastIdx = -1;
        inputsInUse = 0;
        mixChannels = 1;
        for (int i = 0; i < 4; i++) {
            haveIndivOutput[i] = outputs[OUT1_OUTPUT + i].isConnected();
            if (haveIndivOutput[i])
                haveAnyOutput = true;

            haveSignalIn[i] = inputs[CV1_INPUT + i].isConnected();
            channels[i] = haveSignalIn[i] ? inputs[CV1_INPUT + i].getChannels() : 1;

            if (haveIndivOutput[i])
                outputs[OUT1_OUTPUT + i].setChannels(haveSignalIn[i] ? channels[i] : 1);

            if (haveSignalIn[i]) {
                mixChannels = std::max(mixChannels, channels[i]);
                inputsInUse++;
                if (firstIdx < 0)
                    firstIdx = i;
                lastIdx = i;
            }
        }
        if (haveMixOutput)
            outputs[MIX_OUTPUT].setChannels(mixChannels);

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

        if (doProcess && haveAnyOutput) {
            float masterGain = masterKnob;
            if (inputs[MASTER_MIX_CV_INPUT].isConnected())
                masterGain = rack::math::clamp(masterKnob + inputs[MASTER_MIX_CV_INPUT].getVoltage() / 5.f, 0.f, 2.f);

            float channelGain[4];
            for (int i = 0; i < 4; i++)
                channelGain[i] = effectiveChannelGain(i);

            for (int i = 0; i < 4; i++) {
                if (haveIndivOutput[i] && haveSignalIn[i]) {
                    for (int c = 0; c < channels[i]; c++) {
                        float voltage = inputs[CV1_INPUT + i].getPolyVoltage(c) * channelGain[i];
                        voltage = clipToVoltRange(voltage, outClipRange.act);
                        outputs[OUT1_OUTPUT + i].setVoltage(voltage, c);
                    }
                }
                else if (haveIndivOutput[i]) {
                    outputs[OUT1_OUTPUT + i].setVoltage(0.f);
                }
            }

            if (haveMixOutput) {
                if (inputsInUse > 0) {
                    for (int c = 0; c < mixChannels; c++) {
                        float voltage = 0.f;
                        for (int i = firstIdx; i <= lastIdx; i++) {
                            if (haveSignalIn[i] && c < channels[i])
                                voltage += inputs[CV1_INPUT + i].getPolyVoltage(c) * channelGain[i];
                        }
                        voltage = voltage * mixScale * masterGain;
                        voltage = clipToVoltRange(voltage, outClipRange.act);
                        outputs[MIX_OUTPUT].setVoltage(voltage, c);
                    }
                }
                else {
                    outputs[MIX_OUTPUT].setVoltage(0.f);
                }
            }
        }

        cycle256++;
    }
};

struct ManMix4IIModuleWidget : InfNoiseModuleWidget {
    ManMix4IIModuleWidget(ManMix4IIModule *module) {
        initializeWidget(module, "res/ManMix4II");

        const float inpClm = 15.f;
        const float knobClm = 45.f;
        const float mixOutClm = 30.f;
        const float lgtOfs = 10.021f;
        const float masterRow = 52.106f;
        const float mixKnobRows[]{ 88.993f, 120.250f, 152.007f, 183.263f };
        const float normBtnRows[]{ 104.924f, 136.181f, 167.338f };
        const float signalRows[]{ 220.849f, 245.481f, 270.114f, 294.746f };

        addInput(createInputCentered<ThemedPJ301MPort>(Vec(inpClm, masterRow), module, ManMix4IIModule::MASTER_MIX_CV_INPUT));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(knobClm, masterRow), module, ManMix4IIModule::MASTER_MIX_PARAM));

        for (int i = 0; i < 4; i++) {
            addInput(createInputCentered<ThemedPJ301MPort>(Vec(inpClm, mixKnobRows[i]), module, ManMix4IIModule::MIX1_CV_INPUT + i));
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(knobClm, mixKnobRows[i]), module, ManMix4IIModule::MIX1_PARAM + i));
            addChild(createLightCentered<TinyLight<RedLight>>(Vec(knobClm + lgtOfs, mixKnobRows[i] - lgtOfs), module, ManMix4IIModule::INV1_LIGHT + i));
        }

        for (int i = 0; i < 3; i++) {
            infNoiseLtSmallButton* normBtn = createParamCentered<infNoiseLtSmallButton>(
                Vec(22.553f, normBtnRows[i]), module, ManMix4IIModule::MIX2_NORM_PARAM + i);
            normBtn->setup(bc_redGreen, false);
            addParam(normBtn);
        }

        for (int i = 0; i < 4; i++) {
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(inpClm, signalRows[i]), module, ManMix4IIModule::CV1_INPUT + i));
            addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(knobClm, signalRows[i]), module, ManMix4IIModule::OUT1_OUTPUT + i));
        }

        addChild(createLightCentered<TinyLight<RedLight>>(Vec(19.18f, 318.01f), module, ManMix4IIModule::MIX_MODE_LIGHT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(mixOutClm, 333.194f), module, ManMix4IIModule::MIX_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        ManMix4IIModule* module = dynamic_cast<ManMix4IIModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexPtrSubmenuItem("Mix mode",
            { "Averaging mix", "Unity mix" },
            &module->mixMode.req));

        const std::string letters[]{ "A", "B", "C", "D" };
        for (int i = 0; i < 4; i++) {
            menu->addChild(createIndexPtrSubmenuItem(
                letters[i] + "-amplification",
                { "Normal", "Inverted" },
                &module->mixInv[i].req));
        }

        appendInfNoiseMenuItems(menu);
    }
};

Model *modelManMix4II = createModel<ManMix4IIModule, ManMix4IIModuleWidget>("ManMix4II");
