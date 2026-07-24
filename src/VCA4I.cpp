// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct VCA4IModule : InfNoiseModule {
    enum ParamId {
        VCA_KNOB_PARAM,
        VCA_TRIM_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        VCA_INPUT,
        A_INPUT,
        B_INPUT,
        C_INPUT,
        D_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        A_OUTPUT,
        B_OUTPUT,
        C_OUTPUT,
        D_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        ENUMS(SCALE_MODE_LIGHT, 2),
        LIGHTS_LEN
    };

    bool haveOutputs = false;
    int firstIdx = -1;
    int lastIdx = -1;
    actReqValue<scaleCurve> scalingMode = actReqValue<scaleCurve>(sc_linear);

	VCA4IModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configParam(VCA_KNOB_PARAM, 0.0f, 1.0f, 1.0f, "Amplify", "%", 0, 100);
        configParam(VCA_TRIM_PARAM, -1.f, 1.f, 0.f, "Amplify trim (-100% to +100%)", " %", 0, 100);
        configLight(SCALE_MODE_LIGHT, "Scale-mode (unlit=Linear, green=Exp, red=Log)");

        configInput(VCA_INPUT, "VCA");
        configInput(A_INPUT, "A");
        configInput(B_INPUT, "B");
        configInput(C_INPUT, "C");
        configInput(D_INPUT, "D");
        
        configOutput(A_OUTPUT, "A");
        configOutput(B_OUTPUT ,"B");
        configOutput(C_OUTPUT, "C");
        configOutput(D_OUTPUT, "D");

        configBypass(A_INPUT, A_OUTPUT);
        configBypass(B_INPUT, B_OUTPUT);
        configBypass(C_INPUT, C_OUTPUT);
        configBypass(D_INPUT, D_OUTPUT);

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

        if (scalingMode.needsUpdate()) {
            scalingMode.updateActual();
            setScaleModeLight(this, SCALE_MODE_LIGHT, scalingMode.act);
        }

        haveOutputs = false;
        firstIdx = -1;
        lastIdx = -1;
        for (int i = 0; i < 4; i++) {
            if (outputs[A_OUTPUT + i].isConnected()) {
                if (!haveOutputs) {
					haveOutputs = true;
					firstIdx = i;
				}
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
            for (int i = firstIdx; i <= lastIdx; i++) {
                if (outputs[A_OUTPUT + i].isConnected()) {
                    if (inputs[A_INPUT + i].isConnected()) {
                        int channels = inputs[A_INPUT + i].getChannels();
                        outputs[A_OUTPUT + i].setChannels(channels);
                        for (int c = 0; c < channels; c++) {
                            float amp = params[VCA_KNOB_PARAM].getValue();
                            if (inputs[VCA_INPUT].isConnected()) {
                                amp += params[VCA_TRIM_PARAM].getValue() * inputs[VCA_INPUT].getPolyVoltage(c) / 10.f;
                                amp = clamp(amp, 0.f, 1.f);
                            }

                            amp = applyScaleCurveUnipolar(amp, scalingMode.act);

                            float voltage = inputs[A_INPUT + i].getVoltage(c) * amp;
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

struct VCA4IModuleWidget : InfNoiseModuleWidget {
    VCA4IModuleWidget(VCA4IModule *module) {
        initializeWidget(module, "res/VCA4I");

        const float cntrCol = 15.f;
        addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(26.284f, 37.802f), module, VCA4IModule::SCALE_MODE_LIGHT));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrCol, 52.106f), module, VCA4IModule::VCA_KNOB_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(cntrCol, 79.845f), module, VCA4IModule::VCA_TRIM_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 107.934f), module, VCA4IModule::VCA_INPUT));

        const float portSpacing = 24.6323f;
        float row = 155.677f;
        for (int i = 0; i < 4; i++)
        {
			addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, row), module, VCA4IModule::A_INPUT + i));
			row += portSpacing;
		}

        row = 258.797f;
        for (int i = 0; i < 4; i++)
        {
            addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, row), module, VCA4IModule::A_OUTPUT + i));
            row += portSpacing;
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        VCA4IModule* module = dynamic_cast<VCA4IModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexPtrSubmenuItem("Scaling-mode", getScaleCurveMenuNames(),
            &module->scalingMode.req));
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelVCA4I = createModel<VCA4IModule, VCA4IModuleWidget>("VCA4I");