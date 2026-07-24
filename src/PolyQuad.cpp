// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

struct PolyQuadModule : InfNoiseModule {
    enum ParamId {
        //SOME_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        A_INPUT,
        B_INPUT,
        C_INPUT,
        D_INPUT,
        POLY_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        POLY_OUTPUT,
        E_OUTPUT,
        F_OUTPUT,
        G_OUTPUT,
        H_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT, 2),
        ENUMS(CLIP_RANGE_LIGHT, 2),
        FIXED_CHANNELS_LIGHT,
        LIGHTS_LEN
    };

    actReqValue<polyphonyMode> polyChannels = actReqValue<polyphonyMode>(poly_auto);
    bool havePolyOutput = false;
    bool haveMonoOutputs = false;
    int outChannels = 1;
    int firstIn = -1;
    int lastIn = -1;
    
	PolyQuadModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
        configLight(FIXED_CHANNELS_LIGHT, "Fixed channel count if lit");
        
        configInput(A_INPUT, "A");
        configInput(B_INPUT, "B");
        configInput(C_INPUT, "C");
        configInput(D_INPUT, "D");
        configOutput(POLY_OUTPUT, "4>Poly");

        configInput(POLY_INPUT, "Poly>4");
        configOutput(E_OUTPUT, "E");
        configOutput(F_OUTPUT, "F");
        configOutput(G_OUTPUT, "G");
        configOutput(H_OUTPUT, "H");
        
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
        polyChannels.setBoth(poly_auto);
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        polyChannels.setBoth((polyphonyMode)getJsonInt(rootJ, "polyChannels", (int)poly_auto));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "polyChannels", json_integer((int)polyChannels.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        if (polyChannels.needsUpdate()) {
            polyChannels.updateActual();
            lights[FIXED_CHANNELS_LIGHT].setBrightness(polyChannels.act != poly_auto ? 1.f : 0.f);
            if (outputInfos[POLY_OUTPUT])
                outputInfos[POLY_OUTPUT]->name = polyPortPrefix() + "4>Poly: " + getPolyphonyModeName(polyChannels.act);
        }

        havePolyOutput = outputs[POLY_OUTPUT].isConnected();
        haveMonoOutputs = false;
        int connectedCount = 0;
        firstIn = -1;
        lastIn = -1;
        for (int i=0; i<4; i++) {
            if (inputs[A_INPUT + i].isConnected()) {
                connectedCount++;
                if (firstIn == -1) firstIn = i;
                lastIn = i;
            }
            if (outputs[E_OUTPUT + i].isConnected()) {
                haveMonoOutputs = true;
            }
        }

        if (polyChannels.act == poly_auto) {
            outChannels = connectedCount;
            if (outChannels == 0) {
                outputs[POLY_OUTPUT].setChannels(1);
                outputs[POLY_OUTPUT].setVoltage(0.f);
            } else {
                outputs[POLY_OUTPUT].setChannels(outChannels);
            }
        } else {
            outChannels = polyphonyModeChannels[polyChannels.act];
            outputs[POLY_OUTPUT].setChannels(outChannels);
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

        if (doProcess) {
            // 4 > Poly
            if (havePolyOutput) {
                if (polyChannels.act == poly_auto) {
                    int chnl = 0;
                    for (int i=firstIn; i<=lastIn; i++) {
                        if (inputs[A_INPUT + i].isConnected()) {
                            float voltage = clipToVoltRange(inputs[A_INPUT + i].getVoltage(), outClipRange.act);
                            outputs[POLY_OUTPUT].setVoltage(voltage, chnl);
                            chnl++;
                        }
                    }
                } else {
                    for (int i=0; i<outChannels; i++) {
                        float voltage = inputs[A_INPUT + i].isConnected()
                            ? clipToVoltRange(inputs[A_INPUT + i].getVoltage(), outClipRange.act)
                            : 0.f;
                        outputs[POLY_OUTPUT].setVoltage(voltage, i);
                    }
                }
            }

            // Poly > 4
            if (haveMonoOutputs) {
                int inChnl = inputs[POLY_INPUT].isConnected() 
                    ? inputs[POLY_INPUT].getChannels() 
                    : 0;
                for (int i=0; i<4; i++) {
                    float voltage = i < inChnl 
                        ? inputs[POLY_INPUT].getPolyVoltage(i) 
                        : 0.f;
                    voltage = clipToVoltRange(voltage, outClipRange.act);
                    outputs[E_OUTPUT + i].setVoltage(voltage);
                }
            }
        }

        cycle256++;
    }
};

struct PolyQuadModuleWidget : InfNoiseModuleWidget {
    PolyQuadModuleWidget(PolyQuadModule *module) {
        initializeWidget(module, "res/PolyQuad");

        float cntrCol = 15.f;
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrCol, 53.239f), module, PolyQuadModule::A_INPUT));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrCol, 77.871f), module, PolyQuadModule::B_INPUT));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrCol, 102.504f), module, PolyQuadModule::C_INPUT));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(cntrCol, 127.136f), module, PolyQuadModule::D_INPUT));
        addChild(createLightCentered<TinyLight<RedLight>>(Vec(4.615f, 151.181f), module, PolyQuadModule::FIXED_CHANNELS_LIGHT));
        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 166.614f), module, PolyQuadModule::POLY_OUTPUT));
        
        addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(cntrCol, 223.231f), module, PolyQuadModule::POLY_INPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(cntrCol, 258.797f), module, PolyQuadModule::E_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(cntrCol, 283.429f), module, PolyQuadModule::F_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(cntrCol, 308.061f), module, PolyQuadModule::G_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(cntrCol, 332.694f), module, PolyQuadModule::H_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        PolyQuadModule* module = dynamic_cast<PolyQuadModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

        std::vector<std::string> channelNames;
        channelNames.push_back(getPolyphonyModeName(poly_auto));
        for (int i = mono_1; i <= poly_4; i++)
            channelNames.push_back(getPolyphonyModeName((polyphonyMode)i));

        menu->addChild(createIndexSubmenuItem("4>Poly output channels", channelNames,
            [=]() {
                polyphonyMode m = module->polyChannels.req;
                if (m == poly_auto)
                    return 0;
                if (m >= mono_1 && m <= poly_4)
                    return (int)m - (int)mono_1 + 1;
                return 0;
            },
            [=](int idx) {
                static const polyphonyMode modes[] = { poly_auto, mono_1, poly_2, poly_3, poly_4 };
                module->polyChannels.req = modes[idx];
            }
        ));
        
        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelPolyQuad = createModel<PolyQuadModule, PolyQuadModuleWidget>("PolyQuad");
