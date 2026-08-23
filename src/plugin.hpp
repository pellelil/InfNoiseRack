// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#pragma once
#include <rack.hpp>
#include <settings.hpp>
using namespace ::rack;
#include "inUtil.hpp"

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file

extern Model* modelADREnvelope;
extern Model* modelADSDREnvelope;
extern Model* modelArm3XY;
extern Model* modelAutoScale4;
extern Model* modelBernoulliSwitch;
extern Model* modelBitsToValue;
extern Model* modelClamp4;
extern Model* modelCombine;
extern Model* modelCrossFadeSwitch1to4;
extern Model* modelCrossFadeSwitch4to1;
extern Model* modelCvToggle8;
extern Model* modelCvToGt;
extern Model* modelCvToGtTr8;
extern Model* modelCxFade1x2;
extern Model* modelCxFade4x1;
extern Model* modelDelta4;
extern Model* modelFlipFlop;
extern Model* modelFold;
extern Model* modelIncDecOffset;
extern Model* modelVCMP1;
extern Model* modelLCMP2;
extern Model* modelLCMP6x2;
extern Model* modelManCV8I;
extern Model* modelManCV8II;
extern Model* modelManGate8;
extern Model* modelManMix4I;
extern Model* modelManMix4II;
extern Model* modelManMix4st;
extern Model* modelManMute8;
extern Model* modelManPush2;
extern Model* modelManTrGtCv;
extern Model* modelManTrigger8;
extern Model* modelMerge2x4;
extern Model* modelMergeMult4;
extern Model* modelMult2x4;
extern Model* modelMute2;
extern Model* modelOnOffSwitch;
extern Model* modelPatch;
extern Model* modelPolyLCMP;
extern Model* modelPolyMerge;
extern Model* modelPolyOffset;
extern Model* modelPolyQuad;
extern Model* modelPolyScale;
extern Model* modelPolyShuffle;
extern Model* modelPolySplit;
extern Model* modelPolyStereo;
extern Model* modelPolyTweakI;
extern Model* modelPolyTweakII;
extern Model* modelPolyVCMP;
extern Model* modelPhaseDrivenLFO;
extern Model* modelRandom4;
extern Model* modelRandomCurve;
extern Model* modelRingMod3;
extern Model* modelSampleAndUpdate;
extern Model* modelSHTH2;
extern Model* modelSHTH2x4;
extern Model* modelSign;
extern Model* modelSign4I;
extern Model* modelSign4II;
extern Model* modelLFO1;
extern Model* modelSLFO4ss;
extern Model* modelSLFO4st;
extern Model* modelSlopeDetector2;
extern Model* modelTinyLCMP2;
extern Model* modelTLFO;
extern Model* modelTuringMachine;
extern Model* modelTweak2I;
extern Model* modelTweak2II;
extern Model* modelTweak4I;
extern Model* modelTweak4II;
extern Model* modelTweak8;
extern Model* modelValueToBits;
extern Model* modelVCA2;
extern Model* modelVCA4I;
extern Model* modelVCA4II;
extern Model* modelVCMP2I;
extern Model* modelVCMP2II;
extern Model* modelWaveShaper2;


// Forward declarations for global settings helpers used by InfNoiseModule
bool getShowPortPrefix();
bool getShowLogoStatusLights();
void setShowLogoStatusLights(bool show);

/// Port-name prefixes applied by InfNoiseModuleWidget::applyPortPrefixes() when the global setting is on.
extern const char kInfNoiseMonoPortPrefix[];
extern const char kInfNoisePolyPortPrefix[];

//-----------------------------------------------------------------------------
// Common Infinite-Noise Module functionality
//-----------------------------------------------------------------------------
struct InfNoiseModule : Module {
    const uint32_t patternProcessParams = 0xff; // Process params each 256th cycle
    uint32_t cycle256 = getId() & 0xff; // Different start (0-255) for each module, updated by process
	uint32_t proParCalls256 = 0;  // Up to 256 counts of calling processParams (updated by postProcessParams)
    bool mustProcessParams = true; // Params MUST be processed at first/next cycle (e.g. after create, load, reset, sample-rate change or randomize)
    bool wasJustReset = false;  // Set true in InfNoiseModule.onReset, cleared in postProcessParams
    bool wasJustLoaded = false;  // Set true in InfNoiseModule.dataFromJson, cleared in postProcessParams
    const int currentJson = 2;  // Increment for "breaking changes" to json-format
    int jsonVersion = currentJson; // Used to detect if the module has been saved with a previous json-version of the plugin
    processQuality prevProcessQuality = pq_audioRate;  // Used to detect if process-quality has changed
    voltRange prevOutClipRange = vr_mp12;              // Used to detect if clipping-range has changed

