// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct CxFade4x1Module : InfNoiseModule {
    enum ParamId {
        CROSSFADE_M_PARAM,
        CROSSFADE_1_PARAM,
        CROSSFADE_2_PARAM,
        CROSSFADE_3_PARAM,
        CROSSFADE_4_PARAM,
        CROSSFADE_M_TRIM_PARAM,
        CROSSFADE_1_TRIM_PARAM,
        CROSSFADE_2_TRIM_PARAM,
        CROSSFADE_3_TRIM_PARAM,
        CROSSFADE_4_TRIM_PARAM,
        CROSSFADE_M_TOGGLE_PARAM,
        CROSSFADE_1_TOGGLE_PARAM,
        CROSSFADE_2_TOGGLE_PARAM,
        CROSSFADE_3_TOGGLE_PARAM,
        CROSSFADE_4_TOGGLE_PARAM,
        CROSSFADE_M_TRIG_PARAM,
        CROSSFADE_1_TRIG_PARAM,
        CROSSFADE_2_TRIG_PARAM,
        CROSSFADE_3_TRIG_PARAM,
        CROSSFADE_4_TRIG_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        CROSSFADE_M_INPUT,
        CROSSFADE_1_INPUT,
        CROSSFADE_2_INPUT,
        CROSSFADE_3_INPUT,
        CROSSFADE_4_INPUT,
        A1_INPUT,
        A2_INPUT,
        A3_INPUT,
        A4_INPUT,
        B1_INPUT,
        B2_INPUT,
        B3_INPUT,
        B4_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        XFD_1_OUTPUT,
        XFD_2_OUTPUT,
        XFD_3_OUTPUT,
        XFD_4_OUTPUT,
        FLP_1_OUTPUT,
        FLP_2_OUTPUT,
        FLP_3_OUTPUT,
        FLP_4_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        LIGHTS_LEN
    };

    enum fadeKnobMode { fm_log, fm_linear, fm_exp };
    actReqValue<fadeKnobMode> fadeMode = actReqValue<fadeKnobMode>(fm_linear);
    bool haveOutput[4] = { false, false, false, false };
    bool haveAnyOutput = false;
    int channels[4] = { 1, 1, 1, 1 };
    dsp::SchmittTrigger masterToggleTrig;
    dsp::SchmittTrigger sectionToggleTrig[4];
    
    CxFade4x1Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        for (int i = 0; i < 5; i++) {
            // Cross-fade knobs, trim-pots and CV-inputs (upper half of the module)
            std::string name = (i == 0) 
                ? "Master" 
                : string::f("%d", i);
            configParam(CROSSFADE_M_PARAM + i, -1.f, 1.f, 0.f, name + "-Cross-fade ", " %", 0, 100);
            configParam(CROSSFADE_M_TRIM_PARAM + i, -1.f, 1.f, 0.f, name + "-Cross-fade CV Trim-", " %", 0, 100);
            configInput(CROSSFADE_M_INPUT + i, name + "-Cross-fade CV/Switch-Trigger");
            configSwitch(CROSSFADE_M_TOGGLE_PARAM + i, 0.0f, 1.0f, 0.0f, name + "-manual A/B-toggle");
            configSwitch(CROSSFADE_M_TRIG_PARAM + i, 0.0f, 1.0f, 0.0f, name + "-Cross-fade CV/Switch-trigger", {"CV-Mode", "Trigger-mode"});

            // Inputs and outputs (lower half of the module)
            if (i < 4) {
                configInput(A1_INPUT + i, string::f("%dA", i + 1));
                configInput(B1_INPUT + i, string::f("%dB", i + 1));

                configOutput(XFD_1_OUTPUT + i, string::f("Cross-faded signal-%d", i + 1));
                configOutput(FLP_1_OUTPUT + i, string::f("Flipped cross-faded signal-%d", i + 1));
            }
        }

        configBypass(A1_INPUT, XFD_1_OUTPUT);
        configBypass(A2_INPUT, XFD_2_OUTPUT);
        configBypass(A3_INPUT, XFD_3_OUTPUT);
        configBypass(A4_INPUT, XFD_4_OUTPUT);

        configBypass(B1_INPUT, FLP_1_OUTPUT);
        configBypass(B2_INPUT, FLP_2_OUTPUT);
        configBypass(B3_INPUT, FLP_3_OUTPUT);
        configBypass(B4_INPUT, FLP_4_OUTPUT);

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
        masterToggleTrig.reset();
        for (int i = 0; i < 4; i++)
			sectionToggleTrig[i].reset();
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);

        fadeMode.setBoth((fadeKnobMode)getJsonInt(rootJ, "fadeMode", (int)fadeKnobMode::fm_linear));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "fadeMode", json_integer((int)fadeMode.req));
    }

    void processParams(const ProcessArgs& args)
    {
        preProcessParams(args);
        //--------------------

        fadeMode.updateActual();

        haveOutput[0] = outputs[XFD_1_OUTPUT].isConnected() || outputs[FLP_1_OUTPUT].isConnected();
        haveOutput[1] = outputs[XFD_2_OUTPUT].isConnected() || outputs[FLP_2_OUTPUT].isConnected();
        haveOutput[2] = outputs[XFD_3_OUTPUT].isConnected() || outputs[FLP_3_OUTPUT].isConnected();
        haveOutput[3] = outputs[XFD_4_OUTPUT].isConnected() || outputs[FLP_4_OUTPUT].isConnected();
        haveAnyOutput = haveOutput[0] || haveOutput[1] || haveOutput[2] || haveOutput[3];

        // process Master-toggle A/B
        if (!inputs[CROSSFADE_M_INPUT].isConnected()) {
            if (masterToggleTrig.process(params[CROSSFADE_M_TOGGLE_PARAM].getValue())) {
                params[CROSSFADE_M_PARAM].setValue(params[CROSSFADE_M_PARAM].getValue() >= 0.f ? -1.f : 1.f);
            }
        }

        for (int i = 0; i < 4; i++) {
            if (haveOutput[i]) {
                int aChannels = inputs[A1_INPUT + i].isConnected()
                    ? std::max(inputs[A1_INPUT + i].getChannels(), 1)
                    : 1;
                int bChannels = inputs[B1_INPUT + i].isConnected()
                    ? std::max(inputs[B1_INPUT + i].getChannels(), 1)
                    : 1;
                channels[i] = std::max(aChannels, bChannels);

                outputs[XFD_1_OUTPUT + i].setChannels(channels[i]);
                outputs[FLP_1_OUTPUT + i].setChannels(channels[i]);
            }

            // process Section-toggle A/B
            if (!inputs[CROSSFADE_1_INPUT + i].isConnected()) {
                if (sectionToggleTrig[i].process(params[CROSSFADE_1_TOGGLE_PARAM + i].getValue())) {
                    params[CROSSFADE_1_PARAM + i].setValue(params[CROSSFADE_1_PARAM + i].getValue() >= 0.f ? -1.f : 1.f);
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

        if (doProcess && haveAnyOutput) {
            // Calc master cross-fade
            float masterFade = params[CROSSFADE_M_PARAM].getValue();
            if (inputs[CROSSFADE_M_INPUT].isConnected()) {
                if (params[CROSSFADE_M_TRIG_PARAM].getValue() < 0.5) { // CV-mode
                    masterFade += inputs[CROSSFADE_M_INPUT].getVoltage() / 5.f * params[CROSSFADE_M_TRIM_PARAM].getValue();
                    masterFade = clamp(masterFade, -1.f, 1.f);
                }
                else {  // Trigger-mode
                    if (masterToggleTrig.process(inputs[CROSSFADE_M_INPUT].getVoltage(),
                        trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act])) {
                        masterFade = masterFade >= 0.f ? -1.f : 1.f;
                        params[CROSSFADE_M_PARAM].setValue(masterFade);
					}
				}
            }

            for (int i = 0; i < 4; i++) {
                if (haveOutput[i]) {
                    // Calc section cross-fade
                    float crossFade = params[CROSSFADE_1_PARAM + i].getValue();
                    if (inputs[CROSSFADE_1_INPUT + i].isConnected()) {
                        if (params[CROSSFADE_1_TRIG_PARAM + i].getValue() < 0.5) {  // CV-mode
                            crossFade += inputs[CROSSFADE_1_INPUT + i].getVoltage() / 5.f * params[CROSSFADE_1_TRIM_PARAM + i].getValue();
                            crossFade += masterFade;  // Add master-fade
                            crossFade = clamp(crossFade, -1.f, 1.f);
                        }
                        else { // Trigger-mode
                            if (sectionToggleTrig[i].process(inputs[CROSSFADE_1_INPUT + i].getVoltage(),
                                trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act])) {
								crossFade = crossFade >= 0.f ? -1.f : 1.f;
                                params[CROSSFADE_1_PARAM + i].setValue(crossFade);
							}
                        }
                    }
                    else {
                        crossFade += masterFade;  // Add master-fade
                        crossFade = clamp(crossFade, -1.f, 1.f);
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

                    // Calc- and output cross-fade and flip
                    for (int c = 0; c < channels[i]; c++) {
                        float a = inputs[A1_INPUT + i].isConnected()
                            ? inputs[A1_INPUT + i].getPolyVoltage(c)
                            : 0.f;
                        float b = inputs[B1_INPUT + i].isConnected()
                            ? inputs[B1_INPUT + i].getPolyVoltage(c)
                            : 0.f;

                        if (outputs[XFD_1_OUTPUT + i].isConnected()) {
                            float xFade = revCrossFade * a + crossFade * b;
                            xFade = clipToVoltRange(xFade, outClipRange.act);
                            outputs[XFD_1_OUTPUT + i].setVoltage(xFade, c);
                        }
                        if (outputs[FLP_1_OUTPUT + i].isConnected()) {
                            float flp = crossFade * a + revCrossFade * b;
                            flp = clipToVoltRange(flp, outClipRange.act);
                            outputs[FLP_1_OUTPUT + i].setVoltage(flp, c);
                        }
					}
				}
			}
        }

        cycle256++;
    }
};

struct CxFade4x1ModuleWidget : InfNoiseModuleWidget {
    CxFade4x1ModuleWidget( CxFade4x1Module *module) {
        initializeWidget(module, "res/CxFade4x1");

        const float fadeKnobCol = 18.127f;
        const float trimKnobCol = 60.731f;
        const float fadeCvCol = 100.848f;
        const float manToggleBtnCol = 33.175f;
        const float cvTrigToggleBtnCol = 89.730f;
        const float toggleBtnOfs = -10.976f;
        const float fadeKnobSpacing = 35.0735f;
        float fadeKnobRow = 50.679f;

        const float aInpCol = 15.641f;
        const float bInpCol = 44.043f;
        const float xFdCol = 72.446f;
        const float flpCol = 100.848f;
        const float inpOutRowSpacing = 35.0736f;
        float inpOutRow = 227.473f;

        for (int i = 0; i < 5; i++) {
            // Cross-fade knob, trim-pot and CV-input
            addParam(createParamCentered<RoundBlackKnob>(Vec(fadeKnobCol, fadeKnobRow), module, CxFade4x1Module::CROSSFADE_M_PARAM + i));
            addParam(createParamCentered<Trimpot>(Vec(trimKnobCol, fadeKnobRow), module, CxFade4x1Module::CROSSFADE_M_TRIM_PARAM + i));
            addInput(createInputCentered<ThemedPJ301MPort>(Vec(fadeCvCol, fadeKnobRow), module, CxFade4x1Module::CROSSFADE_M_INPUT + i));

            addParam(createParamCentered<infNoiseLtSmallButton<bc_green, true>>(
                Vec(manToggleBtnCol, fadeKnobRow + toggleBtnOfs),
                module, CxFade4x1Module::CROSSFADE_M_TOGGLE_PARAM + i));

            addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(
                Vec(cvTrigToggleBtnCol, fadeKnobRow + toggleBtnOfs),
                module, CxFade4x1Module::CROSSFADE_M_TRIG_PARAM + i));

            fadeKnobRow += fadeKnobSpacing;

            // Inputs and outputs
            if (i < 4) {
                addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(aInpCol, inpOutRow), module, CxFade4x1Module::A1_INPUT + i));
                addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(bInpCol, inpOutRow), module, CxFade4x1Module::B1_INPUT + i));
                addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(xFdCol, inpOutRow), module, CxFade4x1Module::XFD_1_OUTPUT + i));
                addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(flpCol, inpOutRow), module, CxFade4x1Module::FLP_1_OUTPUT + i));
                inpOutRow += inpOutRowSpacing;
            }
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        CxFade4x1Module* module = dynamic_cast<CxFade4x1Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexPtrSubmenuItem("Fade-mode (scaling of fade-knob/input)",
            { "Logarithmic", "Linear", "Exponential" },
            &module->fadeMode.req
        ));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelCxFade4x1 = createModel< CxFade4x1Module,  CxFade4x1ModuleWidget>("CxFade4x1");