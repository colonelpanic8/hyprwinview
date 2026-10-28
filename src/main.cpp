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
#include "settings.hpp"
#include "overview/keys.hpp"
#include "workspace/module.hpp"

static CHyprSignalListener renderStage;
static CHyprSignalListener monitorRemoved;
static CHyprSignalListener configPreReload;
static CHyprSignalListener configReloaded;
static CHyprSignalListener keyboardKey;
static CHyprSignalListener mouseButton;

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
        LOG(Log::ERR, "[hyprwinview] failed to register plugin value \"{}\": {}", value->name(),
            RET.error());
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

#define REGISTER_SETTING(factory, valueType, name, key, description, initial)                      \
    addConfigValue(                                                                                \
        makeShared<Config::Values::factory>("plugin:hyprwinview:" key, description, initial));
    WINVIEW_SETTINGS(REGISTER_SETTING)
#undef REGISTER_SETTING

    renderStage =
        Event::bus()->m_events.render.stage.listen([](const Event::SRenderStageEvent& event) {
            if (event.stage != RENDER_LAST_MOMENT || !event.context)
                return;

            uploadReadyAppIcons();
            if (!g_pWindowOverview)
                return;

            if (event.monitor && g_pWindowOverview->pMonitor == event.monitor)
                g_pWindowOverview->render(event.context->get());
        });

    monitorRemoved = Event::bus()->m_events.monitor.removed.listen([](const PHLMONITOR& monitor) {
        if (g_pWindowOverview && g_pWindowOverview->pMonitor == monitor)
            closeWindowOverviewImmediately();
    });

    configPreReload = Event::bus()->m_events.config.preReload.listen(
        [] { Winview::keyOverrides().beginReload(); });
    // Every reload path, including a syntax error that keeps the old config, ends here.
    configReloaded = Event::bus()->m_events.config.reloaded.listen([] {
        Winview::keyOverrides().endReload(!Config::mgr()->getErrors().empty());
        invalidateAppIconCache();
    });

    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprwinview:overview", ::onWinviewDispatcher);
    HyprlandAPI::addLuaFunction(PHANDLE, "hyprwinview", "overview", ::luaWinviewOverview);
    HyprlandAPI::addLuaFunction(PHANDLE, "hyprwinview", "configure", ::luaWinviewConfigure);

    initializeWorkspaceOverview(handle);
    // After the workspace overview's listeners, which overwrite `cancelled`.
    keyboardKey = Event::bus()->m_events.input.keyboard.key.listen(
        [](const IKeyboard::SKeyEvent& event, Event::SCallbackInfo& info) {
            handleWindowOverviewKey(event, info);
        });
    mouseButton = Event::bus()->m_events.input.mouse.button.listen(
        [](const IPointer::SButtonEvent& event, Event::SCallbackInfo& info) {
            handleWindowOverviewButton(event, info);
        });
    HyprlandAPI::reloadConfig();
    initializeAppIconCache();

    HyprlandAPI::addNotification(PHANDLE,
                                 "[hyprwinview] Window and workspace overviews initialized",
                                 CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);
    return {"hyprwinview", "Window and workspace overviews for Hyprland",
            "Ivan Malison and Raymond Bian", HYPRWINVIEW_VERSION};
}

APICALL EXPORT void PLUGIN_EXIT() {
    renderStage.reset();
    monitorRemoved.reset();
    configPreReload.reset();
    configReloaded.reset();
    keyboardKey.reset();
    mouseButton.reset();
    resetWindowOverviewInput();
    g_pWindowOverview.reset();
    shutdownWorkspaceOverview();
    clearAppIconCache();
    auto& pass = g_pHyprRenderer->context().m_pass;
    pass.removeAllOfType("CWinviewPassElement");
    pass.removeAllOfType("COverviewRenderContextPassElement");
    pass.removeAllOfType("HTDisableSimplification");
}
