// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#pragma once
#include <rack.hpp>
#include <initializer_list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "plugin.hpp"
using namespace ::rack;

//-----------------------------------------------------------------------------
// Switches and buttons
//-----------------------------------------------------------------------------

enum buttonColor {
    bc_black, bc_red, bc_green, bc_blue,
    bc_redGreen
};

struct infNoiseTwoStageButton : SvgSwitch {
    buttonColor getOnColor(buttonColor color) {
        switch (color) {
        case bc_redGreen:
            return bc_green;
        default:
            return color;
        }
    }

    buttonColor getOffColor(buttonColor color) {
        switch (color) {
        case bc_redGreen:
            return bc_red;
        default:
            return bc_black;
        }
    }
};

/// @brief About same size as: TinyLight
struct infNoiseLtTinyButton : infNoiseTwoStageButton {
    infNoiseLtTinyButton() {
        momentary = false;
        box.size = Vec(3.537f, 3.537f); // Important for createParamCentered
    }

    void setup(buttonColor color, bool isMomentary = false) {
        momentary = isMomentary;

        switch (getOffColor(color)) {
        case bc_red:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseLtTinyRedButton.svg")));
            break;
        case bc_green:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseLtTinyGreenButton.svg")));
            break;
        case bc_blue:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseLtTinyBlueButton.svg")));
            break;
        default:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseLtTinyBlackButton.svg")));
            break;
        }

        switch (getOnColor(color)) {
        case bc_red:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseLtTinyRedButton.svg")));
            break;
        case bc_green:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseLtTinyGreenButton.svg")));
            break;
        case bc_blue:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseLtTinyBlueButton.svg")));
            break;
        default:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseLtTinyBlackButton.svg")));
            break;
        }
    }
};

/// @brief About same size as: SmallLight
struct infNoiseLtSmallButton : infNoiseTwoStageButton {
    infNoiseLtSmallButton() {
        momentary = false;
        box.size = Vec(6.968f, 6.968f); // Important for createParamCentered
    }

    void setup(buttonColor color, bool isMomentary = false) {
        momentary = isMomentary;
        switch (getOffColor(color)) {
        case bc_red:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseLtSmallRedButton.svg")));
            break;
        case bc_green:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseLtSmallGreenButton.svg")));
            break;
        case bc_blue:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseLtSmallBlueButton.svg")));
            break;
        default:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseLtSmallBlackButton.svg")));
            break;
        }

        switch (getOnColor(color)) {
        case bc_red:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseLtSmallRedButton.svg")));
            break;
        case bc_green:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseLtSmallGreenButton.svg")));
            break;
        case bc_blue:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseLtSmallBlueButton.svg")));
            break;
        default:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseLtSmallBlackButton.svg")));
            break;
        }
    }
};

/// @brief About same size as: SmallBlackButton (22.676 px)
struct infNoiseSmallButton : infNoiseTwoStageButton {
    infNoiseSmallButton() {
        momentary = false;
        box.size = Vec(21.321f, 21.321f); // Important for createParamCentered
    }

    void setup(buttonColor color, bool isMomentary = false) {
        momentary = isMomentary;
        switch (getOffColor(color)) {
        case bc_red:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseSmallRedButton.svg")));
            break;
        case bc_green:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseSmallGreenButton.svg")));
            break;
        case bc_blue:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseSmallBlueButton.svg")));
            break;
        default:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseSmallBlackButton.svg")));
            break;
        }

        switch (getOnColor(color)) {
        case bc_red:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseSmallRedButton.svg")));
            break;
        case bc_green:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseSmallGreenButton.svg")));
            break;
        case bc_blue:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseSmallBlueButton.svg")));
            break;
        default:
            addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/InfNoiseSmallBlackButton.svg")));
            break;
        }
    }
};

//-----------------------------------------------------------------------------
// Ports
//-----------------------------------------------------------------------------

