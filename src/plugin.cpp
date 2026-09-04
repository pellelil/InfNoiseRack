// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"

Plugin *pluginInstance;

const char kInfNoiseMonoPortPrefix[] = "(m) ";
const char kInfNoisePolyPortPrefix[] = "(p) ";

InfNoiseModuleWidget::~InfNoiseModuleWidget() {
	delete disableOverlayManager;
	disableOverlayManager = nullptr;
}

InfNoiseDisableOverlayManager& InfNoiseModuleWidget::getDisableOverlayManager() {
	if (!disableOverlayManager) {
		disableOverlayManager = new InfNoiseDisableOverlayManager(this);
	}
	else {
		disableOverlayManager->setOwner(this);
	}

	return *disableOverlayManager;
}

void InfNoiseModuleWidget::refreshDisableOverlayThemeColors() {
	const bool d = effectivePanelIsDark();
	lastOverlayPanelIsDarkCache = d;
	if (!disableOverlayManager) {
		return;
	}
	disableOverlayManager->refreshOverlayThemeColors(d);
}

void InfNoiseModuleWidget::tickDisableOverlayThemeFromRack() {
	if (!disableOverlayManager) {
		return;
	}
	if (getPanelTheme() != theme_Auto || darkSkinPath.empty()) {
		return;
	}
	const bool cur = effectivePanelIsDark();
	if (cur != lastOverlayPanelIsDarkCache) {
		refreshDisableOverlayThemeColors();
	}
}

void InfNoiseModuleWidget::syncInfNoiseGlobalSettings() {
	if (getPanelTheme() != appliedPanelTheme) {
		applyPanelTheme();
		appliedPanelTheme = getPanelTheme();
	}
	if (getShowPortPrefix() != appliedPortPrefix) {
		applyPortPrefixes();
		appliedPortPrefix = getShowPortPrefix();
	}
	if (getShowLogoStatusLights() != appliedLogoStatusLights) {
		if (auto* infMod = dynamic_cast<InfNoiseModule*>(module)) {
			infMod->refreshProcessQualityLights(true);
			infMod->refreshClipRangeLights(true);
		}
		appliedLogoStatusLights = getShowLogoStatusLights();
	}
}

void InfNoiseModuleWidget::step() {
	syncInfNoiseGlobalSettings();
	tickDisableOverlayThemeFromRack();
	ModuleWidget::step();
}

void InfNoiseModuleWidget::applyPortPrefixes() {
	engine::Module* mod = getModule();
	if (!mod) return;
	bool show = getShowPortPrefix();
	const std::string prefixM = kInfNoiseMonoPortPrefix;
	const std::string prefixP = kInfNoisePolyPortPrefix;
	for (widget::Widget* w : getInputs()) {
		app::PortWidget* pw = dynamic_cast<app::PortWidget*>(w);
		if (!pw) continue;
		int id = pw->portId;
		engine::PortInfo* info = mod->getInputInfo(id);
		if (!info) continue;
		std::string& name = info->name;
		if (show) {
			if (name.size() < 4 || (name.substr(0, 4) != prefixM && name.substr(0, 4) != prefixP)) {
				bool isPoly = (dynamic_cast<infNoiseThemedPolyPort*>(pw) != nullptr);
				name = (isPoly ? prefixP : prefixM) + name;
			}
		} else {
			if (name.size() >= 4 && (name.substr(0, 4) == prefixM || name.substr(0, 4) == prefixP))
				name = name.substr(4);
		}
	}
	for (widget::Widget* w : getOutputs()) {
		app::PortWidget* pw = dynamic_cast<app::PortWidget*>(w);
		if (!pw) continue;
		int id = pw->portId;
		engine::PortInfo* info = mod->getOutputInfo(id);
		if (!info) continue;
		std::string& name = info->name;
		if (show) {
			if (name.size() < 4 || (name.substr(0, 4) != prefixM && name.substr(0, 4) != prefixP)) {
				bool isPoly = (dynamic_cast<infNoiseThemedPolyPort*>(pw) != nullptr);
				name = (isPoly ? prefixP : prefixM) + name;
			}
		} else {
			if (name.size() >= 4 && (name.substr(0, 4) == prefixM || name.substr(0, 4) == prefixP))
				name = name.substr(4);
		}
	}
}

