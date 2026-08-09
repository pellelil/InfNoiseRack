// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inUtil.hpp"

struct ManTrigger8Module : InfNoiseModule {
    enum ParamId {
        TRIGGER_ALL_PARAM,
        TRIGGER1_PARAM,
        TRIGGER2_PARAM,
        TRIGGER3_PARAM,
        TRIGGER4_PARAM,
        TRIGGER5_PARAM,
        TRIGGER6_PARAM,
        TRIGGER7_PARAM,
        TRIGGER8_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        //SOME_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        TRIGGER_POLY_OUTPUT,
        TRIGGER1_OUTPUT,
        TRIGGER2_OUTPUT,
        TRIGGER3_OUTPUT,
        TRIGGER4_OUTPUT,
        TRIGGER5_OUTPUT,
        TRIGGER6_OUTPUT,
        TRIGGER7_OUTPUT,
        TRIGGER8_OUTPUT,        
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT,2),
        POLY_HINT_LIGHT,
        LIGHTS_LEN
    };

    bool haveOutputs = false;
    dsp::SchmittTrigger inTrigger[8]; // Prevents multiple triggers
    infNoiseOutTrigger outputTrigger[8];  // Timing of triggers
    actReqValue<polyphonyMode> polyOutput = actReqValue<polyphonyMode>(poly_8);
    int polyChannels = 8;
    infNoiseButtonTrigger btAll = infNoiseButtonTrigger();

    ManTrigger8Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
        configLight(POLY_HINT_LIGHT, "Poly output: lit when channel count is not 8");

        configSwitch(TRIGGER_ALL_PARAM, 0.0f, 1.0f, 0.0f, "Trigger-All", { "Off", "On" });
        configOutput(TRIGGER_POLY_OUTPUT, "Polyphonic triggers");
        for (int i = 0; i < 8; i++) {
            configSwitch(TRIGGER1_PARAM + i, 0.0f, 1.0f, 0.0f, string::f("Trigger-button %d", i + 1), { "Low", "High" });
            configOutput(TRIGGER1_OUTPUT + i, string::f("Trigger %d", i + 1));
        }

        // Set InfNoise features (e.g. menu-items) 
		haveProcQuality = true;
        haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = false;
        haveGateDetect = false;
		haveGateHighLow = false;
		haveTrigDetect = false;
		haveTrigHighLow = true;
	}

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);
        
        btAll.reset();
        polyOutput.setBoth(poly_8);
        for (int i = 0; i < 8; i++) {
            inTrigger[i].reset();
            outputTrigger[i].reset();
        }
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);

        polyOutput.setBoth((polyphonyMode)getJsonInt(rootJ, "polyOutput",
            getJsonInt(rootJ, "anyPoly", (int)polyphonyMode::poly_8)));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "polyOutput", json_integer((int)polyOutput.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        haveOutputs = outputs[TRIGGER_POLY_OUTPUT].isConnected();
        if (polyOutput.needsUpdate()) {
            polyOutput.updateActual();
            lights[POLY_HINT_LIGHT].setBrightness(polyOutput.req != poly_8 ? 1.f : 0.f);
            if (outputInfos.size() > (unsigned)TRIGGER_POLY_OUTPUT && outputInfos[TRIGGER_POLY_OUTPUT]) {
                outputInfos[TRIGGER_POLY_OUTPUT]->name = polyPortPrefix() + "Poly output: " + getPolyphonyModeName(polyOutput.act);
            }
        }
        polyChannels = polyphonyModeChannels[polyOutput.act];
        outputs[TRIGGER_POLY_OUTPUT].setChannels(polyChannels);
        for (int i = 0; i < 8; i++) {
            haveOutputs |= outputs[TRIGGER1_OUTPUT + i].isConnected();
            outputs[TRIGGER1_OUTPUT + i].setChannels(1);
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
            // Handle trigger-all button
            float trigAllValue = params[TRIGGER_ALL_PARAM].getValue();
            if (btAll.process(trigAllValue > 0.5f)) {
                for (int i = 0; i < 8; i++) {
					params[TRIGGER1_PARAM + i].setValue(trigAllValue);
				}
			}

            // Handle individual trigger buttons
            for (int i = 0; i < 8; i++) {
                float trigValue = params[TRIGGER1_PARAM + i].getValue();
                outputTrigger[i].process(procSampleTime);
                if (inTrigger[i].process(trigValue,
                    trueDetectValues[td_triggerLow], trueDetectValues[td_triggerHigh])) {
                    outputTrigger[i].trigger();
                }
                float voltage = (outputTrigger[i].isHigh())
                    ? voltValues[trigOutHigh.act]
                    : voltValues[trigOutLow.act];
                outputs[TRIGGER1_OUTPUT + i].setVoltage(voltage);
                if (i < polyChannels)
                    outputs[TRIGGER_POLY_OUTPUT].setVoltage(voltage, i);
            }
        }

        cycle256++;
    }
};

struct ManTrigger8ModuleWidget : InfNoiseModuleWidget {

    ManTrigger8ModuleWidget(ManTrigger8Module *module) {
        initializeWidget(module, "res/ManTrigger8");

        const float butClm = 14.806f;
        const float outClm = 43.545f;
        const float allRow = 52.120f;
        const float lgtOfs = 10.021f;
        addParam(createParamCentered<infNoiseSmallButton<bc_red, true>>(Vec(butClm, allRow), module, ManTrigger8Module::TRIGGER_ALL_PARAM));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(outClm, allRow), module, ManTrigger8Module::TRIGGER_POLY_OUTPUT));
        addChild(createLightCentered<TinyLight<RedLight>>(Vec(outClm - lgtOfs, allRow - lgtOfs), module, ManTrigger8Module::POLY_HINT_LIGHT));

        const float rowSpacing = 35.0734f;
        float row = 87.194f;
        for (int i = 0; i < 8; i++) {
            addParam(createParamCentered<infNoiseSmallButton<bc_red, true>>(Vec(butClm, row), module, ManTrigger8Module::TRIGGER1_PARAM + i));
            addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(outClm, row), module, ManTrigger8Module::TRIGGER1_OUTPUT + i));

            row += rowSpacing;
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        ManTrigger8Module* module = dynamic_cast<ManTrigger8Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> polyNames = getPolyphonyModeNames(false);
        polyNames.resize(8);
        menu->addChild(createIndexPtrSubmenuItem("Poly output channels", polyNames,
            &module->polyOutput.req));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelManTrigger8 = createModel<ManTrigger8Module, ManTrigger8ModuleWidget>("ManTrigger8");