/// @brief Themed port with a thin/red circle to indicate its for poly-signals.
struct infNoiseThemedPolyPort : app::ThemedSvgPort {
    infNoiseThemedPolyPort() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/PolyPort.svg")), 
            Svg::load(asset::plugin(pluginInstance, "res/components/PolyPortDark.svg")));
    }
};

//-----------------------------------------------------------------------------
// Overlay to indicate disabled widgets (e.g. knobs, buttons, switches, etc.)
//-----------------------------------------------------------------------------
constexpr uint8_t OVERLAY_DARK_LEVEL = 0;
constexpr uint8_t OVERLAY_LIGHT_LEVEL = 255;
constexpr uint8_t OVERLAY_TRANSPARENCY = 160;

/// @brief Auto: overlay tint follows InfNoise panel theme (same rules as applyPanelTheme()).
/// Fixed: use the color set with setColor() regardless of theme.
enum class InfNoiseDisableOverlayColorMode {
    Auto,
    Fixed
};

/// Rack's ui::Tooltip::step() moves box.pos to the mouse every frame. Use this for a
/// stable position (set once when shown) while the pointer stays over the overlay.
struct InfNoiseFixedTooltip : rack::ui::Tooltip {
    void step() override {
        // Avoid NVG use during teardown (APP/window may be null before Widget tree is done).
        if (APP && APP->window) {
            nvgSave(APP->window->vg);
            nvgTextLineHeight(APP->window->vg, 1.2);
            box.size.x = bndLabelWidth(APP->window->vg, -1, text.c_str());
            box.size.y = bndLabelHeight(APP->window->vg, -1, text.c_str(), INFINITY);
            if (parent)
                box = box.nudge(parent->box.zeroPos());
            nvgRestore(APP->window->vg);
        }
        rack::widget::Widget::step();
    }
};

struct InfNoiseDisableOverlay : rack::widget::OpaqueWidget {
private:
    bool active = false;
    std::string hint;
    NVGcolor color;
    InfNoiseDisableOverlayColorMode colorMode = InfNoiseDisableOverlayColorMode::Auto;
    NVGcolor fixedColor;
    InfNoiseFixedTooltip* tooltip = nullptr;

public:
    InfNoiseDisableOverlay(float left, float top, float width, float height,
        const std::string& hintText = "")
    {
        box.pos = Vec(left, top);
        box.size = Vec(width, height);
        hint = hintText;
        fixedColor = nvgRGBA(OVERLAY_DARK_LEVEL, OVERLAY_DARK_LEVEL, OVERLAY_DARK_LEVEL, OVERLAY_TRANSPARENCY);
        color = fixedColor;
        // OpaqueWidget still participates in hit-testing when not drawing; keep invisible until active
        // so knobs/ports below receive hover, tooltips, and drags when the overlay is off.
        visible = false;
    }

    ~InfNoiseDisableOverlay() override {
        destroyTooltip();
    }

    InfNoiseDisableOverlay& setActive(bool value) {
        if (active == value)
            return *this;

        active = value;
        visible = active;
        if (!active) 
            destroyTooltip();

        return *this;
    }

    InfNoiseDisableOverlay& setHint(const std::string& hintText) {
        hint = hintText;
        if (tooltip)
            tooltip->text = hint;

        return *this;
    }

    const std::string& getHint() const {
        return hint;
    }

    InfNoiseDisableOverlayColorMode getColorMode() const {
        return colorMode;
    }

    InfNoiseDisableOverlay& setColorMode(InfNoiseDisableOverlayColorMode mode) {
        colorMode = mode;
        if (colorMode == InfNoiseDisableOverlayColorMode::Fixed)
            color = fixedColor;
        return *this;
    }

    /// @brief Sets the color used when colorMode is Fixed; also updates the drawn color in Fixed mode.
    InfNoiseDisableOverlay& setColor(NVGcolor c) {
        fixedColor = c;
        if (colorMode == InfNoiseDisableOverlayColorMode::Fixed)
            color = c;
        return *this;
    }