static std::string inSettingsFileName = asset::user("InfiniteNoise.json");
static const int gSettingsCurrentJson = 1; // Manually incremented for breaking changes to InfiniteNoise.json
static int gSettingsJsonVersion = gSettingsCurrentJson; // Loaded from file; defaults to 1 if missing
static panelThemeType gPanelTheme = theme_Auto;
static bool gShowPortPrefix = true;
static bool gShowLogoStatusLights = true;
static panelMountMode gPanelMountMode = mount_Random;

static const std::vector<std::string> gMountStyleNames = {
	"None",
	"Mounting points only",
	"Two screws",
	"Four screws",
	"Random"
};

const std::vector<std::string>& getMountStyleNames() {
	return gMountStyleNames;
}

void inLoadSettings() { // Load Infinite-Noise settings (panel theme, port prefix, etc.)
	FILE* file = fopen(inSettingsFileName.c_str(), "r");
	if (file) {
		json_error_t error;
		json_t* rootJ = json_loadf(file, 0, &error);

		gSettingsJsonVersion = getJsonInt(rootJ, "jsonVersion", 1);

		int val = getJsonInt(rootJ, "panelTheme", (int)gPanelTheme);
		if (val >= theme_Auto && val <= theme_Black) {
			gPanelTheme = (panelThemeType)val;
		}

		gShowPortPrefix = getJsonBool(rootJ, "showPortPrefix", gShowPortPrefix);
		gShowLogoStatusLights = getJsonBool(rootJ, "showLogoStatusLights", gShowLogoStatusLights);

		val = getJsonInt(rootJ, "panelMountMode", (int)gPanelMountMode);
		if (val >= mount_None && val <= mount_Random) {
			gPanelMountMode = (panelMountMode)val;
		}

		fclose(file);
		json_decref(rootJ);
	}
}

void inSaveSettings() { // Save Infinite-Noise settings (panel theme, port prefix, etc.)
	FILE* file = fopen(inSettingsFileName.c_str(), "w");
	if (file) {
		json_t* rootJ = json_object();

		gSettingsJsonVersion = gSettingsCurrentJson; // Always saved as current version
		json_object_set_new(rootJ, "jsonVersion", json_integer(gSettingsJsonVersion));

		json_object_set_new(rootJ, "panelTheme", json_integer((int)gPanelTheme));
		json_object_set_new(rootJ, "showPortPrefix", json_boolean(gShowPortPrefix));
		json_object_set_new(rootJ, "showLogoStatusLights", json_boolean(gShowLogoStatusLights));
		json_object_set_new(rootJ, "panelMountMode", json_integer((int)gPanelMountMode));
		json_dumpf(rootJ, file, JSON_INDENT(2) | JSON_REAL_PRECISION(9));
		fclose(file);
		json_decref(rootJ);
	}
}

panelThemeType getPanelTheme() {
	return gPanelTheme;
}

bool getShowPortPrefix() {
	return gShowPortPrefix;
}

bool getShowLogoStatusLights() {
	return gShowLogoStatusLights;
}

panelMountMode getPanelMountMode() {
	return gPanelMountMode;
}

void setShowPortPrefix(bool show) {
	if (gShowPortPrefix == show) {
		return;
	}
	gShowPortPrefix = show;
	inSaveSettings();
}

void setShowLogoStatusLights(bool show) {
	if (gShowLogoStatusLights == show) {
		return;
	}
	gShowLogoStatusLights = show;
	inSaveSettings();
}

