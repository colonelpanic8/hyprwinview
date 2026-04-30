#include "overview.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <sstream>
#include <string>

#define private public
#define protected public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#undef private
#undef protected

#include <xkbcommon/xkbcommon.h>

#include "WinviewPassElement.hpp"
#include "globals.hpp"

static const CConfigValue<Config::INTEGER>& PGAP() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:gap_size");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PMARGIN() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:margin");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PBG() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:bg_col");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PBORDER() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:border_col");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PHOVERBORDER() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:hover_border_col");
    return VALUE;
}

static const CConfigValue<Config::INTEGER>& PBORDERSIZE() {
    static const CConfigValue<Config::INTEGER> VALUE("plugin:hyprwinview:border_size");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSLEFT() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_left");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSRIGHT() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_right");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSUP() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_up");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSDOWN() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_down");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSGO() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_go");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSBRING() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_bring");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSCLOSE() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_close");
    return VALUE;
}

static uint32_t framebufferFormatWithAlpha(uint32_t drmFormat) {
    return DRM_FORMAT_ABGR8888;
}

static std::string trimmedLower(std::string token) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    token.erase(token.begin(), std::ranges::find_if(token, notSpace));
    token.erase(std::ranges::find_if(token.rbegin(), token.rend(), notSpace).base(), token.end());
    std::ranges::transform(token, token.begin(), [](unsigned char c) { return std::tolower(c); });
    return token;
}

static std::vector<std::string> keyTokens(const std::string& keys) {
    std::vector<std::string> result;
    std::stringstream        stream(keys);
    std::string              token;

    while (std::getline(stream, token, ',')) {
        token = trimmedLower(token);
        if (!token.empty())
            result.push_back(token);
    }

    return result;
}

static std::string normalizeKeyName(std::string key) {
    key = trimmedLower(key);

    if (key == "enter")
        return "return";
    if (key == "esc")
        return "escape";
    if (key == "arrowleft")
        return "left";
    if (key == "arrowright")
        return "right";
    if (key == "arrowup")
        return "up";
    if (key == "arrowdown")
        return "down";

    return key;
}

