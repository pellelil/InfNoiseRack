// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct VCA2Module : InfNoiseModule {
    enum ParamId {
        VCA_A_KNOB_PARAM,
        VCA_B_KNOB_PARAM,
        VCA_A_TRIM_PARAM,
        VCA_B_TRIM_PARAM,
        LINK_MODE_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        VCA_A_INPUT,
        VCA_B_INPUT,
        A_INPUT,
        B_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        A_OUTPUT,
        B_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        ENUMS(SCALE_MODE_A_LIGHT, 2), // linear og "exponential"
        ENUMS(SCALE_MODE_B_LIGHT, 2), // linear og "exponential"
        LIGHTS_LEN
    };

    bool haveOutputs = false;
    actReqValue<scaleCurve> scalingMode = actReqValue<scaleCurve>(sc_linear);
    bool bLinked = false;
    float ampKnob[2] = { 1.f, 1.f };
    float ampTrim[2] = { 0.f, 0.f };
    
	VCA2Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configParam(VCA_A_KNOB_PARAM, 0.0f, 1.0f, 1.0f, "A-Amplify", "%", 0, 100);
        configParam(VCA_A_TRIM_PARAM, -1.f, 1.f, 0.f, "A-Amplify trim (-100% to +100%)", " %", 0, 100);
        configLight(SCALE_MODE_A_LIGHT, "Scale-mode (unlit=Linear, green=Exp, red=Log)");

        configParam(VCA_B_KNOB_PARAM, 0.0f, 1.0f, 1.0f, "B-Amplify", "%", 0, 100);
        configParam(VCA_B_TRIM_PARAM, -1.f, 1.f, 0.f, "B-Amplify trim (-100% to +100%)", " %", 0, 100);
        configLight(SCALE_MODE_B_LIGHT, "Scale-mode (unlit=Linear, green=Exp, red=Log)");

        configSwitch(LINK_MODE_PARAM, 0.0, 1.0, 0.0, "Link-mode", { "Individual", "Linked to A" });

        configInput(VCA_A_INPUT, "A-VCA");
        configInput(VCA_B_INPUT, "B-VCA (normalized to A-VCA)");
        configInput(A_INPUT, "A");
        configInput(B_INPUT, "B (normalized to A)");

        configOutput(A_OUTPUT, "A");
        configOutput(B_OUTPUT, "B");

        configBypass(A_INPUT, A_OUTPUT);
        configBypass(B_INPUT, B_OUTPUT);

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

        scalingMode.setBoth(sc_linear);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);

        scalingMode.setBoth((scaleCurve)getJsonInt(rootJ, "scalingMode", (int)sc_linear));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "scalingMode", json_integer((int)scalingMode.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        ampKnob[0] = params[VCA_A_KNOB_PARAM].getValue();
        ampTrim[0] = params[VCA_A_TRIM_PARAM].getValue();
        bLinked = params[LINK_MODE_PARAM].getValue() > 0.5f;
        if (bLinked) {
			params[VCA_B_KNOB_PARAM].setValue(ampKnob[0]);
			params[VCA_B_TRIM_PARAM].setValue(ampTrim[0]);
            ampKnob[1] = ampKnob[0];
            ampTrim[1] = ampTrim[0];
		}
        else {
            ampKnob[1] = params[VCA_B_KNOB_PARAM].getValue();
            ampTrim[1] = params[VCA_B_TRIM_PARAM].getValue();
        }
        
        if (scalingMode.needsUpdate()) {
            scalingMode.updateActual();
            setScaleModeLight(this, SCALE_MODE_A_LIGHT, scalingMode.act);
            setScaleModeLight(this, SCALE_MODE_B_LIGHT, scalingMode.act);
        }

        haveOutputs = outputs[A_OUTPUT].isConnected() || outputs[B_OUTPUT].isConnected();

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
            for (int i = 0; i < 2; i++)
            {
                if (outputs[A_OUTPUT + i].isConnected()) {
                    int inpIdx = (i == 0 || !inputs[B_INPUT].isConnected())
                        ? A_INPUT // B normalize to A, if B not connected
                        : B_INPUT;
                    if (inputs[inpIdx].isConnected()) {
                        int ampIdx = (i == 0) || !inputs[VCA_B_INPUT].isConnected()
                            ? VCA_A_INPUT
                            : VCA_B_INPUT;
                        int channels = inputs[inpIdx].getChannels();
                        outputs[A_OUTPUT + i].setChannels(channels);
                        for (int c = 0; c < channels; c++) {
                            float amp = ampKnob[i];
                            if (inputs[ampIdx].isConnected()) {
                                amp += ampTrim[i] * inputs[ampIdx].getPolyVoltage(c) / 10.f;
                                amp = clamp(amp, 0.f, 1.f);
                            }

                            amp = applyScaleCurveUnipolar(amp, scalingMode.act);

                            float voltage = inputs[inpIdx].getVoltage(c) * amp;
                            voltage = clipToVoltRange(voltage, outClipRange.act);
                            outputs[A_OUTPUT + i].setVoltage(voltage, c);
                        }
                    }
                    else {
                        outputs[A_OUTPUT + i].setChannels(1);
                        outputs[A_OUTPUT + i].setVoltage(0.f);
                    }
                }
            }
        }

        cycle256++;
    }
};

struct VCA2ModuleWidget : InfNoiseModuleWidget {
    InfNoiseDisableOverlayGroup* linkBOverlayGroup = nullptr;
    bool bLinked = false;

    VCA2ModuleWidget(VCA2Module *module) {
        initializeWidget(module, "res/VCA2");

        const float cntrCol = 15.f;
        addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(26.284f, 37.802f), module, VCA2Module::SCALE_MODE_A_LIGHT));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrCol, 52.106f), module, VCA2Module::VCA_A_KNOB_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(cntrCol, 79.845f), module, VCA2Module::VCA_A_TRIM_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 107.934f), module, VCA2Module::VCA_A_INPUT));

        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 142.186f), module, VCA2Module::A_INPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 169.694f), module, VCA2Module::A_OUTPUT));

        addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(Vec(4.427f, 199.363f), module, VCA2Module::LINK_MODE_PARAM));
        addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(26.284f, 199.363f), module, VCA2Module::SCALE_MODE_B_LIGHT));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrCol, 215.106f), module, VCA2Module::VCA_B_KNOB_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(cntrCol, 242.845f), module, VCA2Module::VCA_B_TRIM_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 270.934f), module, VCA2Module::VCA_B_INPUT));

        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 305.186f), module, VCA2Module::B_INPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 332.694f), module, VCA2Module::B_OUTPUT));

        InfNoiseDisableOverlayManager& overlayManager = getDisableOverlayManager();
        linkBOverlayGroup = overlayManager.addGroup("B linked to A");
        linkBOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
            VCA2Module::VCA_B_KNOB_PARAM,
            VCA2Module::VCA_B_TRIM_PARAM
        });
    }

    void step() override {
        InfNoiseModuleWidget::step();

        if (!module)
            return;

        auto* m = static_cast<VCA2Module*>(module);
        if (linkBOverlayGroup) {
            if (m->bLinked != bLinked) {
                bLinked = m->bLinked;
                linkBOverlayGroup->setActive(bLinked);
            }
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        VCA2Module* module = dynamic_cast<VCA2Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);
        
        menu->addChild(createIndexPtrSubmenuItem("Scaling-mode", getScaleCurveMenuNames(),
            &module->scalingMode.req));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelVCA2 = createModel<VCA2Module, VCA2ModuleWidget>("VCA2");
