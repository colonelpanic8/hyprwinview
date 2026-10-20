#ifndef HYPRWINVIEW_CONFIG_HELPERS_HPP
#define HYPRWINVIEW_CONFIG_HELPERS_HPP

#include "strings.hpp"
#include <string>
#include <hyprland/src/config/ConfigValue.hpp>

namespace Winview {
    inline std::string configStringOr(const CConfigValue<Config::STRING>& value,
                                      const std::string&                  fallback) {
        try {
            return *value;
        } catch (...) { return fallback; }
    }

}

#endif
