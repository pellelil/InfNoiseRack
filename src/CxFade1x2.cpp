// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct CxFade1x2Module : InfNoiseModule {
    enum ParamId {
        CROSSFADE_PARAM,
        CROSSFADE_TRIM_PARAM,
        CROSSFADE_TOGGLE_PARAM,
        CROSSFADE_TRIG_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        CROSSFADE_INPUT,
        A1_INPUT,
        B1_INPUT,
        A2_INPUT,
        B2_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        LEFT1_OUTPUT,
        RIGHT2_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        LIGHTS_LEN
    };
    
    enum fadeKnobMode { fm_log, fm_linear, fm_exp  };
    actReqValue<fadeKnobMode> fadeMode = actReqValue<fadeKnobMode>(fm_linear);
    int channels[2] = { 1, 1 };  // [0] = left, [1] = right
    dsp::SchmittTrigger toggleTrig;

    CxFade1x2Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configParam(CROSSFADE_PARAM, -1.f, 1.f, 0.0f, "Cross-fade (A to B)", " %", 0, 100);
        configParam(CROSSFADE_TRIM_PARAM, -1.f, 1.f, 0.f, "Cross-fade CV trim", " %", 0, 100);
        configSwitch(CROSSFADE_TOGGLE_PARAM, 0.0f, 1.0f, 0.0f, "Manual A/B-toggle");

        configInput(CROSSFADE_INPUT, "Cross-fade CV/Switch-Trigger");
        configSwitch(CROSSFADE_TRIG_PARAM, 0.0f, 1.0f, 0.0f, "Cross-fade CV/Switch-trigger", { "CV-Mode", "Trigger-mode" });

        configInput(A1_INPUT, "A1");
        configInput(B1_INPUT, "B1");
        configInput(A2_INPUT, "A2 (normalized to B1)");
        configInput(B2_INPUT, "B2 (normalized to A1)");

        configOutput(LEFT1_OUTPUT, "A1/B1");
        configOutput(RIGHT2_OUTPUT, "A2/B2");

        configBypass(A1_INPUT, LEFT1_OUTPUT);
        configBypass(A2_INPUT, RIGHT2_OUTPUT);

        // Set InfNoise features (e.g. menu-items) 
		haveProcQuality = true;
        haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = true;
        haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = true;
		haveTrigHighLow = false;
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        fadeMode.setBoth(fadeKnobMode::fm_linear);
        toggleTrig.reset();
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        fadeMode.setBoth((fadeKnobMode)getJsonInt(rootJ, "fadeMode", (int)fadeKnobMode::fm_linear));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "fadeMode", json_integer((int)fadeMode.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        fadeMode.updateActual();

        // 1(Left)-channels count
        int channelsA1 = inputs[A1_INPUT].isConnected()
            ? std::max(inputs[A1_INPUT].getChannels(), 1)
            : 1;
        int channelsB1 = inputs[B1_INPUT].isConnected()
            ? std::max(inputs[B1_INPUT].getChannels(), 1)
            : 1;
        channels[0] = std::max(channelsA1, channelsB1);
        outputs[LEFT1_OUTPUT].setChannels(channels[0]);

        // 2(Right)-channels count (normalized to 1-channels)
        int channelsA2 = inputs[A2_INPUT].isConnected()
            ? std::max(inputs[A2_INPUT].getChannels(), 1)
            : channelsB1;
        int channelsB2 = inputs[B2_INPUT].isConnected()
            ? std::max(inputs[B2_INPUT].getChannels(), 1)
            : channelsA1;
        channels[1] = std::max(channelsA2, channelsB2);
        outputs[RIGHT2_OUTPUT].setChannels(channels[1]);

        // process Toggle A/B
        if (!inputs[CROSSFADE_INPUT].isConnected()) {
            if (toggleTrig.process(params[CROSSFADE_TOGGLE_PARAM].getValue())) {
                params[CROSSFADE_PARAM].setValue(params[CROSSFADE_PARAM].getValue() >= 0.f ? -1.f : 1.f);
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

        if (doProcess && (outputs[LEFT1_OUTPUT].isConnected() ||
            outputs[RIGHT2_OUTPUT].isConnected())) {
            float a1Input[PORT_MAX_CHANNELS] = { 0.f };
            float b1Input[PORT_MAX_CHANNELS] = { 0.f };
            float a2Input[PORT_MAX_CHANNELS] = { 0.f };
            float b2Input[PORT_MAX_CHANNELS] = { 0.f };

            // Grab 1-inputs (used to normalize 2-inputs)
            for (int c = 0; c < channels[0]; c++) {
                a1Input[c] = (inputs[A1_INPUT].isConnected())
                    ? inputs[A1_INPUT].getPolyVoltage(c)
                    : 0.f;
                b1Input[c] = (inputs[B1_INPUT].isConnected())
                    ? inputs[B1_INPUT].getPolyVoltage(c)
                    : 0.f;
            }

            // Grab 2-inputs, or use 1-inputs
            if (outputs[RIGHT2_OUTPUT].isConnected()) {
                for (int c = 0; c < channels[1]; c++) {
                    a2Input[c] = (inputs[A2_INPUT].isConnected())
                        ? inputs[A2_INPUT].getPolyVoltage(c)
                        : b1Input[c];
                    b2Input[c] = (inputs[B2_INPUT].isConnected())
                        ? inputs[B2_INPUT].getPolyVoltage(c)
                        : a1Input[c];
                }
            }

            // Calc cross-fade
            float crossFade = params[CROSSFADE_PARAM].getValue();
            if (inputs[CROSSFADE_INPUT].isConnected())
            {
                if (params[CROSSFADE_TRIG_PARAM].getValue() < 0.5) { // CV-mode
                    crossFade += inputs[CROSSFADE_INPUT].getVoltage() / 5.f * params[CROSSFADE_TRIM_PARAM].getValue();
                    crossFade = clamp(crossFade, -1.f, 1.f);
                }
    			else { // Trigger-mode
                    if (toggleTrig.process(inputs[CROSSFADE_INPUT].getVoltage(),
                        trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act])) {
                        crossFade = crossFade >= 0.f ? -1.f : 1.f;
                        params[CROSSFADE_PARAM].setValue(crossFade);
                    }
                }
            }
        
            if (fadeMode.act == fm_exp) {
                float sign = (crossFade < 0.f) ? -1.f : 1.f;
                float flipped = 1.f - std::abs(crossFade);
                crossFade = (1.f - (flipped * flipped)) * sign;
            }
            else if (fadeMode.act == fm_log) {
                float sign = (crossFade < 0.f) ? -1.f : 1.f;
                crossFade = crossFade * crossFade * sign;
            }
            
            crossFade = (crossFade + 1.f) / 2.f;  // Normalize to 0-1
            float revCrossFade = 1.f - crossFade;

            // Cross-fade between A1/B1 (Left)
            if (outputs[LEFT1_OUTPUT].isConnected()) {
                for (int c = 0; c < channels[0]; c++) {
					float out1 = revCrossFade * a1Input[c] + crossFade * b1Input[c];
                    out1 = clipToVoltRange(out1, outClipRange.act);
					outputs[LEFT1_OUTPUT].setVoltage(out1, c);
				}
            }

            // Cross-fade between A2/B2 (Right)
            if (outputs[RIGHT2_OUTPUT].isConnected()) {
                for (int c = 0; c < channels[1]; c++) {
                    float out2 = revCrossFade * a2Input[c] + crossFade * b2Input[c];
                    out2 = clipToVoltRange(out2, outClipRange.act);
                    outputs[RIGHT2_OUTPUT].setVoltage(out2, c);
                }
            }
        }

        cycle256++;
    }
};

