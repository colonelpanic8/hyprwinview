#include "overview.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

#include <cairo/cairo.h>
#define private   public
#define protected public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
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

static const CConfigValue<Config::STRING>& PKEYSFILTERTOGGLE() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_filter_toggle");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSFILTERCLOSE() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_filter_close");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSFILTERLEFT() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_filter_left");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSFILTERRIGHT() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_filter_right");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSFILTERUP() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_filter_up");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSFILTERDOWN() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_filter_down");
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
    static const CConfigValue<Config::INTEGER> VALUE(
        "plugin:hyprwinview:app_icon_backplate_padding");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PSHOWWINDOWTEXT() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:show_window_text");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PWINDOWTEXTFONT() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:window_text_font");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PWINDOWTEXTSIZE() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:window_text_size");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PWINDOWTEXTCOLOR() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:window_text_color");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PWINDOWTEXTBACKPLATE() {
    static const CConfigValue<Config::INTEGER> VALUE(
        "plugin:hyprwinview:window_text_backplate_col");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PWINDOWTEXTPADDING() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:window_text_padding");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PFILTERANIMATIONMS() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:filter_animation_ms");
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

static const CConfigValue<Config::FLOAT>& PANIMATIONSPEED() {
    static const CConfigValue<Config::FLOAT> VALUE("plugin:hyprwinview:animation_speed");
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

static const CConfigValue<Config::FLOAT>& PANIMATIONWORKSPACEZOOMSTAGERATIO() {
    static const CConfigValue<Config::FLOAT> VALUE(
        "plugin:hyprwinview:animation_workspace_zoom_stage_ratio");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PANIMATIONWORKSPACEZOOMGAP() {
    static const CConfigValue<Config::INTEGER> VALUE(
        "plugin:hyprwinview:animation_workspace_zoom_gap");
    return VALUE;
}

enum class EOverviewAnimation : uint8_t {
    NONE,
    FADE,
    FADE_SCALE,
    STAGGERED_FADE_SCALE,
    WORKSPACE_ZOOM,
};

struct SWindowOrderingStrategy {
    const char* name;
    std::string (*groupKeyForWindow)(const PHLWINDOW& window, size_t originalIndex);
};

static std::optional<SWinviewKeyConfig> g_winviewKeyConfigOverride;

static constexpr uint64_t               DEFAULT_BACKGROUND = 0x99101014;

SWinviewKeyConfig                       defaultWinviewKeyConfig() {
    return {
                              .left         = {"a", "h", "left"},
                              .right        = {"d", "l", "right"},
                              .up           = {"w", "k", "up"},
                              .down         = {"s", "j", "down"},
                              .go           = {"return", "enter", "space", "g", "f"},
                              .bring        = {"b", "shift+return", "shift+space"},
                              .bringReplace = {"shift+b"},
                              .close        = {"escape", "q"},
                              .filterToggle = {"/"},
                              .filterClose  = {"escape", "ctrl+g"},
                              .filterLeft   = {"left"},
                              .filterRight  = {"right"},
                              .filterUp     = {"up", "ctrl+p"},
                              .filterDown   = {"down", "ctrl+n"},
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
    if (key == "backspace" || key == "bs")
        return "backspace";
    if (key == "delete" || key == "del")
        return "delete";
    if (key == "slash")
        return "/";

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
    if (NORMALIZED == "backspace")
        return XKB_KEY_BackSpace;
    if (NORMALIZED == "delete")
        return XKB_KEY_Delete;
    if (NORMALIZED == "/")
        return XKB_KEY_slash;

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

    constexpr uint32_t HANDLED_MODS =
        HL_MODIFIER_SHIFT | HL_MODIFIER_CTRL | HL_MODIFIER_ALT | HL_MODIFIER_META;

    return requiredKey != XKB_KEY_NoSymbol &&
        xkb_keysym_to_lower(requiredKey) == xkb_keysym_to_lower(keysym) &&
        (mods & HANDLED_MODS) == requiredMods;
}

static bool matchesKeySet(const std::vector<std::string>& keys, xkb_keysym_t keysym,
                          uint32_t mods) {
    for (const auto& token : keys) {
        if (tokenMatchesKey(token, keysym, mods))
            return true;
    }

    return false;
}

static std::string configStringOr(const CConfigValue<Config::STRING>& value,
                                  const std::string&                  fallback) {
    try {
        return *value;
    } catch (...) { return fallback; }
}

static SWinviewKeyConfig keyConfigFromConfigValues() {
    return {
        .left         = keyTokens(configStringOr(PKEYSLEFT(), "a,h,left")),
        .right        = keyTokens(configStringOr(PKEYSRIGHT(), "d,l,right")),
        .up           = keyTokens(configStringOr(PKEYSUP(), "w,k,up")),
        .down         = keyTokens(configStringOr(PKEYSDOWN(), "s,j,down")),
        .go           = keyTokens(configStringOr(PKEYSGO(), "return,enter,space,g,f")),
        .bring        = keyTokens(configStringOr(PKEYSBRING(), "b,shift+return,shift+space")),
        .bringReplace = keyTokens(configStringOr(PKEYSBRINGREPLACE(), "shift+b")),
        .close        = keyTokens(configStringOr(PKEYSCLOSE(), "escape,q")),
        .filterToggle = keyTokens(configStringOr(PKEYSFILTERTOGGLE(), "/")),
        .filterClose  = keyTokens(configStringOr(PKEYSFILTERCLOSE(), "escape,ctrl+g")),
        .filterLeft   = keyTokens(configStringOr(PKEYSFILTERLEFT(), "left")),
        .filterRight  = keyTokens(configStringOr(PKEYSFILTERRIGHT(), "right")),
        .filterUp     = keyTokens(configStringOr(PKEYSFILTERUP(), "up,ctrl+p")),
        .filterDown   = keyTokens(configStringOr(PKEYSFILTERDOWN(), "down,ctrl+n")),
    };
}

static SWinviewKeyConfig activeKeyConfig() {
    return g_winviewKeyConfigOverride.value_or(keyConfigFromConfigValues());
}

static std::string lower(std::string value) {
    std::ranges::transform(value, value.begin(), [](unsigned char c) { return std::tolower(c); });
    return value;
}

static std::vector<std::string> queryTokens(const std::string& query) {
    std::vector<std::string> result;
    std::stringstream        stream(lower(query));
    std::string              token;

    while (stream >> token)
        result.push_back(token);

    return result;
}

static std::string windowTitle(const PHLWINDOW& window) {
    if (!window)
        return "";

    if (!window->m_title.empty())
        return window->m_title;

    return window->m_initialTitle;
}

static std::string windowClass(const PHLWINDOW& window) {
    if (!window)
        return "";

    if (!window->m_class.empty())
        return window->m_class;

    return window->m_initialClass;
}

static std::string searchableWindowText(const PHLWINDOW& window) {
    if (!window)
        return "";

    return lower(windowTitle(window) + " " + windowClass(window) + " " + window->m_initialTitle +
                 " " + window->m_initialClass);
}

static bool windowMatchesQuery(const PHLWINDOW& window, const std::string& query) {
    const auto TOKENS = queryTokens(query);
    if (TOKENS.empty())
        return true;

    const auto SEARCHABLE = searchableWindowText(window);
    return std::ranges::all_of(TOKENS, [&SEARCHABLE](const std::string& token) {
        return SEARCHABLE.find(token) != std::string::npos;
    });
}

static std::string printableTextForKey(xkb_state* state, xkb_keycode_t keycode, uint32_t mods) {
    constexpr uint32_t TEXT_BLOCKING_MODS = HL_MODIFIER_CTRL | HL_MODIFIER_ALT | HL_MODIFIER_META;
    if (!state || (mods & TEXT_BLOCKING_MODS) != 0)
        return "";

    char      buffer[64] = {};
    const int len        = xkb_state_key_get_utf8(state, keycode, buffer, sizeof(buffer));
    if (len <= 0)
        return "";

    std::string result(buffer, std::min<int>(len, sizeof(buffer) - 1));
    if (std::ranges::any_of(result, [](unsigned char c) { return std::iscntrl(c); }))
        return "";

    return result;
}

static SP<IKeyboard> keyboardForKeyEvent(const IKeyboard::SKeyEvent& event) {
    if (event.state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        for (const auto& keyboard : g_pInputManager->m_keyboards) {
            if (keyboard && keyboard->m_enabled && keyboard->getPressed(event.keycode))
                return keyboard;
        }
    }

    return g_pSeatManager && !g_pSeatManager->m_keyboard.expired() ?
        g_pSeatManager->m_keyboard.lock() :
        nullptr;
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
    const auto NAME = trimmedLower(configStringOr(PWINDOWORDER(), "natural"));

    if (NAME.empty() || NAME == "none" || NAME == "natural" || NAME == "compositor")
        return naturalOrderStrategy();

    if (NAME == "app" || NAME == "application" || NAME == "application_grouped" ||
        NAME == "group_app" || NAME == "group_by_app" || NAME == "grouped_by_app")
        return applicationOrderStrategy();

    return naturalOrderStrategy();
}

static EOverviewAnimation overviewAnimation() {
    const auto NAME = trimmedLower(configStringOr(PANIMATION(), "fade_scale"));

    if (NAME == "none" || NAME == "off" || NAME == "disable" || NAME == "disabled")
        return EOverviewAnimation::NONE;
    if (NAME == "fade")
        return EOverviewAnimation::FADE;
    if (NAME == "stagger" || NAME == "staggered" || NAME == "staggered_fade_scale")
        return EOverviewAnimation::STAGGERED_FADE_SCALE;
    if (NAME == "workspace_zoom" || NAME == "workspace-zoom" || NAME == "expo" ||
        NAME == "hyprexpo")
        return EOverviewAnimation::WORKSPACE_ZOOM;

    return EOverviewAnimation::FADE_SCALE;
}

static bool animationScalesTiles(EOverviewAnimation animation) {
    return animation == EOverviewAnimation::FADE_SCALE ||
        animation == EOverviewAnimation::STAGGERED_FADE_SCALE;
}

static bool animationStaggersTiles(EOverviewAnimation animation) {
    return animation == EOverviewAnimation::STAGGERED_FADE_SCALE;
}

static bool animationUsesWorkspaceZoom(EOverviewAnimation animation) {
    return animation == EOverviewAnimation::WORKSPACE_ZOOM;
}

static double workspaceZoomStageRatio() {
    return std::clamp<double>(*PANIMATIONWORKSPACEZOOMSTAGERATIO(), 0.1, 0.9);
}

static int workspaceGridColsForCount(int count) {
    return std::max(1, (int)std::ceil(std::sqrt(std::max(1, count))));
}

static int workspaceGridRowsForCount(int count, int cols) {
    return std::max(1, (int)std::ceil((double)std::max(1, count) / std::max(1, cols)));
}

static double animationDurationMs(bool closing) {
    const double SPEED = std::clamp<double>(*PANIMATIONSPEED(), 0.1, 10.0);
    return static_cast<double>(
               std::max<Config::INTEGER>(0, closing ? *PANIMATIONOUTMS() : *PANIMATIONINMS())) /
        SPEED;
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

static double rawProgressForVisibleAmount(double visible, bool closing) {
    visible = std::clamp(visible, 0.0, 1.0);

    if (closing)
        return std::cbrt(1.0 - visible);

    return 1.0 - std::cbrt(1.0 - visible);
}

static CBox scaleBoxFromCenter(const CBox& box, double scale) {
    const auto CENTER = box.middle();
    const auto W      = box.w * scale;
    const auto H      = box.h * scale;

    return {CENTER.x - W / 2.0, CENTER.y - H / 2.0, W, H};
}

static CHyprColor multiplyAlpha(const CHyprColor& color, double alpha) {
    return color.modifyA(static_cast<float>(color.a * std::clamp(alpha, 0.0, 1.0)));
}

static double lerpDouble(double from, double to, double progress) {
    return from + (to - from) * std::clamp(progress, 0.0, 1.0);
}

static CBox lerpBox(const CBox& from, const CBox& to, double progress) {
    return {
        lerpDouble(from.x, to.x, progress),
        lerpDouble(from.y, to.y, progress),
        lerpDouble(from.w, to.w, progress),
        lerpDouble(from.h, to.h, progress),
    };
}

static CHyprColor activeBackgroundColor() {
    const auto BACKGROUND = static_cast<uint64_t>(*PBACKGROUND());
    const auto BG_COL     = static_cast<uint64_t>(*PBG());

    if (BACKGROUND == DEFAULT_BACKGROUND && BG_COL != DEFAULT_BACKGROUND)
        return CHyprColor(BG_COL);

    return CHyprColor(BACKGROUND);
}

struct STextTexture {
    SP<Render::ITexture> texture;
    Vector2D             size;
};

struct STextTextureCacheEntry {
    SP<Render::ITexture> texture;
    Vector2D             size;
};

static std::unordered_map<std::string, STextTextureCacheEntry> g_textTextureCache;

static double cairoTextWidth(cairo_t* cr, const std::string& text) {
    cairo_text_extents_t extents;
    cairo_text_extents(cr, text.c_str(), &extents);
    return extents.x_advance;
}

static std::string elideTextToWidth(cairo_t* cr, const std::string& text, double maxWidth) {
    if (text.empty() || cairoTextWidth(cr, text) <= maxWidth)
        return text;

    static constexpr const char* ELLIPSIS = "...";
    if (cairoTextWidth(cr, ELLIPSIS) > maxWidth)
        return "";

    std::string result = text;
    while (!result.empty() && cairoTextWidth(cr, result + ELLIPSIS) > maxWidth)
        result.pop_back();

    return result + ELLIPSIS;
}

static STextTexture textTextureForLines(const std::vector<std::string>& lines, int fontSizePx,
                                        int maxWidthPx, const CHyprColor& color,
                                        const std::string& font) {
    if (lines.empty() || fontSizePx <= 0 || maxWidthPx <= 0)
        return {};

    std::string key = font + "|" + std::to_string(fontSizePx) + "|" + std::to_string(maxWidthPx) +
        "|" + std::to_string((uint64_t)(color.r * 255.0)) + "," +
        std::to_string((uint64_t)(color.g * 255.0)) + "," +
        std::to_string((uint64_t)(color.b * 255.0)) + "," +
        std::to_string((uint64_t)(color.a * 255.0));
    for (const auto& line : lines)
        key += "|" + line;

    if (const auto IT = g_textTextureCache.find(key); IT != g_textTextureCache.end())
        return {.texture = IT->second.texture, .size = IT->second.size};

    cairo_surface_t* measureSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t*         measure        = cairo_create(measureSurface);
    cairo_select_font_face(measure, font.c_str(), CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(measure, fontSizePx);

    cairo_font_extents_t fontExtents;
    cairo_font_extents(measure, &fontExtents);

    std::vector<std::string> renderedLines;
    renderedLines.reserve(lines.size());
    double width = 1.0;
    for (const auto& line : lines) {
        auto renderedLine = elideTextToWidth(measure, line, maxWidthPx);
        width             = std::max(width, cairoTextWidth(measure, renderedLine));
        renderedLines.push_back(std::move(renderedLine));
    }

    const int textureWidth  = std::max(1, (int)std::ceil(std::min<double>(width, maxWidthPx)));
    const int lineHeight    = std::max(1, (int)std::ceil(fontExtents.height));
    const int textureHeight = std::max(1, lineHeight * (int)renderedLines.size());

    cairo_destroy(measure);
    cairo_surface_destroy(measureSurface);

    cairo_surface_t* surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, textureWidth, textureHeight);
    cairo_t* cr = cairo_create(surface);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_select_font_face(cr, font.c_str(), CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, fontSizePx);
    cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);

    for (size_t i = 0; i < renderedLines.size(); ++i) {
        cairo_move_to(cr, 0.0, fontExtents.ascent + static_cast<double>(i) * lineHeight);
        cairo_show_text(cr, renderedLines[i].c_str());
    }

    cairo_destroy(cr);

    auto texture = g_pHyprRenderer->createTexture(surface);
    cairo_surface_destroy(surface);

    if (!texture)
        return {};

    const Vector2D size{(double)textureWidth, (double)textureHeight};
    g_textTextureCache.emplace(key, STextTextureCacheEntry{.texture = texture, .size = size});
    return {.texture = texture, .size = size};
}

static Vector2D iconAnchorFromPosition(const std::string& position) {
    auto   anchor = Vector2D{1.0, 1.0};
    auto   value  = lower(position);
    size_t start  = 0;
    bool   hasX   = false;
    bool   hasY   = false;
    bool   center = false;

    while (start < value.size()) {
        const auto END = value.find_first_of(" ,", start);
        const auto TOKEN =
            value.substr(start, END == std::string::npos ? std::string::npos : END - start);

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

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static double edgeSignedMargin(double anchor, double absolute, double relative, double extent) {
    if (anchor < 0.5)
        return absolute + relative * extent;
    if (anchor > 0.5)
        return -(absolute + relative * extent);
    return 0.0;
}

static CBox appIconBoxForTile(const CBox& tileLogical, double scale) {
    const int SIZE   = static_cast<int>(std::max<Config::INTEGER>(1, *PAPPICONSIZE()));
    Vector2D  anchor = iconAnchorFromPosition(configStringOr(PAPPICONPOSITION(), "bottom right"));
    anchor.x         = anchorOverride(*PAPPICONANCHORX(), anchor.x);
    anchor.y         = anchorOverride(*PAPPICONANCHORY(), anchor.y);

    const double xMargin = edgeSignedMargin(anchor.x, static_cast<double>(*PAPPICONMARGINX()),
                                            *PAPPICONMARGINRELX(), tileLogical.w);
    const double yMargin = edgeSignedMargin(anchor.y, static_cast<double>(*PAPPICONMARGINY()),
                                            *PAPPICONMARGINRELY(), tileLogical.h);
    const double x = tileLogical.x + anchor.x * std::max(0.0, tileLogical.w - SIZE) + xMargin +
        static_cast<double>(*PAPPICONOFFSETX());
    const double y = tileLogical.y + anchor.y * std::max(0.0, tileLogical.h - SIZE) + yMargin +
        static_cast<double>(*PAPPICONOFFSETY());

    return CBox{x, y, (double)SIZE, (double)SIZE}.scale(scale).round();
}

static bool previewableWindow(const PHLWINDOW& window) {
    if (!window || !window->m_isMapped || window->isHidden() || window->m_fadingOut ||
        !window->m_workspace)
        return false;

    if (window->m_size.x <= 1 || window->m_size.y <= 1 || window->m_realSize->value().x <= 1 ||
        window->m_realSize->value().y <= 1)
        return false;

    return true;
}

CWindowOverview::CWindowOverview(const PHLMONITOR& monitor, SWindowOverviewOptions options_) :
    pMonitor(monitor), options(options_) {
    animationStartedAt       = Time::steadyNow();
    filterAnimationStartedAt = animationStartedAt;
    filterMode               = options.startInFilterMode;
    initialFocusedWindow     = Desktop::focusState()->window();
    initialFocusedWorkspace  = initialFocusedWindow && initialFocusedWindow->m_workspace ?
         initialFocusedWindow->m_workspace :
         (pMonitor ? pMonitor->m_activeWorkspace : nullptr);

    collectWindows();
    renderSnapshots();
    rebuildVisiblePreviews(false);

    lastMousePosLocal = g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position;
    selectedIndex     = hoveredIndex();
    if (selectedIndex < 0 && !previews.empty())
        selectedIndex = 0;

    auto onCursorMove = [this](Event::SCallbackInfo& info) {
        if (closing) {
            info.cancelled = true;
            return;
        }

        info.cancelled     = true;
        lastMousePosLocal  = g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position;
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

    mouseMoveHook = Event::bus()->m_events.input.mouse.move.listen(
        [onCursorMove](Vector2D, Event::SCallbackInfo& info) { onCursorMove(info); });
    touchMoveHook = Event::bus()->m_events.input.touch.motion.listen(
        [onCursorMove](const ITouch::SMotionEvent&, Event::SCallbackInfo& info) {
            onCursorMove(info);
        });
    mouseButtonHook = Event::bus()->m_events.input.mouse.button.listen(
        [onCursorSelect](const IPointer::SButtonEvent&, Event::SCallbackInfo& info) {
            onCursorSelect(info);
        });
    touchDownHook = Event::bus()->m_events.input.touch.down.listen(
        [onCursorSelect](const ITouch::SDownEvent&, Event::SCallbackInfo& info) {
            onCursorSelect(info);
        });
    keyboardHook = Event::bus()->m_events.input.keyboard.key.listen(
        [onKeyboardKey](IKeyboard::SKeyEvent event, Event::SCallbackInfo& info) {
            onKeyboardKey(event, info);
        });

    damage();
}

CWindowOverview::~CWindowOverview() {
    stopFilterDeleteRepeat();
    if (filterDeleteRepeatTimer && g_pEventLoopManager)
        g_pEventLoopManager->removeTimer(filterDeleteRepeatTimer);
    filterDeleteRepeatTimer.reset();

    Render::GL::g_pHyprOpenGL->makeEGLCurrent();
    for (auto& preview : allPreviews)
        preview.fb.reset();
    allPreviews.clear();
    previews.clear();
    exitingPreviews.clear();
}

void CWindowOverview::collectWindows() {
    allPreviews.clear();

    const auto CURRENT_WORKSPACE = pMonitor ? pMonitor->m_activeWorkspace : nullptr;

    for (auto it = g_pCompositor->m_windows.rbegin(); it != g_pCompositor->m_windows.rend(); ++it) {
        const auto& window = *it;
        if (!previewableWindow(window))
            continue;

        if (!options.includeCurrentWorkspace && CURRENT_WORKSPACE &&
            window->m_workspace == CURRENT_WORKSPACE)
            continue;

        allPreviews.push_back({.window = window});
    }

    std::ranges::reverse(allPreviews);
    applyWindowOrdering(allPreviews);
    updateWorkspaceGrid();
}

void CWindowOverview::applyWindowOrdering(std::vector<SWindowPreview>& windowPreviews) {
    if (windowPreviews.empty())
        return;

    const auto&              STRATEGY = activeWindowOrderingStrategy();
    std::vector<std::string> groupOrder;

    for (size_t i = 0; i < windowPreviews.size(); ++i) {
        auto& preview               = windowPreviews[i];
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

    std::ranges::stable_sort(windowPreviews, [](const SWindowPreview& a, const SWindowPreview& b) {
        if (a.orderGroupIndex != b.orderGroupIndex)
            return a.orderGroupIndex < b.orderGroupIndex;

        return a.orderOriginalIndex < b.orderOriginalIndex;
    });
}

void CWindowOverview::updateWorkspaceGrid() {
    int workspaceCount = 1;

    for (const auto& workspace : g_pCompositor->getWorkspacesCopy()) {
        if (!workspace || workspace->m_isSpecialWorkspace || workspace->m_id <= 0)
            continue;

        workspaceCount = std::max(workspaceCount, (int)workspace->m_id);
    }

    for (const auto& preview : allPreviews) {
        if (!preview.window || !preview.window->m_workspace ||
            preview.window->m_workspace->m_isSpecialWorkspace ||
            preview.window->m_workspace->m_id <= 0)
            continue;

        workspaceCount = std::max(workspaceCount, (int)preview.window->m_workspace->m_id);
    }

    workspaceGridCount = workspaceCount;
    workspaceGridCols  = workspaceGridColsForCount(workspaceGridCount);
    workspaceGridRows  = workspaceGridRowsForCount(workspaceGridCount, workspaceGridCols);
}

void CWindowOverview::rebuildVisiblePreviews(bool animate) {
    auto OLD_PREVIEWS = previews;
    if (filterAnimating) {
        for (auto& preview : OLD_PREVIEWS)
            preview.tileLogical = filterTransitionTileLogicalBox(preview);
    }

    const auto OLD_SELECTED = selectedIndex >= 0 && selectedIndex < (int)previews.size() ?
        previews[selectedIndex].window :
        PHLWINDOW{};

    previews.clear();
    for (const auto& preview : allPreviews) {
        if (windowMatchesQuery(preview.window, filterQuery))
            previews.push_back(preview);
    }

    updateLayout();

    if (animate) {
        exitingPreviews.clear();

        for (const auto& oldPreview : OLD_PREVIEWS) {
            const bool STILL_VISIBLE = std::ranges::any_of(
                previews, [&oldPreview](const auto& p) { return p.window == oldPreview.window; });
            if (!STILL_VISIBLE) {
                auto exiting               = oldPreview;
                exiting.filterStartLogical = oldPreview.tileLogical;
                exitingPreviews.push_back(std::move(exiting));
            }
        }

        for (auto& preview : previews) {
            const auto OLD = std::ranges::find_if(
                OLD_PREVIEWS, [&preview](const auto& old) { return old.window == preview.window; });
            preview.filterStartLogical =
                OLD != OLD_PREVIEWS.end() ? OLD->tileLogical : preview.tileLogical;
        }

        filterAnimating          = true;
        filterAnimationStartedAt = Time::steadyNow();
    } else {
        exitingPreviews.clear();
        filterAnimating = false;
        for (auto& preview : previews)
            preview.filterStartLogical = preview.tileLogical;
    }

    selectedIndex = -1;
    if (OLD_SELECTED) {
        for (size_t i = 0; i < previews.size(); ++i) {
            if (previews[i].window == OLD_SELECTED) {
                selectedIndex = static_cast<int>(i);
                break;
            }
        }
    }

    if (selectedIndex < 0 && !previews.empty())
        selectedIndex = 0;

    if (selectedIndex >= 0)
        lastMousePosLocal = previews[selectedIndex].tileLogical.middle();
}

void CWindowOverview::updateLayout() {
    if (!pMonitor || previews.empty()) {
        gridCols = 1;
        return;
    }

    const double count  = static_cast<double>(previews.size());
    const double aspect = std::max(0.1, pMonitor->m_size.x / std::max(1.0, pMonitor->m_size.y));
    int          cols   = std::max(1, (int)std::ceil(std::sqrt(count * aspect)));
    int          rows   = std::max(1, (int)std::ceil(count / cols));

    while (cols > 1 && (cols - 1) * rows >= (int)previews.size())
        cols--;

    rows     = std::max(1, (int)std::ceil(count / cols));
    gridCols = cols;

    const double margin = static_cast<double>(std::max<Config::INTEGER>(0, *PMARGIN()));
    const double gap    = static_cast<double>(std::max<Config::INTEGER>(0, *PGAP()));
    const double areaW  = std::max(1.0, pMonitor->m_size.x - margin * 2.0);
    const double areaH  = std::max(1.0, pMonitor->m_size.y - margin * 2.0);
    const double cellW  = (areaW - gap * (cols - 1)) / cols;
    const double cellH  = (areaH - gap * (rows - 1)) / rows;

    for (size_t i = 0; i < previews.size(); ++i) {
        auto&        preview = previews[i];
        const auto   cell    = visualCellForPreviewIndex((int)i);
        const double row     = cell.first;
        const double col     = cell.second;
        const auto   winSize = preview.window->m_realSize->value();
        const double scale =
            std::min(cellW / std::max(1.0, winSize.x), cellH / std::max(1.0, winSize.y));
        const double w = std::max(1.0, winSize.x * scale);
        const double h = std::max(1.0, winSize.y * scale);
        const double x = margin + col * (cellW + gap) + (cellW - w) / 2.0;
        const double y = margin + row * (cellH + gap) + (cellH - h) / 2.0;

        preview.tileLogical = {x, y, w, h};
    }
}

void CWindowOverview::renderSnapshots() {
    if (!pMonitor)
        return;

    Render::GL::g_pHyprOpenGL->makeEGLCurrent();

    const auto FORMAT = framebufferFormatWithAlpha(pMonitor->m_output->state->state().drmFormat);

    for (auto& preview : allPreviews) {
        if (!preview.fb)
            preview.fb = g_pHyprRenderer->createFB("hyprwinview");

        if (preview.fb->m_size != pMonitor->m_pixelSize) {
            preview.fb->release();
            preview.fb->alloc(static_cast<int>(pMonitor->m_pixelSize.x),
                              static_cast<int>(pMonitor->m_pixelSize.y), FORMAT);
        }

        CRegion fakeDamage{0, 0,
                           static_cast<double>(static_cast<int>(pMonitor->m_transformedSize.x)),
                           static_cast<double>(static_cast<int>(pMonitor->m_transformedSize.y))};
        if (!g_pHyprRenderer->beginFullFakeRender(pMonitor.lock(), fakeDamage, preview.fb))
            continue;

        g_pHyprRenderer->m_bRenderingSnapshot = true;
        g_pHyprRenderer->draw(CClearPassElement::SClearData{CHyprColor(0, 0, 0, 0)});
        g_pHyprRenderer->startRenderPass();
        g_pHyprRenderer->renderWindow(preview.window, pMonitor.lock(), Time::steadyNow(), false,
                                      Render::RENDER_PASS_ALL, true, true);
        g_pHyprRenderer->m_renderData.blockScreenShader = true;
        g_pHyprRenderer->endRender();
        g_pHyprRenderer->m_bRenderingSnapshot = false;
    }
}

int CWindowOverview::hoveredIndex() const {
    for (size_t i = 0; i < previews.size(); ++i) {
        if (previews[i].tileLogical.containsPoint(lastMousePosLocal))
            return static_cast<int>(i);
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

    const double SCALE      = pMonitor->m_scale;
    const auto   HOVERED    = selectedIndex;
    const int    BORDER     = static_cast<int>(std::max<Config::INTEGER>(0, *PBORDERSIZE()));
    const auto   ANIMATION  = overviewAnimation();
    const auto   VISIBLE    = animationVisibleAmount();
    const auto   BASE_SCALE = std::clamp<double>(*PANIMATIONSCALE(), 0.01, 1.0);
    CRegion      fullDamage = {0, 0, INT16_MAX, INT16_MAX};

    Render::GL::g_pHyprOpenGL->renderRect(
        CBox{{0, 0}, pMonitor->m_pixelSize}, multiplyAlpha(activeBackgroundColor(), VISIBLE),
        {.damage = &fullDamage, .blur = backgroundBlurEnabled(), .blurA = (float)VISIBLE});

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    auto drawPreview = [&](SWindowPreview& preview, CBox tilePx, double tileVisible,
                           double textureAlpha, bool selected) {
        if (!preview.fb || !preview.fb->getTexture())
            return;

        CBox texBox = {
            tilePx.x,
            tilePx.y,
            pMonitor->m_pixelSize.x *
                (tilePx.w / std::max(1.0, preview.window->m_realSize->value().x * SCALE)),
            pMonitor->m_pixelSize.y *
                (tilePx.h / std::max(1.0, preview.window->m_realSize->value().y * SCALE)),
        };

        if (BORDER > 0) {
            const auto COLOR = selected ? CHyprColor(*PHOVERBORDER()) : CHyprColor(*PBORDER());
            Render::GL::g_pHyprOpenGL->renderRect(tilePx.copy().expand(BORDER),
                                                  multiplyAlpha(COLOR, tileVisible),
                                                  {.damage = &fullDamage, .round = BORDER * 2});
        }

        g_pHyprRenderer->m_renderData.clipBox = tilePx;
        Render::GL::g_pHyprOpenGL->renderTexture(
            preview.fb->getTexture(), texBox,
            {.damage = &fullDamage, .a = (float)textureAlpha, .round = BORDER * 2});
        g_pHyprRenderer->m_renderData.clipBox = {};

        if (*PSHOWAPPICON() != 0) {
            const int ICON_SIZE_PX = std::max(
                1,
                (int)std::round(static_cast<double>(std::max<Config::INTEGER>(1, *PAPPICONSIZE())) *
                                SCALE));
            if (const auto ICON = appIconTextureForWindow(preview.window, ICON_SIZE_PX)) {
                CBox      currentLogicalTile{tilePx.x / SCALE, tilePx.y / SCALE, tilePx.w / SCALE,
                                        tilePx.h / SCALE};
                CBox      iconBox = appIconBoxForTile(currentLogicalTile, SCALE);
                const int PADDING =
                    std::max(0,
                             (int)std::round(static_cast<double>(std::max<Config::INTEGER>(
                                                 0, *PAPPICONBACKPLATEPADDING())) *
                                             SCALE));
                if (PADDING > 0)
                    Render::GL::g_pHyprOpenGL->renderRect(
                        iconBox.copy().expand(PADDING).round(),
                        multiplyAlpha(CHyprColor(*PAPPICONBACKPLATE()), tileVisible),
                        {.damage = &fullDamage, .round = std::max(1, PADDING)});

                Render::GL::g_pHyprOpenGL->renderTexture(
                    ICON, iconBox, {.damage = &fullDamage, .a = (float)tileVisible});
            }
        }

        if (*PSHOWWINDOWTEXT() != 0) {
            const int PADDING =
                std::max(0, (int)std::round(static_cast<double>(*PWINDOWTEXTPADDING()) * SCALE));
            const int FONT_SIZE =
                std::max(1, (int)std::round(static_cast<double>(*PWINDOWTEXTSIZE()) * SCALE));
            const int  MAX_WIDTH = std::max(1, (int)std::round(tilePx.w - PADDING * 2));
            const auto TITLE     = windowTitle(preview.window);
            const auto CLASS     = windowClass(preview.window);
            std::vector<std::string> lines;
            if (!TITLE.empty())
                lines.push_back(TITLE);
            if (!CLASS.empty() && CLASS != TITLE)
                lines.push_back(CLASS);

            if (!lines.empty()) {
                const auto TEXT = textTextureForLines(lines, FONT_SIZE, MAX_WIDTH,
                                                      CHyprColor(*PWINDOWTEXTCOLOR()),
                                                      configStringOr(PWINDOWTEXTFONT(), "Sans"));
                if (TEXT.texture) {
                    const auto WIDTH  = std::min<double>(TEXT.size.x, MAX_WIDTH);
                    const auto HEIGHT = TEXT.size.y;
                    CBox       labelBox{
                        tilePx.x + PADDING,
                        std::max(tilePx.y + PADDING, tilePx.y + tilePx.h - HEIGHT - PADDING),
                        WIDTH,
                        HEIGHT,
                    };

                    Render::GL::g_pHyprOpenGL->renderRect(
                        labelBox.copy().expand(PADDING).round(),
                        multiplyAlpha(CHyprColor(*PWINDOWTEXTBACKPLATE()), tileVisible),
                        {.damage = &fullDamage, .round = std::max(1, PADDING)});
                    Render::GL::g_pHyprOpenGL->renderTexture(
                        TEXT.texture, labelBox, {.damage = &fullDamage, .a = (float)tileVisible});
                }
            }
        }
    };

    const auto FILTER_PROGRESS = filterTransitionVisibleAmount();
    for (auto& preview : exitingPreviews) {
        const auto EXIT_VISIBLE = VISIBLE * (1.0 - FILTER_PROGRESS);
        if (EXIT_VISIBLE <= 0.0)
            continue;

        CBox tilePx = scaleBoxFromCenter(preview.tileLogical, 0.96 + 0.04 * EXIT_VISIBLE)
                          .scale(SCALE)
                          .round();
        drawPreview(preview, tilePx, EXIT_VISIBLE, EXIT_VISIBLE, false);
    }

    for (size_t i = 0; i < previews.size(); ++i) {
        auto& preview = previews[i];
        if (!preview.fb || !preview.fb->getTexture())
            continue;

        const auto TILE_VISIBLE  = tileAnimationVisibleAmount(i);
        const auto TEXTURE_ALPHA = animatedTileTextureAlpha(i, TILE_VISIBLE);
        const auto TILE_SCALE =
            animationScalesTiles(ANIMATION) ? BASE_SCALE + (1.0 - BASE_SCALE) * TILE_VISIBLE : 1.0;
        CBox tilePx = scaleBoxFromCenter(animatedTileLogicalBox(i, TILE_VISIBLE), TILE_SCALE)
                          .scale(SCALE)
                          .round();

        drawPreview(preview, tilePx, TILE_VISIBLE, TEXTURE_ALPHA, (int)i == HOVERED);
    }

    if (filterMode || !filterQuery.empty()) {
        const int PADDING = std::max(4, (int)std::round(8.0 * SCALE));
        const int FONT_SIZE =
            std::max(1, (int)std::round(static_cast<double>(*PWINDOWTEXTSIZE() + 2) * SCALE));
        const int MAX_WIDTH = std::max(1, (int)std::round(pMonitor->m_pixelSize.x * 0.72));
        std::vector<std::string> lines = {
            "Filter: " + filterQuery + (filterMode ? "_" : ""),
        };
        if (previews.empty() && !filterQuery.empty())
            lines.push_back("No matches");

        const auto TEXT =
            textTextureForLines(lines, FONT_SIZE, MAX_WIDTH, CHyprColor(*PWINDOWTEXTCOLOR()),
                                configStringOr(PWINDOWTEXTFONT(), "Sans"));
        if (TEXT.texture) {
            CBox promptBox{
                std::round((pMonitor->m_pixelSize.x - TEXT.size.x) / 2.0),
                std::round(static_cast<double>(std::max<Config::INTEGER>(0, *PMARGIN())) * SCALE /
                           2.0),
                TEXT.size.x,
                TEXT.size.y,
            };
            Render::GL::g_pHyprOpenGL->renderRect(
                promptBox.copy().expand(PADDING).round(),
                multiplyAlpha(CHyprColor(*PWINDOWTEXTBACKPLATE()), VISIBLE),
                {.damage = &fullDamage, .round = std::max(1, PADDING)});
            Render::GL::g_pHyprOpenGL->renderTexture(TEXT.texture, promptBox,
                                                     {.damage = &fullDamage, .a = (float)VISIBLE});
        }
    }

    if (filterAnimating && FILTER_PROGRESS >= 1.0) {
        filterAnimating = false;
        exitingPreviews.clear();
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

std::pair<int, int> CWindowOverview::visualCellForPreviewIndex(int index) const {
    const int cols     = std::max(1, gridCols);
    const int row      = index / cols;
    const int colInRow = index % cols;
    const int col      = row % 2 == 0 ? colInRow : cols - 1 - colInRow;

    return {row, col};
}

int CWindowOverview::previewIndexForVisualCell(int row, int col) const {
    const int cols = std::max(1, gridCols);

    if (row < 0 || col < 0 || col >= cols)
        return -1;

    const int colInRow = row % 2 == 0 ? col : cols - 1 - col;
    const int index    = row * cols + colInRow;

    if (index < 0 || index >= (int)previews.size())
        return -1;

    return index;
}

void CWindowOverview::moveSelection(int dx, int dy) {
    if (previews.empty())
        return;

    if (selectedIndex < 0 || selectedIndex >= (int)previews.size())
        selectedIndex = 0;

    const int  cols   = std::max(1, gridCols);
    const auto cell   = visualCellForPreviewIndex(selectedIndex);
    const int  row    = cell.first;
    const int  col    = cell.second;
    const int  newCol = std::clamp(col + dx, 0, cols - 1);
    const int  maxRow = ((int)previews.size() - 1) / cols;
    const int  newRow = std::clamp(row + dy, 0, maxRow);
    int        next   = previewIndexForVisualCell(newRow, newCol);

    if (next < 0)
        next = static_cast<int>(previews.size()) - 1;

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

    const auto DURATION =
        animationDurationMs(closing) + (closing ? maxTileAnimationDelayMs() : 0.0);
    const auto ELAPSED =
        std::chrono::duration<double, std::milli>(Time::steadyNow() - animationStartedAt).count();

    return visibleAmountForElapsed(ELAPSED, DURATION, closing);
}

double CWindowOverview::tileAnimationVisibleAmount(size_t index) const {
    const auto ANIMATION = overviewAnimation();
    if (ANIMATION == EOverviewAnimation::NONE)
        return 1.0;

    const auto DURATION = animationDurationMs(closing);
    const auto ELAPSED =
        std::chrono::duration<double, std::milli>(Time::steadyNow() - animationStartedAt).count() -
        tileAnimationDelayMs(index);

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

    const size_t ORDER_INDEX =
        closing ? previews.size() - 1 - std::min(index, previews.size() - 1) : index;
    return std::min<double>(static_cast<double>(ORDER_INDEX) * static_cast<double>(STAGGER_MS),
                            static_cast<double>(MAX_STAGGER_MS));
}

double CWindowOverview::maxTileAnimationDelayMs() const {
    const auto ANIMATION = overviewAnimation();
    if (!animationStaggersTiles(ANIMATION) || previews.empty())
        return 0.0;

    const auto STAGGER_MS     = std::max<Config::INTEGER>(0, *PANIMATIONSTAGGERMS());
    const auto MAX_STAGGER_MS = std::max<Config::INTEGER>(0, *PANIMATIONSTAGGERMAXMS());
    if (STAGGER_MS <= 0 || MAX_STAGGER_MS <= 0)
        return 0.0;

    return std::min<double>(static_cast<double>(previews.size() - 1) *
                                static_cast<double>(STAGGER_MS),
                            static_cast<double>(MAX_STAGGER_MS));
}

int CWindowOverview::workspacePanelIndexForWorkspace(const PHLWORKSPACE& workspace) const {
    if (workspace && !workspace->m_isSpecialWorkspace && workspace->m_id > 0)
        return std::clamp((int)workspace->m_id - 1, 0, std::max(0, workspaceGridCount - 1));

    return std::clamp(workspaceGridCount / 2, 0, std::max(0, workspaceGridCount - 1));
}

CBox CWindowOverview::workspacePanelCellLogical(int index) const {
    if (!pMonitor)
        return {};

    const int count = std::max(1, workspaceGridCount);
    const int cols  = std::max(1, workspaceGridCols);
    const int rows  = std::max(1, workspaceGridRows);

    index = std::clamp(index, 0, count - 1);

    const double margin = static_cast<double>(std::max<Config::INTEGER>(0, *PMARGIN()));
    const double gap =
        static_cast<double>(std::max<Config::INTEGER>(0, *PANIMATIONWORKSPACEZOOMGAP()));
    const double areaW = std::max(1.0, pMonitor->m_size.x - margin * 2.0);
    const double areaH = std::max(1.0, pMonitor->m_size.y - margin * 2.0);
    const double cellW = std::max(1.0, (areaW - gap * (cols - 1)) / cols);
    const double cellH = std::max(1.0, (areaH - gap * (rows - 1)) / rows);
    const int    col   = index % cols;
    const int    row   = index / cols;

    return {margin + col * (cellW + gap), margin + row * (cellH + gap), cellW, cellH};
}

CBox CWindowOverview::workspacePanelBoxForPreview(const SWindowPreview& preview) const {
    if (!preview.window || !pMonitor)
        return {};

    const auto SOURCE_MONITOR =
        preview.window->m_monitor.lock() ? preview.window->m_monitor.lock() : pMonitor.lock();
    if (!SOURCE_MONITOR)
        return preview.tileLogical;

    const auto panelIndex = workspacePanelIndexForWorkspace(preview.window->m_workspace);
    const auto CELL       = workspacePanelCellLogical(panelIndex);
    const auto WIN_POS    = preview.window->m_realPosition->value();
    const auto WIN_SIZE   = preview.window->m_realSize->value();
    const auto MONITOR_W  = std::max(1.0, SOURCE_MONITOR->m_size.x);
    const auto MONITOR_H  = std::max(1.0, SOURCE_MONITOR->m_size.y);
    const auto LOCAL_X    = (WIN_POS.x - SOURCE_MONITOR->m_position.x) / MONITOR_W;
    const auto LOCAL_Y    = (WIN_POS.y - SOURCE_MONITOR->m_position.y) / MONITOR_H;
    const auto LOCAL_W    = WIN_SIZE.x / MONITOR_W;
    const auto LOCAL_H    = WIN_SIZE.y / MONITOR_H;

    return {
        CELL.x + LOCAL_X * CELL.w,
        CELL.y + LOCAL_Y * CELL.h,
        std::max(1.0, LOCAL_W * CELL.w),
        std::max(1.0, LOCAL_H * CELL.h),
    };
}

CBox CWindowOverview::workspaceZoomCameraBoxForPanelBox(const CBox& panelBox,
                                                        double      cameraProgress) const {
    if (!pMonitor)
        return panelBox;

    const auto START_WORKSPACE = initialFocusedWorkspace ?
        initialFocusedWorkspace :
        (pMonitor ? pMonitor->m_activeWorkspace : nullptr);
    const auto START_CELL =
        workspacePanelCellLogical(workspacePanelIndexForWorkspace(START_WORKSPACE));
    const auto   PROGRESS = std::clamp(cameraProgress, 0.0, 1.0);
    const auto   VIEWPORT = CBox{0, 0, pMonitor->m_size.x, pMonitor->m_size.y};

    const double startScaleX = VIEWPORT.w / std::max(1.0, START_CELL.w);
    const double startScaleY = VIEWPORT.h / std::max(1.0, START_CELL.h);
    const double startX      = VIEWPORT.x - START_CELL.x * startScaleX;
    const double startY      = VIEWPORT.y - START_CELL.y * startScaleY;
    const double scaleX      = lerpDouble(startScaleX, 1.0, PROGRESS);
    const double scaleY      = lerpDouble(startScaleY, 1.0, PROGRESS);
    const double x           = lerpDouble(startX, 0.0, PROGRESS);
    const double y           = lerpDouble(startY, 0.0, PROGRESS);

    return {
        panelBox.x * scaleX + x,
        panelBox.y * scaleY + y,
        panelBox.w * scaleX,
        panelBox.h * scaleY,
    };
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
CBox CWindowOverview::animatedTileLogicalBox(size_t index, double progress) const {
    if (index >= previews.size())
        return {};

    if (!animationUsesWorkspaceZoom(overviewAnimation()))
        return filterTransitionTileLogicalBox(previews[index]);

    const auto& PREVIEW = previews[index];
    const auto  SPLIT   = workspaceZoomStageRatio();
    const auto  PANEL   = workspacePanelBoxForPreview(PREVIEW);
    const auto  FINAL   = filterTransitionTileLogicalBox(PREVIEW);
    const auto  RAW     = rawProgressForVisibleAmount(progress, closing);

    if (closing) {
        const auto GATHER_DURATION = 1.0 - SPLIT;
        if (RAW <= GATHER_DURATION)
            return lerpBox(FINAL, PANEL, easeInCubic(RAW / GATHER_DURATION));

        return workspaceZoomCameraBoxForPanelBox(
            PANEL, 1.0 - easeInCubic((RAW - GATHER_DURATION) / SPLIT));
    }

    if (RAW <= SPLIT)
        return workspaceZoomCameraBoxForPanelBox(PANEL, easeOutCubic(RAW / SPLIT));

    return lerpBox(PANEL, FINAL, easeOutCubic((RAW - SPLIT) / (1.0 - SPLIT)));
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
double CWindowOverview::animatedTileTextureAlpha(size_t index, double progress) const {
    if (index >= previews.size())
        return 0.0;

    if (!animationUsesWorkspaceZoom(overviewAnimation()))
        return progress;

    return 1.0;
}

CBox CWindowOverview::filterTransitionTileLogicalBox(const SWindowPreview& preview) const {
    if (closing || !filterAnimating)
        return preview.tileLogical;

    return lerpBox(preview.filterStartLogical, preview.tileLogical,
                   filterTransitionVisibleAmount());
}

double CWindowOverview::filterTransitionVisibleAmount() const {
    if (!filterAnimating)
        return 1.0;

    const double DURATION =
        static_cast<double>(std::max<Config::INTEGER>(0, *PFILTERANIMATIONMS()));
    if (DURATION <= 0.0)
        return 1.0;

    const auto ELAPSED =
        std::chrono::duration<double, std::milli>(Time::steadyNow() - filterAnimationStartedAt)
            .count();
    return easeOutCubic(ELAPSED / DURATION);
}

bool CWindowOverview::animationComplete() const {
    const auto ANIMATION = overviewAnimation();
    if (ANIMATION == EOverviewAnimation::NONE)
        return true;

    const auto DURATION = animationDurationMs(closing);
    if (DURATION <= 0.0)
        return true;

    const auto ELAPSED =
        std::chrono::duration<double, std::milli>(Time::steadyNow() - animationStartedAt).count();
    const auto TOTAL_TIME_MS = DURATION + maxTileAnimationDelayMs();
    return ELAPSED >= TOTAL_TIME_MS;
}

bool CWindowOverview::isAnimating() const {
    return !animationComplete() || (filterAnimating && filterTransitionVisibleAmount() < 1.0);
}

bool CWindowOverview::backgroundBlurEnabled() const {
    return *PBACKGROUNDBLUR() != 0;
}

bool CWindowOverview::occludesScene() const {
    return !isAnimating() && activeBackgroundColor().a >= 1.0;
}

bool CWindowOverview::handleKey(const IKeyboard::SKeyEvent& event) {
    const auto KEYBOARD = keyboardForKeyEvent(event);
    if (!KEYBOARD || !KEYBOARD->m_xkbState)
        return false;

    const auto KEYCODE = event.keycode + 8;
    const auto KEYSYM  = xkb_state_key_get_one_sym(KEYBOARD->m_xkbState, KEYCODE);
    const auto MODS    = g_pInputManager->getModsFromAllKBs();
    const auto KEYS    = activeKeyConfig();

    if (filterMode)
        return handleFilterKey(event, KEYSYM, KEYBOARD->m_xkbState, MODS, KEYS);

    const bool RECOGNIZED = matchesKeySet(KEYS.left, KEYSYM, MODS) ||
        matchesKeySet(KEYS.right, KEYSYM, MODS) || matchesKeySet(KEYS.up, KEYSYM, MODS) ||
        matchesKeySet(KEYS.down, KEYSYM, MODS) || matchesKeySet(KEYS.go, KEYSYM, MODS) ||
        matchesKeySet(KEYS.bring, KEYSYM, MODS) || matchesKeySet(KEYS.bringReplace, KEYSYM, MODS) ||
        matchesKeySet(KEYS.close, KEYSYM, MODS) || matchesKeySet(KEYS.filterToggle, KEYSYM, MODS);

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
    else if (matchesKeySet(KEYS.filterToggle, KEYSYM, MODS))
        toggleFilterMode();

    return true;
}

bool CWindowOverview::handleFilterKey(const IKeyboard::SKeyEvent& event, xkb_keysym_t keysym,
                                      xkb_state* keyboardState, uint32_t mods,
                                      const SWinviewKeyConfig& keys) {
    const bool DELETE_KEY = keysym == XKB_KEY_BackSpace || keysym == XKB_KEY_Delete;
    if (event.state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        if (DELETE_KEY)
            stopFilterDeleteRepeat();
        return true;
    }

    if (event.state != WL_KEYBOARD_KEY_STATE_PRESSED)
        return true;

    if (matchesKeySet(keys.filterClose, keysym, mods)) {
        close(false);
        return true;
    }

    if (matchesKeySet(keys.filterToggle, keysym, mods)) {
        toggleFilterMode();
        return true;
    }

    const bool RETURN_KEY = keysym == XKB_KEY_Return || keysym == XKB_KEY_KP_Enter;
    if (RETURN_KEY && matchesKeySet(keys.bringReplace, keysym, mods)) {
        runSelected(true, true);
        return true;
    }

    if (RETURN_KEY && matchesKeySet(keys.bring, keysym, mods)) {
        runSelected(true);
        return true;
    }

    if (RETURN_KEY && matchesKeySet(keys.go, keysym, mods)) {
        runSelected(false);
        return true;
    }

    if (matchesKeySet(keys.filterLeft, keysym, mods)) {
        moveSelection(-1, 0);
        return true;
    }

    if (matchesKeySet(keys.filterRight, keysym, mods)) {
        moveSelection(1, 0);
        return true;
    }

    if (matchesKeySet(keys.filterUp, keysym, mods)) {
        moveSelection(0, -1);
        return true;
    }

    if (matchesKeySet(keys.filterDown, keysym, mods)) {
        moveSelection(0, 1);
        return true;
    }

    if (DELETE_KEY) {
        deleteFilterCharacter();
        startFilterDeleteRepeat();
        return true;
    }

    if ((mods & HL_MODIFIER_CTRL) != 0 &&
        xkb_keysym_to_lower(keysym) == xkb_keysym_to_lower(XKB_KEY_u)) {
        setFilterQuery("");
        return true;
    }

    const auto TEXT = printableTextForKey(keyboardState, event.keycode + 8, mods);
    if (!TEXT.empty()) {
        setFilterQuery(filterQuery + TEXT);
        return true;
    }

    return true;
}

bool CWindowOverview::deleteFilterCharacter() {
    if (filterQuery.empty())
        return false;

    auto query = filterQuery;
    query.pop_back();
    setFilterQuery(std::move(query));
    return true;
}

void CWindowOverview::startFilterDeleteRepeat() {
    filterDeleteHeld = true;

    if (!filterDeleteRepeatTimer) {
        filterDeleteRepeatTimer = makeShared<CEventLoopTimer>(
            std::nullopt,
            [this](SP<CEventLoopTimer> self, void*) {
                if (!filterDeleteHeld || closing)
                    return;

                deleteFilterCharacter();
                self->updateTimeout(std::chrono::milliseconds(35));
            },
            nullptr);

        if (g_pEventLoopManager)
            g_pEventLoopManager->addTimer(filterDeleteRepeatTimer);
    }

    filterDeleteRepeatTimer->updateTimeout(std::chrono::milliseconds(300));
}

void CWindowOverview::stopFilterDeleteRepeat() {
    filterDeleteHeld = false;
    if (filterDeleteRepeatTimer)
        filterDeleteRepeatTimer->updateTimeout(std::nullopt);
}

void CWindowOverview::toggleFilterMode() {
    filterMode = !filterMode;
    damage();
}

void CWindowOverview::setFilterQuery(std::string query, bool animate) {
    if (filterQuery == query)
        return;

    filterQuery = std::move(query);
    rebuildVisiblePreviews(animate);
    damage();
}

void CWindowOverview::focusWindow(const PHLWINDOW& window, bool bring, bool replaceInitial) {
    if (!window || !window->m_workspace)
        return;

    const auto FOCUSSTATE                  = Desktop::focusState();
    const auto MONITOR                     = FOCUSSTATE->monitor();
    const auto TARGET_WORKSPACE            = replaceInitial && initialFocusedWorkspace ?
                   initialFocusedWorkspace :
                   (MONITOR ? MONITOR->m_activeWorkspace : nullptr);
    const auto SELECTED_ORIGINAL_WORKSPACE = window->m_workspace;
    const auto SELECTED_TARGET             = window->layoutTarget();
    const auto INITIAL_TARGET =
        initialFocusedWindow ? initialFocusedWindow->layoutTarget() : SP<Layout::ITarget>{};
    const bool CAN_REPLACE_INITIAL = replaceInitial && g_layoutManager && initialFocusedWindow &&
        initialFocusedWindow != window && initialFocusedWindow->m_isMapped &&
        initialFocusedWindow->m_workspace && SELECTED_ORIGINAL_WORKSPACE && SELECTED_TARGET &&
        INITIAL_TARGET && !window->isFullscreen() && !initialFocusedWindow->isFullscreen();

    if (CAN_REPLACE_INITIAL) {
        g_layoutManager->switchTargets(SELECTED_TARGET, INITIAL_TARGET, true);
    } else if (replaceInitial && initialFocusedWindow && initialFocusedWindow != window &&
               initialFocusedWindow->m_isMapped && initialFocusedWindow->m_workspace &&
               SELECTED_ORIGINAL_WORKSPACE &&
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

void CWindowOverview::close(bool focusSelection, bool bringSelection,
                            bool replaceInitialSelection) {
    if (closing)
        return;

    stopFilterDeleteRepeat();

    closing            = true;
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
