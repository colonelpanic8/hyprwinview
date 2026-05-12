#define WLR_USE_UNSTABLE

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/values/types/ColorValue.hpp>
#include <hyprland/src/config/values/types/FloatValue.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>
#include <hyprland/src/config/values/types/StringValue.hpp>
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>

#include <stdexcept>
#include <string>

#include "app_icon.hpp"
#include "dispatcher.hpp"
#include "globals.hpp"
#include "lua_api.hpp"
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
        "plugin:hyprwinview:keys_default_action", "default action keys", ""));
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
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:keys_filter_bring",
                                                            "filter mode bring keys", "ctrl+b"));
    addConfigValue(
        makeShared<Config::Values::CStringValue>("plugin:hyprwinview:keys_filter_bring_replace",
                                                 "filter mode bring replace keys", "ctrl+shift+b"));
    addConfigValue(makeShared<Config::Values::CStringValue>(
        "plugin:hyprwinview:keys_filter_left", "filter mode left keys", "left,ctrl+a,super+a"));
    addConfigValue(makeShared<Config::Values::CStringValue>(
        "plugin:hyprwinview:keys_filter_right", "filter mode right keys", "right,ctrl+d,super+d"));
    addConfigValue(makeShared<Config::Values::CStringValue>(
        "plugin:hyprwinview:keys_filter_up", "filter mode up keys", "up,ctrl+p,ctrl+w,super+w"));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:keys_filter_down",
                                                            "filter mode down keys",
                                                            "down,ctrl+n,ctrl+s,super+s"));
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
