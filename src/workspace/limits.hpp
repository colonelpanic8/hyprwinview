#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace HT {
    // Bounds the grid to at most 32,768 slots, including inactive layers.
    inline constexpr int MAX_GRID_DIMENSION = 32;

    // Legacy hyprlang configs ignore the registered minimums, so every read clamps.
    inline int positive_count(int64_t value) {
        return static_cast<int>(std::clamp<int64_t>(value, 1, MAX_GRID_DIMENSION));
    }

    inline double positive_distance(double value) {
        return std::isfinite(value) && value >= 1.0 ? value : 1.0;
    }

    // Animation end callbacks outlive the transition that installed them: hyprutils keeps one
    // end callback per variable across retargets. Each callback captures a token and only
    // acts while its transition is still the latest one.
    class CTransition {
      public:
        uint64_t begin() {
            return ++current;
        }

        bool is_current(uint64_t token) const {
            return token == current;
        }

      private:
        uint64_t current = 0;
    };
}
