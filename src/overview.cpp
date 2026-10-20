#include "overview.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#define private   public
#define protected public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/state/GlobalWindowController.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
#include <hyprland/src/managers/fullscreen/FullscreenController.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/pointer/PointerController.hpp>
#include <hyprland/src/pointer/PointerManager.hpp>
#include <hyprland/src/pointer/cursor/CursorShapeOverrideController.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/state/WorkspaceState.hpp>
#undef private
#undef protected

#include <xkbcommon/xkbcommon.h>

#include "app_icon.hpp"
#include "globals.hpp"
#include "settings.hpp"
#include "config_helpers.hpp"
#include "overview/animation.hpp"

#include "overview/text.hpp"
#include "winview_pass_element.hpp"
#include "workspace/render.hpp"

using namespace WinviewConfig;
using Winview::configStringOr;
using Winview::trimmedLower;

static constexpr uint64_t       DEFAULT_BACKGROUND = PBACKGROUNDDefault;

static std::vector<std::string> queryTokens(const std::string& query) {
    std::vector<std::string> result;
    std::stringstream        stream(Winview::lowercase(query));
    std::string              token;

    while (stream >> token)
        result.push_back(token);

    return result;
}

static std::string windowTitle(const PHLWINDOW& window) {
    if (!window)
        return "";

    if (!window->m_title.empty())
        return window->m_title;

    return window->m_initialTitle;
}

static std::string windowClass(const PHLWINDOW& window) {
    if (!window)
        return "";

    if (!window->m_class.empty())
        return window->m_class;

    return window->m_initialClass;
}

static std::string searchableWindowText(const PHLWINDOW& window) {
    if (!window)
        return "";

    return Winview::lowercase(windowTitle(window) + " " + windowClass(window) + " " +
                              window->m_initialTitle + " " + window->m_initialClass);
}

static bool windowMatchesQuery(const PHLWINDOW& window, const std::string& query) {
    const auto TOKENS = queryTokens(query);
    if (TOKENS.empty())
        return true;

    const auto SEARCHABLE = searchableWindowText(window);
    return std::ranges::all_of(TOKENS, [&SEARCHABLE](const std::string& token) {
        return SEARCHABLE.find(token) != std::string::npos;
    });
}

static int workspaceGridColsForCount(int count) {
    return std::max(1, (int)std::ceil(std::sqrt(std::max(1, count))));
}

static int workspaceGridRowsForCount(int count, int cols) {
    return std::max(1, (int)std::ceil((double)std::max(1, count) / std::max(1, cols)));
}

static CBox scaleBoxFromCenter(const CBox& box, double scale) {
    const auto CENTER = box.middle();
    const auto W      = box.w * scale;
    const auto H      = box.h * scale;

    return {CENTER.x - W / 2.0, CENTER.y - H / 2.0, W, H};
}

static CHyprColor multiplyAlpha(const CHyprColor& color, double alpha) {
    return color.modifyA(static_cast<float>(color.a * std::clamp(alpha, 0.0, 1.0)));
}

static CHyprColor activeBackgroundColor() {
    const auto BACKGROUND = static_cast<uint64_t>(*PBACKGROUND());
    const auto BG_COL     = static_cast<uint64_t>(*PBG());

    if (BACKGROUND == DEFAULT_BACKGROUND && BG_COL != DEFAULT_BACKGROUND)
        return CHyprColor(BG_COL);

    return CHyprColor(BACKGROUND);
}

static Vector2D iconAnchorFromPosition(const std::string& position) {
    auto   anchor = Vector2D{1.0, 1.0};
    auto   value  = Winview::lowercase(position);
    size_t start  = 0;
    bool   hasX   = false;
    bool   hasY   = false;
    bool   center = false;

    while (start < value.size()) {
        const auto END = value.find_first_of(" ,", start);
        const auto TOKEN =
            value.substr(start, END == std::string::npos ? std::string::npos : END - start);

        if (TOKEN == "left") {
            anchor.x = 0.0;
            hasX     = true;
        } else if (TOKEN == "right") {
            anchor.x = 1.0;
            hasX     = true;
        } else if (TOKEN == "top") {
            anchor.y = 0.0;
            hasY     = true;
        } else if (TOKEN == "bottom") {
            anchor.y = 1.0;
            hasY     = true;
        } else if (TOKEN == "center" || TOKEN == "middle")
            center = true;

        if (END == std::string::npos)
            break;

        start = value.find_first_not_of(" ,", END);
        if (start == std::string::npos)
            break;
    }

    if (center || (hasY && !hasX))
        anchor.x = hasX ? anchor.x : 0.5;

    if (center || (hasX && !hasY))
        anchor.y = hasY ? anchor.y : 0.5;

    return anchor;
}

