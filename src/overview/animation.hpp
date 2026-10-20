#ifndef HYPRWINVIEW_OVERVIEW_ANIMATION_HPP
#define HYPRWINVIEW_OVERVIEW_ANIMATION_HPP

#include <cstdint>

enum class EOverviewAnimation : uint8_t {
    NONE,
    FADE,
    FADE_SCALE,
    STAGGERED_FADE_SCALE,
    WORKSPACE_ZOOM,
};

EOverviewAnimation overviewAnimation();
bool               animationScalesTiles(EOverviewAnimation animation);

#endif
