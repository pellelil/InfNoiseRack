// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

struct CvToggle8Module : InfNoiseModule {
    enum ParamId {
        MANUAL_PARAM,
        MANUAL_LATCH_PARAM,
        ON_PARAM,
        OFF_PARAM,
        GATE_TRIG1_PARAM,
        GATE_TRIG2_PARAM,
        GATE_TRIG3_PARAM,
        GATE_TRIG4_PARAM,
        GATE_TRIG5_PARAM,
        GATE_TRIG6_PARAM,
        GATE_TRIG7_PARAM,
        GATE_TRIG8_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        ATTENUVERT_INPUT,
        GATE_TRIG1_INPUT,
        GATE_TRIG2_INPUT,
        GATE_TRIG3_INPUT,
        GATE_TRIG4_INPUT,
        GATE_TRIG5_INPUT,
        GATE_TRIG6_INPUT,
        GATE_TRIG7_INPUT,
        GATE_TRIG8_INPUT,
        ON1_INPUT,
        ON2_INPUT,
        ON3_INPUT,
        ON4_INPUT,
        ON5_INPUT,
        ON6_INPUT,
        ON7_INPUT,
        ON8_INPUT,
        OFF1_INPUT,
        OFF2_INPUT,
        OFF3_INPUT,
        OFF4_INPUT,
        OFF5_INPUT,
        OFF6_INPUT,
        OFF7_INPUT,
        OFF8_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        CV1_OUTPUT,
        CV2_OUTPUT,
        CV3_OUTPUT,
        CV4_OUTPUT,
        CV5_OUTPUT,
        CV6_OUTPUT,
        CV7_OUTPUT,
        CV8_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        ENUMS(ON_OFF1_LIGHT, 2), // Green/Red-light hence 2
        ENUMS(ON_OFF2_LIGHT, 2),
        ENUMS(ON_OFF3_LIGHT, 2),
        ENUMS(ON_OFF4_LIGHT, 2),
        ENUMS(ON_OFF5_LIGHT, 2),
        ENUMS(ON_OFF6_LIGHT, 2),
        ENUMS(ON_OFF7_LIGHT, 2),
        ENUMS(ON_OFF8_LIGHT, 2),
        LIGHTS_LEN
    };

