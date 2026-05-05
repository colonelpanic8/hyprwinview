#define WLR_USE_UNSTABLE

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/values/types/ColorValue.hpp>
#include <hyprland/src/config/values/types/FloatValue.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>
#include <hyprland/src/config/values/types/StringValue.hpp>
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>

#include <lua.hpp>

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "AppIcon.hpp"
#include "globals.hpp"
#include "overview.hpp"

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

static void failNotif(const std::string& reason) {
    HyprlandAPI::addNotification(PHANDLE, "[hyprwinview] Failure in initialization: " + reason,
                                 CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
}

static bool addConfigValue(const SP<Config::Values::IValue>& value) {
    const auto RET = Config::mgr()->registerPluginValue(PHANDLE, value);
    if (!RET) {
        Log::logger->log(Log::ERR, "[hyprwinview] failed to register plugin value \"{}\": {}",
                         value->name(), RET.error());
        return false;
    }

    return true;
}

struct SWinviewDispatcherArgs {
    std::string            action = "toggle";
    SWindowOverviewOptions options;
};

static std::string normalizedArgToken(std::string token) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    token.erase(token.begin(), std::ranges::find_if(token, notSpace));
    token.erase(std::ranges::find_if(token.rbegin(), token.rend(), notSpace).base(), token.end());
    std::ranges::transform(token, token.begin(), [](unsigned char c) {
        if (c == '_')
            return '-';
        return (char)std::tolower(c);
    });
    return token;
}

static std::vector<std::string> argTokens(const std::string& arg) {
    std::vector<std::string> result;
    std::string              normalized = arg;
    std::ranges::replace(normalized, ',', ' ');
    std::stringstream stream(normalized);
    std::string       token;

    while (stream >> token) {
        token = normalizedArgToken(token);
        if (!token.empty())
            result.push_back(token);
    }

    return result;
}

static bool isDispatcherAction(const std::string& token) {
    return token == "select" || token == "bring" || token == "bring-replace" ||
        token == "replace" || token == "off" || token == "close" || token == "disable" ||
        token == "toggle" || token == "open" || token == "show" || token == "on" ||
        token == "toggle-filter" || token == "filter-toggle" || token == "toggle-search" ||
        token == "search-toggle";
}

static bool applyOverviewOption(const std::string& token, SWindowOverviewOptions& options) {
    if (token == "exclude-current-workspace" || token == "without-current-workspace" ||
        token == "no-current-workspace" || token == "other-workspaces" ||
        token == "not-current-workspace" || token == "include-current-workspace=false" ||
        token == "current-workspace=false") {
        options.includeCurrentWorkspace = false;
        return true;
    }

    if (token == "all" || token == "default" || token == "include-current-workspace" ||
        token == "with-current-workspace" || token == "current-workspace" ||
        token == "include-current-workspace=true" || token == "current-workspace=true") {
        options.includeCurrentWorkspace = true;
        return true;
    }

    if (token == "filter" || token == "search" || token == "start-filter" ||
        token == "start-in-filter-mode" || token == "filter-mode" || token == "start-filter=true" ||
        token == "filter-mode=true") {
        options.startInFilterMode = true;
        return true;
    }

    if (token == "normal" || token == "navigation" || token == "nav" ||
        token == "start-filter=false" || token == "filter-mode=false") {
        options.startInFilterMode = false;
        return true;
    }

    return false;
}

static std::optional<SWinviewDispatcherArgs> parseWinviewDispatcherArgs(const std::string& arg,
                                                                        std::string&       error) {
    SWinviewDispatcherArgs args;
    bool                   sawAction = false;

    for (const auto& token : argTokens(arg.empty() ? "toggle" : arg)) {
        if (isDispatcherAction(token)) {
            if (sawAction) {
                error = "multiple overview actions provided";
                return std::nullopt;
            }

            args.action = token;
            sawAction   = true;
            continue;
        }

        if (applyOverviewOption(token, args.options))
            continue;

        error = "unknown overview argument: " + token;
        return std::nullopt;
    }

    return args;
}

