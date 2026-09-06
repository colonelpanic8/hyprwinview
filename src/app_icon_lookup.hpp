#ifndef HYPRWINVIEW_APP_ICON_LOOKUP_HPP
#define HYPRWINVIEW_APP_ICON_LOOKUP_HPP

#include <compare>
#include <optional>
#include <string>
#include <vector>

namespace AppIconLookup {
    struct SConfig {
        std::string theme;
        std::string source = "auto";
        std::string overrides;
        auto        operator<=>(const SConfig&) const = default;
    };

    std::string                lowercase(std::string value);
    std::optional<std::string> findAppIconPath(const std::vector<std::string>& appIds, int sizePx,
                                               const SConfig& config);
    // Clears the calling thread's resolver caches.
    void clearCache();
}

#endif
