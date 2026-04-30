#include "overview.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#define private public
#define protected public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#undef private
#undef protected

#include <xkbcommon/xkbcommon.h>

#include "AppIcon.hpp"
#include "WinviewPassElement.hpp"
#include "globals.hpp"

static const CConfigValue<Config::INTEGER>& PGAP() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:gap_size");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PMARGIN() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:margin");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PBG() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:bg_col");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PBACKGROUND() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:background");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PBACKGROUNDBLUR() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:background_blur");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PBORDER() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:border_col");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PHOVERBORDER() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:hover_border_col");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PBORDERSIZE() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:border_size");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PWINDOWORDER() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:window_order");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSLEFT() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_left");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSRIGHT() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_right");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSUP() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_up");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSDOWN() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_down");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSGO() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_go");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSBRING() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_bring");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSBRINGREPLACE() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_bring_replace");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSCLOSE() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_close");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PSHOWAPPICON() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:show_app_icon");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PAPPICONSIZE() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:app_icon_size");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PAPPICONPOSITION() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:app_icon_position");
    return VALUE;
}

static const CConfigValue<Config::FLOAT>& PAPPICONANCHORX() {
    static const CConfigValue<Config::FLOAT> VALUE("plugin:hyprwinview:app_icon_anchor_x");
    return VALUE;
}

static const CConfigValue<Config::FLOAT>& PAPPICONANCHORY() {
    static const CConfigValue<Config::FLOAT> VALUE("plugin:hyprwinview:app_icon_anchor_y");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PAPPICONMARGINX() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:app_icon_margin_x");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PAPPICONMARGINY() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:app_icon_margin_y");
    return VALUE;
}

static const CConfigValue<Config::FLOAT>& PAPPICONMARGINRELX() {
    static const CConfigValue<Config::FLOAT> VALUE("plugin:hyprwinview:app_icon_margin_relative_x");
    return VALUE;
}

static const CConfigValue<Config::FLOAT>& PAPPICONMARGINRELY() {
    static const CConfigValue<Config::FLOAT> VALUE("plugin:hyprwinview:app_icon_margin_relative_y");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PAPPICONOFFSETX() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:app_icon_offset_x");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PAPPICONOFFSETY() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:app_icon_offset_y");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PAPPICONBACKPLATE() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:app_icon_backplate_col");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PAPPICONBACKPLATEPADDING() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:app_icon_backplate_padding");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PANIMATION() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:animation");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PANIMATIONINMS() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:animation_in_ms");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PANIMATIONOUTMS() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:animation_out_ms");
    return VALUE;
}

static const CConfigValue<Config::FLOAT>& PANIMATIONSCALE() {
    static const CConfigValue<Config::FLOAT> VALUE("plugin:hyprwinview:animation_scale");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PANIMATIONSTAGGERMS() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:animation_stagger_ms");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PANIMATIONSTAGGERMAXMS() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:animation_stagger_max_ms");
    return VALUE;
}

enum class EOverviewAnimation {
    NONE,
    FADE,
    FADE_SCALE,
    STAGGERED_FADE_SCALE,
};

struct SWindowOrderingStrategy {
    const char* name;
    std::string (*groupKeyForWindow)(const PHLWINDOW& window, size_t originalIndex);
};

static std::optional<SWinviewKeyConfig> g_winviewKeyConfigOverride;

static constexpr uint64_t DEFAULT_BACKGROUND = 0x99101014;

SWinviewKeyConfig defaultWinviewKeyConfig() {
    return {
        .left  = {"a", "h", "left"},
        .right = {"d", "l", "right"},
        .up    = {"w", "k", "up"},
        .down  = {"s", "j", "down"},
        .go    = {"return", "enter", "space", "g", "f"},
        .bring = {"b", "shift+return", "shift+space"},
        .bringReplace = {"shift+b"},
        .close = {"escape", "q"},
    };
}

void setWinviewKeyConfig(SWinviewKeyConfig config) {
    g_winviewKeyConfigOverride = std::move(config);
}

static uint32_t framebufferFormatWithAlpha(uint32_t drmFormat) {
    return DRM_FORMAT_ABGR8888;
}

static std::string trimmedLower(std::string token) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    token.erase(token.begin(), std::ranges::find_if(token, notSpace));
    token.erase(std::ranges::find_if(token.rbegin(), token.rend(), notSpace).base(), token.end());
    std::ranges::transform(token, token.begin(), [](unsigned char c) { return std::tolower(c); });
    return token;
}

