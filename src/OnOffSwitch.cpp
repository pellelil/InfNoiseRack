// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inUtil.hpp"

struct OnOffSwitchModule : InfNoiseModule {
    enum ParamId {
        ON_OFF_PARAM,
        ON_OFF_LATCH_PARAM,
        ON_OFF_TRIGGATE_PARAM,
        ON_PARAM,
        ON_TRIM_PARAM,
        OFF_PARAM,
        OFF_TRIM_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        ON_OFF_INPUT,
        ON_INPUT,
        OFF_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        VALUE_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT, 2),
        ON_LIGHT,
        OFF_LIGHT,
        LIGHTS_LEN
    };

    bool haveOutput = false;
    int channels = 0;
    actReqValue<bool> onStage = actReqValue<bool>(false);
    dsp::SchmittTrigger onOffTrigger;

	OnOffSwitchModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configSwitch(ON_OFF_PARAM, 0.0f, 1.0f, 0.0f, "ON/OFF-button", { "OFF", "ON" });
        configSwitch(ON_OFF_LATCH_PARAM, 0.0f, 1.0f, 0.0f, "Latch ON/OFF-button", { "Unlatched", "Latched" });

        configInput(ON_OFF_INPUT, "ON/OFF trigger/gate");
        configSwitch(ON_OFF_TRIGGATE_PARAM, 0.0f, 1.0f, 1.0f, "ON/OFF trigger/gate", { "Trigger-mode", "Gate-mode" });

        configLight(ON_LIGHT, "ON-stage active if lit");
        configParam(ON_PARAM, -10.0f, 10.0f, 0.0f, "ON-value", " V");
        configParam(ON_TRIM_PARAM, -1.f, 1.f, 0.f, "ON-value CV-trim", "%", 0, 100);
        configInput(ON_INPUT, "ON-value");

        configLight(OFF_LIGHT, "OFF-stage active if lit");
        configParam(OFF_PARAM, -10.0f, 10.0f, 0.0f, "OFF-value", " V");
        configParam(OFF_TRIM_PARAM, -1.f, 1.f, 0.f, "OFF-value CV-trim", "%", 0, 100);
        configInput(OFF_INPUT, "OFF-value");

        configOutput(VALUE_OUTPUT, "Value (ON or OFF)");

        configBypass(ON_INPUT, VALUE_OUTPUT);

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
		haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = true;  
		haveGateDetect = true;
		haveGateHighLow = false;
		haveTrigDetect = true;
		haveTrigHighLow = false;
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        onStage.setBoth(false);
        onOffTrigger.reset();
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        onStage.setBoth(getJsonInt(rootJ, "onStage", 0) == 1);
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "onStage", json_integer(onStage.req ? 1 : 0));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        // Update ON/OFF-stage lights
        if (onStage.needsUpdate()) {
			onStage.updateActual();
			lights[ON_LIGHT].setBrightness(onStage.act ? 1.f : 0.f);
			lights[OFF_LIGHT].setBrightness(onStage.act ? 0.f : 1.f);
		}

        // Determine number of channels
        channels = 1;
        if (inputs[ON_INPUT].isConnected())
            channels = std::max(channels, inputs[ON_INPUT].getChannels());
        if (inputs[OFF_INPUT].isConnected())
            channels = std::max(channels, inputs[OFF_INPUT].getChannels());
        outputs[VALUE_OUTPUT].setChannels(channels);

        // Detect output-channels
        haveOutput = outputs[VALUE_OUTPUT].isConnected();

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

        if (doProcess) {
            // Handle ON/OFF-switching (perhaps change onStage)
            if (params[ON_OFF_PARAM].getValue() > 0.5f) {  // Button pressed ON
                onStage.setBoth(true);
			}
            else {
                if (inputs[ON_OFF_INPUT].isConnected())
                {
                    float gateTrigInput = inputs[ON_OFF_INPUT].getVoltage();
                    if (params[ON_OFF_TRIGGATE_PARAM].getValue() > 0.5f) // Gate-Input
                    {   
                        bool newStage = gateTrigInput >= trueDetectValues[gateDetHigh.act];
                        if (newStage != onStage.req)
							onStage.setBoth(newStage);
                    }
                    else // Trigger-Input
                    {
                        if (onOffTrigger.process(gateTrigInput,
                            trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act]))
                            onStage.setBoth(!onStage.req);
                    }
                }
                else // No input, and button not pressed
                {
                    if (onStage.req)
					    onStage.setBoth(false);
				}
			}

            // Handle output
            if (haveOutput)
            {
                float knobValue = params[onStage.act ? ON_PARAM : OFF_PARAM].getValue();
                float valueTrim = params[onStage.act ? ON_TRIM_PARAM : OFF_TRIM_PARAM].getValue();
                int valueInputIdx = onStage.act ? ON_INPUT : OFF_INPUT;
                for (int c = 0; c < channels; c++) {
                    float voltage = knobValue;
                    if (inputs[valueInputIdx].isConnected())
						voltage += valueTrim * inputs[valueInputIdx].getPolyVoltage(c);
                    voltage = clipToVoltRange(voltage, outClipRange.act);
                    outputs[VALUE_OUTPUT].setVoltage(voltage, c);
                }
            }
        }

        cycle256++;
    }
};