    // Quality, quantize and clip-range
    actReqValue<bool> autoProcQuality = actReqValue<bool>(false); // Process at each cycle
    actReqValue<processQuality> procQuality = actReqValue<processQuality>(pq_audioRate); // Process at each cycle
    actReqValue<voltRange> outClipRange = actReqValue<voltRange>(vr_mp12); // -12V to 12V
    actReqValue<quantizeMode> outQuantize = actReqValue<quantizeMode>(qm_off); // Quantize off
    float procSampleTime = 1.f / 48000.f; // args.sampleTime multiplied by processQualityCycles[procQuality] (updated by preProcessParams/postProcessParams)

    // Gate-detect and gate-high/low
    actReqValue<trueDetectValue> gateDetHigh = actReqValue<trueDetectValue>(td_gateHigh); // Detect gate high
    actReqValue<voltValue> gateOutHigh = actReqValue<voltValue>(v_GateHigh); // Volt-output for gate high
    actReqValue<voltValue> gateOutLow = actReqValue<voltValue>(v_GateLow); // Volt-output for gate low

    // Trigger-detect and trigger-high/low
    actReqValue<trueDetectValue> trigDetHigh = actReqValue<trueDetectValue>(td_triggerHigh); // Detect trigger high
    actReqValue<trueDetectValue> trigDetLow = actReqValue<trueDetectValue>(td_triggerLow); // Detect trigger low
    actReqValue<voltValue> trigOutHigh = actReqValue<voltValue>(v_TriggerHigh); // Volt-output for trigger high
    actReqValue<voltValue> trigOutLow = actReqValue<voltValue>(v_TriggerLow); // Volt-output for trigger low

    // Features (decendants should set/overwrite these in their constructor)
    bool haveProcQuality = false;  // Adds menu to specify process-quality
    bool haveAutoProcQuality = false;  // Adds menu to specify auto process-quality
    bool haveOutQuantize = false;  // Adds menu to specify output quantize
    bool haveOutClipRange = false;  // Adds menu to specify output clip-range
    bool haveGateDetect = false;  // Adds menu to specify gate-detect input-level
    bool haveGateHighLow = false;  // Adds menu to specify gate-high/low output-levels
    bool haveTrigDetect = false;  // Adds menu to specify trigger-detect input-levels  
    bool haveTrigHighLow = false;  // Adds menu to specify trigger-high/low output-levels

    /// @brief Sets a param (knob) to a volt-interval value (for use from context menus).
    /// @param paramId Param index (e.g. from module's ParamId enum).
    /// @param interval The note-interval enum value (-11/12 .. 0 .. +11/12 V).
    void setParamKnobToVoltInterval(int paramId, voltIntervalValue interval) {
        params[paramId].setValue(voltIntervalValues[interval]);
    }

    /// @brief Sets a param (knob) to a volt-level value (for use from context menus).
    /// @param paramId Param index (e.g. from module's ParamId enum).
    /// @param v The volt-level enum value (e.g. -10V .. 0 .. +10V).
    void setParamKnobToVolt(int paramId, voltValue v) {
        params[paramId].setValue(voltValues[v]);
    }

    /// @brief Whether global port-prefix labels (m)/(p) are currently shown.
    bool showPortPrefix() const { return getShowPortPrefix(); }

    /// @brief Mono port prefix when shown, otherwise empty. Use when assigning inputInfos/outputInfos names.
    std::string monoPortPrefix() const { return getShowPortPrefix() ? kInfNoiseMonoPortPrefix : ""; }

    /// @brief Poly port prefix when shown, otherwise empty. Use when assigning inputInfos/outputInfos names.
    std::string polyPortPrefix() const { return getShowPortPrefix() ? kInfNoisePolyPortPrefix : ""; }

    void onSampleRateChange() override {
        mustProcessParams = true;  // Ensure processParams is called by process
    }

    void onReset(const ResetEvent& e) override {
        Module::onReset(e);
        proParCalls256 = 0;
        wasJustReset = true;
        mustProcessParams = true;  // Ensure processParams is called by process

        // Quality, quantize and clip-range
        autoProcQuality.setBoth(haveAutoProcQuality);
        procQuality.setBoth(processQuality::pq_audioRate);
        outClipRange.setBoth(voltRange::vr_mp12);
        outQuantize.setBoth(quantizeMode::qm_off);

        // Gate-detect and gate-high/low
        gateDetHigh.setBoth(td_gateHigh);
        gateOutHigh.setBoth(v_GateHigh);
        gateOutLow.setBoth(v_GateLow);

        // Trigger-detect and trigger-high/low
        trigDetHigh.setBoth(td_triggerHigh);
        trigDetLow.setBoth(td_triggerLow);
        trigOutHigh.setBoth(v_TriggerHigh);
        trigOutLow.setBoth(v_TriggerLow);
    }

