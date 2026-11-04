#ifndef HYPRWINVIEW_AUDIO_INDICATOR_HPP
#define HYPRWINVIEW_AUDIO_INDICATOR_HPP

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/math/Math.hpp>
#include <hyprutils/math/Region.hpp>

namespace Render {
    class CRenderContext;
}

namespace Winview {
    // Registers hyprwinview.configure_audio and the config listeners that start and stop the
    // audio adapters to match show_audio_icon.
    void initAudioIndicator();
    void shutdownAudioIndicator();

    void drawAudioBadge(Render::CRenderContext& ctx, const PHLMONITOR& monitor,
                        const PHLWINDOW& window, const CBox& tilePx, double alpha, CRegion& damage);
}

#endif