static SDispatchResult onWinviewDispatcher(const std::string& arg) {
    std::string error;
    const auto  ARGS = parseWinviewDispatcherArgs(arg, error);
    if (!ARGS)
        return {.success = false, .error = error};

    const auto& ACTION = ARGS->action;

    if (ACTION == "select") {
        if (g_pWindowOverview) {
            g_pWindowOverview->selectHoveredWindow();
            g_pWindowOverview->close(true);
        }
        return {};
    }

    if (ACTION == "bring") {
        if (g_pWindowOverview) {
            g_pWindowOverview->selectHoveredWindow();
            g_pWindowOverview->close(true, true);
        }
        return {};
    }

    if (ACTION == "bring-replace" || ACTION == "replace") {
        if (g_pWindowOverview) {
            g_pWindowOverview->selectHoveredWindow();
            g_pWindowOverview->close(true, true, true);
        }
        return {};
    }

    if (ACTION == "off" || ACTION == "close" || ACTION == "disable") {
        if (g_pWindowOverview)
            g_pWindowOverview->close(false);
        return {};
    }

    if (ACTION == "toggle" && g_pWindowOverview) {
        g_pWindowOverview->close(false);
        return {};
    }

    if (ACTION == "toggle-filter" || ACTION == "filter-toggle" || ACTION == "toggle-search" ||
        ACTION == "search-toggle") {
        if (g_pWindowOverview)
            g_pWindowOverview->toggleFilterMode();
        else {
            const auto MONITOR = Desktop::focusState()->monitor();
            if (!MONITOR)
                return {.success = false, .error = "no focused monitor"};

            auto options              = ARGS->options;
            options.startInFilterMode = true;
            g_pWindowOverview         = std::make_unique<CWindowOverview>(MONITOR, options);
        }
        return {};
    }

    const auto MONITOR = Desktop::focusState()->monitor();
    if (!MONITOR)
        return {.success = false, .error = "no focused monitor"};

    g_pWindowOverview = std::make_unique<CWindowOverview>(MONITOR, ARGS->options);
    return {};
}

static int luaWinviewOverview(lua_State* L) {
    std::string arg = "toggle";

    if (lua_gettop(L) >= 1 && !lua_isnil(L, 1)) {
        if (lua_istable(L, 1)) {
            lua_getfield(L, 1, "action");
            if (lua_isstring(L, -1))
                arg = lua_tostring(L, -1);
            else if (!lua_isnil(L, -1))
                return luaL_error(L, "hyprwinview.overview: field \"action\" must be a string");
            lua_pop(L, 1);

            lua_getfield(L, 1, "exclude_current_workspace");
            if (lua_isboolean(L, -1) && lua_toboolean(L, -1))
                arg += " exclude-current-workspace";
            else if (!lua_isnil(L, -1) && !lua_isboolean(L, -1))
                return luaL_error(
                    L,
                    "hyprwinview.overview: field \"exclude_current_workspace\" must be a boolean");
            lua_pop(L, 1);

            lua_getfield(L, 1, "include_current_workspace");
            if (lua_isboolean(L, -1) && !lua_toboolean(L, -1))
                arg += " exclude-current-workspace";
            else if (!lua_isnil(L, -1) && !lua_isboolean(L, -1))
                return luaL_error(
                    L,
                    "hyprwinview.overview: field \"include_current_workspace\" must be a boolean");
            lua_pop(L, 1);

            lua_getfield(L, 1, "filter_mode");
            if (lua_isboolean(L, -1) && lua_toboolean(L, -1))
                arg += " filter";
            else if (!lua_isnil(L, -1) && !lua_isboolean(L, -1))
                return luaL_error(L,
                                  "hyprwinview.overview: field \"filter_mode\" must be a boolean");
            lua_pop(L, 1);

            lua_getfield(L, 1, "start_in_filter_mode");
            if (lua_isboolean(L, -1) && lua_toboolean(L, -1))
                arg += " filter";
            else if (!lua_isnil(L, -1) && !lua_isboolean(L, -1))
                return luaL_error(
                    L, "hyprwinview.overview: field \"start_in_filter_mode\" must be a boolean");
            lua_pop(L, 1);
        } else if (lua_isstring(L, 1))
            arg = lua_tostring(L, 1);
        else
            return luaL_error(L, "hyprwinview.overview: argument must be a string or table");
    }

    const auto RESULT = onWinviewDispatcher(arg);
    if (!RESULT.success)
        return luaL_error(L, "%s", RESULT.error.c_str());
    return 0;
}