    /// @brief Updates draw color from panel theme (Auto) or fixedColor (Fixed). Called when the
    /// global InfNoise panel theme is applied (see InfNoiseModuleWidget::applyPanelTheme()).
    void syncDrawColor(bool panelIsDark) {
        if (colorMode == InfNoiseDisableOverlayColorMode::Fixed) {
            color = fixedColor;
            return;
        }
        if (panelIsDark)
            color = nvgRGBA(OVERLAY_DARK_LEVEL, OVERLAY_DARK_LEVEL, OVERLAY_DARK_LEVEL, OVERLAY_TRANSPARENCY);
        else
            color = nvgRGBA(OVERLAY_LIGHT_LEVEL, OVERLAY_LIGHT_LEVEL, OVERLAY_LIGHT_LEVEL, OVERLAY_TRANSPARENCY);
    }

    void draw(const DrawArgs& args) override {
        if (!active)
            return;

        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 3.f);
        nvgFillColor(args.vg, color);
        nvgFill(args.vg);
    }

    void createTooltip() {
        if (tooltip || hint.empty())
            return;
        if (!APP || !APP->scene)
            return;

        tooltip = new InfNoiseFixedTooltip;
        tooltip->text = hint;
        // Scene coordinates (same as rack::ui::Tooltip); do not use rack->getMousePos() here.
        tooltip->box.pos = APP->scene->mousePos.plus(math::Vec(15.f, 15.f));
        APP->scene->addChild(tooltip);
    }

    void destroyTooltip() {
        if (!tooltip)
            return;

        if (tooltip->parent && APP && APP->scene)
            tooltip->parent->removeChild(tooltip);
        delete tooltip;
        tooltip = nullptr;
    }

    void onEnter(const EnterEvent& e) override {
        OpaqueWidget::onEnter(e);

        if (active && !hint.empty())
            createTooltip();
    }

    void onLeave(const LeaveEvent& e) override {
        OpaqueWidget::onLeave(e);
        destroyTooltip();
    }

    void onButton(const ButtonEvent& e) override {
        if (active)
            e.consume(this);
        OpaqueWidget::onButton(e);
    }

    void onDragStart(const DragStartEvent& e) override {
        if (active)
            e.consume(this);
        OpaqueWidget::onDragStart(e);
    }

    void onDragMove(const DragMoveEvent& e) override {
        if (active)
            e.consume(this);
        OpaqueWidget::onDragMove(e);
    }

    void onDragEnd(const DragEndEvent& e) override {
        if (active)
            e.consume(this);
        OpaqueWidget::onDragEnd(e);
    }
};

enum class InfNoiseOverlayTargetType {
    param,
    input,
    output
};

struct InfNoiseOverlayTargetKey {
    InfNoiseOverlayTargetType type = InfNoiseOverlayTargetType::param;
    int id = -1;

    bool operator<(const InfNoiseOverlayTargetKey& other) const {
        if (type != other.type)
            return static_cast<int>(type) < static_cast<int>(other.type);
        return id < other.id;
    }
};

struct InfNoiseDisableOverlayManager;

struct InfNoiseDisableOverlayGroup {
private:
    friend struct InfNoiseDisableOverlayManager;

    InfNoiseDisableOverlayManager* manager = nullptr;
    bool active = false;
    std::string hint;
    std::set<InfNoiseOverlayTargetKey> keyedTargets;
    std::set<InfNoiseDisableOverlay*> manualTargets;

    InfNoiseDisableOverlayGroup(InfNoiseDisableOverlayManager* manager, const std::string& hintText = "")
        : manager(manager), hint(hintText) {
    }

public:
    InfNoiseDisableOverlayGroup& addTarget(InfNoiseOverlayTargetType type, int id);
    InfNoiseDisableOverlayGroup& addTarget(const InfNoiseOverlayTargetKey& key);
    InfNoiseDisableOverlayGroup& addTargets(std::initializer_list<InfNoiseOverlayTargetKey> keys);
    InfNoiseDisableOverlayGroup& addTargets(InfNoiseOverlayTargetType type, std::initializer_list<int> ids);
    InfNoiseDisableOverlayGroup& addManualOverlay(InfNoiseDisableOverlay* overlay);
    InfNoiseDisableOverlayGroup& setActive(bool value);
    bool isActive() const {
        return active;
    }
    const std::string& getHint() const {
        return hint;
    }
};

