#include "winview_pass_element.hpp"
#include "overview.hpp"
#include <hyprland/src/output/Monitor.hpp>

std::vector<UP<IPassElement>> CWinviewPassElement::draw() {
    if (g_pWindowOverview) {
        if (foreground)
            g_pWindowOverview->drawForeground();
        else
            g_pWindowOverview->drawBackground();
    }

    return {};
}

bool CWinviewPassElement::needsLiveBlur() {
    return !foreground && g_pWindowOverview && g_pWindowOverview->backgroundBlurEnabled();
}

bool CWinviewPassElement::needsPrecomputeBlur() {
    return false;
}

std::optional<CBox> CWinviewPassElement::boundingBox() {
    if (!g_pWindowOverview || !g_pWindowOverview->pMonitor)
        return std::nullopt;

    return CBox{{0, 0}, g_pWindowOverview->pMonitor->m_size};
}

CRegion CWinviewPassElement::opaqueRegion() {
    if (foreground || !g_pWindowOverview || !g_pWindowOverview->pMonitor)
        return {};

    if (!g_pWindowOverview->occludesScene())
        return {};

    return CBox{{0, 0}, g_pWindowOverview->pMonitor->m_size};
}
