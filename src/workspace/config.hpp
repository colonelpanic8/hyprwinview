#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/values/ConfigValues.hpp>
#include <hyprutils/math/Vector2D.hpp>

#include "globals.hpp"
#include "limits.hpp"

using namespace Config::Values;

namespace HTConfig {

    template <typename T>
    inline T value(std::string config) {
        static std::unordered_map<std::string, CConfigValue<T>> cache;

        if (!cache.count(config)) {
            const CConfigValue<T> val("plugin:hyprtasking:" + config);
            cache[config] = val;
        }

        return *cache[config];
    }

    inline int positive_int(const std::string& config) {
        return HT::positive_count(value<Config::INTEGER>(config));
    }

    inline float positive_float(const std::string& config) {
        return static_cast<float>(HT::positive_distance(value<Config::FLOAT>(config)));
    }

} // namespace HTConfig