    void onRandomize(const RandomizeEvent& e) override {
        Module::onRandomize(e);
        mustProcessParams = true;  // Ensure processParams is called by process
    }

    // Don't overwrite in decendants
    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        
        json_object_set_new(rootJ, "jsonVersion", json_integer(currentJson));

        // Quality, quantize and clip-range
        if (haveProcQuality) {
            json_object_set_new(rootJ, "procQuality", json_integer((int)procQuality.req));
            if (haveAutoProcQuality)
				json_object_set_new(rootJ, "autoProcQuality", json_boolean(autoProcQuality.req));
        }
        if (haveOutQuantize)
            json_object_set_new(rootJ, "outQuantize", json_integer((int)outQuantize.req));
        if (haveOutClipRange)
            json_object_set_new(rootJ, "outClipRange", json_integer((int)outClipRange.req));

        // Gate-detect and gate-high/low
        if (haveGateDetect) {
            json_object_set_new(rootJ, "gateDetHigh", json_integer((int)gateDetHigh.req));
        }
        if (haveGateHighLow) {
            json_object_set_new(rootJ, "gateOutHigh", json_integer((int)gateOutHigh.req));
            json_object_set_new(rootJ, "gateOutLow", json_integer((int)gateOutLow.req));
        }

        // Trigger-detect and trigger-high/low
        if (haveTrigDetect) {
            json_object_set_new(rootJ, "trigDetHigh", json_integer((int)trigDetHigh.req));
            json_object_set_new(rootJ, "trigDetLow", json_integer((int)trigDetLow.req));
        }
        if (haveTrigHighLow){
            json_object_set_new(rootJ, "trigOutHigh", json_integer((int)trigOutHigh.req));
            json_object_set_new(rootJ, "trigOutLow", json_integer((int)trigOutLow.req));
        }

