#include "overview.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

#define private public
#define protected public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#undef private
#undef protected

#include "WinviewPassElement.hpp"
#include "globals.hpp"

static const CConfigValue<Hyprlang::INT>& PGAP() {
    static const CConfigValue<Hyprlang::INT> VALUE("plugin:hyprwinview:gap_size");
    return VALUE;
}

static const CConfigValue<Hyprlang::INT>& PMARGIN() {
    static const CConfigValue<Hyprlang::INT> VALUE("plugin:hyprwinview:margin");
    return VALUE;
}

static const CConfigValue<Hyprlang::INT>& PBG() {
    static const CConfigValue<Hyprlang::INT> VALUE("plugin:hyprwinview:bg_col");
    return VALUE;
}

static const CConfigValue<Hyprlang::INT>& PBORDER() {
    static const CConfigValue<Hyprlang::INT> VALUE("plugin:hyprwinview:border_col");
    return VALUE;
}

static const CConfigValue<Hyprlang::INT>& PHOVERBORDER() {
    static const CConfigValue<Hyprlang::INT> VALUE("plugin:hyprwinview:hover_border_col");
    return VALUE;
}

static const CConfigValue<Hyprlang::INT>& PBORDERSIZE() {
    static const CConfigValue<Hyprlang::INT> VALUE("plugin:hyprwinview:border_size");
    return VALUE;
}

static uint32_t framebufferFormatWithAlpha(uint32_t drmFormat) {
    return DRM_FORMAT_ABGR8888;
}

static bool previewableWindow(const PHLWINDOW& window) {
    if (!window || !window->m_isMapped || window->isHidden() || window->m_fadingOut || !window->m_workspace)
        return false;

    if (window->m_size.x <= 1 || window->m_size.y <= 1 || window->m_realSize->value().x <= 1 || window->m_realSize->value().y <= 1)
        return false;

    return true;
}

CWindowOverview::CWindowOverview(PHLMONITOR monitor) : pMonitor(monitor) {
    collectWindows();
    updateLayout();
    renderSnapshots();

    lastMousePosLocal = g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position;

    auto onCursorMove = [this](Event::SCallbackInfo& info) {
        if (closing)
            return;

        info.cancelled    = true;
        lastMousePosLocal = g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position;
        damage();
    };

    auto onCursorSelect = [this](Event::SCallbackInfo& info) {
        if (closing)
            return;

        info.cancelled = true;
        selectHoveredWindow();
        close(true);
    };

    mouseMoveHook   = Event::bus()->m_events.input.mouse.move.listen([onCursorMove](Vector2D, Event::SCallbackInfo& info) { onCursorMove(info); });
    touchMoveHook   = Event::bus()->m_events.input.touch.motion.listen([onCursorMove](ITouch::SMotionEvent, Event::SCallbackInfo& info) { onCursorMove(info); });
    mouseButtonHook = Event::bus()->m_events.input.mouse.button.listen([onCursorSelect](IPointer::SButtonEvent, Event::SCallbackInfo& info) { onCursorSelect(info); });
    touchDownHook   = Event::bus()->m_events.input.touch.down.listen([onCursorSelect](ITouch::SDownEvent, Event::SCallbackInfo& info) { onCursorSelect(info); });

    damage();
}

CWindowOverview::~CWindowOverview() {
    Render::GL::g_pHyprOpenGL->makeEGLCurrent();
    for (auto& preview : previews)
        preview.fb.reset();
    previews.clear();
}

void CWindowOverview::collectWindows() {
    previews.clear();

    for (auto it = g_pCompositor->m_windows.rbegin(); it != g_pCompositor->m_windows.rend(); ++it) {
        const auto& window = *it;
        if (!previewableWindow(window))
            continue;

        previews.push_back({.window = window});
    }

    std::ranges::reverse(previews);
}

void CWindowOverview::updateLayout() {
    if (!pMonitor || previews.empty())
        return;

    const double count  = previews.size();
    const double aspect = std::max(0.1, pMonitor->m_size.x / std::max(1.0, pMonitor->m_size.y));
    int          cols   = std::max(1, (int)std::ceil(std::sqrt(count * aspect)));
    int          rows   = std::max(1, (int)std::ceil(count / cols));

    while (cols > 1 && (cols - 1) * rows >= (int)previews.size())
        cols--;

    rows = std::max(1, (int)std::ceil(count / cols));

    const double margin = std::max<Hyprlang::INT>(0, *PMARGIN());
    const double gap    = std::max<Hyprlang::INT>(0, *PGAP());
    const double areaW  = std::max(1.0, pMonitor->m_size.x - margin * 2.0);
    const double areaH  = std::max(1.0, pMonitor->m_size.y - margin * 2.0);
    const double cellW  = (areaW - gap * (cols - 1)) / cols;
    const double cellH  = (areaH - gap * (rows - 1)) / rows;

    for (size_t i = 0; i < previews.size(); ++i) {
        auto&        preview = previews[i];
        const double col     = i % cols;
        const double row     = i / cols;
        const auto   winSize = preview.window->m_realSize->value();
        const double scale   = std::min(cellW / std::max(1.0, winSize.x), cellH / std::max(1.0, winSize.y));
        const double w       = std::max(1.0, winSize.x * scale);
        const double h       = std::max(1.0, winSize.y * scale);
        const double x       = margin + col * (cellW + gap) + (cellW - w) / 2.0;
        const double y       = margin + row * (cellH + gap) + (cellH - h) / 2.0;

        preview.tileLogical = {x, y, w, h};
    }
}

