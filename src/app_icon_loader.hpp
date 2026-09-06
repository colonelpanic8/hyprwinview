#ifndef HYPRWINVIEW_APP_ICON_LOADER_HPP
#define HYPRWINVIEW_APP_ICON_LOADER_HPP

#include "app_icon_lookup.hpp"

#include <cairo/cairo.h>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
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

    class CLoader {
      public:
        using LoadFunction = std::function<Icon(const SRequest&, const std::stop_token&)>;
        explicit CLoader(LoadFunction load = {});
        ~CLoader();

        // An empty optional is pending; a null icon is a completed miss.
        std::optional<Icon> request(const SRequest& request);
        Icon                takeReady();

      private:
        struct SEntry {
            std::optional<Icon>                   result;
            std::chrono::steady_clock::time_point queuedAt;
            bool                                  pending = false;
        };

        void                        run(const std::stop_token& stop);

        LoadFunction                m_load;
        std::mutex                  m_mutex;
        std::condition_variable_any m_wake;
        std::map<SRequest, SEntry>  m_entries;
        std::deque<SRequest>        m_queue;
        std::deque<Icon>            m_ready;
        std::jthread                m_worker;
    };
}

#endif
