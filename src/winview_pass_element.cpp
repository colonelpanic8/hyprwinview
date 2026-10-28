#include "winview_pass_element.hpp"
#include "overview.hpp"
#include <hyprland/src/output/Monitor.hpp>

std::vector<UP<IPassElement>> CWinviewPassElement::draw(Render::CRenderContext& ctx) {
    if (g_pWindowOverview) {
        if (foreground)
            g_pWindowOverview->drawForeground(ctx);
        else
            g_pWindowOverview->drawBackground(ctx);
    }

    return {};
}

bool CWinviewPassElement::needsLiveBlur(Render::CRenderContext&) {
    return !foreground && g_pWindowOverview && g_pWindowOverview->backgroundBlurEnabled();
}

bool CWinviewPassElement::needsPrecomputeBlur(Render::CRenderContext&) {
    return false;
}

std::optional<CBox> CWinviewPassElement::boundingBox(Render::CRenderContext&) {
    if (!g_pWindowOverview || !g_pWindowOverview->pMonitor)
        return std::nullopt;

    return CBox{{0, 0}, g_pWindowOverview->pMonitor->m_size};
}

CRegion CWinviewPassElement::opaqueRegion(Render::CRenderContext&) {
    if (foreground || !g_pWindowOverview || !g_pWindowOverview->pMonitor)
        return {};

    if (!g_pWindowOverview->occludesScene())
        return {};

    return CBox{{0, 0}, g_pWindowOverview->pMonitor->m_size};
}
