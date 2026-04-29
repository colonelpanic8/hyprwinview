#define WLR_USE_UNSTABLE

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>

#include "globals.hpp"
#include "overview.hpp"

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

static void failNotif(const std::string& reason) {
    HyprlandAPI::addNotification(PHANDLE, "[hyprwinview] Failure in initialization: " + reason, CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
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

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        failNotif("Version mismatch (headers ver is not equal to running hyprland ver)");
        throw std::runtime_error("[hyprwinview] Version mismatch");
    }

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprwinview:gap_size", Hyprlang::INT{24});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprwinview:margin", Hyprlang::INT{48});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprwinview:bg_col", Hyprlang::INT{*configStringToInt("rgba(101014ee)")});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprwinview:border_col", Hyprlang::INT{*configStringToInt("rgba(ffffff33)")});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprwinview:hover_border_col", Hyprlang::INT{*configStringToInt("rgba(66ccffee)")});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprwinview:border_size", Hyprlang::INT{3});

    static auto renderStage = Event::bus()->m_events.render.stage.listen([](eRenderStage stage) {
        if (stage != RENDER_LAST_MOMENT || !g_pWindowOverview)
            return;

        const auto MONITOR = g_pHyprRenderer->m_renderData.pMonitor.lock();
        if (MONITOR && g_pWindowOverview->pMonitor == MONITOR)
            g_pWindowOverview->render();
    });

    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprwinview:overview", ::onWinviewDispatcher);
    HyprlandAPI::reloadConfig();

    HyprlandAPI::addNotification(PHANDLE, "[hyprwinview] Initialized successfully", CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);
    return {"hyprwinview", "A window overview plugin for Hyprland", "Ivan Malison", "0.1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_pWindowOverview.reset();
    g_pHyprRenderer->m_renderPass.removeAllOfType("CWinviewPassElement");
}