struct OnOffSwitchModuleWidget : InfNoiseModuleWidget {
    infNoiseSmallButton* onOffBtn;
    infNoiseLtSmallButton* onOffLatchBtn;
    infNoiseLtSmallButton* trigGateBtn;

    OnOffSwitchModuleWidget(OnOffSwitchModule *module) {
        initializeWidget(module, "res/OnOffSwitch");

        // ON/OFF-button and latch
        const float cntrCol = 15.f;
        const float latchClm = 25.152f;
        onOffBtn = createParamCentered<infNoiseSmallButton>(Vec(cntrCol, 51.397f), module, OnOffSwitchModule::ON_OFF_PARAM);
        onOffBtn->setup(bc_green, true);
        addParam(onOffBtn);
        onOffLatchBtn = createParamCentered<infNoiseLtSmallButton>(Vec(latchClm, 63.689f), module, OnOffSwitchModule::ON_OFF_LATCH_PARAM);
        onOffLatchBtn->setup(bc_red, false);
        addParam(onOffLatchBtn);

        // ON/OFF-input and trigger/gate-switch
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrCol, 81.103f), module, OnOffSwitchModule::ON_OFF_INPUT));
        trigGateBtn = createParamCentered<infNoiseLtSmallButton>(Vec(latchClm, 93.595f), module, OnOffSwitchModule::ON_OFF_TRIGGATE_PARAM);
        trigGateBtn->setup(bc_redGreen, false);
        addParam(trigGateBtn);

        // ON-value
        const float lgtClm = 3.744f;
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(lgtClm, 112.327f), module, OnOffSwitchModule::ON_LIGHT));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrCol, 127.343f), module, OnOffSwitchModule::ON_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(cntrCol, 156.951f), module, OnOffSwitchModule::ON_TRIM_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 186.923f), module, OnOffSwitchModule::ON_INPUT));

        // OFF-value
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(lgtClm, 213.662f), module, OnOffSwitchModule::OFF_LIGHT));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(cntrCol, 228.678f), module, OnOffSwitchModule::OFF_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(cntrCol, 258.227f), module, OnOffSwitchModule::OFF_TRIM_PARAM));
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 288.259f), module, OnOffSwitchModule::OFF_INPUT));

        // Output (ON- or OFF-value)
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 333.194f), module, OnOffSwitchModule::VALUE_OUTPUT));
    }

    void step() override {
        if (module) {
            onOffBtn->momentary = module->params[OnOffSwitchModule::ON_OFF_LATCH_PARAM].getValue() < 0.5f;
        }

        InfNoiseModuleWidget::step();
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        OnOffSwitchModule* module = dynamic_cast<OnOffSwitchModule*>(this->module);
        assert(module);

        //menu->addChild(new MenuSeparator);
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelOnOffSwitch = createModel<OnOffSwitchModule, OnOffSwitchModuleWidget>("OnOffSwitch");