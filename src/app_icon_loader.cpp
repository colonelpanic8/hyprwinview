#include "app_icon_loader.hpp"
#include "strings.hpp"

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

        Icon decode(const std::string& path, int sizePx) {
            const bool SVG =
                Winview::lowercase(std::filesystem::path(path).extension().string()) == ".svg";
            auto* surface = SVG ? renderSvg(path, sizePx) : renderPng(path, sizePx);
            if (!surface)
                return nullptr;
            if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
                cairo_surface_destroy(surface);
                return nullptr;
            }
            cairo_surface_flush(surface);
            return std::make_shared<SLoadedIcon>(
                path, sizePx, std::shared_ptr<cairo_surface_t>(surface, cairo_surface_destroy));
        }

        class CFileSource final : public ISource {
          public:
            Icon load(const SRequest& request, const std::stop_token& stop) override {
                const auto PATH =
                    m_resolver.find(request.appIds, request.sizePx, request.config, stop);
                if (!PATH || stop.stop_requested())
                    return nullptr;

                const Key KEY{*PATH, request.sizePx};
                if (const auto IT = m_images.find(KEY); IT != m_images.end()) {
                    if (auto icon = IT->second.lock())
                        return icon;
                }

                std::erase_if(m_images, [](const auto& entry) { return entry.second.expired(); });
                auto icon = decode(*PATH, request.sizePx);
                if (icon)
                    m_images[KEY] = icon;
                return icon;
            }

            void reset() override {
                m_resolver.clear();
                m_images.clear();
            }

            bool refresh() override {
                return m_resolver.refreshThemes();
            }

          private:
            using Key = std::pair<std::string, int>;

            AppIconLookup::CResolver                        m_resolver;
            std::map<Key, std::weak_ptr<const SLoadedIcon>> m_images;
        };
    }

    std::unique_ptr<ISource> makeFileSource() {
        return std::make_unique<CFileSource>();
    }

    CLoader::CLoader(std::unique_ptr<ISource> source, SOptions options) :
        m_source(std::move(source)),
        m_options{options.pollInterval, std::max<size_t>(1, options.maxEntries)},
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
        const auto [IT, INSERTED] = m_entries.try_emplace(request);
        IT->second.lastUsed       = ++m_clock;
        if (INSERTED) {
            m_queue.push_back(request);
            m_wake.notify_one();
            evictLocked();
        }
        return IT->second.result;
    }

    Icon CLoader::takeReady() {
        std::lock_guard lock(m_mutex);
        if (m_ready.empty())
            return nullptr;
        auto icon = std::move(m_ready.front());
        m_ready.pop_front();
        return icon;
    }

    void CLoader::invalidate() {
        std::lock_guard lock(m_mutex);
        m_resetPending = true;
        requeueAllLocked();
        m_wake.notify_one();
    }

    void CLoader::requeueAllLocked() {
        ++m_generation;
        m_queue.clear();
        for (const auto& [request, entry] : m_entries)
            m_queue.push_back(request);
    }

    void CLoader::evictLocked() {
        while (m_entries.size() > m_options.maxEntries) {
            const auto OLDEST = std::ranges::min_element(
                m_entries, {}, [](const auto& entry) { return entry.second.lastUsed; });
            std::erase(m_queue, OLDEST->first);
            m_entries.erase(OLDEST);
        }
    }

    void CLoader::run(const std::stop_token& stop) {
        while (!stop.stop_requested()) {
            SRequest request;
            uint64_t generation = 0;
            bool     reset      = false;
            {
                std::unique_lock lock(m_mutex);
                if (!m_wake.wait_for(lock, stop, m_options.pollInterval,
                                     [this] { return !m_queue.empty(); })) {
                    if (stop.stop_requested())
                        return;
                    lock.unlock();
                    bool changed = false;
                    try {
                        changed = m_source->refresh();
                    } catch (...) { continue; }
                    if (changed) {
                        lock.lock();
                        requeueAllLocked();
                    }
                    continue;
                }
                if (stop.stop_requested())
                    return;
                request = std::move(m_queue.front());
                m_queue.pop_front();
                if (!m_entries.contains(request))
                    continue;
                generation = m_generation;
                reset      = std::exchange(m_resetPending, false);
            }

            Icon icon;
            try {
                if (reset)
                    m_source->reset();
                icon = m_source->load(request, stop);
            } catch (...) { icon.reset(); }
            if (stop.stop_requested())
                return;

            std::lock_guard lock(m_mutex);
            const auto      IT = m_entries.find(request);
            if (generation != m_generation || IT == m_entries.end())
                continue;
            auto& entry = IT->second;
            if (icon && (!entry.result || *entry.result != icon)) {
                if (m_ready.size() >= m_options.maxEntries)
                    m_ready.pop_front();
                m_ready.push_back(icon);
            }
            entry.result = std::move(icon);
        }
    }
}
