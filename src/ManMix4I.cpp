// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct ManMix4IModule : InfNoiseModule {
    enum ParamId {
        MASTER_MIX_PARAM,
        MIX1_PARAM,
        MIX2_PARAM,
        MIX3_PARAM,
        MIX4_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        CV1_INPUT,
        CV2_INPUT,
        CV3_INPUT,
        CV4_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
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

    int inputsInUse = 0;
    bool haveOutputs = false;
    int channels = 1;
    int firstIdx = -1;
    int lastIdx = -1;
    float knobValues[4] = { 1.f, 1.f, 1.f, 1.f };
    float knobSigns[4] = { 1.f, 1.f, 1.f, 1.f };
    float mixScale = 1.f;

	ManMix4IModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configParam(MASTER_MIX_PARAM, 0.f, 2.f, 1.f, "Master-amplification (0% to 200%)", " %", 0, 100);
        configParam(MIX1_PARAM, 0.f, 2.f, 1.f, "A-amplification (0% to 200%)", " %", 0, 100);
        configParam(MIX2_PARAM, 0.f, 2.f, 1.f, "B-amplification (0% to 200%)", " %", 0, 100);
        configParam(MIX3_PARAM, 0.f, 2.f, 1.f, "C-amplification (0% to 200%)", " %", 0, 100);
        configParam(MIX4_PARAM, 0.f, 2.f, 1.f, "D-amplification (0% to 200%)", " %", 0, 100);

        const std::string letters[]{ "A", "B", "C", "D" };
        for (int i = 0; i < 4; i++)
            configLight(INV1_LIGHT + i, letters[i] + "-amplification inverted (if lit)");
        configLight(MIX_MODE_LIGHT, "Dim: Averaging mix (default), Red: Unity mix");
        
        configInput(CV1_INPUT, "A-CV");
        configInput(CV2_INPUT, "B-CV");
        configInput(CV3_INPUT, "C-CV");
        configInput(CV4_INPUT, "D-CV");

        configOutput(MIX_OUTPUT, "Mix");

        configBypass(CV1_INPUT, MIX_OUTPUT);

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

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        haveOutputs = outputs[MIX_OUTPUT].isConnected();
        firstIdx = -1;
        lastIdx = -1;
        inputsInUse = 0;
        channels = 1;
        for (int i = 0; i < 4; i++) {
            if (mixInv[i].needsUpdate()) {
                mixInv[i].updateActual();
                knobSigns[i] = (mixInv[i].act == mki_inverted) ? -1.f : 1.f;
                lights[INV1_LIGHT + i].setBrightness(mixInv[i].act == mki_inverted ? 1.f : 0.f);
            }
        }
        if (haveOutputs) {
            for (int i = 0; i < 4; i++) {
                knobValues[i] = params[MIX1_PARAM + i].getValue() * knobSigns[i];
                if (inputs[CV1_INPUT + i].isConnected()) {
                    channels = (inputsInUse == 0)
                        ? inputs[CV1_INPUT + i].getChannels()
                        : std::max(channels, inputs[CV1_INPUT + i].getChannels());                   
                    inputsInUse++;
                    if (firstIdx < 0)
						firstIdx = i;
					lastIdx = i;
				}
			}
		}
        outputs[MIX_OUTPUT].setChannels(channels);

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
            if (inputsInUse > 0) {
                for (int c = 0; c < channels; c++) {
                    float voltage = 0.f;
                    for (int i = firstIdx; i <= lastIdx; i++) {
                        if (inputs[CV1_INPUT + i].isConnected()) {
                            voltage += inputs[CV1_INPUT + i].getPolyVoltage(c) * knobValues[i];
                        }
                    }

                    voltage = voltage * mixScale * params[MASTER_MIX_PARAM].getValue();
                    voltage = clipToVoltRange(voltage, outClipRange.act);
                    outputs[MIX_OUTPUT].setVoltage(voltage, c);
                }
            }
            else
            {
				outputs[MIX_OUTPUT].setVoltage(0.f); // Nothing was mixed (no inputs connected)
			}
        }

        cycle256++;
    }
};

struct ManMix4IModuleWidget : InfNoiseModuleWidget {
    ManMix4IModuleWidget(ManMix4IModule *module) {
        initializeWidget(module, "res/ManMix4I");

        const float cntrClm = 15.f;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrClm, 52.106f), module, ManMix4IModule::MASTER_MIX_PARAM));

        const float mixKnobRows[]{ 88.993f, 120.250f, 152.007f, 183.263f };
        const float lgtOfs = 10.021f;
        for (int i = 0; i < 4; i++) {
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrClm, mixKnobRows[i]), module, ManMix4IModule::MIX1_PARAM + i));
            addChild(createLightCentered<TinyLight<RedLight>>(Vec(cntrClm + lgtOfs, mixKnobRows[i] - lgtOfs), module, ManMix4IModule::INV1_LIGHT + i));
        }

        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 220.849f), module, ManMix4IModule::CV1_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 245.481f), module, ManMix4IModule::CV2_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 270.114f), module, ManMix4IModule::CV3_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 294.746f), module, ManMix4IModule::CV4_INPUT));

        addChild(createLightCentered<TinyLight<RedLight>>(Vec(4.737f, 317.911f), module, ManMix4IModule::MIX_MODE_LIGHT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrClm, 333.194f), module, ManMix4IModule::MIX_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        ManMix4IModule* module = dynamic_cast<ManMix4IModule*>(this->module);
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

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelManMix4I = createModel<ManMix4IModule, ManMix4IModuleWidget>("ManMix4I");