    bool outputsInUse = false;
    int firstIdx = -1;
    int lastIdx = -1;
    int maxChannels = 16;
    int outChannels[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
    dsp::TSchmittTrigger<float> toggleTrigger[8];
    bool virtualGate[8] = { false };
    enum attenuateMode { am_both, am_onlyOn, am_onlyOff };
    actReqValue<attenuateMode> attMode = actReqValue<attenuateMode>(am_both);
    
    CvToggle8Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configSwitch(MANUAL_PARAM, 0.0f, 1.0f, 0.0f, "Manual-gate/trigger", { "Off", "On" });
        configSwitch(MANUAL_LATCH_PARAM, 0.0f, 1.0f, 0.0f, "Latch Manual-gate/trigger", { "Unlatched", "Latched" });
        configParam(ON_PARAM, -10.0f, 10.0f, 0.0f, "ON-CV", " V");
        configParam(OFF_PARAM, -10.0f, 10.0f, 0.0f, "OFF-CV", " V");
        configInput(ATTENUVERT_INPUT, "Attenuvert-CV");

        for (int i = 0; i < 8; i++) {
            std::string normalizedTrig = (i > 0) ? " (normalized to previous)" : " (normalized to button)";
            configInput(GATE_TRIG1_INPUT + i, string::f("ON/OFF-%d gate/trigger", i + 1) + normalizedTrig);
            configSwitch(GATE_TRIG1_PARAM + i, 0.0f, 1.0f, 1.0f, string::f("Gate/Trigger-%d", i + 1), 
                { "Trigger when red", "Gate when green" });
            std::string normalizedKnob = (i > 0) ? " (normalized to previous)" : " (normalized to knob)";
			configInput(ON1_INPUT + i, string::f("ON-%d", i + 1) + normalizedKnob);
			configInput(OFF1_INPUT + i, string::f("OFF-%d", i + 1) + normalizedKnob);
			configOutput(CV1_OUTPUT + i, string::f("CV-%d", i + 1));
            configLight(ON_OFF1_LIGHT + i * 2, // Green/Red-light, hence 2
                string::f("Green when outputting ON(%d), Red when outputting OFF(%d)", i + 1, i + 1));
        }

        configBypass(ON1_INPUT, CV1_OUTPUT);
        configBypass(ON2_INPUT, CV2_OUTPUT);
        configBypass(ON3_INPUT, CV3_OUTPUT);
        configBypass(ON4_INPUT, CV4_OUTPUT);
        configBypass(ON5_INPUT, CV5_OUTPUT);
        configBypass(ON6_INPUT, CV6_OUTPUT);
        configBypass(ON7_INPUT, CV7_OUTPUT);
        configBypass(ON8_INPUT, CV8_OUTPUT);

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
        haveAutoProcQuality = false;
        haveOutQuantize = true;
        haveOutClipRange = true;
        haveGateDetect = true;
		haveGateHighLow = false;
		haveTrigDetect = true;
		haveTrigHighLow = false;
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        
        attMode.setBoth(am_both);
        for (int i = 0; i < 8; i++) {
			virtualGate[i] = false;
            toggleTrigger[i].reset();
		}
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        attMode.setBoth((attenuateMode)getJsonInt(rootJ, "attMode", (int)attenuateMode::am_both));
        getJsonBoolArray(rootJ, "virtualGate", virtualGate, 8, false);
        for (int i = 0; i < 8; i++)
            toggleTrigger[i].reset();
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "attMode", json_integer((int)attMode.req));
        setJsonBoolArray(rootJ, "virtualGate", virtualGate, 8);
    }

    void updateLaneOutputs(int i) {
        if (!outputs[CV1_OUTPUT + i].isConnected())
            return;

        lights[ON_OFF1_LIGHT + i * 2].setBrightness(virtualGate[i] ? 1.f : 0.f);
        lights[ON_OFF1_LIGHT + i * 2 + 1].setBrightness(virtualGate[i] ? 0.f : 1.f);

        float att = (inputs[ATTENUVERT_INPUT].isConnected())
            ? inputs[ATTENUVERT_INPUT].getPolyVoltage(1) / 5.f
            : 1.f;
        float attOn = (attMode.act == am_onlyOff) ? 1.f : att;
        float attOff = (attMode.act == am_onlyOn) ? 1.f : att;

        float normOn[PORT_MAX_CHANNELS] = { 0 };
        float normOff[PORT_MAX_CHANNELS] = { 0 };
        for (int c = 0; c < maxChannels; c++) {
            normOn[c] = params[ON_PARAM].getValue() * attOn;
            normOff[c] = params[OFF_PARAM].getValue() * attOff;
        }

        for (int c = 0; c < outChannels[i]; c++) {
            if (c > 0 && inputs[ATTENUVERT_INPUT].isConnected()) {
                att = inputs[ATTENUVERT_INPUT].getPolyVoltage(c) / 5.f;
                attOn = (attMode.act == am_onlyOff) ? 1.f : att;
                attOff = (attMode.act == am_onlyOn) ? 1.f : att;
            }

            if (inputs[ON1_INPUT + i].isConnected())
                normOn[c] = inputs[ON1_INPUT + i].getPolyVoltage(c) * attOn;
            if (inputs[OFF1_INPUT + i].isConnected())
                normOff[c] = inputs[OFF1_INPUT + i].getPolyVoltage(c) * attOff;

            float voltage = virtualGate[i] ? normOn[c] : normOff[c];
            voltage = quantizeToMode(voltage, outQuantize.act);
            voltage = clipToVoltRange(voltage, outClipRange.act);
            outputs[CV1_OUTPUT + i].setVoltage(voltage, c);
        }
    }

    void applyLoadedOutputs() {
        for (int i = 0; i < 8; i++)
            toggleTrigger[i].reset();
        if (firstIdx < 0)
            return;
        for (int i = firstIdx; i <= lastIdx; i++)
            updateLaneOutputs(i);
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        attMode.updateActual();

        outputsInUse = false;
        firstIdx = -1;
        lastIdx = -1;
        maxChannels = 1;
        int onChannels = 1;
        int offChannels = 1;
        for (int i = 0; i < 8; i++) {
            bool haveGate = inputs[GATE_TRIG1_INPUT + i].isConnected();  // have gate/trigger input
            bool haveOn = inputs[ON1_INPUT + i].isConnected();  // have ON-input
            bool haveOff = inputs[OFF1_INPUT + i].isConnected();  // have OFF-input
            bool haveOut = outputs[CV1_OUTPUT + i].isConnected(); // have CV-output
            if (haveGate || haveOn || haveOff || haveOut) {
                if (firstIdx < 0)
                    firstIdx = i;
                lastIdx = i;
            }

            if (haveOn || haveOff) {
                if (haveOn)
                    onChannels = std::max(inputs[ON1_INPUT + i].getChannels(), 1);
                if (haveOff)
                    offChannels = std::max(inputs[OFF1_INPUT + i].getChannels(), 1);
                outChannels[i] = std::max(onChannels, offChannels);
            }
            else
                outChannels[i] = (i > 0) ? outChannels[i - 1] : 1;
                        
            maxChannels = std::max(maxChannels, outChannels[i]);

            if (haveOut) {
                outputsInUse = true;
                outputs[CV1_OUTPUT + i].setChannels(outChannels[i]);
            } else {
                lights[ON_OFF1_LIGHT + i  * 2].setBrightness(0.f);  // Green  
                lights[ON_OFF1_LIGHT + i  * 2 + 1].setBrightness(0.f);  // Red
            }
        }

        if (wasJustLoaded && outputsInUse)
            applyLoadedOutputs();

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

        if (doProcess && outputsInUse) {
            for (int i = firstIdx; i <= lastIdx; i++) {
                float gateInput = 0.f;
                if (inputs[GATE_TRIG1_INPUT + i].isConnected())
                    gateInput = inputs[GATE_TRIG1_INPUT + i].getVoltage();

                // update virtual gate, based on gate/trigger input and bate/trig-button
                if (params[GATE_TRIG1_PARAM + i].getValue() < 0.5f) { // Latched = trigger
                    if (toggleTrigger[i].process(gateInput,
                        trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act]))
                        virtualGate[i] = !virtualGate[i];
                }
                else {  // Unlatched = gate
                    virtualGate[i] = (gateInput >= trueDetectValues[gateDetHigh.act]);
                }

                updateLaneOutputs(i);
			}
        }

        cycle256++;
    }
};