// Auto-created overlays are owned by ModuleWidget (addChild). This manager only tracks pointers.
struct InfNoiseDisableOverlayManager {
private:
    friend struct InfNoiseDisableOverlayGroup;

    ModuleWidget* owner = nullptr;
    std::map<InfNoiseOverlayTargetKey, InfNoiseDisableOverlay*> keyedOverlays;
    std::vector<std::unique_ptr<InfNoiseDisableOverlayGroup>> groups;
    std::map<InfNoiseDisableOverlay*, int> activeRequests;
    std::set<InfNoiseDisableOverlay*> themeOverlayRegistry;

    void registerThemeOverlay(InfNoiseDisableOverlay* o) {
        if (o)
            themeOverlayRegistry.insert(o);
    }

    Widget* getWidgetByKey(const InfNoiseOverlayTargetKey& key) {
        if (!owner || key.id < 0)
            return nullptr;

        switch (key.type) {
        case InfNoiseOverlayTargetType::param:
            return owner->getParam(key.id);
        case InfNoiseOverlayTargetType::input:
            return owner->getInput(key.id);
        case InfNoiseOverlayTargetType::output:
            return owner->getOutput(key.id);
        default:
            return nullptr;
        }
    }

    void refreshOverlayState(InfNoiseDisableOverlay* overlay) {
        if (!overlay)
            return;

        int activeCount = 0;
        auto it = activeRequests.find(overlay);
        if (it != activeRequests.end())
            activeCount = it->second;

        overlay->setActive(activeCount > 0);
    }

    void addActiveRequest(InfNoiseDisableOverlay* overlay) {
        if (!overlay)
            return;

        activeRequests[overlay]++;
        refreshOverlayState(overlay);
    }

    void removeActiveRequest(InfNoiseDisableOverlay* overlay) {
        if (!overlay)
            return;

        auto it = activeRequests.find(overlay);
        if (it == activeRequests.end()) {
            refreshOverlayState(overlay);
            return;
        }

        if (it->second > 1) {
            it->second--;
        }
        else {
            activeRequests.erase(it);
        }

        refreshOverlayState(overlay);
    }

    InfNoiseDisableOverlay* getOrCreateOverlay(const InfNoiseOverlayTargetKey& key, const std::string& hint = "") {
        auto it = keyedOverlays.find(key);
        if (it != keyedOverlays.end()) {
            if (!hint.empty() && it->second->getHint().empty())
                it->second->setHint(hint);
            return it->second;
        }

        Widget* target = getWidgetByKey(key);
        if (!target)
            return nullptr;

        InfNoiseDisableOverlay* overlayRaw = new InfNoiseDisableOverlay(
            target->box.pos.x,
            target->box.pos.y,
            target->box.size.x,
            target->box.size.y,
            hint
        );
        if (owner)
            owner->addChild(overlayRaw);
        keyedOverlays.emplace(key, overlayRaw);
        registerThemeOverlay(overlayRaw);
        bool panelDark = false;
        if (auto* infW = dynamic_cast<InfNoiseModuleWidget*>(owner))
            panelDark = infW->effectivePanelIsDark();
        overlayRaw->syncDrawColor(panelDark);

        refreshOverlayState(overlayRaw);
        return overlayRaw;
    }

    void applyGroupChangeForTarget(const InfNoiseOverlayTargetKey& key, bool active, const std::string& hint = "") {
        InfNoiseDisableOverlay* overlay = getOrCreateOverlay(key, hint);
        if (!overlay)
            return;

        if (active)
            addActiveRequest(overlay);
        else
            removeActiveRequest(overlay);
    }

    void applyGroupChangeForManual(InfNoiseDisableOverlay* overlay, bool active) {
        if (!overlay)
            return;

        if (active)
            addActiveRequest(overlay);
        else
            removeActiveRequest(overlay);
    }

public:
    InfNoiseDisableOverlayManager() = default;