void CWindowOverview::renderSnapshots() {
    if (!pMonitor)
        return;

    Render::GL::g_pHyprOpenGL->makeEGLCurrent();

    const auto FORMAT = framebufferFormatWithAlpha(pMonitor->m_output->state->state().drmFormat);

    for (auto& preview : previews) {
        if (!preview.fb)
            preview.fb = g_pHyprRenderer->createFB("hyprwinview");

        if (preview.fb->m_size != pMonitor->m_pixelSize) {
            preview.fb->release();
            preview.fb->alloc(pMonitor->m_pixelSize.x, pMonitor->m_pixelSize.y, FORMAT);
        }

        CRegion fakeDamage{0, 0, (int)pMonitor->m_transformedSize.x, (int)pMonitor->m_transformedSize.y};
        g_pHyprRenderer->beginFullFakeRender(pMonitor.lock(), fakeDamage, preview.fb);
        glClearColor(0.F, 0.F, 0.F, 0.F);
        glClear(GL_COLOR_BUFFER_BIT);
        g_pHyprRenderer->renderWindow(preview.window, pMonitor.lock(), Time::steadyNow(), false, Render::RENDER_PASS_ALL, true, true);
        g_pHyprRenderer->m_renderData.blockScreenShader = true;
        g_pHyprRenderer->endRender();
    }
}

int CWindowOverview::hoveredIndex() const {
    for (size_t i = 0; i < previews.size(); ++i) {
        if (previews[i].tileLogical.containsPoint(lastMousePosLocal))
            return i;
    }

    return -1;
}

void CWindowOverview::render() {
    g_pHyprRenderer->m_renderPass.add(makeUnique<CWinviewPassElement>());
}

void CWindowOverview::draw() {
    if (!pMonitor)
        return;

    const auto SCALE       = pMonitor->m_scale;
    const auto HOVERED    = hoveredIndex();
    const int  BORDER     = std::max<Hyprlang::INT>(0, *PBORDERSIZE());
    CRegion    fullDamage = {0, 0, INT16_MAX, INT16_MAX};

    Render::GL::g_pHyprOpenGL->renderRect(CBox{{0, 0}, pMonitor->m_pixelSize}, CHyprColor(*PBG()).stripA(), {.damage = &fullDamage});

    for (size_t i = 0; i < previews.size(); ++i) {
        auto& preview = previews[i];
        if (!preview.fb || !preview.fb->getTexture())
            continue;

        CBox tilePx = preview.tileLogical.copy().scale(SCALE).round();
        CBox texBox = {
            tilePx.x,
            tilePx.y,
            pMonitor->m_pixelSize.x * (tilePx.w / std::max(1.0, preview.window->m_realSize->value().x * SCALE)),
            pMonitor->m_pixelSize.y * (tilePx.h / std::max(1.0, preview.window->m_realSize->value().y * SCALE)),
        };

        if (BORDER > 0) {
            const auto COLOR = (int)i == HOVERED ? CHyprColor(*PHOVERBORDER()) : CHyprColor(*PBORDER());
            Render::GL::g_pHyprOpenGL->renderRect(tilePx.copy().expand(BORDER), COLOR, {.damage = &fullDamage, .round = BORDER * 2});
        }

        g_pHyprRenderer->m_renderData.clipBox = tilePx;
        Render::GL::g_pHyprOpenGL->renderTexture(preview.fb->getTexture(), texBox, {.damage = &fullDamage, .a = 1.0, .round = BORDER * 2});
        g_pHyprRenderer->m_renderData.clipBox = {};
    }
}

void CWindowOverview::damage() {
    if (pMonitor)
        g_pHyprRenderer->damageMonitor(pMonitor.lock());
}

void CWindowOverview::selectHoveredWindow() {
    selectedIndex = hoveredIndex();
}

void CWindowOverview::focusWindow(PHLWINDOW window, bool bring) {
    if (!window || !window->m_workspace)
        return;

    const auto FOCUSSTATE = Desktop::focusState();
    const auto MONITOR    = FOCUSSTATE->monitor();

    if (bring && MONITOR && MONITOR->m_activeWorkspace && window->m_workspace != MONITOR->m_activeWorkspace) {
        g_pCompositor->moveWindowToWorkspaceSafe(window, MONITOR->m_activeWorkspace);
        window->m_workspace = MONITOR->m_activeWorkspace;
    }

    if (MONITOR && MONITOR->m_activeWorkspace != window->m_workspace)
        MONITOR->changeWorkspace(window->m_workspace);

    FOCUSSTATE->fullWindowFocus(window, Desktop::FOCUS_REASON_KEYBIND);
    g_pCompositor->warpCursorTo(window->middle());
}

void CWindowOverview::close(bool focusSelection, bool bringSelection) {
    if (closing)
        return;

    closing = true;

    PHLWINDOW selectedWindow;
    if (focusSelection && selectedIndex >= 0 && selectedIndex < (int)previews.size())
        selectedWindow = previews[selectedIndex].window;

    damage();
    g_pWindowOverview.reset();

    if (selectedWindow)
        focusWindow(selectedWindow, bringSelection);
}