struct CvToggle8ModuleWidget : InfNoiseModuleWidget {
    infNoiseSmallButton<bc_green, true>* manBtn;

    CvToggle8ModuleWidget(CvToggle8Module *module) {
        initializeWidget(module, "res/CvToggle8");
            
        const float onOffClm = 14.810f;
        const float latchClm = 27.142f;
        const float latchOffset = 10.539;
        const float onClm = 44.389f;
        const float offClm = 73.967f;
        const float outClm = 103.545f;
        float row = 52.120f;
        manBtn = createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(onOffClm, row), module, CvToggle8Module::MANUAL_PARAM);
        addParam(manBtn);
        addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(
            Vec(latchClm, row + latchOffset), 
            module, CvToggle8Module::MANUAL_LATCH_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(onClm, row), module, CvToggle8Module::ON_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(offClm, row), module, CvToggle8Module::OFF_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(outClm, row), module, CvToggle8Module::ATTENUVERT_INPUT));

        const float rowSpacing = 35.0735f;
        row = 87.179f;
        float lightRow = 97.601f;
        for (int i = 0; i < 8; i++) {
            addInput(createInputCentered<ThemedPJ301MPort>(Vec(onOffClm, row), module, CvToggle8Module::GATE_TRIG1_INPUT + i));
            addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_red, bc_green>>(
                Vec(latchClm, row + latchOffset), module, CvToggle8Module::GATE_TRIG1_PARAM + i));
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(onClm, row), module, CvToggle8Module::ON1_INPUT + i));
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(offClm, row), module, CvToggle8Module::OFF1_INPUT + i));
            addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(outClm, row), module, CvToggle8Module::CV1_OUTPUT + i));
            row += rowSpacing;

            int lightIdx = CvToggle8Module::ON_OFF1_LIGHT + i * 2;
            addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(93.924f, lightRow), module, lightIdx));
            lightRow += rowSpacing;
        }
    }

    void step() override {
        if (module) {
            manBtn->momentary = module->params[CvToggle8Module::MANUAL_LATCH_PARAM].getValue() < 0.5f;
        }

        InfNoiseModuleWidget::step();
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        CvToggle8Module* module = dynamic_cast<CvToggle8Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createSubmenuItem("Set gate/trigger-input 1-8", "", [=](Menu* menu) {
            menu->addChild(createMenuItem("To: Gate", "", [=]() {
                for (int i=0; i<8; i++)
                    module->params[CvToggle8Module::GATE_TRIG1_PARAM + i].setValue(1.f);
                }));
            menu->addChild(createMenuItem("To: Trigger", "", [=]() {
                for (int i=0; i<8; i++)
                    module->params[CvToggle8Module::GATE_TRIG1_PARAM + i].setValue(0.f);
                }));
        }));

        menu->addChild(createIndexPtrSubmenuItem("Attenuate mode",
            { "Both", "Only ON-levels", "Only OFF-levels" },
            &module->attMode.req));
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelCvToggle8 = createModel<CvToggle8Module, CvToggle8ModuleWidget>("CvToggle8");