struct CxFade1x2ModuleWidget : InfNoiseModuleWidget {
    CxFade1x2ModuleWidget(CxFade1x2Module *module) {
        initializeWidget(module, "res/CxFade1x2");

        float centerCol = 15.f;
        // Cross-fade knob, trim-pot and CV-input
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, 47.843f), module, CxFade1x2Module::CROSSFADE_PARAM));
        infNoiseLtSmallButton* toggleBtn = createParamCentered<infNoiseLtSmallButton>(Vec(25.287f, 36.287f), module, CxFade1x2Module::CROSSFADE_TOGGLE_PARAM);
        toggleBtn->setup(bc_green, true);
        addParam(toggleBtn);
        addParam(createParamCentered<Trimpot>(Vec(centerCol, 75.792f), module, CxFade1x2Module::CROSSFADE_TRIM_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(centerCol, 104.365f), module, CxFade1x2Module::CROSSFADE_INPUT));
        infNoiseLtSmallButton* trigBtn = createParamCentered<infNoiseLtSmallButton>(Vec(25.287f, 92.307f), module, CxFade1x2Module::CROSSFADE_TRIG_PARAM);
        trigBtn->setup(bc_red, false);
        addParam(trigBtn);

        // 1(Left) Inputs/output  
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 151.000f), module, CxFade1x2Module::A1_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 182.739f), module, CxFade1x2Module::B1_INPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 218.013f), module, CxFade1x2Module::LEFT1_OUTPUT));

        // 2(Right) Inputs/output
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 265.881f), module, CxFade1x2Module::A2_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 297.620f), module, CxFade1x2Module::B2_INPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, 332.895f), module, CxFade1x2Module::RIGHT2_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        CxFade1x2Module* module = dynamic_cast<CxFade1x2Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

		menu->addChild(createIndexPtrSubmenuItem("Fade-mode (scaling of fade-knob/input)",
		 	{"Logarithmic", "Linear", "Exponential"},
		 	&module->fadeMode.req
        ));
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelCxFade1x2 = createModel<CxFade1x2Module, CxFade1x2ModuleWidget>("CxFade1x2");