        dataToJson(rootJ);
        return rootJ;
    }

    virtual void dataToJson(json_t* rootJ) {
        // Override this in decendants, in stead of using dataToJson()
    }

    // Decendants should call this first in their dataFromJson() method
    void dataFromJson(json_t* rootJ) override {
        jsonVersion = getJsonInt(rootJ, "jsonVersion", currentJson);
        mustProcessParams = true;  // Ensure processParams is called by process
        wasJustLoaded = true;

        // Quality, quantize and clip-range (sets default values if not found)
        procQuality.setBoth((processQuality)getJsonInt(rootJ, "procQuality", (int)processQuality::pq_audioRate));
    	autoProcQuality.setBoth(getJsonBool(rootJ, "autoProcQuality", haveAutoProcQuality));
        outQuantize.setBoth((quantizeMode)getJsonInt(rootJ, "outQuantize", (int)quantizeMode::qm_off));
        outClipRange.setBoth((voltRange)getJsonInt(rootJ, "outClipRange", (int)voltRange::vr_mp12));
        
        // Gate-detect and gate-high/low (sets default values if not found)
        gateDetHigh.setBoth((trueDetectValue)getJsonInt(rootJ, "gateDetHigh", (int)td_gateHigh));
        gateOutHigh.setBoth((voltValue)getJsonInt(rootJ, "gateOutHigh", (int)v_GateHigh));
        gateOutLow.setBoth((voltValue)getJsonInt(rootJ, "gateOutLow", (int)v_GateLow));

        // Trigger-detect and trigger-high/low (sets default values if not found)
        trigDetHigh.setBoth((trueDetectValue)getJsonInt(rootJ, "trigDetHigh", (int)td_triggerHigh));
        trigDetLow.setBoth((trueDetectValue)getJsonInt(rootJ, "trigDetLow", (int)td_triggerLow));
        trigOutHigh.setBoth((voltValue)getJsonInt(rootJ, "trigOutHigh", (int)v_TriggerHigh));
        trigOutLow.setBoth((voltValue)getJsonInt(rootJ, "trigOutLow", (int)v_TriggerLow));
    }

    /// @brief Decendants should call this in BEGINNING of their processParams method.
    /// Updates various feature-values (quality, quantize, clip-range, etc.)
    /// @param args ProcessArgs from the process method
    void preProcessParams(const ProcessArgs& args) {
        // Quality, quantize and clip-range
        prevProcessQuality = procQuality.act;
        procQuality.updateActual();
        autoProcQuality.updateActual();
        outQuantize.updateActual();
        outClipRange.updateActual();
        float sampleRate = args.sampleRate > 0.f ? args.sampleRate : 44100;  // fallback to 44.1 kHz
        procSampleTime = processQualityCycles[procQuality.act] / sampleRate;

        // Gate-detect and gate-high/low
        gateDetHigh.updateActual();
        gateOutHigh.updateActual();
        gateOutLow.updateActual();

        // Trigger-detect and trigger-high/low
        trigDetHigh.updateActual();
        trigDetLow.updateActual();
        trigOutHigh.updateActual();
        trigOutLow.updateActual();
    }

    /// @brief Decendants should call this in END of their processParams method.
    /// Clears "mustProcessParams" and updates procSampleTime when auto
    /// process-quality is used (as it might have changed during ProcessParams).
    void postProcessParams(const ProcessArgs& args) {
        // Update shared logo lights (process-quality + clipping-range)
        refreshProcessQualityLights(false);
        refreshClipRangeLights(false);

        // Set status-variables
        mustProcessParams = args.sampleRate < 1.f; // force extra call if sample-rate is invalid
        wasJustLoaded = false;
        wasJustReset = false;
        cycle256 &= 0xff;

        // Number of times processParams has been called.
		// Typically processParams is called each 256th cycle,
		// so proParCalls256 = 0x00 occurs every 65536 (256*256) cycles
        // (every 1.365s @ 48kHz / 1.486s @ 44.1kHz).
        proParCalls256 = (proParCalls256 + 1) & 0xff;

        // If using auto-process-quality, process-quality might have changed
        if (haveAutoProcQuality && autoProcQuality.act) {
            float sampleRate = args.sampleRate > 0.f ? args.sampleRate : 44100;  // fallback to 44.1 kHz
            procSampleTime = processQualityCycles[procQuality.act] / sampleRate;
        }
    }

    /// @brief Update process-quality logo lights (ids 0 and 1).
    /// @param forceUpdate If true, always update regardless of mustProcessParams/prevProcessQuality.
    void refreshProcessQualityLights(bool forceUpdate = false) {
        bool showLogoLights = getShowLogoStatusLights();

        if (!forceUpdate && !(mustProcessParams || prevProcessQuality != procQuality.act)) {
            return;
        }

        prevProcessQuality = procQuality.act; // track last value for change detection

        if (lights.size() >= 2) {
            float green = showLogoLights ? processQualityGreenBrightness[procQuality.act] : 0.f;
            float red   = showLogoLights ? processQualityRedBrightness[procQuality.act]   : 0.f;
            lights[0].setBrightness(green);
            lights[1].setBrightness(red);

            // Always keep tooltip/hint text in sync, even when lights are hidden
            if (lightInfos.size() > 0 && lightInfos[0])
                lightInfos[0]->name = processQualityRateNames[procQuality.act];
        }
    }

    /// @brief Update clipping-range logo lights (base id 2, uses 2 and 3).
    /// @param forceUpdate If true, always update regardless of mustProcessParams/prevOutClipRange.
    void refreshClipRangeLights(bool forceUpdate = false) {
        bool showLogoLights = getShowLogoStatusLights();

        if (!forceUpdate && !(mustProcessParams || prevOutClipRange != outClipRange.act)) {
            return;
        }

        prevOutClipRange = outClipRange.act; // track last value for change detection
        voltRange lightClipRange = (haveOutClipRange)
            ? outClipRange.act
            : vr_off;

        if (lights.size() >= 4) {
            float green = showLogoLights ? clipRangeGreenBrightness[lightClipRange] : 0.f;
            float red   = showLogoLights ? clipRangeRedBrightness[lightClipRange]   : 0.f;
            lights[2].setBrightness(green);
            lights[3].setBrightness(red);

            // Always keep tooltip/hint text in sync, even when lights are hidden
            if (lightInfos.size() > 2 && lightInfos[2])
                lightInfos[2]->name = getClipRangeLightName(lightClipRange);
        }
    }
};


//-----------------------------------------------------------------------------
// Global panel theme for all Infinite-Noise modules
//-----------------------------------------------------------------------------
enum panelThemeType {
    theme_Auto = 0,   // Follow Rack's global panel preference
    theme_White = 1,  // Force light panels
    theme_Black = 2   // Force dark panels
};

panelThemeType getPanelTheme();
void setPanelTheme(panelThemeType theme);

bool getShowPortPrefix();
void setShowPortPrefix(bool show);

bool getShowLogoStatusLights();
void setShowLogoStatusLights(bool show);

enum panelMountMode {
	mount_None = 0,
	mount_MountingPointsOnly = 1,
	mount_TwoScrews = 2,
	mount_FourScrews = 3,
	mount_Random = 4   // default — current random screw layout for new panels
};

panelMountMode getPanelMountMode();
void setPanelMountMode(panelMountMode mode);
const std::vector<std::string>& getMountStyleNames();

