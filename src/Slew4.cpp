// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"

struct Slew4Module : InfNoiseModule {
    enum ParamId {
        RISE_TIME_PARAM,
        FALL_TIME_PARAM,
        SHAPE_PARAM,
        TIME_LINK_PARAM,
        FALL_SHAPE_LINK_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
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
        ENUMS(RISE_TIME_SCALE_LIGHT, 2),
        ENUMS(FALL_TIME_SCALE_LIGHT, 2),
        RISE_SLEW_MODE_LIGHT,
        FALL_SLEW_MODE_LIGHT,
        LIGHTS_LEN
    };
    actReqValue<infNoiseTimeScale> timeScale = actReqValue<infNoiseTimeScale>(ts_1x);
    actReqValue<infNoiseSlewMode> slewMode = actReqValue<infNoiseSlewMode>(sm_constantRate);
    actReqValue<infNoiseLinearMode> linearMode = actReqValue<infNoiseLinearMode>(lm_linear);
    bool timeLinked = false;
    int firstIdx = -1;
    int lastIdx = -1;
    bool haveConnections = false;
    int channels[4] = { 1, 1, 1, 1 };
    bool inConn[4] = { false, false, false, false };
    bool outConn[4] = { false, false, false, false };
    infNoiseSlew slew[64];
    float prevRiseTime = -999.f;
    float prevFallTime = -999.f;
    float prevRiseShape = -999.f;
    float prevFallShape = -999.f;

    Slew4Module() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        configParam<infNoiseSqTimeQnt>(RISE_TIME_PARAM, 0.f, 1.f, 0.f, "Rise time (0 to 10 s)", " s");
        configParam<infNoiseSqTimeQnt>(FALL_TIME_PARAM, 0.f, 1.f, 0.f, "Fall time (0 to 10 s)", " s");
        configParam(SHAPE_PARAM, -1.f, 1.f, 0.f, "Shape (Exp/Lin/Log)");
        configLight(RISE_TIME_SCALE_LIGHT, "Rise time scale (green=0.1x, off=1x, red=10x)");
        configLight(FALL_TIME_SCALE_LIGHT, "Fall time scale (green=0.1x, off=1x, red=10x)");
        configLight(RISE_SLEW_MODE_LIGHT, "Rise slew mode (dim=constant rate, blue=constant time)");
        configLight(FALL_SLEW_MODE_LIGHT, "Fall slew mode (dim=constant rate, blue=constant time)");
        configSwitch(TIME_LINK_PARAM, 0.f, 1.f, 0.f, "Time link", { "Disabled", "Enabled" });
        configSwitch(FALL_SHAPE_LINK_PARAM, 0.f, 1.f, 0.f, "Shape", { "Fall same as rise", "Fall reversed" });

        configInput(A_INPUT, "A");
        configInput(B_INPUT, "B");
        configInput(C_INPUT, "C");
        configInput(D_INPUT, "D");

        configOutput(A_OUTPUT, "A");
        configOutput(B_OUTPUT, "B");
        configOutput(C_OUTPUT, "C");
        configOutput(D_OUTPUT, "D");

        configBypass(A_INPUT, A_OUTPUT);
        configBypass(B_INPUT, B_OUTPUT);
        configBypass(C_INPUT, C_OUTPUT);
        configBypass(D_INPUT, D_OUTPUT);

        haveProcQuality = true;
        haveAutoProcQuality = false;
        haveOutQuantize = false;
        haveOutClipRange = true;
        haveGateDetect = false;
        haveGateHighLow = false;
        haveTrigDetect = false;
        haveTrigHighLow = false;

