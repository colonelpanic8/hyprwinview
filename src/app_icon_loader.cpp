#include "app_icon_loader.hpp"

#include <algorithm>
#include <filesystem>
#include <librsvg/rsvg.h>
#include <utility>

namespace AppIconLoader {
    namespace {
        cairo_surface_t* renderPng(const std::string& path, int sizePx) {
            cairo_surface_t* source = cairo_image_surface_create_from_png(path.c_str());
            if (cairo_surface_status(source) != CAIRO_STATUS_SUCCESS) {
                cairo_surface_destroy(source);
                return nullptr;
            }

            const int        SOURCE_W = cairo_image_surface_get_width(source);
            const int        SOURCE_H = cairo_image_surface_get_height(source);
            cairo_surface_t* target =
                cairo_image_surface_create(CAIRO_FORMAT_ARGB32, sizePx, sizePx);
            cairo_t* cr = cairo_create(target);

            cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
            cairo_paint(cr);
            cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

            const double SCALE = std::min(sizePx / std::max(1.0, (double)SOURCE_W),
                                          sizePx / std::max(1.0, (double)SOURCE_H));
            cairo_translate(cr, (sizePx - SOURCE_W * SCALE) / 2.0,
                            (sizePx - SOURCE_H * SCALE) / 2.0);
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

            cairo_surface_t* target =
                cairo_image_surface_create(CAIRO_FORMAT_ARGB32, sizePx, sizePx);
            cairo_t* cr = cairo_create(target);

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

        Icon loadIcon(const SRequest& request, const std::stop_token&) {
            const auto PATH =
                AppIconLookup::findAppIconPath(request.appIds, request.sizePx, request.config);
            if (!PATH)
                return nullptr;

            using Key = std::pair<std::string, int>;
            thread_local std::map<Key, Icon> images;
            const Key                        KEY{*PATH, request.sizePx};
            if (const auto IT = images.find(KEY); IT != images.end())
                return IT->second;

            const bool SVG = AppIconLookup::lowercase(
                                 std::filesystem::path(*PATH).extension().string()) == ".svg";
            auto* surface =
                SVG ? renderSvg(*PATH, request.sizePx) : renderPng(*PATH, request.sizePx);
            Icon icon;
            if (surface && cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS) {
                cairo_surface_flush(surface);
                icon = std::make_shared<SLoadedIcon>(
                    *PATH, request.sizePx,
                    std::shared_ptr<cairo_surface_t>(surface, cairo_surface_destroy));
            } else if (surface)
                cairo_surface_destroy(surface);
            images.emplace(KEY, icon);
            return icon;
        }
    }

    CLoader::CLoader(LoadFunction load) :
        m_load(load ? std::move(load) : loadIcon),
        m_worker([this](const std::stop_token& stop) { run(stop); }) {}

    CLoader::~CLoader() {
        m_worker.request_stop();
        m_wake.notify_all();
        m_worker.join();
    }

    std::optional<Icon> CLoader::request(const SRequest& request) {
        if (request.appIds.empty() || request.sizePx <= 0)
            return Icon{};

        std::lock_guard lock(m_mutex);
        auto&           entry = m_entries[request];
        const auto      NOW   = std::chrono::steady_clock::now();
        if (!entry.pending && (!entry.result || NOW - entry.queuedAt >= std::chrono::seconds(1))) {
            entry.pending  = true;
            entry.queuedAt = NOW;
            m_queue.push_back(request);
            m_wake.notify_one();
        }
        return entry.result;
    }

    Icon CLoader::takeReady() {
        std::lock_guard lock(m_mutex);
        if (m_ready.empty())
            return nullptr;
        auto icon = std::move(m_ready.front());
        m_ready.pop_front();
        return icon;
    }

    void CLoader::run(const std::stop_token& stop) {
        while (!stop.stop_requested()) {
            SRequest request;
            {
                std::unique_lock lock(m_mutex);
                if (!m_wake.wait(lock, stop, [this] { return !m_queue.empty(); }) ||
                    stop.stop_requested())
                    return;
                request = std::move(m_queue.front());
                m_queue.pop_front();
            }

            Icon icon;
            try {
                icon = m_load(request, stop);
            } catch (...) { icon.reset(); }

            std::lock_guard lock(m_mutex);
            auto&           entry = m_entries.at(request);
            if (icon && (!entry.result || *entry.result != icon))
                m_ready.push_back(icon);
            entry.result  = std::move(icon);
            entry.pending = false;
        }
    }
}