static std::vector<std::string> keyTokens(const std::string& keys) {
    std::vector<std::string> result;
    std::stringstream        stream(keys);
    std::string              token;

    while (std::getline(stream, token, ',')) {
        token = trimmedLower(token);
        if (!token.empty())
            result.push_back(token);
    }

    return result;
}

static std::string normalizeKeyName(std::string key) {
    key = trimmedLower(key);

    if (key == "enter")
        return "return";
    if (key == "esc")
        return "escape";
    if (key == "arrowleft")
        return "left";
    if (key == "arrowright")
        return "right";
    if (key == "arrowup")
        return "up";
    if (key == "arrowdown")
        return "down";

    return key;
}

static xkb_keysym_t keysymForName(const std::string& key) {
    const auto NORMALIZED = normalizeKeyName(key);

    if (NORMALIZED == "left")
        return XKB_KEY_Left;
    if (NORMALIZED == "right")
        return XKB_KEY_Right;
    if (NORMALIZED == "up")
        return XKB_KEY_Up;
    if (NORMALIZED == "down")
        return XKB_KEY_Down;
    if (NORMALIZED == "return")
        return XKB_KEY_Return;
    if (NORMALIZED == "space")
        return XKB_KEY_space;
    if (NORMALIZED == "escape")
        return XKB_KEY_Escape;

    return xkb_keysym_from_name(NORMALIZED.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
}

static bool tokenMatchesKey(const std::string& token, xkb_keysym_t keysym, uint32_t mods) {
    std::stringstream stream(token);
    std::string       part;
    uint32_t          requiredMods = 0;
    xkb_keysym_t      requiredKey  = XKB_KEY_NoSymbol;

    while (std::getline(stream, part, '+')) {
        part = normalizeKeyName(part);

        if (part == "shift")
            requiredMods |= HL_MODIFIER_SHIFT;
        else if (part == "ctrl" || part == "control")
            requiredMods |= HL_MODIFIER_CTRL;
        else if (part == "alt")
            requiredMods |= HL_MODIFIER_ALT;
        else if (part == "super" || part == "mod" || part == "meta")
            requiredMods |= HL_MODIFIER_META;
        else
            requiredKey = keysymForName(part);
    }

    constexpr uint32_t HANDLED_MODS = HL_MODIFIER_SHIFT | HL_MODIFIER_CTRL | HL_MODIFIER_ALT | HL_MODIFIER_META;

    return requiredKey != XKB_KEY_NoSymbol && xkb_keysym_to_lower(requiredKey) == xkb_keysym_to_lower(keysym) && (mods & HANDLED_MODS) == requiredMods;
}

static bool matchesKeySet(const std::vector<std::string>& keys, xkb_keysym_t keysym, uint32_t mods) {
    for (const auto& token : keys) {
        if (tokenMatchesKey(token, keysym, mods))
            return true;
    }

    return false;
}

static SWinviewKeyConfig keyConfigFromConfigValues() {
    return {
        .left  = keyTokens(*PKEYSLEFT()),
        .right = keyTokens(*PKEYSRIGHT()),
        .up    = keyTokens(*PKEYSUP()),
        .down  = keyTokens(*PKEYSDOWN()),
        .go    = keyTokens(*PKEYSGO()),
        .bring = keyTokens(*PKEYSBRING()),
        .bringReplace = keyTokens(*PKEYSBRINGREPLACE()),
        .close = keyTokens(*PKEYSCLOSE()),
    };
}

static SWinviewKeyConfig activeKeyConfig() {
    return g_winviewKeyConfigOverride.value_or(keyConfigFromConfigValues());
}

static std::string lower(std::string value) {
    std::ranges::transform(value, value.begin(), [](unsigned char c) { return std::tolower(c); });
    return value;
}

static std::string uniqueWindowGroupKey(const PHLWINDOW&, size_t originalIndex) {
    return "window:" + std::to_string(originalIndex);
}

static std::string applicationGroupKey(const PHLWINDOW& window, size_t originalIndex) {
    if (window) {
        for (const auto& candidate : {window->m_class, window->m_initialClass}) {
            const auto KEY = trimmedLower(candidate);
            if (!KEY.empty())
                return "app:" + KEY;
        }
    }

    return uniqueWindowGroupKey(window, originalIndex);
}

static const SWindowOrderingStrategy& naturalOrderStrategy() {
    static const SWindowOrderingStrategy STRATEGY = {
        .name              = "natural",
        .groupKeyForWindow = uniqueWindowGroupKey,
    };
    return STRATEGY;
}

static const SWindowOrderingStrategy& applicationOrderStrategy() {
    static const SWindowOrderingStrategy STRATEGY = {
        .name              = "application",
        .groupKeyForWindow = applicationGroupKey,
    };
    return STRATEGY;
}

static const SWindowOrderingStrategy& activeWindowOrderingStrategy() {
    const auto NAME = trimmedLower(*PWINDOWORDER());

    if (NAME.empty() || NAME == "none" || NAME == "natural" || NAME == "compositor")
        return naturalOrderStrategy();

    if (NAME == "app" || NAME == "application" || NAME == "application_grouped" || NAME == "group_app" || NAME == "group_by_app" || NAME == "grouped_by_app")
        return applicationOrderStrategy();

    return naturalOrderStrategy();
}

static EOverviewAnimation overviewAnimation() {
    const auto NAME = trimmedLower(*PANIMATION());

    if (NAME == "none" || NAME == "off" || NAME == "disable" || NAME == "disabled")
        return EOverviewAnimation::NONE;
    if (NAME == "fade")
        return EOverviewAnimation::FADE;
    if (NAME == "stagger" || NAME == "staggered" || NAME == "staggered_fade_scale")
        return EOverviewAnimation::STAGGERED_FADE_SCALE;

    return EOverviewAnimation::FADE_SCALE;
}

static bool animationScalesTiles(EOverviewAnimation animation) {
    return animation == EOverviewAnimation::FADE_SCALE || animation == EOverviewAnimation::STAGGERED_FADE_SCALE;
}

static bool animationStaggersTiles(EOverviewAnimation animation) {
    return animation == EOverviewAnimation::STAGGERED_FADE_SCALE;
}

static double animationDurationMs(bool closing) {
    return std::max<Config::INTEGER>(0, closing ? *PANIMATIONOUTMS() : *PANIMATIONINMS());
}

static double easeOutCubic(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return 1.0 - std::pow(1.0 - t, 3.0);
}

static double easeInCubic(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return t * t * t;
}

static double visibleAmountForElapsed(double elapsedMs, double durationMs, bool closing) {
    if (durationMs <= 0.0)
        return closing ? 0.0 : 1.0;

    const auto T = std::clamp(elapsedMs / durationMs, 0.0, 1.0);
    return closing ? 1.0 - easeInCubic(T) : easeOutCubic(T);
}

static CBox scaleBoxFromCenter(const CBox& box, double scale) {
    const auto CENTER = box.middle();
    const auto W      = box.w * scale;
    const auto H      = box.h * scale;

    return {CENTER.x - W / 2.0, CENTER.y - H / 2.0, W, H};
}

static CHyprColor multiplyAlpha(const CHyprColor& color, double alpha) {
    return color.modifyA(color.a * std::clamp(alpha, 0.0, 1.0));
}

static CHyprColor activeBackgroundColor() {
    const auto BACKGROUND = static_cast<uint64_t>(*PBACKGROUND());
    const auto BG_COL     = static_cast<uint64_t>(*PBG());

    if (BACKGROUND == DEFAULT_BACKGROUND && BG_COL != DEFAULT_BACKGROUND)
        return CHyprColor(BG_COL);

    return CHyprColor(BACKGROUND);
}

static Vector2D iconAnchorFromPosition(const std::string& position) {
    auto   anchor = Vector2D{1.0, 1.0};
    auto   value  = lower(position);
    size_t start  = 0;
    bool   hasX   = false;
    bool   hasY   = false;
    bool   center = false;

    while (start < value.size()) {
        const auto END   = value.find_first_of(" ,", start);
        const auto TOKEN = value.substr(start, END == std::string::npos ? std::string::npos : END - start);

        if (TOKEN == "left") {
            anchor.x = 0.0;
            hasX     = true;
        } else if (TOKEN == "right") {
            anchor.x = 1.0;
            hasX     = true;
        } else if (TOKEN == "top") {
            anchor.y = 0.0;
            hasY     = true;
        } else if (TOKEN == "bottom") {
            anchor.y = 1.0;
            hasY     = true;
        } else if (TOKEN == "center" || TOKEN == "middle")
            center = true;

        if (END == std::string::npos)
            break;

        start = value.find_first_not_of(" ,", END);
        if (start == std::string::npos)
            break;
    }

    if (center || (hasY && !hasX))
        anchor.x = hasX ? anchor.x : 0.5;

    if (center || (hasX && !hasY))
        anchor.y = hasY ? anchor.y : 0.5;

    return anchor;
}

static double anchorOverride(double configured, double fallback) {
    return configured >= 0.0 ? std::clamp(configured, 0.0, 1.0) : fallback;
}

static double edgeSignedMargin(double anchor, double absolute, double relative, double extent) {
    if (anchor < 0.5)
        return absolute + relative * extent;
    if (anchor > 0.5)
        return -(absolute + relative * extent);
    return 0.0;
}

static CBox appIconBoxForTile(const CBox& tileLogical, double scale) {
    const int  SIZE = std::max<Config::INTEGER>(1, *PAPPICONSIZE());
    Vector2D   anchor = iconAnchorFromPosition(*PAPPICONPOSITION());
    anchor.x          = anchorOverride(*PAPPICONANCHORX(), anchor.x);
    anchor.y          = anchorOverride(*PAPPICONANCHORY(), anchor.y);

    const double xMargin = edgeSignedMargin(anchor.x, *PAPPICONMARGINX(), *PAPPICONMARGINRELX(), tileLogical.w);
    const double yMargin = edgeSignedMargin(anchor.y, *PAPPICONMARGINY(), *PAPPICONMARGINRELY(), tileLogical.h);
    const double x       = tileLogical.x + anchor.x * std::max(0.0, tileLogical.w - SIZE) + xMargin + *PAPPICONOFFSETX();
    const double y       = tileLogical.y + anchor.y * std::max(0.0, tileLogical.h - SIZE) + yMargin + *PAPPICONOFFSETY();

    return CBox{x, y, (double)SIZE, (double)SIZE}.scale(scale).round();
}

static bool previewableWindow(const PHLWINDOW& window) {
    if (!window || !window->m_isMapped || window->isHidden() || window->m_fadingOut || !window->m_workspace)
        return false;

    if (window->m_size.x <= 1 || window->m_size.y <= 1 || window->m_realSize->value().x <= 1 || window->m_realSize->value().y <= 1)
        return false;

    return true;
}

CWindowOverview::CWindowOverview(PHLMONITOR monitor, SWindowOverviewOptions options_) : pMonitor(monitor), options(options_) {
    animationStartedAt = Time::steadyNow();
    initialFocusedWindow = Desktop::focusState()->window();
    initialFocusedWorkspace = initialFocusedWindow && initialFocusedWindow->m_workspace ? initialFocusedWindow->m_workspace : (pMonitor ? pMonitor->m_activeWorkspace : nullptr);

    collectWindows();
    updateLayout();
    renderSnapshots();

    lastMousePosLocal = g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position;
    selectedIndex     = hoveredIndex();
    if (selectedIndex < 0 && !previews.empty())
        selectedIndex = 0;

    auto onCursorMove = [this](Event::SCallbackInfo& info) {
        if (closing) {
            info.cancelled = true;
            return;
        }

        info.cancelled    = true;
        lastMousePosLocal = g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position;
        const auto HOVERED = hoveredIndex();
        if (HOVERED >= 0)
            selectedIndex = HOVERED;
        damage();
    };

    auto onCursorSelect = [this](Event::SCallbackInfo& info) {
        if (closing) {
            info.cancelled = true;
            return;
        }

        info.cancelled = true;
        selectHoveredWindow();
        close(true);
    };

    auto onKeyboardKey = [this](const IKeyboard::SKeyEvent& event, Event::SCallbackInfo& info) {
        if (closing) {
            info.cancelled = true;
            return;
        }

        if (handleKey(event))
            info.cancelled = true;
    };

    mouseMoveHook   = Event::bus()->m_events.input.mouse.move.listen([onCursorMove](Vector2D, Event::SCallbackInfo& info) { onCursorMove(info); });
    touchMoveHook   = Event::bus()->m_events.input.touch.motion.listen([onCursorMove](ITouch::SMotionEvent, Event::SCallbackInfo& info) { onCursorMove(info); });
    mouseButtonHook = Event::bus()->m_events.input.mouse.button.listen([onCursorSelect](IPointer::SButtonEvent, Event::SCallbackInfo& info) { onCursorSelect(info); });
    touchDownHook   = Event::bus()->m_events.input.touch.down.listen([onCursorSelect](ITouch::SDownEvent, Event::SCallbackInfo& info) { onCursorSelect(info); });
    keyboardHook    = Event::bus()->m_events.input.keyboard.key.listen([onKeyboardKey](IKeyboard::SKeyEvent event, Event::SCallbackInfo& info) { onKeyboardKey(event, info); });

    damage();
}

CWindowOverview::~CWindowOverview() {
    Render::GL::g_pHyprOpenGL->makeEGLCurrent();
    for (auto& preview : previews)
        preview.fb.reset();
    previews.clear();
}

void CWindowOverview::collectWindows() {
    previews.clear();

    const auto CURRENT_WORKSPACE = pMonitor ? pMonitor->m_activeWorkspace : nullptr;

    for (auto it = g_pCompositor->m_windows.rbegin(); it != g_pCompositor->m_windows.rend(); ++it) {
        const auto& window = *it;
        if (!previewableWindow(window))
            continue;

        if (!options.includeCurrentWorkspace && CURRENT_WORKSPACE && window->m_workspace == CURRENT_WORKSPACE)
            continue;

        previews.push_back({.window = window});
    }

    std::ranges::reverse(previews);
    applyWindowOrdering();
}

void CWindowOverview::applyWindowOrdering() {
    if (previews.empty())
        return;

    const auto&              STRATEGY = activeWindowOrderingStrategy();
    std::vector<std::string> groupOrder;

    for (size_t i = 0; i < previews.size(); ++i) {
        auto& preview               = previews[i];
        preview.orderOriginalIndex  = i;
        preview.orderGroupKey       = STRATEGY.groupKeyForWindow(preview.window, i);
        const auto GROUP_ORDER_ITER = std::ranges::find(groupOrder, preview.orderGroupKey);

        if (GROUP_ORDER_ITER == groupOrder.end()) {
            preview.orderGroupIndex = groupOrder.size();
            groupOrder.push_back(preview.orderGroupKey);
        } else {
            preview.orderGroupIndex = GROUP_ORDER_ITER - groupOrder.begin();
        }
    }

    std::ranges::stable_sort(previews, [](const SWindowPreview& a, const SWindowPreview& b) {
        if (a.orderGroupIndex != b.orderGroupIndex)
            return a.orderGroupIndex < b.orderGroupIndex;

        return a.orderOriginalIndex < b.orderOriginalIndex;
    });
}

void CWindowOverview::updateLayout() {
    if (!pMonitor || previews.empty())
        return;

    const double count  = previews.size();
    const double aspect = std::max(0.1, pMonitor->m_size.x / std::max(1.0, pMonitor->m_size.y));
    int          cols   = std::max(1, (int)std::ceil(std::sqrt(count * aspect)));
    int          rows   = std::max(1, (int)std::ceil(count / cols));

    while (cols > 1 && (cols - 1) * rows >= (int)previews.size())
        cols--;

    rows = std::max(1, (int)std::ceil(count / cols));
    gridCols = cols;

    const double margin = std::max<Config::INTEGER>(0, *PMARGIN());
    const double gap    = std::max<Config::INTEGER>(0, *PGAP());
    const double areaW  = std::max(1.0, pMonitor->m_size.x - margin * 2.0);
    const double areaH  = std::max(1.0, pMonitor->m_size.y - margin * 2.0);
    const double cellW  = (areaW - gap * (cols - 1)) / cols;
    const double cellH  = (areaH - gap * (rows - 1)) / rows;

    for (size_t i = 0; i < previews.size(); ++i) {
        auto&        preview = previews[i];
        const double col     = i % cols;
        const double row     = i / cols;
        const auto   winSize = preview.window->m_realSize->value();
        const double scale   = std::min(cellW / std::max(1.0, winSize.x), cellH / std::max(1.0, winSize.y));
        const double w       = std::max(1.0, winSize.x * scale);
        const double h       = std::max(1.0, winSize.y * scale);
        const double x       = margin + col * (cellW + gap) + (cellW - w) / 2.0;
        const double y       = margin + row * (cellH + gap) + (cellH - h) / 2.0;

        preview.tileLogical = {x, y, w, h};
    }
}

void CWindowOverview::renderSnapshots() {
    if (!pMonitor)
        return;

    Render::GL::g_pHyprOpenGL->makeEGLCurrent();

    const auto FORMAT = framebufferFormatWithAlpha(pMonitor->m_output->state->state().drmFormat);

    for (auto& preview : previews) {
        if (!preview.fb)
            preview.fb = g_pHyprRenderer->createFB("hyprwinview");

        if (preview.fb->m_size != pMonitor->m_pixelSize) {
            preview.fb->release();
            preview.fb->alloc(pMonitor->m_pixelSize.x, pMonitor->m_pixelSize.y, FORMAT);
        }

        CRegion fakeDamage{0, 0, (int)pMonitor->m_transformedSize.x, (int)pMonitor->m_transformedSize.y};
        if (!g_pHyprRenderer->beginFullFakeRender(pMonitor.lock(), fakeDamage, preview.fb))
            continue;

        g_pHyprRenderer->m_bRenderingSnapshot = true;
        g_pHyprRenderer->draw(CClearPassElement::SClearData{CHyprColor(0, 0, 0, 0)});
        g_pHyprRenderer->startRenderPass();
        g_pHyprRenderer->renderWindow(preview.window, pMonitor.lock(), Time::steadyNow(), false, Render::RENDER_PASS_ALL, true, true);
        g_pHyprRenderer->m_renderData.blockScreenShader = true;
        g_pHyprRenderer->endRender();
        g_pHyprRenderer->m_bRenderingSnapshot = false;
    }
}

int CWindowOverview::hoveredIndex() const {
    for (size_t i = 0; i < previews.size(); ++i) {
        if (previews[i].tileLogical.containsPoint(lastMousePosLocal))
            return i;
    }

    return -1;
}

void CWindowOverview::render() {
    if (closing && animationComplete()) {
        finishClose();
        return;
    }

    g_pHyprRenderer->m_renderPass.add(makeUnique<CWinviewPassElement>());
}

void CWindowOverview::draw() {
    if (!pMonitor)
        return;

    const auto SCALE       = pMonitor->m_scale;
    const auto HOVERED    = selectedIndex;
    const int  BORDER     = std::max<Config::INTEGER>(0, *PBORDERSIZE());
    const auto ANIMATION   = overviewAnimation();
    const auto VISIBLE     = animationVisibleAmount();
    const auto BASE_SCALE  = std::clamp<double>(*PANIMATIONSCALE(), 0.01, 1.0);
    CRegion    fullDamage = {0, 0, INT16_MAX, INT16_MAX};

    Render::GL::g_pHyprOpenGL->renderRect(CBox{{0, 0}, pMonitor->m_pixelSize}, multiplyAlpha(activeBackgroundColor(), VISIBLE),
                                          {.damage = &fullDamage, .blur = backgroundBlurEnabled(), .blurA = (float)VISIBLE});

    for (size_t i = 0; i < previews.size(); ++i) {
        auto& preview = previews[i];
        if (!preview.fb || !preview.fb->getTexture())
            continue;

        const auto TILE_VISIBLE = tileAnimationVisibleAmount(i);
        const auto TILE_SCALE   = animationScalesTiles(ANIMATION) ? BASE_SCALE + (1.0 - BASE_SCALE) * TILE_VISIBLE : 1.0;
        CBox tilePx = scaleBoxFromCenter(preview.tileLogical, TILE_SCALE).scale(SCALE).round();
        CBox texBox = {
            tilePx.x,
            tilePx.y,
            pMonitor->m_pixelSize.x * (tilePx.w / std::max(1.0, preview.window->m_realSize->value().x * SCALE)),
            pMonitor->m_pixelSize.y * (tilePx.h / std::max(1.0, preview.window->m_realSize->value().y * SCALE)),
        };

        if (BORDER > 0) {
            const auto COLOR = (int)i == HOVERED ? CHyprColor(*PHOVERBORDER()) : CHyprColor(*PBORDER());
            Render::GL::g_pHyprOpenGL->renderRect(tilePx.copy().expand(BORDER), multiplyAlpha(COLOR, TILE_VISIBLE), {.damage = &fullDamage, .round = BORDER * 2});
        }

        g_pHyprRenderer->m_renderData.clipBox = tilePx;
        Render::GL::g_pHyprOpenGL->renderTexture(preview.fb->getTexture(), texBox, {.damage = &fullDamage, .a = (float)TILE_VISIBLE, .round = BORDER * 2});
        g_pHyprRenderer->m_renderData.clipBox = {};

        if (*PSHOWAPPICON() != 0) {
            const int ICON_SIZE_PX = std::max(1, (int)std::round(std::max<Config::INTEGER>(1, *PAPPICONSIZE()) * SCALE));
            if (const auto ICON = appIconTextureForWindow(preview.window, ICON_SIZE_PX)) {
                CBox       iconBox = scaleBoxFromCenter(appIconBoxForTile(preview.tileLogical, SCALE), TILE_SCALE);
                const int PADDING = std::max(0, (int)std::round(std::max<Config::INTEGER>(0, *PAPPICONBACKPLATEPADDING()) * SCALE));
                if (PADDING > 0)
                    Render::GL::g_pHyprOpenGL->renderRect(iconBox.copy().expand(PADDING).round(), multiplyAlpha(CHyprColor(*PAPPICONBACKPLATE()), TILE_VISIBLE),
                                                          {.damage = &fullDamage, .round = std::max(1, PADDING)});

                Render::GL::g_pHyprOpenGL->renderTexture(ICON, iconBox, {.damage = &fullDamage, .a = (float)TILE_VISIBLE});
            }
        }
    }

    if (isAnimating())
        damage();
}

void CWindowOverview::damage() {
    if (pMonitor)
        g_pHyprRenderer->damageMonitor(pMonitor.lock());
}

void CWindowOverview::selectHoveredWindow() {
    selectedIndex = hoveredIndex();
}

void CWindowOverview::moveSelection(int dx, int dy) {
    if (previews.empty())
        return;

    if (selectedIndex < 0 || selectedIndex >= (int)previews.size())
        selectedIndex = 0;

    const int col    = selectedIndex % gridCols;
    const int row    = selectedIndex / gridCols;
    const int newCol = std::clamp(col + dx, 0, gridCols - 1);
    const int maxRow = (previews.size() - 1) / gridCols;
    const int newRow = std::clamp(row + dy, 0, maxRow);
    int       next   = newRow * gridCols + newCol;

    if (next >= (int)previews.size())
        next = previews.size() - 1;

    if (next != selectedIndex) {
        selectedIndex     = next;
        lastMousePosLocal = previews[selectedIndex].tileLogical.middle();
        damage();
    }
}

void CWindowOverview::runSelected(bool bring, bool replaceInitial) {
    close(true, bring, replaceInitial);
}

double CWindowOverview::animationVisibleAmount() const {
    const auto ANIMATION = overviewAnimation();
    if (ANIMATION == EOverviewAnimation::NONE)
        return 1.0;

    const auto DURATION = animationDurationMs(closing) + (closing ? maxTileAnimationDelayMs() : 0.0);
    const auto ELAPSED = std::chrono::duration<double, std::milli>(Time::steadyNow() - animationStartedAt).count();

    return visibleAmountForElapsed(ELAPSED, DURATION, closing);
}

double CWindowOverview::tileAnimationVisibleAmount(size_t index) const {
    const auto ANIMATION = overviewAnimation();
    if (ANIMATION == EOverviewAnimation::NONE)
        return 1.0;

    const auto DURATION = animationDurationMs(closing);
    const auto ELAPSED  = std::chrono::duration<double, std::milli>(Time::steadyNow() - animationStartedAt).count() - tileAnimationDelayMs(index);

    return visibleAmountForElapsed(ELAPSED, DURATION, closing);
}

double CWindowOverview::tileAnimationDelayMs(size_t index) const {
    const auto ANIMATION = overviewAnimation();
    if (!animationStaggersTiles(ANIMATION) || previews.empty())
        return 0.0;

    const auto STAGGER_MS     = std::max<Config::INTEGER>(0, *PANIMATIONSTAGGERMS());
    const auto MAX_STAGGER_MS = std::max<Config::INTEGER>(0, *PANIMATIONSTAGGERMAXMS());
    if (STAGGER_MS <= 0 || MAX_STAGGER_MS <= 0)
        return 0.0;

    const size_t ORDER_INDEX = closing ? previews.size() - 1 - std::min(index, previews.size() - 1) : index;
    return std::min<double>(ORDER_INDEX * STAGGER_MS, MAX_STAGGER_MS);
}

double CWindowOverview::maxTileAnimationDelayMs() const {
    const auto ANIMATION = overviewAnimation();
    if (!animationStaggersTiles(ANIMATION) || previews.empty())
        return 0.0;

    const auto STAGGER_MS     = std::max<Config::INTEGER>(0, *PANIMATIONSTAGGERMS());
    const auto MAX_STAGGER_MS = std::max<Config::INTEGER>(0, *PANIMATIONSTAGGERMAXMS());
    if (STAGGER_MS <= 0 || MAX_STAGGER_MS <= 0)
        return 0.0;

    return std::min<double>((previews.size() - 1) * STAGGER_MS, MAX_STAGGER_MS);
}

bool CWindowOverview::animationComplete() const {
    const auto ANIMATION = overviewAnimation();
    if (ANIMATION == EOverviewAnimation::NONE)
        return true;

    const auto DURATION = animationDurationMs(closing);
    if (DURATION <= 0.0)
        return true;

    const auto ELAPSED = std::chrono::duration<double, std::milli>(Time::steadyNow() - animationStartedAt).count();
    const auto TOTAL_TIME_MS = DURATION + maxTileAnimationDelayMs();
    return ELAPSED >= TOTAL_TIME_MS;
}

bool CWindowOverview::isAnimating() const {
    return !animationComplete();
}

bool CWindowOverview::backgroundBlurEnabled() const {
    return *PBACKGROUNDBLUR() != 0;
}

bool CWindowOverview::backgroundOpaque() const {
    return !isAnimating() && activeBackgroundColor().a >= 1.0;
}

bool CWindowOverview::handleKey(const IKeyboard::SKeyEvent& event) {
    const auto KEYBOARD = g_pSeatManager && !g_pSeatManager->m_keyboard.expired() ? g_pSeatManager->m_keyboard.lock() : nullptr;
    if (!KEYBOARD || !KEYBOARD->m_xkbState)
        return false;

    const auto KEYCODE = event.keycode + 8;
    const auto KEYSYM  = xkb_state_key_get_one_sym(KEYBOARD->m_xkbState, KEYCODE);
    const auto MODS    = g_pInputManager->getModsFromAllKBs();
    const auto KEYS    = activeKeyConfig();

    const bool RECOGNIZED = matchesKeySet(KEYS.left, KEYSYM, MODS) || matchesKeySet(KEYS.right, KEYSYM, MODS) || matchesKeySet(KEYS.up, KEYSYM, MODS) ||
        matchesKeySet(KEYS.down, KEYSYM, MODS) || matchesKeySet(KEYS.go, KEYSYM, MODS) || matchesKeySet(KEYS.bring, KEYSYM, MODS) ||
        matchesKeySet(KEYS.bringReplace, KEYSYM, MODS) || matchesKeySet(KEYS.close, KEYSYM, MODS);

    if (!RECOGNIZED)
        return false;

    if (event.state != WL_KEYBOARD_KEY_STATE_PRESSED)
        return true;

    if (matchesKeySet(KEYS.left, KEYSYM, MODS))
        moveSelection(-1, 0);
    else if (matchesKeySet(KEYS.right, KEYSYM, MODS))
        moveSelection(1, 0);
    else if (matchesKeySet(KEYS.up, KEYSYM, MODS))
        moveSelection(0, -1);
    else if (matchesKeySet(KEYS.down, KEYSYM, MODS))
        moveSelection(0, 1);
    else if (matchesKeySet(KEYS.bringReplace, KEYSYM, MODS))
        runSelected(true, true);
    else if (matchesKeySet(KEYS.bring, KEYSYM, MODS))
        runSelected(true);
    else if (matchesKeySet(KEYS.go, KEYSYM, MODS))
        runSelected(false);
    else if (matchesKeySet(KEYS.close, KEYSYM, MODS))
        close(false);

    return true;
}

void CWindowOverview::focusWindow(PHLWINDOW window, bool bring, bool replaceInitial) {
    if (!window || !window->m_workspace)
        return;

    const auto FOCUSSTATE = Desktop::focusState();
    const auto MONITOR    = FOCUSSTATE->monitor();
    const auto TARGET_WORKSPACE = replaceInitial && initialFocusedWorkspace ? initialFocusedWorkspace : (MONITOR ? MONITOR->m_activeWorkspace : nullptr);
    const auto SELECTED_ORIGINAL_WORKSPACE = window->m_workspace;

    if (replaceInitial && initialFocusedWindow && initialFocusedWindow != window && initialFocusedWindow->m_isMapped && initialFocusedWindow->m_workspace && SELECTED_ORIGINAL_WORKSPACE &&
        initialFocusedWindow->m_workspace != SELECTED_ORIGINAL_WORKSPACE) {
        g_pCompositor->moveWindowToWorkspaceSafe(initialFocusedWindow, SELECTED_ORIGINAL_WORKSPACE);
        initialFocusedWindow->m_workspace = SELECTED_ORIGINAL_WORKSPACE;
    }

    if ((bring || replaceInitial) && TARGET_WORKSPACE && window->m_workspace != TARGET_WORKSPACE) {
        g_pCompositor->moveWindowToWorkspaceSafe(window, TARGET_WORKSPACE);
        window->m_workspace = TARGET_WORKSPACE;
    }

    if (MONITOR && MONITOR->m_activeWorkspace != window->m_workspace)
        MONITOR->changeWorkspace(window->m_workspace);

    FOCUSSTATE->fullWindowFocus(window, Desktop::FOCUS_REASON_KEYBIND);
    g_pCompositor->warpCursorTo(window->middle());
}

void CWindowOverview::finishClose() {
    const auto MONITOR = pMonitor.lock();

    g_pWindowOverview.reset();

    if (MONITOR)
        g_pHyprRenderer->damageMonitor(MONITOR);
}

void CWindowOverview::close(bool focusSelection, bool bringSelection, bool replaceInitialSelection) {
    if (closing)
        return;

    closing = true;
    animationStartedAt = Time::steadyNow();

    PHLWINDOW selectedWindow;
    if (focusSelection && selectedIndex >= 0 && selectedIndex < (int)previews.size())
        selectedWindow = previews[selectedIndex].window;

    if (selectedWindow)
        focusWindow(selectedWindow, bringSelection, replaceInitialSelection);

    damage();

    if (animationComplete())
        finishClose();
}
