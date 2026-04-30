#include "AppIcon.hpp"

#define private public
#define protected public
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/render/Renderer.hpp>
#undef private
#undef protected

#include <cairo/cairo.h>
#include <librsvg/rsvg.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {
    namespace fs = std::filesystem;

    struct SDesktopEntry {
        std::string id;
        std::string name;
        std::string startupWMClass;
        std::string icon;
    };

    struct STextureCacheEntry {
        SP<Render::ITexture> texture;
        std::string          path;
        int                  sizePx = 0;
    };

    std::unordered_map<std::string, std::optional<SDesktopEntry>> g_desktopEntryCache;
    std::unordered_map<std::string, std::optional<std::string>>   g_iconPathCache;
    std::unordered_map<std::string, STextureCacheEntry>           g_textureCache;
    std::optional<std::vector<SDesktopEntry>>                     g_desktopEntries;

    std::string trim(std::string value) {
        const auto BEGIN = std::ranges::find_if_not(value, [](unsigned char c) { return std::isspace(c); });
        const auto END   = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) { return std::isspace(c); }).base();

        if (BEGIN >= END)
            return "";

        return std::string(BEGIN, END);
    }

    std::string lowercase(std::string value) {
        std::ranges::transform(value, value.begin(), [](unsigned char c) { return std::tolower(c); });
        return value;
    }

    std::vector<std::string> splitEnvPaths(const char* value, const std::vector<std::string>& defaults) {
        if (!value || std::string_view(value).empty())
            return defaults;

        std::vector<std::string> paths;
        std::stringstream        stream(value);
        std::string              path;
        while (std::getline(stream, path, ':')) {
            path = trim(path);
            if (!path.empty())
                paths.push_back(path);
        }

        return paths.empty() ? defaults : paths;
    }

    std::vector<fs::path> dataDirs() {
        std::vector<fs::path> dirs;

        const auto HOME = std::getenv("HOME");
        const auto XDG_DATA_HOME = std::getenv("XDG_DATA_HOME");
        if (XDG_DATA_HOME && *XDG_DATA_HOME)
            dirs.emplace_back(XDG_DATA_HOME);
        else if (HOME && *HOME)
            dirs.emplace_back(fs::path(HOME) / ".local/share");

        for (const auto& path : splitEnvPaths(std::getenv("XDG_DATA_DIRS"), {"/usr/local/share", "/usr/share"}))
            dirs.emplace_back(path);

        return dirs;
    }

    std::vector<fs::path> iconRoots() {
        auto              dirs = dataDirs();
        std::vector<fs::path> roots;

        const auto HOME = std::getenv("HOME");
        if (HOME && *HOME)
            roots.emplace_back(fs::path(HOME) / ".icons");

        for (const auto& dir : dirs) {
            roots.emplace_back(dir / "icons");
            roots.emplace_back(dir / "pixmaps");
        }

        return roots;
    }

    bool hasImageExtension(const fs::path& path) {
        const auto EXT = lowercase(path.extension().string());
        return EXT == ".png" || EXT == ".svg";
    }

    std::optional<SDesktopEntry> parseDesktopFile(const fs::path& path) {
        std::ifstream file(path);
        if (!file)
            return std::nullopt;

        SDesktopEntry entry;
        entry.id = path.stem().string();

        bool        inDesktopEntry = false;
        std::string line;
        while (std::getline(file, line)) {
            line = trim(line);
            if (line.empty() || line.starts_with('#'))
                continue;

            if (line.starts_with('[') && line.ends_with(']')) {
                inDesktopEntry = line == "[Desktop Entry]";
                continue;
            }

            if (!inDesktopEntry)
                continue;

            const auto EQUAL = line.find('=');
            if (EQUAL == std::string::npos)
                continue;

            const auto KEY = line.substr(0, EQUAL);
            const auto VAL = trim(line.substr(EQUAL + 1));

            if (KEY == "Name")
                entry.name = VAL;
            else if (KEY == "StartupWMClass")
                entry.startupWMClass = VAL;
            else if (KEY == "Icon")
                entry.icon = VAL;
        }

        if (entry.icon.empty())
            return std::nullopt;

        return entry;
    }

    const std::vector<SDesktopEntry>& desktopEntries() {
        if (g_desktopEntries)
            return *g_desktopEntries;

        std::vector<SDesktopEntry> entries;

        for (const auto& dir : dataDirs()) {
            const auto APPDIR = dir / "applications";
            std::error_code ec;
            if (!fs::exists(APPDIR, ec))
                continue;

            for (fs::recursive_directory_iterator it(APPDIR, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
                if (ec || !it->is_regular_file(ec) || it->path().extension() != ".desktop")
                    continue;

                if (auto entry = parseDesktopFile(it->path()))
                    entries.push_back(*entry);
            }
        }

        g_desktopEntries = std::move(entries);
        return *g_desktopEntries;
    }

    std::optional<SDesktopEntry> findDesktopEntry(const std::string& appId) {
        const auto KEY = lowercase(appId);
        if (const auto IT = g_desktopEntryCache.find(KEY); IT != g_desktopEntryCache.end())
            return IT->second;

        for (const auto& dir : dataDirs()) {
            for (const auto& candidate : {appId, lowercase(appId)}) {
                const auto PATH = dir / "applications" / (candidate + ".desktop");
                std::error_code ec;
                if (!fs::is_regular_file(PATH, ec))
                    continue;

                auto entry = parseDesktopFile(PATH);
                g_desktopEntryCache.emplace(KEY, entry);
                return entry;
            }
        }

        for (const auto& entry : desktopEntries()) {
            if (lowercase(entry.id) == KEY || lowercase(entry.startupWMClass) == KEY) {
                g_desktopEntryCache.emplace(KEY, entry);
                return entry;
            }
        }

        g_desktopEntryCache.emplace(KEY, std::nullopt);
        return std::nullopt;
    }

    std::optional<std::string> findIconPath(const std::string& iconName) {
        if (iconName.empty())
            return std::nullopt;

        if (const auto IT = g_iconPathCache.find(iconName); IT != g_iconPathCache.end())
            return IT->second;

        const fs::path ICON(iconName);
        std::error_code ec;
        if (ICON.is_absolute() && fs::is_regular_file(ICON, ec)) {
            g_iconPathCache.emplace(iconName, ICON.string());
            return ICON.string();
        }

        std::optional<fs::path> fallbackPng;
        std::optional<fs::path> fallbackSvg;

        for (const auto& root : iconRoots()) {
            if (!fs::exists(root, ec))
                continue;

            for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
                if (ec || !it->is_regular_file(ec) || it->path().stem() != iconName || !hasImageExtension(it->path()))
                    continue;

                const auto EXT = lowercase(it->path().extension().string());
                if (EXT == ".svg")
                    fallbackSvg = it->path();
                else if (!fallbackPng)
                    fallbackPng = it->path();
            }
        }

        const auto RESULT = fallbackSvg ? fallbackSvg->string() : fallbackPng ? fallbackPng->string() : std::optional<std::string>{};
        g_iconPathCache.emplace(iconName, RESULT);
        return RESULT;
    }

    cairo_surface_t* renderPng(const std::string& path, int sizePx) {
        cairo_surface_t* source = cairo_image_surface_create_from_png(path.c_str());
        if (cairo_surface_status(source) != CAIRO_STATUS_SUCCESS) {
            cairo_surface_destroy(source);
            return nullptr;
        }

        const int SOURCE_W = cairo_image_surface_get_width(source);
        const int SOURCE_H = cairo_image_surface_get_height(source);
        cairo_surface_t* target = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, sizePx, sizePx);
        cairo_t*         cr     = cairo_create(target);

        cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
        cairo_paint(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

        const double SCALE = std::min(sizePx / std::max(1.0, (double)SOURCE_W), sizePx / std::max(1.0, (double)SOURCE_H));
        cairo_translate(cr, (sizePx - SOURCE_W * SCALE) / 2.0, (sizePx - SOURCE_H * SCALE) / 2.0);
        cairo_scale(cr, SCALE, SCALE);
        cairo_set_source_surface(cr, source, 0, 0);
        cairo_paint(cr);

        cairo_destroy(cr);
        cairo_surface_destroy(source);
        return target;
    }

    cairo_surface_t* renderSvg(const std::string& path, int sizePx) {
        GError*     error  = nullptr;
        RsvgHandle* handle = rsvg_handle_new_from_file(path.c_str(), &error);
        if (!handle) {
            if (error)
                g_error_free(error);
            return nullptr;
        }

        cairo_surface_t* target = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, sizePx, sizePx);
        cairo_t*         cr     = cairo_create(target);

        cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
        cairo_paint(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

        RsvgRectangle viewport = {0, 0, (double)sizePx, (double)sizePx};
        if (!rsvg_handle_render_document(handle, cr, &viewport, &error)) {
            cairo_destroy(cr);
            cairo_surface_destroy(target);
            g_object_unref(handle);
            if (error)
                g_error_free(error);
            return nullptr;
        }

        cairo_destroy(cr);
        g_object_unref(handle);
        return target;
    }

    SP<Render::ITexture> textureFromPath(const std::string& path, int sizePx) {
        const auto CACHE_KEY = path + ":" + std::to_string(sizePx);
        if (const auto IT = g_textureCache.find(CACHE_KEY); IT != g_textureCache.end())
            return IT->second.texture;

        const auto EXT = lowercase(fs::path(path).extension().string());
        cairo_surface_t* surface = EXT == ".svg" ? renderSvg(path, sizePx) : renderPng(path, sizePx);
        if (!surface)
            return nullptr;

        auto texture = g_pHyprRenderer->createTexture(surface);
        cairo_surface_destroy(surface);

        if (texture)
            g_textureCache.emplace(CACHE_KEY, STextureCacheEntry{.texture = texture, .path = path, .sizePx = sizePx});

        return texture;
    }

    std::vector<std::string> appIdsForWindow(PHLWINDOW window) {
        std::vector<std::string> ids;
        if (!window)
            return ids;

        for (const auto& candidate : {window->m_class, window->m_initialClass}) {
            if (!candidate.empty() && std::ranges::find(ids, candidate) == ids.end())
                ids.push_back(candidate);
        }

        return ids;
    }
}

SP<Render::ITexture> appIconTextureForWindow(PHLWINDOW window, int sizePx) {
    if (!window || sizePx <= 0)
        return nullptr;

    for (const auto& appId : appIdsForWindow(window)) {
        if (auto entry = findDesktopEntry(appId)) {
            if (auto path = findIconPath(entry->icon))
                return textureFromPath(*path, sizePx);
        }

        if (auto path = findIconPath(appId))
            return textureFromPath(*path, sizePx);
    }

    return nullptr;
}

void clearAppIconCache() {
    g_textureCache.clear();
    g_iconPathCache.clear();
    g_desktopEntryCache.clear();
    g_desktopEntries.reset();
}