    explicit InfNoiseDisableOverlayManager(ModuleWidget* ownerWidget)
        : owner(ownerWidget) {
    }

    InfNoiseDisableOverlayManager& setOwner(ModuleWidget* ownerWidget) {
        owner = ownerWidget;
        return *this;
    }

    ModuleWidget* getOwner() const {
        return owner;
    }

    InfNoiseDisableOverlayGroup* addGroup(const std::string& hint = "") {
        groups.emplace_back(new InfNoiseDisableOverlayGroup(this, hint));
        return groups.back().get();
    }

    InfNoiseDisableOverlayGroup* addGroup(std::initializer_list<InfNoiseOverlayTargetKey> keyedTargets, const std::string& hint = "") {
        InfNoiseDisableOverlayGroup* group = addGroup(hint);
        for (const auto& key : keyedTargets)
            group->addTarget(key);
        return group;
    }

    void registerManualOverlay(InfNoiseDisableOverlay* overlay) {
        if (!overlay)
            return;

        activeRequests.emplace(overlay, 0);
        registerThemeOverlay(overlay);
    }

    /// @brief Sync overlay colors with the current InfNoise panel theme (see InfNoiseModuleWidget::applyPanelTheme()).
    void refreshOverlayThemeColors(bool panelIsDark) {
        for (InfNoiseDisableOverlay* o : themeOverlayRegistry)
            o->syncDrawColor(panelIsDark);
    }
};

inline InfNoiseDisableOverlayGroup& InfNoiseDisableOverlayGroup::addTarget(InfNoiseOverlayTargetType type, int id) {
    InfNoiseOverlayTargetKey key;
    key.type = type;
    key.id = id;
    return addTarget(key);
}

inline InfNoiseDisableOverlayGroup& InfNoiseDisableOverlayGroup::addTarget(const InfNoiseOverlayTargetKey& key) {
    if (!manager)
        return *this;

    auto result = keyedTargets.insert(key);
    if (!result.second)
        return *this;

    if (active)
        manager->applyGroupChangeForTarget(key, true, hint);
    else
        manager->getOrCreateOverlay(key, hint);

    return *this;
}

inline InfNoiseDisableOverlayGroup& InfNoiseDisableOverlayGroup::addTargets(std::initializer_list<InfNoiseOverlayTargetKey> keys) {
    for (std::initializer_list<InfNoiseOverlayTargetKey>::const_iterator it = keys.begin(); it != keys.end(); ++it)
        addTarget(*it);
    return *this;
}

inline InfNoiseDisableOverlayGroup& InfNoiseDisableOverlayGroup::addTargets(InfNoiseOverlayTargetType type, std::initializer_list<int> ids) {
    for (std::initializer_list<int>::const_iterator it = ids.begin(); it != ids.end(); ++it)
        addTarget(type, *it);
    return *this;
}

inline InfNoiseDisableOverlayGroup& InfNoiseDisableOverlayGroup::addManualOverlay(InfNoiseDisableOverlay* overlay) {
    if (!manager || !overlay)
        return *this;

    auto result = manualTargets.insert(overlay);
    if (!result.second)
        return *this;

    manager->registerManualOverlay(overlay);
    if (active)
        manager->applyGroupChangeForManual(overlay, true);

    return *this;
}

inline InfNoiseDisableOverlayGroup& InfNoiseDisableOverlayGroup::setActive(bool value) {
    if (!manager || active == value)
        return *this;

    active = value;
    for (const auto& key : keyedTargets)
        manager->applyGroupChangeForTarget(key, active, hint);
    for (auto* overlay : manualTargets)
        manager->applyGroupChangeForManual(overlay, active);

    return *this;
}

template<typename T = InfNoiseDisableOverlay>
T* createInfNoiseOverlay(float left, float top, float width, float height,
    const std::string& hint = "")
{
    return new T(left, top, width, height, hint);
}
