#ifndef HYPRWINVIEW_APP_ICON_LOOKUP_HPP
#define HYPRWINVIEW_APP_ICON_LOOKUP_HPP

#include <compare>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace AppIconLookup {
    struct SConfig {
        std::string theme;
        std::string source = "auto";
        std::string overrides;
        auto        operator<=>(const SConfig&) const = default;
    };

    // Caches lookups, including misses, until clear() or a theme change seen by refreshThemes().
    // A stopped lookup returns nothing and caches nothing derived from a partial scan.
    class CResolver {
      public:
        CResolver();
        ~CResolver();

        std::optional<std::string> find(const std::vector<std::string>& appIds, int sizePx,
                                        const SConfig& config, const std::stop_token& stop = {});
        // Re-reads GTK/Qt theme settings; true when a cached theme list changed.
        bool refreshThemes();
        void clear();

      private:
        struct SState;
        std::unique_ptr<SState> m_state;
    };
}

#endif