static std::vector<std::string> luaStringListField(lua_State* L, int tableIdx, const char* field,
                                                   const std::vector<std::string>& fallback) {
    tableIdx = lua_absindex(L, tableIdx);
    lua_getfield(L, tableIdx, field);

    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return fallback;
    }

    if (!lua_istable(L, -1))
        luaL_error(L, "hyprwinview.configure: field \"%s\" must be an array of strings", field);

    std::vector<std::string> result;
    const auto               LEN = lua_rawlen(L, -1);
    result.reserve(LEN);

    for (size_t i = 1; i <= LEN; ++i) {
        lua_rawgeti(L, -1, static_cast<lua_Integer>(i));
        if (!lua_isstring(L, -1))
            luaL_error(L, "hyprwinview.configure: field \"%s\" item %zu must be a string", field,
                       i);
        result.emplace_back(lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
    return result;
}

static void readKeyTable(lua_State* L, int tableIdx, SWinviewKeyConfig& config) {
    config.left         = luaStringListField(L, tableIdx, "left", config.left);
    config.right        = luaStringListField(L, tableIdx, "right", config.right);
    config.up           = luaStringListField(L, tableIdx, "up", config.up);
    config.down         = luaStringListField(L, tableIdx, "down", config.down);
    config.go           = luaStringListField(L, tableIdx, "go", config.go);
    config.bring        = luaStringListField(L, tableIdx, "bring", config.bring);
    config.bringReplace = luaStringListField(L, tableIdx, "bring_replace", config.bringReplace);
    config.close        = luaStringListField(L, tableIdx, "close", config.close);
    config.filterToggle = luaStringListField(L, tableIdx, "filter_toggle", config.filterToggle);
    config.filterClose  = luaStringListField(L, tableIdx, "filter_close", config.filterClose);
    config.filterLeft   = luaStringListField(L, tableIdx, "filter_left", config.filterLeft);
    config.filterRight  = luaStringListField(L, tableIdx, "filter_right", config.filterRight);
    config.filterUp     = luaStringListField(L, tableIdx, "filter_up", config.filterUp);
    config.filterDown   = luaStringListField(L, tableIdx, "filter_down", config.filterDown);

    config.left  = luaStringListField(L, tableIdx, "keys_left", config.left);
    config.right = luaStringListField(L, tableIdx, "keys_right", config.right);
    config.up    = luaStringListField(L, tableIdx, "keys_up", config.up);
    config.down  = luaStringListField(L, tableIdx, "keys_down", config.down);
    config.go    = luaStringListField(L, tableIdx, "keys_go", config.go);
    config.bring = luaStringListField(L, tableIdx, "keys_bring", config.bring);
    config.bringReplace =
        luaStringListField(L, tableIdx, "keys_bring_replace", config.bringReplace);
    config.close = luaStringListField(L, tableIdx, "keys_close", config.close);
    config.filterToggle =
        luaStringListField(L, tableIdx, "keys_filter_toggle", config.filterToggle);
    config.filterClose = luaStringListField(L, tableIdx, "keys_filter_close", config.filterClose);
    config.filterLeft  = luaStringListField(L, tableIdx, "keys_filter_left", config.filterLeft);
    config.filterRight = luaStringListField(L, tableIdx, "keys_filter_right", config.filterRight);
    config.filterUp    = luaStringListField(L, tableIdx, "keys_filter_up", config.filterUp);
    config.filterDown  = luaStringListField(L, tableIdx, "keys_filter_down", config.filterDown);
}

static int luaWinviewConfigure(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);

    auto config = defaultWinviewKeyConfig();

    readKeyTable(L, 1, config);

    lua_getfield(L, 1, "keys");
    if (!lua_isnil(L, -1)) {
        if (!lua_istable(L, -1))
            return luaL_error(L, "hyprwinview.configure: field \"keys\" must be a table");
        readKeyTable(L, -1, config);
    }
    lua_pop(L, 1);

    setWinviewKeyConfig(std::move(config));
    return 0;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        failNotif("Version mismatch (headers ver is not equal to running hyprland ver)");
        throw std::runtime_error("[hyprwinview] Version mismatch");
    }

    addConfigValue(
        makeShared<Config::Values::CIntValue>("plugin:hyprwinview:gap_size", "gap size", 24));
    addConfigValue(
        makeShared<Config::Values::CIntValue>("plugin:hyprwinview:margin", "margin", 48));
    addConfigValue(makeShared<Config::Values::CColorValue>("plugin:hyprwinview:background",
                                                           "background color", 0x99101014));
    addConfigValue(makeShared<Config::Values::CIntValue>(
        "plugin:hyprwinview:background_blur", "blur the background behind the overview", 0));
    addConfigValue(makeShared<Config::Values::CColorValue>(
        "plugin:hyprwinview:bg_col", "legacy background color alias", 0x99101014));
    addConfigValue(makeShared<Config::Values::CColorValue>("plugin:hyprwinview:border_col",
                                                           "border color", 0x33FFFFFF));
    addConfigValue(makeShared<Config::Values::CColorValue>("plugin:hyprwinview:hover_border_col",
                                                           "hover border color", 0xEE66CCFF));
    addConfigValue(
        makeShared<Config::Values::CIntValue>("plugin:hyprwinview:border_size", "border size", 3));
    addConfigValue(makeShared<Config::Values::CStringValue>(
        "plugin:hyprwinview:window_order", "overview window ordering strategy", "natural"));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:keys_left",
                                                            "left keys", "a,h,left"));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:keys_right",
                                                            "right keys", "d,l,right"));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:keys_up", "up keys",
                                                            "w,k,up"));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:keys_down",
                                                            "down keys", "s,j,down"));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:keys_go", "go keys",
                                                            "return,enter,space,g,f"));
    addConfigValue(makeShared<Config::Values::CStringValue>(
        "plugin:hyprwinview:keys_bring", "bring keys", "b,shift+return,shift+space"));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:keys_bring_replace",
                                                            "bring replace keys", "shift+b"));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:keys_close",
                                                            "close keys", "escape,q"));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:keys_filter_toggle",
                                                            "filter mode toggle keys", "/"));
    addConfigValue(makeShared<Config::Values::CStringValue>(
        "plugin:hyprwinview:keys_filter_close", "filter mode close keys", "escape,ctrl+g"));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:keys_filter_left",
                                                            "filter mode left keys", "left"));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:keys_filter_right",
                                                            "filter mode right keys", "right"));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:keys_filter_up",
                                                            "filter mode up keys", "up,ctrl+p"));
    addConfigValue(makeShared<Config::Values::CStringValue>(
        "plugin:hyprwinview:keys_filter_down", "filter mode down keys", "down,ctrl+n"));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprwinview:show_app_icon",
                                                         "show app icon overlays", 0));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprwinview:app_icon_size",
                                                         "app icon size", 48));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:app_icon_theme",
                                                            "app icon theme override", ""));
    addConfigValue(makeShared<Config::Values::CStringValue>(
        "plugin:hyprwinview:app_icon_theme_source", "app icon theme source", "auto"));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:app_icon_overrides",
                                                            "app icon app_id=icon overrides", ""));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:app_icon_position",
                                                            "app icon position", "bottom right"));
    addConfigValue(makeShared<Config::Values::CFloatValue>(
        "plugin:hyprwinview:app_icon_anchor_x", "app icon normalized x anchor override", -1.0F));
    addConfigValue(makeShared<Config::Values::CFloatValue>(
        "plugin:hyprwinview:app_icon_anchor_y", "app icon normalized y anchor override", -1.0F));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprwinview:app_icon_margin_x",
                                                         "app icon horizontal margin", 10));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprwinview:app_icon_margin_y",
                                                         "app icon vertical margin", 10));
    addConfigValue(
        makeShared<Config::Values::CFloatValue>("plugin:hyprwinview:app_icon_margin_relative_x",
                                                "app icon relative horizontal margin", 0.0F));
    addConfigValue(
        makeShared<Config::Values::CFloatValue>("plugin:hyprwinview:app_icon_margin_relative_y",
                                                "app icon relative vertical margin", 0.0F));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprwinview:app_icon_offset_x",
                                                         "app icon horizontal offset", 0));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprwinview:app_icon_offset_y",
                                                         "app icon vertical offset", 0));
    addConfigValue(makeShared<Config::Values::CColorValue>(
        "plugin:hyprwinview:app_icon_backplate_col", "app icon backplate color", 0x66000000));
    addConfigValue(makeShared<Config::Values::CIntValue>(
        "plugin:hyprwinview:app_icon_backplate_padding", "app icon backplate padding", 6));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprwinview:show_window_text",
                                                         "show window title and class labels", 1));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:window_text_font",
                                                            "window text font", "Sans"));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprwinview:window_text_size",
                                                         "window text size", 14));
    addConfigValue(makeShared<Config::Values::CColorValue>("plugin:hyprwinview:window_text_color",
                                                           "window text color", 0xFFFFFFFF));
    addConfigValue(makeShared<Config::Values::CColorValue>(
        "plugin:hyprwinview:window_text_backplate_col", "window text backplate color", 0x99000000));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprwinview:window_text_padding",
                                                         "window text padding", 6));
    addConfigValue(makeShared<Config::Values::CIntValue>(
        "plugin:hyprwinview:filter_animation_ms", "filter narrowing animation duration", 140));
    addConfigValue(makeShared<Config::Values::CStringValue>(
        "plugin:hyprwinview:animation", "overview animation mode", "workspace_zoom"));
    addConfigValue(makeShared<Config::Values::CIntValue>(
        "plugin:hyprwinview:animation_in_ms", "overview open animation duration in milliseconds",
        280));
    addConfigValue(makeShared<Config::Values::CIntValue>(
        "plugin:hyprwinview:animation_out_ms", "overview close animation duration in milliseconds",
        220));
    addConfigValue(makeShared<Config::Values::CFloatValue>(
        "plugin:hyprwinview:animation_speed", "overview animation speed multiplier", 1.0F));
    addConfigValue(makeShared<Config::Values::CFloatValue>(
        "plugin:hyprwinview:animation_scale", "overview fade_scale starting scale", 0.94F));
    addConfigValue(makeShared<Config::Values::CIntValue>(
        "plugin:hyprwinview:animation_stagger_ms",
        "overview staggered animation delay between tiles in milliseconds", 16));
    addConfigValue(makeShared<Config::Values::CIntValue>(
        "plugin:hyprwinview:animation_stagger_max_ms",
        "overview staggered animation maximum tile delay in milliseconds", 120));
    addConfigValue(makeShared<Config::Values::CFloatValue>(
        "plugin:hyprwinview:animation_workspace_zoom_stage_ratio",
        "overview workspace_zoom first-stage fraction", 0.45F));
    addConfigValue(
        makeShared<Config::Values::CIntValue>("plugin:hyprwinview:animation_workspace_zoom_gap",
                                              "overview workspace_zoom panel gap", 18));

    static auto renderStage = Event::bus()->m_events.render.stage.listen([](eRenderStage stage) {
        if (stage != RENDER_LAST_MOMENT || !g_pWindowOverview)
            return;

        const auto MONITOR = g_pHyprRenderer->m_renderData.pMonitor.lock();
        if (MONITOR && g_pWindowOverview->pMonitor == MONITOR)
            g_pWindowOverview->render();
    });

    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprwinview:overview", ::onWinviewDispatcher);
    HyprlandAPI::addLuaFunction(PHANDLE, "hyprwinview", "overview", ::luaWinviewOverview);
    HyprlandAPI::addLuaFunction(PHANDLE, "hyprwinview", "configure", ::luaWinviewConfigure);
    HyprlandAPI::reloadConfig();

    HyprlandAPI::addNotification(PHANDLE, "[hyprwinview] Initialized successfully",
                                 CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);
    return {"hyprwinview", "A window overview plugin for Hyprland", "Ivan Malison", "0.1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_pWindowOverview.reset();
    clearAppIconCache();
    g_pHyprRenderer->m_renderPass.removeAllOfType("CWinviewPassElement");
}
