// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct VCA4IIModule : InfNoiseModule {
    enum ParamId {
        VCA_A_KNOB_PARAM,
        VCA_B_KNOB_PARAM,
        VCA_C_KNOB_PARAM,
        VCA_D_KNOB_PARAM,
        VCA_A_TRIM_PARAM,
        VCA_B_TRIM_PARAM,
        VCA_C_TRIM_PARAM,
        VCA_D_TRIM_PARAM,
        B_LINK_PARAM,
        C_LINK_PARAM,
        D_LINK_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        VCA_A_INPUT,
        VCA_B_INPUT,
        VCA_C_INPUT,
        VCA_D_INPUT,
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
        ENUMS(PROCQUAL_LIGHT, 2),
        ENUMS(CLIP_RANGE_LIGHT, 2),
        ENUMS(SCALE_MODE_A_LIGHT, 2), // linear og "exponential"
        ENUMS(SCALE_MODE_B_LIGHT, 2), // linear og "exponential"
        ENUMS(SCALE_MODE_C_LIGHT, 2), // linear og "exponential"
        ENUMS(SCALE_MODE_D_LIGHT, 2), // linear og "exponential"
        LIGHTS_LEN
    };

    actReqValue<scaleCurve> scalingMode[4] = {
        actReqValue<scaleCurve>(sc_linear),
        actReqValue<scaleCurve>(sc_linear),
        actReqValue<scaleCurve>(sc_linear),
        actReqValue<scaleCurve>(sc_linear)
    };
    bool haveOutputs = false;
    int firstIdx = -1;
    int lastIdx = -1;
    bool haveOut[4] = { false, false, false, false };
    bool haveIn[4] = { false, false, false, false };
    bool haveCvIn[4] = { false, false, false, false };
    int outChannels[4] = { 1, 1, 1, 1 };
    int maxCvChannels = 1;
    int processChannels[4] = { 1, 1, 1, 1 };
    float ampKnob[4] = { 1.f, 1.f, 1.f, 1.f };
    float ampTrim[4] = { 0.f, 0.f, 0.f, 0.f };
    /// Set in processParams; used by widget overlays when sections are linked.
    bool linkBToA = false;
    bool linkCToB = false;
    bool linkDToC = false;

	VCA4IIModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        const std::string letter[]{ "A", "B", "C", "D"};
        const int scaleModeLightBase[4]{ SCALE_MODE_A_LIGHT, SCALE_MODE_B_LIGHT, SCALE_MODE_C_LIGHT, SCALE_MODE_D_LIGHT };
        for (int i = 0; i < 4; i++) {
            configParam(VCA_A_KNOB_PARAM + i, 0.0f, 1.0f, 1.0f, letter[i] + "-Amplify", "%", 0, 100);
            configParam(VCA_A_TRIM_PARAM + i, -1.f, 1.f, 0.f, letter[i] + "-Amplify trim (-100% to +100%)", " %", 0, 100);
            configLight(scaleModeLightBase[i], letter[i] + "-Scale-mode (unlit=Linear, green=Exp, red=Log)");

            configOutput(A_OUTPUT + i, letter[i]);

            configBypass(A_INPUT + i, A_OUTPUT + i);
        }

        configInput(VCA_A_INPUT, "A-VCA");
        configInput(VCA_B_INPUT, "B-VCA (normalized to A-VCA)");
        configInput(VCA_C_INPUT, "C-VCA (normalized to B-VCA)");
        configInput(VCA_D_INPUT, "D-VCA (normalized to C-VCA)");

        configInput(A_INPUT, "A");
        configInput(B_INPUT, "B");
        configInput(C_INPUT, "C (normalized to A)");
        configInput(D_INPUT, "D (normalized to B)");
        
        configSwitch(B_LINK_PARAM, 0.0, 1.0, 0.0, "Link-B to A", { "Individual", "Linked B to A" });
        configSwitch(C_LINK_PARAM, 0.0, 1.0, 0.0, "Link-C to B", { "Individual", "Linked C to B" });
        configSwitch(D_LINK_PARAM, 0.0, 1.0, 0.0, "Link-D to C", { "Individual", "Linked D to C" });

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
        
        for (int i = 0; i < 4; i++)
            scalingMode[i].setBoth(sc_linear);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        int scalingModeTmp[4];
        getJsonIntArray(rootJ, "scalingMode", scalingModeTmp, 4, (int)sc_linear);
        for (int i = 0; i < 4; i++)
            scalingMode[i].setBoth((scaleCurve)scalingModeTmp[i]);
    }

    void dataToJson(json_t* rootJ) override {
        int scalingModeTmp[4];
        for (int i = 0; i < 4; i++)
            scalingModeTmp[i] = (int)scalingMode[i].req;
        setJsonIntArray(rootJ, "scalingMode", scalingModeTmp, 4);
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        // Read VCA knob/trim, and handle link-mode for B, C and D
        linkBToA = params[B_LINK_PARAM].getValue() > 0.5f;
        linkCToB = params[C_LINK_PARAM].getValue() > 0.5f;
        linkDToC = params[D_LINK_PARAM].getValue() > 0.5f;
        ampKnob[0] = params[VCA_A_KNOB_PARAM].getValue(); // [0] is not part of the loop
        ampTrim[0] = params[VCA_A_TRIM_PARAM].getValue(); // [0] is not part of the loop
        for (int i = 1; i < 4;  i++) { // 1, 2, 3 (not 0)
            bool linkToPrev = params[B_LINK_PARAM + i - 1].getValue() > 0.5f;
            if (linkToPrev) {
                ampKnob[i] = ampKnob[i-1];
                ampTrim[i] = ampTrim[i-1];
                params[VCA_A_KNOB_PARAM + i].setValue(ampKnob[i - 1]);
                params[VCA_A_TRIM_PARAM + i].setValue(ampTrim[i - 1]);
                scalingMode[i].req = scalingMode[i - 1].req;
            }
            else {
                ampKnob[i] = params[VCA_A_KNOB_PARAM + i].getValue();
                ampTrim[i] = params[VCA_A_TRIM_PARAM + i].getValue();
            }
        }

        // Detect/handle input/output and update lights as needed
        haveOutputs = false;
        firstIdx = -1;
        lastIdx = -1;
        maxCvChannels = 1;
        const int scaleModeLightId[4]{ SCALE_MODE_A_LIGHT, SCALE_MODE_B_LIGHT, SCALE_MODE_C_LIGHT, SCALE_MODE_D_LIGHT };
        for (int i = 0; i < 4; i++) {
            // Detect/handle input/output
            haveCvIn[i] = inputs[VCA_A_INPUT + i].isConnected();
            if (haveCvIn[i])
                maxCvChannels = std::max(maxCvChannels, inputs[VCA_A_INPUT + i].getChannels());

            haveIn[i] = inputs[A_INPUT + i].isConnected();
            int inIdx = i; // Normalize C/D to A/B-input
                    if (i > 1 && !haveIn[i])
                        inIdx -= 2;
            outChannels[i] = (haveIn[inIdx])
                ? std::max(inputs[A_INPUT + inIdx].getChannels(), 1)
                : 1;
            haveOut[i] = outputs[A_OUTPUT + i].isConnected();
            if (haveOut[i]) {
                haveOutputs = true;
                outputs[A_OUTPUT + i].setChannels(outChannels[i]);
            }

            if (haveCvIn[i] || haveIn[i] || haveOut[i]) {
                if (firstIdx < 0)
                    firstIdx = i;
                lastIdx = i;
            }

            // Update scaling mode light as needed
            if (scalingMode[i].needsUpdate()) {
                scalingMode[i].updateActual();
                setScaleModeLight(this, scaleModeLightId[i], scalingMode[i].act);
            }
        }   

        processChannels[0] = std::max(maxCvChannels, outChannels[0]);
        processChannels[1] = std::max(maxCvChannels, outChannels[1]);
        processChannels[2] = std::max(maxCvChannels, outChannels[2]);
        processChannels[3] = std::max(maxCvChannels, outChannels[3]);

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
            float normCvVoltage[PORT_MAX_CHANNELS] = { 0.f };
            for (int i = firstIdx; i <= lastIdx; i++) {
                // Normalize A->C and B->D if C/D not connected
                int inIdx = i;
                if (i > 1 && !haveIn[i])
                    inIdx -= 2;

                for (int c = 0; c < processChannels[i]; c++) {
                    if (haveCvIn[i] && c < maxCvChannels) {
                        normCvVoltage[c] = inputs[VCA_A_INPUT + i].getPolyVoltage(c);
                    }

                    float amp = clamp(ampKnob[i] + ampTrim[i] * normCvVoltage[c] / 10.f, 0.f, 1.f);
                    amp = applyScaleCurveUnipolar(amp, scalingMode[i].act);

                    // Output if applicable
                    if (haveOut[i] && c < outChannels[i]) {
                        float voltage = (haveIn[inIdx]) 
                            ? inputs[A_INPUT + inIdx].getPolyVoltage(c)
                            : 0.f;
                        voltage = clipToVoltRange(voltage * amp, outClipRange.act);
                        outputs[A_OUTPUT + i].setVoltage(voltage, c);
                    }
                }
            }
        }

        cycle256++;
    }
};