void setPanelTheme(panelThemeType theme) {
	if (gPanelTheme == theme) {
		return;
	}

	gPanelTheme = theme;
	inSaveSettings();
}

void setPanelMountMode(panelMountMode mode) {
	if (gPanelMountMode == mode) {
		return;
	}
	if (mode < mount_None || mode > mount_Random) {
		return;
	}
	gPanelMountMode = mode;
	inSaveSettings();
}

void init(Plugin *p) {
	pluginInstance = p;

	// Add modules here
	p->addModel(modelADREnvelope);
	p->addModel(modelADSDREnvelope);
	p->addModel(modelEnvelopePhaseExpander);
	p->addModel(modelArm3XY);
	p->addModel(modelAutoScale4);
	p->addModel(modelBernoulliSwitch);
	p->addModel(modelBitsToValue);
	p->addModel(modelClamp4);
	p->addModel(modelCombine);
	p->addModel(modelCrossFadeSwitch1to4);
	p->addModel(modelCrossFadeSwitch4to1);
	p->addModel(modelCvToggle8);
	p->addModel(modelCvToGt);
	p->addModel(modelCvToGtTr8);
	p->addModel(modelCxFade1x2);
	p->addModel(modelCxFade4x1);
	p->addModel(modelDelta4);
	p->addModel(modelFlipFlop);
	p->addModel(modelFold);
	p->addModel(modelIncDecOffset);
	p->addModel(modelVCMP1);
	p->addModel(modelLCMP2);
	p->addModel(modelLCMP6x2);
	p->addModel(modelManCV8I);
	p->addModel(modelManCV8II);
	p->addModel(modelManGate8);
	p->addModel(modelManMix4I);
	p->addModel(modelManMix4II);
	p->addModel(modelManMix4st);
	p->addModel(modelManMute8);
	p->addModel(modelManPush2);
	p->addModel(modelManTrGtCv);
	p->addModel(modelManTrigger8);
	p->addModel(modelMerge2x4);
	p->addModel(modelMergeMult4);
	p->addModel(modelMult2x4);
	p->addModel(modelMute2);
	p->addModel(modelOnOffSwitch);
	p->addModel(modelPatch);
	p->addModel(modelPolyLCMP);
	p->addModel(modelPolyMerge);
	p->addModel(modelPolyOffset);
	p->addModel(modelPolyQuad);
	p->addModel(modelPolyScale);
	p->addModel(modelPolyShuffle);
	p->addModel(modelPolySplit);
	p->addModel(modelPolyStereo);
	p->addModel(modelPolyTweakI);
	p->addModel(modelPolyTweakII);
	p->addModel(modelPolyVCMP);
	p->addModel(modelPhaseDrivenLFO);
	p->addModel(modelRandom4);
	p->addModel(modelRandomCurve);
	p->addModel(modelRingMod3);
	p->addModel(modelSampleAndUpdate);
	p->addModel(modelSHTH2);
	p->addModel(modelSHTH2x4);
	p->addModel(modelSign);
	p->addModel(modelSign4I);
	p->addModel(modelSign4II);
	p->addModel(modelLFO1);
	p->addModel(modelSLFO4ss);
	p->addModel(modelSLFO4st);
	p->addModel(modelSlopeDetector2);
	p->addModel(modelTinyLCMP2);
	p->addModel(modelTLFO);
	p->addModel(modelTuringMachine);
	p->addModel(modelTweak2I);
	p->addModel(modelTweak2II);
	p->addModel(modelTweak4I);
	p->addModel(modelTweak4II);
	p->addModel(modelTweak8);
	p->addModel(modelValueToBits);
	p->addModel(modelVCA2);
	p->addModel(modelVCA4I);
	p->addModel(modelVCA4II);
	p->addModel(modelVCMP2I);
	p->addModel(modelVCMP2II);
	p->addModel(modelWaveShaper2);

	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
	inLoadSettings();  
}