static xkb_keysym_t keysymForName(const std::string& key) {
    const auto NORMALIZED = normalizeKeyName(key);

    if (NORMALIZED == "left")
        return XKB_KEY_Left;
    if (NORMALIZED == "right")
        return XKB_KEY_Right;
    if (NORMALIZED == "up")
        return XKB_KEY_Up;
    if (NORMALIZED == "down")
        return XKB_KEY_Down;
    if (NORMALIZED == "return")
        return XKB_KEY_Return;
    if (NORMALIZED == "space")
        return XKB_KEY_space;
    if (NORMALIZED == "escape")
        return XKB_KEY_Escape;

    return xkb_keysym_from_name(NORMALIZED.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
}

static bool tokenMatchesKey(const std::string& token, xkb_keysym_t keysym, uint32_t mods) {
    std::stringstream stream(token);
    std::string       part;
    uint32_t          requiredMods = 0;
    xkb_keysym_t      requiredKey  = XKB_KEY_NoSymbol;

    while (std::getline(stream, part, '+')) {
        part = normalizeKeyName(part);

        if (part == "shift")
            requiredMods |= HL_MODIFIER_SHIFT;
        else if (part == "ctrl" || part == "control")
            requiredMods |= HL_MODIFIER_CTRL;
        else if (part == "alt")
            requiredMods |= HL_MODIFIER_ALT;
        else if (part == "super" || part == "mod" || part == "meta")
            requiredMods |= HL_MODIFIER_META;
        else
            requiredKey = keysymForName(part);
    }

    return requiredKey != XKB_KEY_NoSymbol && xkb_keysym_to_lower(requiredKey) == xkb_keysym_to_lower(keysym) && (mods & requiredMods) == requiredMods;
}

static bool matchesKeySet(const CConfigValue<Config::STRING>& keys, xkb_keysym_t keysym, uint32_t mods) {
    for (const auto& token : keyTokens(*keys)) {
        if (tokenMatchesKey(token, keysym, mods))
            return true;
    }

    return false;
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
    selectedIndex     = hoveredIndex();
    if (selectedIndex < 0 && !previews.empty())
        selectedIndex = 0;

    auto onCursorMove = [this](Event::SCallbackInfo& info) {
        if (closing)
            return;

        info.cancelled    = true;
        lastMousePosLocal = g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position;
        const auto HOVERED = hoveredIndex();
        if (HOVERED >= 0)
            selectedIndex = HOVERED;
        damage();
    };

    auto onCursorSelect = [this](Event::SCallbackInfo& info) {
        if (closing)
            return;

        info.cancelled = true;
        selectHoveredWindow();
        close(true);
    };

    auto onKeyboardKey = [this](const IKeyboard::SKeyEvent& event, Event::SCallbackInfo& info) {
        if (closing)
            return;

        if (handleKey(event))
            info.cancelled = true;
    };

    mouseMoveHook   = Event::bus()->m_events.input.mouse.move.listen([onCursorMove](Vector2D, Event::SCallbackInfo& info) { onCursorMove(info); });
    touchMoveHook   = Event::bus()->m_events.input.touch.motion.listen([onCursorMove](ITouch::SMotionEvent, Event::SCallbackInfo& info) { onCursorMove(info); });
    mouseButtonHook = Event::bus()->m_events.input.mouse.button.listen([onCursorSelect](IPointer::SButtonEvent, Event::SCallbackInfo& info) { onCursorSelect(info); });
    touchDownHook   = Event::bus()->m_events.input.touch.down.listen([onCursorSelect](ITouch::SDownEvent, Event::SCallbackInfo& info) { onCursorSelect(info); });
    keyboardHook    = Event::bus()->m_events.input.keyboard.key.listen([onKeyboardKey](IKeyboard::SKeyEvent event, Event::SCallbackInfo& info) { onKeyboardKey(event, info); });

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
    gridCols = cols;

    const double margin = std::max<Config::INTEGER>(0, *PMARGIN());
    const double gap    = std::max<Config::INTEGER>(0, *PGAP());
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
    const auto HOVERED    = selectedIndex;
    const int  BORDER     = std::max<Config::INTEGER>(0, *PBORDERSIZE());
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

void CWindowOverview::moveSelection(int dx, int dy) {
    if (previews.empty())
        return;

    if (selectedIndex < 0 || selectedIndex >= (int)previews.size())
        selectedIndex = 0;

    const int col    = selectedIndex % gridCols;
    const int row    = selectedIndex / gridCols;
    const int newCol = std::clamp(col + dx, 0, gridCols - 1);
    const int maxRow = (previews.size() - 1) / gridCols;
    const int newRow = std::clamp(row + dy, 0, maxRow);
    int       next   = newRow * gridCols + newCol;

    if (next >= (int)previews.size())
        next = previews.size() - 1;

    if (next != selectedIndex) {
        selectedIndex     = next;
        lastMousePosLocal = previews[selectedIndex].tileLogical.middle();
        damage();
    }
}

void CWindowOverview::runSelected(bool bring) {
    close(true, bring);
}

bool CWindowOverview::handleKey(const IKeyboard::SKeyEvent& event) {
    const auto KEYBOARD = g_pSeatManager && !g_pSeatManager->m_keyboard.expired() ? g_pSeatManager->m_keyboard.lock() : nullptr;
    if (!KEYBOARD || !KEYBOARD->m_xkbState)
        return false;

    const auto KEYCODE = event.keycode + 8;
    const auto KEYSYM  = xkb_state_key_get_one_sym(KEYBOARD->m_xkbState, KEYCODE);
    const auto MODS    = g_pInputManager->getModsFromAllKBs();

    const bool RECOGNIZED = matchesKeySet(PKEYSLEFT(), KEYSYM, MODS) || matchesKeySet(PKEYSRIGHT(), KEYSYM, MODS) || matchesKeySet(PKEYSUP(), KEYSYM, MODS) ||
        matchesKeySet(PKEYSDOWN(), KEYSYM, MODS) || matchesKeySet(PKEYSGO(), KEYSYM, MODS) || matchesKeySet(PKEYSBRING(), KEYSYM, MODS) || matchesKeySet(PKEYSCLOSE(), KEYSYM, MODS);

    if (!RECOGNIZED)
        return false;

    if (event.state != WL_KEYBOARD_KEY_STATE_PRESSED)
        return true;

    if (matchesKeySet(PKEYSLEFT(), KEYSYM, MODS))
        moveSelection(-1, 0);
    else if (matchesKeySet(PKEYSRIGHT(), KEYSYM, MODS))
        moveSelection(1, 0);
    else if (matchesKeySet(PKEYSUP(), KEYSYM, MODS))
        moveSelection(0, -1);
    else if (matchesKeySet(PKEYSDOWN(), KEYSYM, MODS))
        moveSelection(0, 1);
    else if (matchesKeySet(PKEYSBRING(), KEYSYM, MODS))
        runSelected(true);
    else if (matchesKeySet(PKEYSGO(), KEYSYM, MODS))
        runSelected(false);
    else if (matchesKeySet(PKEYSCLOSE(), KEYSYM, MODS))
        close(false);

    return true;
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