struct InfNoiseDisableOverlayManager;

/// MenuItem that refreshes ui::MenuItem::disabled each frame (e.g. while menu stays open on Ctrl/Cmd-click).
struct DynamicDisabledMenuItem : ui::MenuItem {
	std::function<bool()> disabledWhen;

	void step() override {
		if (disabledWhen)
			disabled = disabledWhen();
		ui::MenuItem::step();
	}
};

//-----------------------------------------------------------------------------
// Common Infinite-Noise ModuleWidget functionality
//-----------------------------------------------------------------------------
struct InfNoiseModuleWidget : ModuleWidget {
    std::string lightSkinPath = "";
    std::string darkSkinPath = "";
    InfNoiseDisableOverlayManager* disableOverlayManager = nullptr;
    /// Cached effectivePanelIsDark() for disable overlays (theme Auto + Rack panel preference).
    bool lastOverlayPanelIsDarkCache = false;

    /// Last-applied global settings (step() compares to get*(); avoids rack-wide iteration in setters).
    panelThemeType appliedPanelTheme = theme_Auto;
    bool appliedPortPrefix = true;
    bool appliedLogoStatusLights = true;

    ~InfNoiseModuleWidget() override;

    /// Per-frame: sync disable-overlay tint when Rack panel preference changes (Auto theme). Subclasses that override step() must call InfNoiseModuleWidget::step().
    void step() override;

    InfNoiseDisableOverlayManager& getDisableOverlayManager();
    InfNoiseDisableOverlayManager* tryGetDisableOverlayManager() const {
        return disableOverlayManager;
    }

    panelMountMode mountMode = mount_Random;  // snapshot at panel creation from global setting
    float hpWidth = 0.f;  // Panel-width in HP (set by initializeWidget)

    /// @brief Random screw layout for mount_Random (sets corner flags directly).
    void getRandomScrewCorners(bool& topLeftScrew, bool& topRightScrew,
        bool& bottomLeftScrew, bool& bottomRightScrew)
    {
        topLeftScrew = topRightScrew = bottomLeftScrew = bottomRightScrew = false;

        if (hpWidth <= 1.f)
            return;

        if (hpWidth > 8.f) {
            topLeftScrew = topRightScrew = bottomLeftScrew = bottomRightScrew = true;
            return;
        }

        // Chance for four screws increase with width (10% at 2HP, up to 70% at 8HP)
        if (rack::random::get<float>() < 0.1f * hpWidth - 0.1f) {
            topLeftScrew = topRightScrew = bottomLeftScrew = bottomRightScrew = true;
            return;
        }

        // Chance for 1 screw decreases with width (50% at 2HP, down to 35% at 3HP)
        if (hpWidth < 4.f && rack::random::get<float>() < 0.8f - 0.15f * hpWidth) {
            if (rack::random::get<float>() < 0.85f) {
                if (rack::random::get<float>() < 0.5f)
                    topLeftScrew = true;
                else
                    topRightScrew = true;
            } else {
                if (rack::random::get<float>() < 0.5f)
                    bottomLeftScrew = true;
                else
                    bottomRightScrew = true;
            }
            return;
        }

        // Two screws: 75% diagonal, otherwise vertical pair
        if (rack::random::get<float>() < 0.75f) {
            if (rack::random::get<float>() < 0.5f) {
                topLeftScrew = true;
                bottomRightScrew = true;
            } else {
                topRightScrew = true;
                bottomLeftScrew = true;
            }
        } else {
            if (rack::random::get<float>() < 0.5f) {
                topLeftScrew = true;
                bottomLeftScrew = true;
            } else {
                topRightScrew = true;
                bottomRightScrew = true;
            }
        }
    }