        ensureNormExpLogLuts();
    }

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);

        for(int i=0; i<64; i++) {
            slew[i].reset();
        }
        timeScale.setBoth(ts_1x);
        slewMode.setBoth(sm_constantRate);
        linearMode.setBoth(lm_linear);
        prevRiseTime = -999.f;
        prevFallTime = -999.f;
        prevRiseShape = -999.f;
        prevFallShape = -999.f;
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);

        float lastOut[64];
        getJsonFloatArray(rootJ, "lastOut", lastOut, 64, 0.f);
        for (int i = 0; i < 64; i++)
            slew[i].snap(lastOut[i]);
        timeScale.setBoth((infNoiseTimeScale)getJsonInt(rootJ, "timeScale", (int)ts_1x));
        slewMode.setBoth((infNoiseSlewMode)getJsonInt(rootJ, "slewMode", (int)sm_constantRate));
        int loadedLinear = getJsonInt(rootJ, "linearMode", -1);
        if (loadedLinear < 0)
            loadedLinear = getJsonBool(rootJ, "useSCurve", false) ? (int)lm_sCurve : (int)lm_linear;
        linearMode.setBoth((infNoiseLinearMode)loadedLinear);
        prevRiseTime = -999.f;
        prevFallTime = -999.f;
        prevRiseShape = -999.f;
        prevFallShape = -999.f;
    }

    void dataToJson(json_t* rootJ) override {
        float lastOut[64];
        for (int i = 0; i < 64; i++)
            lastOut[i] = slew[i].last();
        setJsonFloatArray(rootJ, "lastOut", lastOut, 64);
        json_object_set_new(rootJ, "timeScale", json_integer((int)timeScale.req));
        json_object_set_new(rootJ, "slewMode", json_integer((int)slewMode.req));
        json_object_set_new(rootJ, "linearMode", json_integer((int)linearMode.req));
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        // Handle time link
        timeLinked = params[TIME_LINK_PARAM].getValue() > 0.5f;
        if (timeLinked)
            params[FALL_TIME_PARAM].setValue(params[RISE_TIME_PARAM].getValue());

        // Detect connections
        haveConnections = false;
        firstIdx = -1;
        lastIdx = -1;
        for (int i = 0; i < 4; i++) {
            inConn[i] = inputs[A_INPUT + i].isConnected();
            channels[i] = inConn[i]
                ? std::max(inputs[A_INPUT + i].getChannels(), 1)
                : 1;
            outputs[A_OUTPUT + i].setChannels(channels[i]);
            outConn[i] = outputs[A_OUTPUT + i].isConnected();
            if (inConn[i] || outConn[i]) {
                if (firstIdx < 0)
                    firstIdx = i;
                lastIdx = i;
                haveConnections = true;
            }
        }

        // Apply time-knob scale
        if (timeScale.needsUpdate() || mustProcessParams) {
            timeScale.updateActual();
            applyInfNoiseSqTimeQntScale(paramQuantities[RISE_TIME_PARAM], "Rise time", timeScale.act);
            applyInfNoiseSqTimeQntScale(paramQuantities[FALL_TIME_PARAM], "Fall time", timeScale.act);
            setInfNoiseTimeScaleLights(this, RISE_TIME_SCALE_LIGHT, timeScale.act);
            setInfNoiseTimeScaleLights(this, FALL_TIME_SCALE_LIGHT, timeScale.act);
        }

        if (slewMode.needsUpdate() || mustProcessParams) {
            slewMode.updateActual();
            for (int i = 0; i < 64; i++)
                slew[i].setMode(slewMode.act);
            float modeLight = (slewMode.act == sm_constantTime) ? 1.f : 0.f;
            lights[RISE_SLEW_MODE_LIGHT].setBrightness(modeLight);
            lights[FALL_SLEW_MODE_LIGHT].setBrightness(modeLight);
        }

        if (linearMode.needsUpdate() || mustProcessParams) {
            linearMode.updateActual();
            for (int i = 0; i < 64; i++)
                slew[i].setLinearMode(linearMode.act);
        }

        // Read/update rise/fall times
        float newRiseTime = getParamQuantity(RISE_TIME_PARAM)->getDisplayValue();
        float newFallTime = getParamQuantity(FALL_TIME_PARAM)->getDisplayValue();
        if (newRiseTime != prevRiseTime || newFallTime != prevFallTime || mustProcessParams) {
            prevRiseTime = newRiseTime;
            prevFallTime = newFallTime;
            for(int i=0; i<64; i++) {
                slew[i].setTimes(newRiseTime, newFallTime);
            }
        }

        // Read/update rise/fall shapes
        float newRiseShape = params[SHAPE_PARAM].getValue();
        float newFallShape = params[FALL_SHAPE_LINK_PARAM].getValue() > 0.5f ? -newRiseShape : newRiseShape;
        if (newRiseShape != prevRiseShape || newFallShape != prevFallShape || mustProcessParams) {
            prevRiseShape = newRiseShape;
            prevFallShape = newFallShape;
            for(int i=0; i<64; i++) {
                slew[i].setShapes(newRiseShape, newFallShape);
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

        if (doProcess && haveConnections) {
            for (int i = firstIdx; i <= lastIdx; i++) {
                int baseIdx = i * 16;
                for (int c = 0; c < channels[i]; c++) {
                    float voltage = inConn[i] ? inputs[A_INPUT + i].getVoltage(c) : 0.f;
                    int idx = baseIdx + c;
                    voltage = clipToVoltRange(slew[idx].next(voltage, procSampleTime), outClipRange.act);
                    outputs[A_OUTPUT + i].setVoltage(voltage, c);
                }
            }
        }

        cycle256++;
    }
};

struct Slew4ModuleWidget : InfNoiseModuleWidget {
    InfNoiseDisableOverlayGroup* fallTimeLinkOverlayGroup = nullptr;
    bool timeLinked = false;

    Slew4ModuleWidget(Slew4Module* module) {
        initializeWidget(module, "res/Slew4");

        const float centerCol = 15.0f;
        const float timeScaleLgtOfs = 10.f;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, 53.213f), module, Slew4Module::RISE_TIME_PARAM));
        addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(centerCol + timeScaleLgtOfs, 53.213f - timeScaleLgtOfs), module, Slew4Module::RISE_TIME_SCALE_LIGHT));
        addChild(createLightCentered<TinyLight<BlueLight>>(Vec(centerCol - timeScaleLgtOfs, 53.213f - timeScaleLgtOfs), module, Slew4Module::RISE_SLEW_MODE_LIGHT));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, false>>(Vec(4.499f, 69.908f), module, Slew4Module::TIME_LINK_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, 85.521f), module, Slew4Module::FALL_TIME_PARAM));
        addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(centerCol + timeScaleLgtOfs, 85.521f - timeScaleLgtOfs), module, Slew4Module::FALL_TIME_SCALE_LIGHT));
        addChild(createLightCentered<TinyLight<BlueLight>>(Vec(centerCol - timeScaleLgtOfs, 85.521f - timeScaleLgtOfs), module, Slew4Module::FALL_SLEW_MODE_LIGHT));
        
        addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_green, bc_red>>(Vec(4.499f, 107.760f), module, Slew4Module::FALL_SHAPE_LINK_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerCol, 124.314f), module, Slew4Module::SHAPE_PARAM));

        InfNoiseDisableOverlayManager& overlayManager = getDisableOverlayManager();
        fallTimeLinkOverlayGroup = overlayManager.addGroup("Fall time linked to Rise time");
        fallTimeLinkOverlayGroup->addTarget(InfNoiseOverlayTargetType::param, Slew4Module::FALL_TIME_PARAM);

        const float rowSpacing = 24.632f;
        float row = 156.726f;
        for (int i = 0; i < 4; i++) {
            addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(centerCol, row), module, Slew4Module::A_INPUT + i));
            row += rowSpacing;
        }

        row = 261.050f;
        for (int i = 0; i < 4; i++) {
            addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(centerCol, row), module, Slew4Module::A_OUTPUT + i));
            row += rowSpacing;
        }
    }

    void step() override {
        InfNoiseModuleWidget::step();

        if (!module)
            return;

        auto* m = static_cast<Slew4Module*>(module);
        if (fallTimeLinkOverlayGroup) {
            if (m->timeLinked != timeLinked) {
                timeLinked = m->timeLinked;
                fallTimeLinkOverlayGroup->setActive(timeLinked);
            }
        }
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        Slew4Module* module = dynamic_cast<Slew4Module*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);
        menu->addChild(createIndexPtrSubmenuItem("Time scale", infNoiseTimeScaleMenuNames(), &module->timeScale.req));
        menu->addChild(createIndexPtrSubmenuItem("Slew mode", infNoiseSlewModeMenuNames(), &module->slewMode.req));
        menu->addChild(createIndexPtrSubmenuItem("Linear mode", infNoiseLinearModeMenuNames(), &module->linearMode.req));

        appendInfNoiseMenuItems(menu);
    }
};

Model* modelSlew4 = createModel<Slew4Module, Slew4ModuleWidget>("Slew4");
