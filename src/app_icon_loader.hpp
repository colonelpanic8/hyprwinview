#ifndef HYPRWINVIEW_APP_ICON_LOADER_HPP
#define HYPRWINVIEW_APP_ICON_LOADER_HPP

#include "app_icon_lookup.hpp"

#include <cairo/cairo.h>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

namespace AppIconLoader {
    struct SRequest {
        std::vector<std::string> appIds;
        int                      sizePx = 0;
        AppIconLookup::SConfig   config;
        auto                     operator<=>(const SRequest&) const = default;
    };

    struct SLoadedIcon {
        std::string                      path;
        int                              sizePx = 0;
        std::shared_ptr<cairo_surface_t> surface;
    };

    using Icon = std::shared_ptr<const SLoadedIcon>;

    // All methods run on the loader's worker thread.
    class ISource {
      public:
        virtual ~ISource()                                                      = default;
        virtual Icon load(const SRequest& request, const std::stop_token& stop) = 0;
        // Drops every cached lookup and decoded image.
        virtual void reset() {}
        // Polled while idle; true when the source dropped state that earlier results used.
        virtual bool refresh() {
            return false;
        }
    };

    // Resolves through the icon theme and desktop-entry lookup and decodes PNG/SVG files.
    std::unique_ptr<ISource> makeFileSource();

    struct SOptions {
        std::chrono::milliseconds pollInterval = std::chrono::seconds(1);
        size_t                    maxEntries   = 256;
    };

    class CLoader {
      public:
        explicit CLoader(std::unique_ptr<ISource> source = makeFileSource(), SOptions options = {});
        ~CLoader();

        // An empty optional is pending; a null icon is a completed miss.
        std::optional<Icon> request(const SRequest& request);
        Icon                takeReady();
        // Resets the source on the worker and re-resolves every entry, keeping old results
        // visible until their replacements finish.
        void invalidate();

      private:
        struct SEntry {
            std::optional<Icon> result;
            uint64_t            lastUsed = 0;
        };

        void                        requeueAllLocked();
        void                        evictLocked();
        void                        run(const std::stop_token& stop);

        std::unique_ptr<ISource>    m_source;
        SOptions                    m_options;
        std::mutex                  m_mutex;
        std::condition_variable_any m_wake;
        std::map<SRequest, SEntry>  m_entries;
        std::deque<SRequest>        m_queue;
        std::deque<Icon>            m_ready;
        uint64_t                    m_generation   = 0;
        uint64_t                    m_clock        = 0;
        bool                        m_resetPending = false;
        std::jthread                m_worker;
    };
}

#endif