    /// @brief Adds screws or mounting points at all potential corner positions
    /// (4 corners if hpWidth <= 4, otherwise offset inwards by 1 position)
    void addScrewsAndHoles() {
        if (hpWidth <= 1.f || mountMode == mount_None)
            return;

        bool topLeftScrew = false;
        bool topRightScrew = false;
        bool bottomLeftScrew = false;
        bool bottomRightScrew = false;

        switch (mountMode) {
        case mount_MountingPointsOnly:
            break;
        case mount_TwoScrews:
            topLeftScrew = true;
            bottomRightScrew = true;
            break;
        case mount_FourScrews:
            topLeftScrew = topRightScrew = bottomLeftScrew = bottomRightScrew = true;
            break;
        case mount_Random:
            getRandomScrewCorners(topLeftScrew, topRightScrew, bottomLeftScrew, bottomRightScrew);
            break;
        default:
            break;
        }

        const bool showScrews = (mountMode != mount_MountingPointsOnly);
        const bool needMountingPoints = !topLeftScrew || !topRightScrew
            || !bottomLeftScrew || !bottomRightScrew;

        int leftIdx = (hpWidth <= 4.f) ? 0 : 1;
        int rightIdx = (hpWidth <= 4.f) ? (int)(hpWidth - 1.f) : (int)(hpWidth - 2.f);
        if (rightIdx < leftIdx)
            rightIdx = leftIdx;

        std::shared_ptr<Svg> holeSvg = nullptr;
        if (needMountingPoints) {
            holeSvg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/ScrewHole.svg"));
        }

        const float topStripCenterY = 7.5f;
        const float bottomStripCenterY = RACK_GRID_HEIGHT - 7.5f;
        const float leftCenterX = (leftIdx + 0.5f) * RACK_GRID_WIDTH;
        const float rightCenterX = (rightIdx + 0.5f) * RACK_GRID_WIDTH;

        if (topLeftScrew) {
            if (showScrews) {
                addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH * leftIdx, 0)));
            }
        } else if (holeSvg) {
            auto topLeft = new SvgWidget();
            topLeft->setSvg(holeSvg);
            topLeft->box.pos = Vec(leftCenterX - topLeft->box.size.x / 2.f, topStripCenterY - topLeft->box.size.y / 2.f);
            addChild(topLeft);
        }

        if (topRightScrew) {
            if (showScrews) {
                addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH * rightIdx, 0)));
            }
        } else if (holeSvg) {
            auto topRight = new SvgWidget();
            topRight->setSvg(holeSvg);
            topRight->box.pos = Vec(rightCenterX - topRight->box.size.x / 2.f, topStripCenterY - topRight->box.size.y / 2.f);
            addChild(topRight);
        }

        if (bottomLeftScrew) {
            if (showScrews) {
                addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH * leftIdx, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
            }
        } else if (holeSvg) {
            auto bottomLeft = new SvgWidget();
            bottomLeft->setSvg(holeSvg);
            bottomLeft->box.pos = Vec(leftCenterX - bottomLeft->box.size.x / 2.f, bottomStripCenterY - bottomLeft->box.size.y / 2.f);
            addChild(bottomLeft);
        }

        if (bottomRightScrew) {
            if (showScrews) {
                addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH * rightIdx, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
            }
        } else if (holeSvg) {
            auto bottomRight = new SvgWidget();
            bottomRight->setSvg(holeSvg);
            bottomRight->box.pos = Vec(rightCenterX - bottomRight->box.size.x / 2.f, bottomStripCenterY - bottomRight->box.size.y / 2.f);
            addChild(bottomRight);
        }
    }

    /// @brief Add the logo to the panel (2 different sizes, based on hpWidth). 
    // Returns the logo widget for positioning other widgets (e.g. procQual light).
    SvgWidget* addLogo() {
        auto logo = new SvgWidget();
        /*
        logo->setSvg(APP->window->loadSvg(asset::plugin(
            pluginInstance,
            hpWidth <= 2 ? "res/logos/InfNoiseLogoSmall.svg" : "res/logos/InfNoiseLogoLarge.svg"
        )));
        */
        logo->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/logos/InfNoiseLogoSmall.svg")));
        
        // Small 17x11, Large 29x18; center horizontally, vertical center at 348.764
        const float panelCenterX = (hpWidth * 15.f) / 2.f;
        const float logoCenterY = 358.f;
        logo->box.pos = Vec(panelCenterX - logo->box.size.x / 2.f, logoCenterY - logo->box.size.y / 2.f);
        addChild(logo);
        return logo;
    }

    inline std::string getFullSkinPath(const std::string& skinPath, const std::string& skinSuffix = "") {
        return skinPath + skinSuffix + ".svg";
    }

    /// @brief Apply or remove (m)/(p) prefix on all input/output port names from global setting and widget type. Implemented in plugin.cpp.
    void applyPortPrefixes();

    /// @brief Whether the panel is currently dark: same rules as applyPanelTheme() (theme_White / theme_Black / theme_Auto + Rack panel preference).
    bool effectivePanelIsDark() const {
        if (lightSkinPath.empty()) {
            return false;
        }
        if (darkSkinPath.empty()) {
            return false;
        }
        switch (getPanelTheme()) {
        case theme_White:
            return false;
        case theme_Black:
            return true;
        case theme_Auto:
        default:
            return settings::preferDarkPanels;
        }
    }

    /// @brief Sync disable-overlay tint with effectivePanelIsDark(). Implemented in plugin.cpp.
    void refreshDisableOverlayThemeColors();

    /// @brief Internal: follow Rack panel preference when InfNoise theme is Auto (called from step()).
    void tickDisableOverlayThemeFromRack();

    /// @brief Apply global theme / port-prefix / logo-light settings when they differ from cache (cheap checks only).
    void syncInfNoiseGlobalSettings();

    /// @brief Apply the current global panel theme to this widget.
    void applyPanelTheme() {
        // Require at least a light skin path
        if (lightSkinPath.empty()) {
            return;
        }

        auto light = asset::plugin(pluginInstance, lightSkinPath);

        // If no dark skin is available, always use light
        if (darkSkinPath.empty()) {
            setPanel(createPanel(light));
            refreshDisableOverlayThemeColors();
            return;
        }

        auto dark = asset::plugin(pluginInstance, darkSkinPath);

        switch (getPanelTheme()) {
        case theme_White:
            setPanel(createPanel(light));
            break;
        case theme_Black:
            setPanel(createPanel(dark));
            break;
        case theme_Auto:
        default:
            // Let Rack decide based on its own panel preference
            setPanel(createPanel(light, dark));
            break;
        }
        refreshDisableOverlayThemeColors();
    }

    /// @brief Initialize the widget with a module and skin path.
    /// Screws/mounting points (per global mount style) and proqQual-light are added to the panel.
    /// @param module Module to initialize
    /// @param skinPath Path of default/light skin (without ".svg")
    void initializeWidget(Module* module, const std::string& skinPath) {
        lightSkinPath = skinPath + ".svg";
        darkSkinPath = skinPath + "_dark.svg";

        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, lightSkinPath), 
                        asset::plugin(pluginInstance, darkSkinPath)));

        // Calculate panel width in HP
        hpWidth = std::floorf(box.size.x / RACK_GRID_WIDTH + 0.5f); 
        
        // Add logo
        SvgWidget* logo = addLogo();

        mountMode = getPanelMountMode();
        if (mountMode != mount_None)
            addScrewsAndHoles();

        // Add clipping-range and process-quality lights around the logo, vertically centered with logo
        const int clipRangeLgtId = 2; // uses indices 2 and 3 (green/red)
        const int procQualLgtId = 0;  // uses indices 0 and 1 (green/red)
        const float logoMargin = 2.5f;
        const float proqQualLgtY = 358.f; // same vertical center as logo
        const float clipRangeLgtX = logo->box.pos.x - logoMargin;
        const float proqQualLgtX = logo->box.pos.x + logo->box.size.x + logoMargin;
        addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(clipRangeLgtX, proqQualLgtY), module, clipRangeLgtId)); 
        addChild(createLightCentered<TinyLight<GreenRedLight>>(Vec(proqQualLgtX, proqQualLgtY), module, procQualLgtId)); 

        // Apply global panel theme (Auto / Light / Dark)
        applyPanelTheme();
        appliedPanelTheme = getPanelTheme();
        appliedPortPrefix = getShowPortPrefix();
        appliedLogoStatusLights = getShowLogoStatusLights();
    }

    void onAdd(const AddEvent& e) override {
        ModuleWidget::onAdd(e);
        applyPortPrefixes();
        appliedPortPrefix = getShowPortPrefix();
        appliedLogoStatusLights = getShowLogoStatusLights();
    }

    void appendInfNoiseMenuItems(Menu* menu) {
        InfNoiseModule* module = dynamic_cast<InfNoiseModule*>(this->module);
	    assert(module);

        if (module->haveOutQuantize || module->haveOutClipRange || module->haveProcQuality) {
            menu->addChild(new MenuSeparator);

            if (module ->haveOutQuantize) {
                std::vector<std::string> quantModeNames = getquantizeModeNames();
                menu->addChild(createIndexPtrSubmenuItem("Quantize mode", quantModeNames, 
                    &module->outQuantize.req));
            }
            if (module->haveOutClipRange) {
                std::vector<std::string> clipRangeNames = getVoltRangesNames(true);
                menu->addChild(createIndexPtrSubmenuItem("Clipping range", clipRangeNames,
                    &module->outClipRange.req));
            }
            if (module->haveProcQuality) {
                std::vector<std::string> procQualNames = getProcessQualityNames();
                DynamicDisabledMenuItem* procQualItem = createIndexSubmenuItem<DynamicDisabledMenuItem>(
                    "Process quality", procQualNames,
                    [=]() { return (size_t)module->procQuality.req; },
                    [=](size_t index) { module->procQuality.req = (processQuality)index; });
                // Disable manual selection while Auto process-quality is on (refreshed each frame for Ctrl/Cmd-open menu)
                procQualItem->disabledWhen = [=]() {
                    return module->haveAutoProcQuality && module->autoProcQuality.req;
                };
                menu->addChild(procQualItem);

                if (module->haveAutoProcQuality)
                    menu->addChild(createBoolPtrMenuItem("Auto select process quality", "", &module->autoProcQuality.req));
            }
        }

		bool needGateNames = module->haveGateDetect || module->haveGateHighLow;
		bool needTrigNames = module->haveTrigDetect || module->haveTrigHighLow;
        if (needGateNames || needTrigNames)
        {
            std::vector<std::string> voltNames = getVoltValuesNames();
            std::vector<std::string> highDetectNames = getTrueDetectVoltNames();
            std::vector<std::string> lowDetectNames = getFalseDetectVoltNames();

            if (needGateNames) {
                menu->addChild(createSubmenuItem("Gates", "",
                    [=](Menu* menu) {
                        if (module->haveGateDetect) {
                            menu->addChild(createIndexPtrSubmenuItem("Gate-high detect-level", highDetectNames,
                                &module->gateDetHigh.req));
                        }
                        if (module->haveGateHighLow) {
                            menu->addChild(createIndexPtrSubmenuItem("Gate-high output-level", voltNames,
                                &module->gateOutHigh.req));
                            menu->addChild(createIndexPtrSubmenuItem("Gate-low output-level", voltNames,
                                &module->gateOutLow.req));
                        }
                    }
                ));
            }
            if (needTrigNames) {
                menu->addChild(createSubmenuItem("Triggers", "",
                    [=](Menu* menu) {
                        if (module->haveTrigDetect) {
                            menu->addChild(createIndexPtrSubmenuItem("Trigger-high detect-level", highDetectNames,
                                &module->trigDetHigh.req));
                            menu->addChild(createIndexPtrSubmenuItem("Trigger-low detect-level", lowDetectNames,
                                &module->trigDetLow.req));
                        }
                        if (module->haveTrigHighLow) {
                            menu->addChild(createIndexPtrSubmenuItem("Trigger-high output-level", voltNames,
                                &module->trigOutHigh.req));
                            menu->addChild(createIndexPtrSubmenuItem("Trigger-low output-level", voltNames,
                                &module->trigOutLow.req));
                        }
                    }
                ));
            }
        }

        // Global Infinite-Noise settings (panel theme, etc.)
        menu->addChild(new MenuSeparator);
        menu->addChild(createSubmenuItem("Global", "",
            [=](Menu* globalMenu) {
                // Select panel theme (Auto / Light / Dark)
                globalMenu->addChild(createSubmenuItem("Select panel theme", "",
                    [=](Menu* themeMenu) {
                        panelThemeType cur = getPanelTheme();

                        auto addThemeItem = [&](const char* label, panelThemeType value) {
                            bool isCurrent = (cur == value);
                            std::string text = std::string(isCurrent ? "● " : "  ") + label;
                            themeMenu->addChild(createMenuItem(text, "",
                                [=]() { setPanelTheme(value); }
                            ));
                        };

                        addThemeItem("Auto (Rack default)", theme_Auto);
                        addThemeItem("Light",               theme_White);
                        addThemeItem("Dark",                theme_Black);
                    }
                ));

                // Port prefix (show (m)/(p) in port labels or not)
                globalMenu->addChild(createSubmenuItem("Port prefix", "(m) or (p) for mono/poly",
                    [=](Menu* prefixMenu) {
                        bool cur = getShowPortPrefix();
                        auto addPrefixItem = [&](const char* label, bool value) {
                            bool isCurrent = (cur == value);
                            std::string text = std::string(isCurrent ? "● " : "  ") + label;
                            prefixMenu->addChild(createMenuItem(text, "",
                                [=]() { setShowPortPrefix(value); }
                            ));
                        };
                        addPrefixItem("Show (m)/(p)", true);
                        addPrefixItem("Hide (m)/(p)", false);
                    }
                ));

                // Panel screw / mounting-point layout for newly placed modules
                globalMenu->addChild(createIndexSubmenuItem("Mount style", getMountStyleNames(),
                    [=]() { return (size_t)getPanelMountMode(); },
                    [=](size_t index) { setPanelMountMode((panelMountMode)index); }
                ));

                // Logo status lights on/off (process-quality + clipping-range)
                globalMenu->addChild(createSubmenuItem("Logo status lights", "",
                    [=](Menu* statusMenu) {
                        bool cur = getShowLogoStatusLights();
                        auto addStatusItem = [&](const char* label, bool value) {
                            bool isCurrent = (cur == value);
                            std::string text = std::string(isCurrent ? "● " : "  ") + label;
                            statusMenu->addChild(createMenuItem(text, "",
                                [=]() { setShowLogoStatusLights(value); }
                            ));
                        };
                        addStatusItem("Show logo status lights", true);
                        addStatusItem("Hide logo status lights", false);
                    }
                ));
            }
        ));
    }
};
