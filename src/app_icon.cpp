#include "app_icon.hpp"

#define private   public
#define protected public
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/render/Renderer.hpp>
#undef private
#undef protected

#include <cairo/cairo.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "app_icon_loader.hpp"

namespace {
    struct STextureCacheEntry {
        SP<Render::ITexture> texture;
        std::string          path;
        int                  sizePx = 0;
    };

    std::unordered_map<std::string, STextureCacheEntry> g_textureCache;
    std::unique_ptr<AppIconLoader::CLoader>             g_loader;
    SP<CEventLoopTimer>                                 g_warmTimer;
    std::vector<CHyprSignalListener>                    g_warmListeners;

    AppIconLookup::SConfig                              iconConfig() {
        static const CConfigValue<Config::STRING> THEME("plugin:hyprwinview:app_icon_theme");
        static const CConfigValue<Config::STRING> SOURCE(
            "plugin:hyprwinview:app_icon_theme_source");
        static const CConfigValue<Config::STRING> OVERRIDES(
            "plugin:hyprwinview:app_icon_overrides");
        return {*THEME, *SOURCE, *OVERRIDES};
    }

    bool iconsEnabled() {
        static const CConfigValue<Config::INTEGER> SHOW("plugin:hyprwinview:show_app_icon");
        return *SHOW != 0;
    }

    AppIconLoader::CLoader& loader() {
        if (!g_loader)
            g_loader = std::make_unique<AppIconLoader::CLoader>();
        return *g_loader;
    }

    SP<Render::ITexture> textureForIcon(const AppIconLoader::Icon& icon) {
        if (!icon)
            return nullptr;
        const auto KEY = icon->path + ":" + std::to_string(icon->sizePx);
        if (const auto IT = g_textureCache.find(KEY); IT != g_textureCache.end())
            return IT->second.texture;

        auto texture = g_pHyprRenderer->createTexture(icon->surface.get());
        if (texture)
            g_textureCache.emplace(KEY, STextureCacheEntry{texture, icon->path, icon->sizePx});
        return texture;
    }

    std::vector<std::string> appIdsForWindow(const PHLWINDOW& window) {
        std::vector<std::string> ids;
        if (!window)
            return ids;

        for (const auto& candidate : {window->m_class, window->m_initialClass}) {
            if (!candidate.empty() && std::ranges::find(ids, candidate) == ids.end())
                ids.push_back(candidate);
        }

        return ids;
    }

    void warmWindow(const PHLWINDOW& window) {
        if (!window || !window->m_isMapped || !iconsEnabled())
            return;
        const auto IDS = appIdsForWindow(window);
        if (IDS.empty())
            return;
        static const CConfigValue<Config::INTEGER> SIZE("plugin:hyprwinview:app_icon_size");
        const auto                                 CONFIG = iconConfig();
        for (const auto& monitor : State::monitorState()->monitors()) {
            const int PIXELS =
                std::max(1,
                         (int)std::round(static_cast<double>(std::max<Config::INTEGER>(1, *SIZE)) *
                                         monitor->m_scale));
            loader().request({IDS, PIXELS, CONFIG});
        }
    }

    void warmWindows() {
        if (!iconsEnabled())
            return;
        for (const auto& window : Desktop::windowState()->windows())
            warmWindow(window);
    }
}

void initializeAppIconCache() {
    g_warmListeners.push_back(Event::bus()->m_events.window.open.listen(warmWindow));
    g_warmListeners.push_back(Event::bus()->m_events.window.class_.listen(warmWindow));
    g_warmListeners.push_back(Event::bus()->m_events.config.reloaded.listen(warmWindows));
    g_warmListeners.push_back(Event::bus()->m_events.monitor.layoutChanged.listen(warmWindows));
    g_warmTimer = makeShared<CEventLoopTimer>(
        std::chrono::milliseconds(1),
        [](const SP<CEventLoopTimer>& self, void*) {
            warmWindows();
            self->updateTimeout(std::chrono::seconds(1));
        },
        nullptr);
    g_pEventLoopManager->addTimer(g_warmTimer);
}

void uploadReadyAppIcons() {
    if (g_loader && iconsEnabled())
        textureForIcon(g_loader->takeReady());
}

SP<Render::ITexture> appIconTextureForWindow(const PHLWINDOW& window, int sizePx) {
    if (!window || sizePx <= 0)
        return nullptr;
    const auto RESULT = loader().request({appIdsForWindow(window), sizePx, iconConfig()});
    return RESULT ? textureForIcon(*RESULT) : nullptr;
}

void clearAppIconCache() {
    g_warmListeners.clear();
    if (g_warmTimer && g_pEventLoopManager)
        g_pEventLoopManager->removeTimer(g_warmTimer);
    g_warmTimer.reset();
    g_loader.reset();
    g_textureCache.clear();
}
