#define WLR_USE_UNSTABLE

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/values/types/BoolValue.hpp>
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

#include "AppIcon.hpp"
#include "globals.hpp"
#include "overview.hpp"

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

static void failNotif(const std::string& reason) {
    HyprlandAPI::addNotification(PHANDLE, "[hyprwinview] Failure in initialization: " + reason, CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
}

static bool addConfigValue(SP<Config::Values::IValue> value) {
    const auto RET = Config::mgr()->registerPluginValue(PHANDLE, value);
    if (!RET) {
        Log::logger->log(Log::ERR, "[hyprwinview] failed to register plugin value \"{}\": {}", value->name(), RET.error());
        return false;
    }

    return true;
}

static SDispatchResult onWinviewDispatcher(std::string arg) {
    if (arg.empty())
        arg = "toggle";

    if (arg == "select") {
        if (g_pWindowOverview) {
            g_pWindowOverview->selectHoveredWindow();
            g_pWindowOverview->close(true);
        }
        return {};
    }

    if (arg == "bring") {
        if (g_pWindowOverview) {
            g_pWindowOverview->selectHoveredWindow();
            g_pWindowOverview->close(true, true);
        }
        return {};
    }

    if (arg == "off" || arg == "close" || arg == "disable") {
        if (g_pWindowOverview)
            g_pWindowOverview->close(false);
        return {};
    }

    if (arg == "toggle" && g_pWindowOverview) {
        g_pWindowOverview->close(false);
        return {};
    }

    const auto MONITOR = Desktop::focusState()->monitor();
    if (!MONITOR)
        return {.success = false, .error = "no focused monitor"};

    g_pWindowOverview = std::make_unique<CWindowOverview>(MONITOR);
    return {};
}

static int luaWinviewOverview(lua_State* L) {
    const auto RESULT = onWinviewDispatcher(luaL_optstring(L, 1, "toggle"));
    if (!RESULT.success)
        return luaL_error(L, "%s", RESULT.error.c_str());
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

    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprwinview:gap_size", "gap size", 24));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprwinview:margin", "margin", 48));
    addConfigValue(makeShared<Config::Values::CColorValue>("plugin:hyprwinview:bg_col", "background color", 0xEE101014));
    addConfigValue(makeShared<Config::Values::CColorValue>("plugin:hyprwinview:border_col", "border color", 0x33FFFFFF));
    addConfigValue(makeShared<Config::Values::CColorValue>("plugin:hyprwinview:hover_border_col", "hover border color", 0xEE66CCFF));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprwinview:border_size", "border size", 3));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:keys_left", "left keys", "a,h,left"));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:keys_right", "right keys", "d,l,right"));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:keys_up", "up keys", "w,k,up"));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:keys_down", "down keys", "s,j,down"));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:keys_go", "go keys", "return,enter,space,g"));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:keys_bring", "bring keys", "b,shift+return,shift+space"));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:keys_close", "close keys", "escape,q"));
    addConfigValue(makeShared<Config::Values::CBoolValue>("plugin:hyprwinview:show_app_icon", "show app icon overlays", false));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprwinview:app_icon_size", "app icon size", 48));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprwinview:app_icon_position", "app icon position", "bottom right"));
    addConfigValue(makeShared<Config::Values::CFloatValue>("plugin:hyprwinview:app_icon_anchor_x", "app icon normalized x anchor override", -1.0F));
    addConfigValue(makeShared<Config::Values::CFloatValue>("plugin:hyprwinview:app_icon_anchor_y", "app icon normalized y anchor override", -1.0F));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprwinview:app_icon_margin_x", "app icon horizontal margin", 10));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprwinview:app_icon_margin_y", "app icon vertical margin", 10));
    addConfigValue(makeShared<Config::Values::CFloatValue>("plugin:hyprwinview:app_icon_margin_relative_x", "app icon relative horizontal margin", 0.0F));
    addConfigValue(makeShared<Config::Values::CFloatValue>("plugin:hyprwinview:app_icon_margin_relative_y", "app icon relative vertical margin", 0.0F));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprwinview:app_icon_offset_x", "app icon horizontal offset", 0));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprwinview:app_icon_offset_y", "app icon vertical offset", 0));
    addConfigValue(makeShared<Config::Values::CColorValue>("plugin:hyprwinview:app_icon_backplate_col", "app icon backplate color", 0x66000000));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprwinview:app_icon_backplate_padding", "app icon backplate padding", 6));

    static auto renderStage = Event::bus()->m_events.render.stage.listen([](eRenderStage stage) {
        if (stage != RENDER_LAST_MOMENT || !g_pWindowOverview)
            return;

        const auto MONITOR = g_pHyprRenderer->m_renderData.pMonitor.lock();
        if (MONITOR && g_pWindowOverview->pMonitor == MONITOR)
            g_pWindowOverview->render();
    });

    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprwinview:overview", ::onWinviewDispatcher);
    HyprlandAPI::addLuaFunction(PHANDLE, "hyprwinview", "overview", ::luaWinviewOverview);
    HyprlandAPI::reloadConfig();

    HyprlandAPI::addNotification(PHANDLE, "[hyprwinview] Initialized successfully", CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);
    return {"hyprwinview", "A window overview plugin for Hyprland", "Ivan Malison", "0.1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_pWindowOverview.reset();
    clearAppIconCache();
    g_pHyprRenderer->m_renderPass.removeAllOfType("CWinviewPassElement");
}
