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
#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "app_icon_loader.hpp"
#include "settings.hpp"

namespace {
    struct STextureCacheEntry {
        std::weak_ptr<const AppIconLoader::SLoadedIcon> icon;
        SP<Render::ITexture>                            texture;
    };

    std::unordered_map<const AppIconLoader::SLoadedIcon*, STextureCacheEntry> g_textureCache;
    std::unique_ptr<AppIconLoader::CLoader>                                   g_loader;
    UP<SEventLoopDoLaterLock>                                                 g_initialWarm;
    std::vector<CHyprSignalListener>                                          g_warmListeners;

    AppIconLookup::SConfig                                                    iconConfig() {
        using namespace WinviewConfig;
        return {*PAPPICONTHEME(), *PAPPICONTHEMESOURCE(), *PAPPICONOVERRIDES()};
    }

    bool iconsEnabled() {
        return *WinviewConfig::PSHOWAPPICON() != 0;
    }

    AppIconLoader::CLoader& loader() {
        if (!g_loader)
            g_loader = std::make_unique<AppIconLoader::CLoader>();
        return *g_loader;
    }

    SP<Render::ITexture> textureForIcon(const AppIconLoader::Icon& icon) {
        if (!icon)
            return nullptr;
        if (const auto IT = g_textureCache.find(icon.get());
            IT != g_textureCache.end() && IT->second.icon.lock() == icon)
            return IT->second.texture;

        std::erase_if(g_textureCache,
                      [](const auto& entry) { return entry.second.icon.expired(); });
        auto texture = g_pHyprRenderer->createTexture(icon->surface.get());
        g_textureCache.insert_or_assign(icon.get(), STextureCacheEntry{icon, texture});
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
        const auto CONFIG = iconConfig();
        for (const auto& monitor : State::monitorState()->monitors())
            loader().request({IDS, appIconPixelSize(monitor->m_scale), CONFIG});
    }

    void warmWindows() {
        if (!iconsEnabled())
            return;
        for (const auto& window : Desktop::windowState()->windows())
            warmWindow(window);
    }
}

int appIconPixelSize(double monitorScale) {
    const auto SIZE = std::max<Config::INTEGER>(1, *WinviewConfig::PAPPICONSIZE());
    return std::max(1, (int)std::round(static_cast<double>(SIZE) * monitorScale));
}

void initializeAppIconCache() {
    g_warmListeners.push_back(Event::bus()->m_events.window.open.listen(warmWindow));
    g_warmListeners.push_back(Event::bus()->m_events.window.class_.listen(warmWindow));
    g_warmListeners.push_back(Event::bus()->m_events.monitor.layoutChanged.listen(warmWindows));
    g_initialWarm = g_pEventLoopManager->doLaterLock(warmWindows);
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

void invalidateAppIconCache() {
    if (g_loader)
        g_loader->invalidate();
    warmWindows();
}

void clearAppIconCache() {
    g_warmListeners.clear();
    g_initialWarm.reset();
    g_loader.reset();
    g_textureCache.clear();
}
