#include "WinviewPassElement.hpp"
#include "overview.hpp"
#include <hyprland/src/helpers/Monitor.hpp>

std::vector<UP<IPassElement>> CWinviewPassElement::draw() {
    if (g_pWindowOverview)
        g_pWindowOverview->draw();

    return {};
}

bool CWinviewPassElement::needsLiveBlur() {
    return false;
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
    if (!g_pWindowOverview || !g_pWindowOverview->pMonitor)
        return {};

    return CBox{{0, 0}, g_pWindowOverview->pMonitor->m_size};
}