static double anchorOverride(double configured, double fallback) {
    return configured >= 0.0 ? std::clamp(configured, 0.0, 1.0) : fallback;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static double edgeSignedMargin(double anchor, double absolute, double relative, double extent) {
    if (anchor < 0.5)
        return absolute + relative * extent;
    if (anchor > 0.5)
        return -(absolute + relative * extent);
    return 0.0;
}

static CBox appIconBoxForTile(const CBox& tileLogical, double scale) {
    const int SIZE   = static_cast<int>(std::max<Config::INTEGER>(1, *PAPPICONSIZE()));
    Vector2D  anchor = iconAnchorFromPosition(configStringOr(PAPPICONPOSITION(), "bottom right"));
    anchor.x         = anchorOverride(*PAPPICONANCHORX(), anchor.x);
    anchor.y         = anchorOverride(*PAPPICONANCHORY(), anchor.y);

    const double xMargin = edgeSignedMargin(anchor.x, static_cast<double>(*PAPPICONMARGINX()),
                                            *PAPPICONMARGINRELX(), tileLogical.w);
    const double yMargin = edgeSignedMargin(anchor.y, static_cast<double>(*PAPPICONMARGINY()),
                                            *PAPPICONMARGINRELY(), tileLogical.h);
    const double x = tileLogical.x + anchor.x * std::max(0.0, tileLogical.w - SIZE) + xMargin +
        static_cast<double>(*PAPPICONOFFSETX());
    const double y = tileLogical.y + anchor.y * std::max(0.0, tileLogical.h - SIZE) + yMargin +
        static_cast<double>(*PAPPICONOFFSETY());

    return CBox{x, y, (double)SIZE, (double)SIZE}.scale(scale).round();
}

static bool previewableWindow(const PHLWINDOW& window) {
    if (!window || !window->m_isMapped || window->isHidden() || !window->m_workspace)
        return false;

    const auto SIZE = window->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
    return SIZE.x > 1 && SIZE.y > 1 && window->m_realSize->value().x > 1 &&
        window->m_realSize->value().y > 1;
}

static bool minimizedWorkspace(const PHLWORKSPACE& workspace) {
    if (!workspace)
        return false;

    const auto& NAME = workspace->m_name;
    return NAME == "special:minimized" || NAME == "minimized" ||
        (workspace->m_isSpecialWorkspace && NAME.find("minimized") != std::string::npos);
}

static bool scratchpadWorkspace(const PHLWORKSPACE& workspace) {
    if (!workspace)
        return false;

    const auto& NAME = workspace->m_name;
    return NAME == "special:NSP" || NAME == "NSP" || NAME.starts_with("scratch-hidden-");
}

static bool recoverableWindow(const PHLWINDOW& window) {
    if (!window)
        return false;

    return minimizedWorkspace(window->m_workspace) || scratchpadWorkspace(window->m_workspace);
}

CWindowOverview::CWindowOverview(const PHLMONITOR& monitor, SWindowOverviewOptions options_) :
    pMonitor(monitor), options(options_) {
    animationStartedAt       = Time::steadyNow();
    filterAnimationStartedAt = animationStartedAt;
    filterMode               = options.startInFilterMode;
    initialFocusedWindow     = Desktop::focusState()->window();
    initialFocusedWorkspace  = initialFocusedWindow && initialFocusedWindow->m_workspace ?
         initialFocusedWindow->m_workspace :
         (pMonitor ? pMonitor->m_activeWorkspace : nullptr);

    collectWindows();
    rebuildVisiblePreviews(false);

    lastMousePosLocal = g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position;
    selectedIndex     = hoveredIndex();
    if (selectedIndex < 0 && !previews.empty())
        selectedIndex = 0;

    auto onCursorMove = [this](Event::SCallbackInfo& info) {
        if (closing) {
            info.cancelled = true;
            return;
        }

        info.cancelled     = true;
        lastMousePosLocal  = g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position;
        const auto HOVERED = hoveredIndex();
        if (HOVERED >= 0)
            selectedIndex = HOVERED;
        damage();
    };

    mouseMoveHook = Event::bus()->m_events.input.mouse.move.listen(
        [onCursorMove](Vector2D, Event::SCallbackInfo& info) { onCursorMove(info); });
    touchMoveHook = Event::bus()->m_events.input.touch.motion.listen(
        [onCursorMove](const ITouch::SMotionEvent&, Event::SCallbackInfo& info) {
            onCursorMove(info);
        });
    touchDownHook = Event::bus()->m_events.input.touch.down.listen(
        [this](const ITouch::SDownEvent& event, Event::SCallbackInfo& info) {
            info.cancelled = true;
            selectAt(pMonitor->m_position + event.pos * pMonitor->m_size);
        });

    auto refreshWindows = [this] {
        windowsDirty = true;
        damage();
    };
    auto& events = Event::bus()->m_events;
    windowChangeHooks.push_back(events.window.open.listen(refreshWindows));
    windowChangeHooks.push_back(events.window.close.listen(refreshWindows));
    windowChangeHooks.push_back(events.window.title.listen(refreshWindows));
    windowChangeHooks.push_back(events.window.class_.listen(refreshWindows));
    windowChangeHooks.push_back(events.window.moveToWorkspace.listen(refreshWindows));
    windowChangeHooks.push_back(events.monitor.layoutChanged.listen(refreshWindows));

    Pointer::Cursor::overrideController->setOverride("left_ptr",
                                                     Pointer::Cursor::CURSOR_OVERRIDE_UNKNOWN);
    cursorOverrideSet = true;

    damage();
}

CWindowOverview::~CWindowOverview() {
    if (cursorOverrideSet)
        Pointer::Cursor::overrideController->unsetOverride(
            Pointer::Cursor::CURSOR_OVERRIDE_UNKNOWN);

    stopFilterDeleteRepeat();
    if (filterDeleteRepeatTimer && g_pEventLoopManager)
        g_pEventLoopManager->removeTimer(filterDeleteRepeatTimer);
    filterDeleteRepeatTimer.reset();

    allPreviews.clear();
    previews.clear();
    exitingPreviews.clear();
    framePreviews.clear();
}

void CWindowOverview::collectWindows() {
    allPreviews.clear();

    const auto  CURRENT_WORKSPACE = pMonitor ? pMonitor->m_activeWorkspace : nullptr;

    const auto& WINDOWS = Desktop::windowState()->windows();
    for (auto it = WINDOWS.rbegin(); it != WINDOWS.rend(); ++it) {
        const auto& window = *it;
        if (!previewableWindow(window))
            continue;

        if (!options.includeCurrentWorkspace && CURRENT_WORKSPACE &&
            window->m_workspace == CURRENT_WORKSPACE)
            continue;

        allPreviews.push_back({.window = window});
    }

    std::ranges::reverse(allPreviews);
    applyWindowOrdering(allPreviews);
    updateWorkspaceGrid();
}

void CWindowOverview::updateWorkspaceGrid() {
    int workspaceCount = 1;

    for (const auto& workspace : State::workspaceState()->workspaces()) {
        if (!workspace || workspace->m_isSpecialWorkspace || workspace->m_id <= 0)
            continue;

        workspaceCount = std::max(workspaceCount, (int)workspace->m_id);
    }

    for (const auto& preview : allPreviews) {
        if (!preview.window || !preview.window->m_workspace ||
            preview.window->m_workspace->m_isSpecialWorkspace ||
            preview.window->m_workspace->m_id <= 0)
            continue;

        workspaceCount = std::max(workspaceCount, (int)preview.window->m_workspace->m_id);
    }

    workspaceGridCount = workspaceCount;
    workspaceGridCols  = workspaceGridColsForCount(workspaceGridCount);
    workspaceGridRows  = workspaceGridRowsForCount(workspaceGridCount, workspaceGridCols);
}

void CWindowOverview::rebuildVisiblePreviews(bool animate) {
    auto OLD_PREVIEWS = previews;
    if (filterAnimating) {
        for (auto& preview : OLD_PREVIEWS)
            preview.tileLogical = filterTransitionTileLogicalBox(preview);
    }

    const auto OLD_SELECTED = selectedIndex >= 0 && selectedIndex < (int)previews.size() ?
        previews[selectedIndex].window :
        PHLWINDOW{};

    previews.clear();
    for (const auto& preview : allPreviews) {
        if (windowMatchesQuery(preview.window, filterQuery))
            previews.push_back(preview);
    }

    updateLayout();

    if (animate) {
        exitingPreviews.clear();

        for (const auto& oldPreview : OLD_PREVIEWS) {
            const bool STILL_VISIBLE = std::ranges::any_of(
                previews, [&oldPreview](const auto& p) { return p.window == oldPreview.window; });
            if (!STILL_VISIBLE) {
                auto exiting               = oldPreview;
                exiting.filterStartLogical = oldPreview.tileLogical;
                exitingPreviews.push_back(std::move(exiting));
            }
        }

        for (auto& preview : previews) {
            const auto OLD = std::ranges::find_if(
                OLD_PREVIEWS, [&preview](const auto& old) { return old.window == preview.window; });
            preview.filterStartLogical =
                OLD != OLD_PREVIEWS.end() ? OLD->tileLogical : preview.tileLogical;
        }

        filterAnimating          = true;
        filterAnimationStartedAt = Time::steadyNow();
    } else {
        exitingPreviews.clear();
        filterAnimating = false;
        for (auto& preview : previews)
            preview.filterStartLogical = preview.tileLogical;
    }

    selectedIndex = -1;
    if (OLD_SELECTED) {
        for (size_t i = 0; i < previews.size(); ++i) {
            if (previews[i].window == OLD_SELECTED) {
                selectedIndex = static_cast<int>(i);
                break;
            }
        }
    }

    if (selectedIndex < 0 && !previews.empty())
        selectedIndex = 0;

    if (selectedIndex >= 0)
        lastMousePosLocal = previews[selectedIndex].tileLogical.middle();
}

void CWindowOverview::updateLayout() {
    if (!pMonitor || previews.empty()) {
        gridCols = 1;
        return;
    }

    const double count  = static_cast<double>(previews.size());
    const double aspect = std::max(0.1, pMonitor->m_size.x / std::max(1.0, pMonitor->m_size.y));
    int          cols   = std::max(1, (int)std::ceil(std::sqrt(count * aspect)));
    int          rows   = std::max(1, (int)std::ceil(count / cols));

    while (cols > 1 && (cols - 1) * rows >= (int)previews.size())
        cols--;

    rows     = std::max(1, (int)std::ceil(count / cols));
    gridCols = cols;

    const double margin = static_cast<double>(std::max<Config::INTEGER>(0, *PMARGIN()));
    const double gap    = static_cast<double>(std::max<Config::INTEGER>(0, *PGAP()));
    const double areaW  = std::max(1.0, pMonitor->m_size.x - margin * 2.0);
    const double areaH  = std::max(1.0, pMonitor->m_size.y - margin * 2.0);
    const double cellW  = (areaW - gap * (cols - 1)) / cols;
    const double cellH  = (areaH - gap * (rows - 1)) / rows;

    for (size_t i = 0; i < previews.size(); ++i) {
        auto&        preview = previews[i];
        const auto   cell    = visualCellForPreviewIndex((int)i);
        const double row     = cell.first;
        const double col     = cell.second;
        const auto   winSize = preview.window->m_realSize->value();
        const double scale =
            std::min(cellW / std::max(1.0, winSize.x), cellH / std::max(1.0, winSize.y));
        const double w = std::max(1.0, winSize.x * scale);
        const double h = std::max(1.0, winSize.y * scale);
        const double x = margin + col * (cellW + gap) + (cellW - w) / 2.0;
        const double y = margin + row * (cellH + gap) + (cellH - h) / 2.0;

        preview.tileLogical = {x, y, w, h};
    }
}

int CWindowOverview::hoveredIndex() const {
    for (size_t i = 0; i < previews.size(); ++i) {
        if (previews[i].tileLogical.containsPoint(lastMousePosLocal))
            return static_cast<int>(i);
    }

    return -1;
}

void CWindowOverview::render() {
    if (closing && animationComplete()) {
        finishClose();
        return;
    }

    if (!pMonitor)
        return;

    if (windowsDirty && !closing) {
        windowsDirty = false;
        collectWindows();
        rebuildVisiblePreviews(false);
    }

    const double SCALE      = pMonitor->m_scale;
    const auto   ANIMATION  = overviewAnimation();
    const auto   VISIBLE    = animationVisibleAmount();
    const auto   BASE_SCALE = std::clamp<double>(*PANIMATIONSCALE(), 0.01, 1.0);

    framePreviews.clear();
    framePreviews.reserve(exitingPreviews.size() + previews.size());

    const auto FILTER_PROGRESS = filterTransitionVisibleAmount();
    for (auto& preview : exitingPreviews) {
        const auto EXIT_VISIBLE = VISIBLE * (1.0 - FILTER_PROGRESS);
        if (EXIT_VISIBLE <= 0.0 || !previewableWindow(preview.window))
            continue;

        CBox tilePx = scaleBoxFromCenter(preview.tileLogical, 0.96 + 0.04 * EXIT_VISIBLE)
                          .scale(SCALE)
                          .round();
        framePreviews.push_back({preview.window, tilePx, EXIT_VISIBLE, EXIT_VISIBLE, false,
                                 preview.tileLogical.w * SCALE});
    }

    for (size_t i = 0; i < previews.size(); ++i) {
        auto& preview = previews[i];
        if (!previewableWindow(preview.window))
            continue;

        const auto TILE_VISIBLE = tileAnimationVisibleAmount(i);
        const auto WINDOW_ALPHA = animatedTileTextureAlpha(i, TILE_VISIBLE);
        const auto TILE_SCALE =
            animationScalesTiles(ANIMATION) ? BASE_SCALE + (1.0 - BASE_SCALE) * TILE_VISIBLE : 1.0;
        CBox tilePx = scaleBoxFromCenter(animatedTileLogicalBox(i, TILE_VISIBLE), TILE_SCALE)
                          .scale(SCALE)
                          .round();
        framePreviews.push_back({preview.window, tilePx, TILE_VISIBLE, WINDOW_ALPHA,
                                 (int)i == selectedIndex, preview.tileLogical.w * SCALE});
    }

    g_pHyprRenderer->m_renderPass.add(makeUnique<CWinviewPassElement>(false));

    const auto MONITOR = pMonitor.lock();
    if (!MONITOR)
        return;
    const auto NOW = Time::steadyNow();
    for (const auto& preview : framePreviews) {
        CBox globalLogical = preview.tilePx.copy().scale(1.0 / SCALE);
        globalLogical      = globalLogical.translate(MONITOR->m_position);
        render_window_at_box(preview.window, MONITOR, NOW, globalLogical,
                             static_cast<float>(preview.windowAlpha));
    }

    g_pHyprRenderer->m_renderPass.add(makeUnique<CWinviewPassElement>(true));

    // Software cursors are queued before RENDER_LAST_MOMENT, so the overview passes
    // above would cover them; queue another cursor draw on top. No-op with hardware
    // cursors (renderSoftwareCursorsFor returns early when none are locked).
    if (g_pHyprRenderer->shouldRenderCursor())
        Pointer::mgr()->renderSoftwareCursorsFor(pMonitor.lock(), Time::steadyNow(),
                                                 g_pHyprRenderer->m_renderData.damage);

    // Windows on inactive workspaces do not necessarily damage this monitor when their
    // contents update. Keep the overview on the monitor's refresh loop so every tile is
    // genuinely live, matching the workspace overview behavior.
    damage();
}

void CWindowOverview::drawBackground() {
    if (!pMonitor)
        return;

    const auto VISIBLE    = animationVisibleAmount();
    const int  BORDER     = static_cast<int>(std::max<Config::INTEGER>(0, *PBORDERSIZE()));
    CRegion    fullDamage = {0, 0, INT16_MAX, INT16_MAX};

    Render::GL::g_pHyprOpenGL->renderRect(
        CBox{{0, 0}, pMonitor->m_transformedSize}, multiplyAlpha(activeBackgroundColor(), VISIBLE),
        {.damage = &fullDamage, .blur = backgroundBlurEnabled(), .blurA = (float)VISIBLE});

    if (BORDER <= 0)
        return;

    for (const auto& preview : framePreviews) {
        const auto COLOR = preview.selected ? CHyprColor(*PHOVERBORDER()) : CHyprColor(*PBORDER());
        Render::GL::g_pHyprOpenGL->renderRect(preview.tilePx.copy().expand(BORDER),
                                              multiplyAlpha(COLOR, preview.visible),
                                              {.damage = &fullDamage, .round = BORDER * 2});
    }
}

void CWindowOverview::drawForeground() {
    if (!pMonitor)
        return;

    const double SCALE      = pMonitor->m_scale;
    const auto   VISIBLE    = animationVisibleAmount();
    CRegion      fullDamage = {0, 0, INT16_MAX, INT16_MAX};

    labels.beginFrame();
    for (const auto& preview : framePreviews) {
        const auto& tilePx      = preview.tilePx;
        const auto  tileVisible = preview.visible;

        if (*PSHOWAPPICON() != 0) {
            const int ICON_SIZE_PX = appIconPixelSize(SCALE);
            if (const auto ICON = appIconTextureForWindow(preview.window, ICON_SIZE_PX)) {
                CBox      currentLogicalTile{tilePx.x / SCALE, tilePx.y / SCALE, tilePx.w / SCALE,
                                        tilePx.h / SCALE};
                CBox      iconBox = appIconBoxForTile(currentLogicalTile, SCALE);
                const int PADDING =
                    std::max(0,
                             (int)std::round(static_cast<double>(std::max<Config::INTEGER>(
                                                 0, *PAPPICONBACKPLATEPADDING())) *
                                             SCALE));
                if (PADDING > 0)
                    Render::GL::g_pHyprOpenGL->renderRect(
                        iconBox.copy().expand(PADDING).round(),
                        multiplyAlpha(CHyprColor(*PAPPICONBACKPLATE()), tileVisible),
                        {.damage = &fullDamage, .round = std::max(1, PADDING)});

                Render::GL::g_pHyprOpenGL->renderTexture(
                    ICON, iconBox, {.damage = &fullDamage, .a = (float)tileVisible});
            }
        }

        if (*PSHOWWINDOWTEXT() != 0) {
            const int PADDING =
                std::max(0, (int)std::round(static_cast<double>(*PWINDOWTEXTPADDING()) * SCALE));
            const int FONT_SIZE =
                std::max(1, (int)std::round(static_cast<double>(*PWINDOWTEXTSIZE()) * SCALE));
            const int  MAX_WIDTH = std::max(1, (int)std::round(preview.labelWidthPx - PADDING * 2));
            const auto TITLE     = windowTitle(preview.window);
            const auto CLASS     = windowClass(preview.window);
            std::vector<std::string> lines;
            if (!TITLE.empty())
                lines.push_back(TITLE);
            if (!CLASS.empty() && CLASS != TITLE)
                lines.push_back(CLASS);

            if (!lines.empty()) {
                const auto TEXT = labels.textureForLines(lines, FONT_SIZE, MAX_WIDTH,
                                                         CHyprColor(*PWINDOWTEXTCOLOR()),
                                                         configStringOr(PWINDOWTEXTFONT(), "Sans"));
                if (TEXT.texture) {
                    const auto LABEL_SCALE =
                        std::min(1.0, std::max(0.0, tilePx.w - PADDING * 2) / TEXT.size.x);
                    const auto WIDTH  = TEXT.size.x * LABEL_SCALE;
                    const auto HEIGHT = TEXT.size.y * LABEL_SCALE;
                    CBox       labelBox{
                        tilePx.x + PADDING,
                        std::max(tilePx.y + PADDING, tilePx.y + tilePx.h - HEIGHT - PADDING),
                        WIDTH,
                        HEIGHT,
                    };

                    Render::GL::g_pHyprOpenGL->renderRect(
                        labelBox.copy().expand(PADDING).round(),
                        multiplyAlpha(CHyprColor(*PWINDOWTEXTBACKPLATE()), tileVisible),
                        {.damage = &fullDamage, .round = std::max(1, PADDING)});
                    Render::GL::g_pHyprOpenGL->renderTexture(
                        TEXT.texture, labelBox, {.damage = &fullDamage, .a = (float)tileVisible});
                }
            }
        }
    }

    if (filterMode || !filterQuery.empty()) {
        const int PADDING = std::max(4, (int)std::round(8.0 * SCALE));
        const int FONT_SIZE =
            std::max(1, (int)std::round(static_cast<double>(*PWINDOWTEXTSIZE() + 2) * SCALE));
        const int MAX_WIDTH = std::max(1, (int)std::round(pMonitor->m_transformedSize.x * 0.72));
        std::vector<std::string> lines = {
            "Filter: " + filterQuery + (filterMode ? "_" : ""),
        };
        if (previews.empty() && !filterQuery.empty())
            lines.push_back("No matches");

        const auto TEXT =
            labels.textureForLines(lines, FONT_SIZE, MAX_WIDTH, CHyprColor(*PWINDOWTEXTCOLOR()),
                                   configStringOr(PWINDOWTEXTFONT(), "Sans"));
        if (TEXT.texture) {
            CBox promptBox{
                std::round((pMonitor->m_transformedSize.x - TEXT.size.x) / 2.0),
                std::round(static_cast<double>(std::max<Config::INTEGER>(0, *PMARGIN())) * SCALE /
                           2.0),
                TEXT.size.x,
                TEXT.size.y,
            };
            Render::GL::g_pHyprOpenGL->renderRect(
                promptBox.copy().expand(PADDING).round(),
                multiplyAlpha(CHyprColor(*PWINDOWTEXTBACKPLATE()), VISIBLE),
                {.damage = &fullDamage, .round = std::max(1, PADDING)});
            Render::GL::g_pHyprOpenGL->renderTexture(TEXT.texture, promptBox,
                                                     {.damage = &fullDamage, .a = (float)VISIBLE});
        }
    }

    labels.endFrame();

    if (filterAnimating && filterTransitionVisibleAmount() >= 1.0) {
        filterAnimating = false;
        exitingPreviews.clear();
    }
}

void CWindowOverview::damage() {
    if (pMonitor)
        g_pHyprRenderer->damageMonitor(pMonitor.lock());
}

void CWindowOverview::selectHoveredWindow() {
    selectedIndex = hoveredIndex();
}

void CWindowOverview::selectAt(const Vector2D& globalPos) {
    if (closing)
        return;

    lastMousePosLocal = globalPos - pMonitor->m_position;
    selectHoveredWindow();
    runDefaultSelected();
}

std::pair<int, int> CWindowOverview::visualCellForPreviewIndex(int index) const {
    const int cols     = std::max(1, gridCols);
    const int row      = index / cols;
    const int colInRow = index % cols;
    const int col      = row % 2 == 0 ? colInRow : cols - 1 - colInRow;

    return {row, col};
}

int CWindowOverview::previewIndexForVisualCell(int row, int col) const {
    const int cols = std::max(1, gridCols);

    if (row < 0 || col < 0 || col >= cols)
        return -1;

    const int colInRow = row % 2 == 0 ? col : cols - 1 - col;
    const int index    = row * cols + colInRow;

    if (index < 0 || index >= (int)previews.size())
        return -1;

    return index;
}

void CWindowOverview::moveSelection(int dx, int dy) {
    if (previews.empty())
        return;

    if (selectedIndex < 0 || selectedIndex >= (int)previews.size())
        selectedIndex = 0;

    const int  cols   = std::max(1, gridCols);
    const auto cell   = visualCellForPreviewIndex(selectedIndex);
    const int  row    = cell.first;
    const int  col    = cell.second;
    const int  newCol = std::clamp(col + dx, 0, cols - 1);
    const int  maxRow = ((int)previews.size() - 1) / cols;
    const int  newRow = std::clamp(row + dy, 0, maxRow);
    int        next   = previewIndexForVisualCell(newRow, newCol);

    if (next < 0)
        next = static_cast<int>(previews.size()) - 1;

    if (next != selectedIndex) {
        selectedIndex     = next;
        lastMousePosLocal = previews[selectedIndex].tileLogical.middle();
        damage();
    }
}

void CWindowOverview::runSelected(bool bring, bool replaceInitial) {
    close(true, bring, replaceInitial);
}

void CWindowOverview::runDefaultSelected() {
    switch (options.defaultAction) {
        case EWinviewDefaultAction::BRING_REPLACE: runSelected(true, true); return;
        case EWinviewDefaultAction::BRING: runSelected(true); return;
        case EWinviewDefaultAction::SELECT: runSelected(false); return;
    }
}

bool CWindowOverview::backgroundBlurEnabled() const {
    return *PBACKGROUNDBLUR() != 0;
}

bool CWindowOverview::occludesScene() const {
    return !isAnimating() && activeBackgroundColor().a >= 1.0;
}

bool CWindowOverview::deleteFilterCharacter() {
    auto query = filterQuery;
    if (!Winview::eraseLastCodepoint(query))
        return false;

    setFilterQuery(std::move(query));
    return true;
}

void CWindowOverview::startFilterDeleteRepeat() {
    filterDeleteHeld = true;

    if (!filterDeleteRepeatTimer) {
        filterDeleteRepeatTimer = makeShared<CEventLoopTimer>(
            std::nullopt,
            [this](const SP<CEventLoopTimer>& self, void*) {
                if (!filterDeleteHeld || closing)
                    return;

                deleteFilterCharacter();
                self->updateTimeout(std::chrono::milliseconds(35));
            },
            nullptr);

        if (g_pEventLoopManager)
            g_pEventLoopManager->addTimer(filterDeleteRepeatTimer);
    }

    filterDeleteRepeatTimer->updateTimeout(std::chrono::milliseconds(300));
}

void CWindowOverview::stopFilterDeleteRepeat() {
    filterDeleteHeld = false;
    if (filterDeleteRepeatTimer)
        filterDeleteRepeatTimer->updateTimeout(std::nullopt);
}

void CWindowOverview::toggleFilterMode() {
    stopFilterDeleteRepeat();
    filterMode = !filterMode;
    damage();
}

void CWindowOverview::setFilterQuery(std::string query, bool animate) {
    if (filterQuery == query)
        return;

    filterQuery = std::move(query);
    rebuildVisiblePreviews(animate);
    damage();
}

void CWindowOverview::focusWindow(const PHLWINDOW& window, bool bring, bool replaceInitial) {
    if (!window || !window->m_isMapped || !window->m_workspace)
        return;

    const auto FOCUSSTATE                  = Desktop::focusState();
    const auto MONITOR                     = FOCUSSTATE->monitor();
    const auto TARGET_WORKSPACE            = replaceInitial && initialFocusedWorkspace ?
                   initialFocusedWorkspace :
                   (MONITOR ? MONITOR->m_activeWorkspace : nullptr);
    const auto SELECTED_ORIGINAL_WORKSPACE = window->m_workspace;
    const auto SELECTED_TARGET             = window->layoutTarget();
    const auto INITIAL_TARGET =
        initialFocusedWindow ? initialFocusedWindow->layoutTarget() : SP<Layout::ITarget>{};
    const bool CAN_REPLACE_INITIAL = replaceInitial && g_layoutManager && initialFocusedWindow &&
        initialFocusedWindow != window && initialFocusedWindow->m_isMapped &&
        initialFocusedWindow->m_workspace && SELECTED_ORIGINAL_WORKSPACE && SELECTED_TARGET &&
        INITIAL_TARGET && !Fullscreen::controller()->isFullscreen(window) &&
        !Fullscreen::controller()->isFullscreen(initialFocusedWindow);

    if (CAN_REPLACE_INITIAL) {
        g_layoutManager->switchTargets(SELECTED_TARGET, INITIAL_TARGET, true);
    } else if (replaceInitial && initialFocusedWindow && initialFocusedWindow != window &&
               initialFocusedWindow->m_isMapped && initialFocusedWindow->m_workspace &&
               SELECTED_ORIGINAL_WORKSPACE &&
               initialFocusedWindow->m_workspace != SELECTED_ORIGINAL_WORKSPACE) {
        Desktop::globalWindowController()->moveWindowToWorkspace(initialFocusedWindow,
                                                                 SELECTED_ORIGINAL_WORKSPACE);
    }

    if ((bring || replaceInitial) && TARGET_WORKSPACE && window->m_workspace != TARGET_WORKSPACE) {
        Desktop::globalWindowController()->moveWindowToWorkspace(window, TARGET_WORKSPACE);
    }

    FOCUSSTATE->fullWindowFocus(window, Desktop::FOCUS_REASON_KEYBIND);
    Pointer::pointerController()->warpTo(window->middle());
}

void CWindowOverview::finishClose() {
    const auto MONITOR = pMonitor.lock();

    g_pWindowOverview.reset();

    if (MONITOR)
        g_pHyprRenderer->damageMonitor(MONITOR);
}

void CWindowOverview::close(bool focusSelection, bool bringSelection,
                            bool replaceInitialSelection) {
    if (closing)
        return;

    stopFilterDeleteRepeat();

    if (cursorOverrideSet) {
        Pointer::Cursor::overrideController->unsetOverride(
            Pointer::Cursor::CURSOR_OVERRIDE_UNKNOWN);
        cursorOverrideSet = false;
    }

    closing            = true;
    animationStartedAt = Time::steadyNow();

    PHLWINDOW selectedWindow;
    if (focusSelection && selectedIndex >= 0 && selectedIndex < (int)previews.size())
        selectedWindow = previews[selectedIndex].window;

    if (selectedWindow)
        focusWindow(selectedWindow, bringSelection || recoverableWindow(selectedWindow),
                    replaceInitialSelection);

    damage();
}

bool windowOverviewActive() {
    return g_pWindowOverview != nullptr;
}

void closeWindowOverviewImmediately() {
    const auto MONITOR = g_pWindowOverview ? g_pWindowOverview->pMonitor.lock() : nullptr;
    g_pWindowOverview.reset();

    if (g_pHyprRenderer)
        g_pHyprRenderer->m_renderPass.removeAllOfType("CWinviewPassElement");
    if (MONITOR && g_pHyprRenderer)
        g_pHyprRenderer->damageMonitor(MONITOR);
}
