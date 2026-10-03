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
    actReqValue<fixedSlewTimes> fadeTime = actReqValue<fixedSlewTimes>(fst_0);
    actReqValue<bool> fadeSCurve = actReqValue<bool>(false);
    infNoiseUnitFade fade;
    bool doFade = false;
    bool haveOnIn = false;
    bool haveOffIn = false;

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
        fadeTime.setBoth(fst_0);
        fadeSCurve.setBoth(false);
        doFade = false;
        fade.reset(0.f);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        onStage.setBoth(getJsonInt(rootJ, "onStage", 0) == 1);
        fadeTime.setBoth((fixedSlewTimes)getJsonInt(rootJ, "fadeTime", (int)fst_0, (int)fst_len - 2));
        fadeSCurve.setBoth(getJsonBool(rootJ, "fadeSCurve", false));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "onStage", json_integer(onStage.req ? 1 : 0));
        json_object_set_new(rootJ, "fadeTime", json_integer((int)fadeTime.req));
        json_object_set_new(rootJ, "fadeSCurve", json_boolean(fadeSCurve.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        bool stageLights = onStage.needsUpdate() || wasJustLoaded;
        if (stageLights)
			onStage.updateActual();

        if (fadeSCurve.needsUpdate() || wasJustLoaded) {
            fadeSCurve.updateActual();
            fade.setUseSCurve(fadeSCurve.act);
        }
        if (fadeTime.needsUpdate() || wasJustLoaded) {
            bool wasOn = doFade;
            fadeTime.updateActual();
            doFade = fadeTime.act != fst_0;
            fade.setTime(doFade ? fixedSlewTimesValues[(int)fadeTime.act] : 0.f);
            if (wasJustLoaded || !doFade || !wasOn)
                fade.snap(onStage.act ? 1.f : 0.f);
            if (!doFade && wasOn)
                stageLights = true;
        }

        if (doFade) {
            float mix = fade.amount;
            lights[ON_LIGHT].setBrightness(mix);
            lights[OFF_LIGHT].setBrightness(1.f - mix);
        }
        else if (stageLights) {
			lights[ON_LIGHT].setBrightness(onStage.act ? 1.f : 0.f);
			lights[OFF_LIGHT].setBrightness(onStage.act ? 0.f : 1.f);
		}

        // Determine number of channels
        channels = 1;
        haveOnIn = inputs[ON_INPUT].isConnected();
        haveOffIn = inputs[OFF_INPUT].isConnected();
        if (haveOnIn)
            channels = std::max(channels, inputs[ON_INPUT].getChannels());
        if (haveOffIn)
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
                if (!doFade) {
                    float knobValue = params[onStage.act ? ON_PARAM : OFF_PARAM].getValue();
                    float valueTrim = params[onStage.act ? ON_TRIM_PARAM : OFF_TRIM_PARAM].getValue();
                    int valueInputIdx = onStage.act ? ON_INPUT : OFF_INPUT;
                    bool haveIn = onStage.act ? haveOnIn : haveOffIn;
                    for (int c = 0; c < channels; c++) {
                        float voltage = knobValue;
                        if (haveIn)
                            voltage += valueTrim * inputs[valueInputIdx].getPolyVoltage(c);
                        voltage = clipToVoltRange(voltage, outClipRange.act);
                        outputs[VALUE_OUTPUT].setVoltage(voltage, c);
                    }
                }
                else {
                    fade.setTarget(onStage.act ? 1.f : 0.f);
                    float mix = fade.next(procSampleTime);
                    float mixOff = 1.f - mix;
                    float onKnob = params[ON_PARAM].getValue();
                    float offKnob = params[OFF_PARAM].getValue();
                    float onTrim = params[ON_TRIM_PARAM].getValue();
                    float offTrim = params[OFF_TRIM_PARAM].getValue();
                    for (int c = 0; c < channels; c++) {
                        float onV = onKnob;
                        if (haveOnIn)
                            onV += onTrim * inputs[ON_INPUT].getPolyVoltage(c);
                        float offV = offKnob;
                        if (haveOffIn)
                            offV += offTrim * inputs[OFF_INPUT].getPolyVoltage(c);
                        float voltage = clipToVoltRange(mixOff * offV + mix * onV, outClipRange.act);
                        outputs[VALUE_OUTPUT].setVoltage(voltage, c);
                    }
                }
            }
        }

        cycle256++;
    }
};

struct OnOffSwitchModuleWidget : InfNoiseModuleWidget {
    infNoiseSmallButton<bc_green, true>* onOffBtn;

    OnOffSwitchModuleWidget(OnOffSwitchModule *module) {
        initializeWidget(module, "res/OnOffSwitch");

        // ON/OFF-button and latch
        const float cntrCol = 15.f;
        const float latchClm = 25.152f;
        onOffBtn = createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(cntrCol, 51.397f), module, OnOffSwitchModule::ON_OFF_PARAM);
        addParam(onOffBtn);
        addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(latchClm, 63.689f), module, OnOffSwitchModule::ON_OFF_LATCH_PARAM));

        // ON/OFF-input and trigger/gate-switch
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrCol, 81.103f), module, OnOffSwitchModule::ON_OFF_INPUT));
        addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_red, bc_green>>(
            Vec(latchClm, 93.595f), module, OnOffSwitchModule::ON_OFF_TRIGGATE_PARAM));

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
            applyButtonMomentary(onOffBtn, module->params[OnOffSwitchModule::ON_OFF_LATCH_PARAM].getValue() < 0.5f);
        }

        InfNoiseModuleWidget::step();
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        OnOffSwitchModule* module = dynamic_cast<OnOffSwitchModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);
        menu->addChild(createIndexPtrSubmenuItem("Fade time", getFixedSlewTimesNames(false),
            &module->fadeTime.req));
        menu->addChild(createBoolPtrMenuItem("S-curve fade", "", &module->fadeSCurve.req));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelOnOffSwitch = createModel<OnOffSwitchModule, OnOffSwitchModuleWidget>("OnOffSwitch");