struct VCA4IIModuleWidget : InfNoiseModuleWidget {
    InfNoiseDisableOverlayGroup* linkBOverlayGroup = nullptr;
    InfNoiseDisableOverlayGroup* linkCOverlayGroup = nullptr;
    InfNoiseDisableOverlayGroup* linkDOverlayGroup = nullptr;
    bool linkBToA = false;
    bool linkCToB = false;
    bool linkDToC = false;

    VCA4IIModuleWidget(VCA4IIModule *module) {
        initializeWidget(module, "res/VCA4II");

        // A-Section
        const float leftCol = 15.f;
        addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(26.030f, 36.726f), module, VCA4IIModule::SCALE_MODE_A_LIGHT));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(leftCol, 52.106f), module, VCA4IIModule::VCA_A_KNOB_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(leftCol, 79.845f), module, VCA4IIModule::VCA_A_TRIM_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftCol, 107.934f), module, VCA4IIModule::VCA_A_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftCol, 142.186f), module, VCA4IIModule::A_INPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftCol, 169.694f), module, VCA4IIModule::A_OUTPUT));

        // B-Section
        const float rightCol = 45.f;
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(Vec(34.358f, 36.726f), module, VCA4IIModule::B_LINK_PARAM));
        addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(56.030f, 36.726f), module, VCA4IIModule::SCALE_MODE_B_LIGHT));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(rightCol, 52.106f), module, VCA4IIModule::VCA_B_KNOB_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(rightCol, 79.845f), module, VCA4IIModule::VCA_B_TRIM_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(rightCol, 107.934f), module, VCA4IIModule::VCA_B_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(rightCol, 142.186f), module, VCA4IIModule::B_INPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightCol, 169.694f), module, VCA4IIModule::B_OUTPUT));

        // C-Section
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(Vec(4.358f, 199.284f), module, VCA4IIModule::C_LINK_PARAM));
        addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(26.030f, 199.284f), module, VCA4IIModule::SCALE_MODE_C_LIGHT));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(leftCol, 215.106f), module, VCA4IIModule::VCA_C_KNOB_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(leftCol, 242.845f), module, VCA4IIModule::VCA_C_TRIM_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftCol, 270.934f), module, VCA4IIModule::VCA_C_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftCol, 305.186f), module, VCA4IIModule::C_INPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftCol, 332.694f), module, VCA4IIModule::C_OUTPUT));

        // D-Section
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green>>(Vec(34.358f, 199.284f), module, VCA4IIModule::D_LINK_PARAM));
        addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(56.030f, 199.284f), module, VCA4IIModule::SCALE_MODE_D_LIGHT));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(rightCol, 215.106f), module, VCA4IIModule::VCA_D_KNOB_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(rightCol, 242.845f), module, VCA4IIModule::VCA_D_TRIM_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(rightCol, 270.934f), module, VCA4IIModule::VCA_D_INPUT));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(rightCol, 305.186f), module, VCA4IIModule::D_INPUT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightCol, 332.694f), module, VCA4IIModule::D_OUTPUT));

        InfNoiseDisableOverlayManager& overlayManager = getDisableOverlayManager();
        linkBOverlayGroup = overlayManager.addGroup("B linked to A");
        linkBOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
            VCA4IIModule::VCA_B_KNOB_PARAM,
            VCA4IIModule::VCA_B_TRIM_PARAM
        });
        linkCOverlayGroup = overlayManager.addGroup("C linked to B");
        linkCOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
            VCA4IIModule::VCA_C_KNOB_PARAM,
            VCA4IIModule::VCA_C_TRIM_PARAM
        });
        linkDOverlayGroup = overlayManager.addGroup("D linked to C");
        linkDOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
            VCA4IIModule::VCA_D_KNOB_PARAM,
            VCA4IIModule::VCA_D_TRIM_PARAM
        });
    }

    void step() override {
        InfNoiseModuleWidget::step();

        if (!module)
            return;

        auto* m = static_cast<VCA4IIModule*>(module);
        if (linkBOverlayGroup) {
            if (m->linkBToA != linkBToA) {
                linkBToA = m->linkBToA;
                linkBOverlayGroup->setActive(linkBToA);
            }
        }
        if (linkCOverlayGroup) {
            if (m->linkCToB != linkCToB) {
                linkCToB = m->linkCToB;
                linkCOverlayGroup->setActive(linkCToB);
            }
        }
        if (linkDOverlayGroup) {
            if (m->linkDToC != linkDToC) {
                linkDToC = m->linkDToC;
                linkDOverlayGroup->setActive(linkDToC);
            }
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        VCA4IIModule* module = dynamic_cast<VCA4IIModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexPtrSubmenuItem("A-Scaling-mode", getScaleCurveMenuNames(),
            &module->scalingMode[0].req));
        menu->addChild(createIndexPtrSubmenuItem("B-Scaling-mode", getScaleCurveMenuNames(),
            &module->scalingMode[1].req));
        menu->addChild(createIndexPtrSubmenuItem("C-Scaling-mode", getScaleCurveMenuNames(),
            &module->scalingMode[2].req));
        menu->addChild(createIndexPtrSubmenuItem("D-Scaling-mode", getScaleCurveMenuNames(),
            &module->scalingMode[3].req));
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelVCA4II = createModel<VCA4IIModule, VCA4IIModuleWidget>("VCA4II");