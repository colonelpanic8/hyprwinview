#include "../overview.hpp"
#include "animation.hpp"
#include "../settings.hpp"
#include "../config_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <hyprland/src/helpers/time/Time.hpp>

using namespace WinviewConfig;
using Winview::configStringOr;
using Winview::trimmedLower;

static EOverviewAnimation parseAnimation(const std::string& name) {
    const auto NAME = trimmedLower(name);

    if (NAME == "none" || NAME == "off" || NAME == "disable" || NAME == "disabled")
        return EOverviewAnimation::NONE;
    if (NAME == "fade")
        return EOverviewAnimation::FADE;
    if (NAME == "stagger" || NAME == "staggered" || NAME == "staggered_fade_scale")
        return EOverviewAnimation::STAGGERED_FADE_SCALE;
    if (NAME == "workspace_zoom" || NAME == "workspace-zoom" || NAME == "expo" ||
        NAME == "hyprexpo")
        return EOverviewAnimation::WORKSPACE_ZOOM;

    return EOverviewAnimation::FADE_SCALE;
}

EOverviewAnimation overviewAnimation() {
    static std::string        previousName;
    static EOverviewAnimation animation = EOverviewAnimation::WORKSPACE_ZOOM;
    const auto                name      = configStringOr(PANIMATION(), PANIMATIONDefault);
    if (name != previousName) {
        previousName = name;
        animation    = parseAnimation(name);
    }
    return animation;
}

bool animationScalesTiles(EOverviewAnimation animation) {
    return animation == EOverviewAnimation::FADE_SCALE ||
        animation == EOverviewAnimation::STAGGERED_FADE_SCALE;
}

static bool animationStaggersTiles(EOverviewAnimation animation) {
    return animation == EOverviewAnimation::STAGGERED_FADE_SCALE;
}

static bool animationUsesWorkspaceZoom(EOverviewAnimation animation) {
    return animation == EOverviewAnimation::WORKSPACE_ZOOM;
}

static double workspaceZoomStageRatio() {
    return std::clamp<double>(*PANIMATIONWORKSPACEZOOMSTAGERATIO(), 0.1, 0.9);
}

static double animationDurationMs(bool closing) {
    const double SPEED = std::clamp<double>(*PANIMATIONSPEED(), 0.1, 10.0);
    return static_cast<double>(
               std::max<Config::INTEGER>(0, closing ? *PANIMATIONOUTMS() : *PANIMATIONINMS())) /
        SPEED;
}

static double easeOutCubic(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return 1.0 - std::pow(1.0 - t, 3.0);
}

static double easeInCubic(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return t * t * t;
}

static double visibleAmountForElapsed(double elapsedMs, double durationMs, bool closing) {
    if (durationMs <= 0.0)
        return closing ? 0.0 : 1.0;

    const auto T = std::clamp(elapsedMs / durationMs, 0.0, 1.0);
    return closing ? 1.0 - easeInCubic(T) : easeOutCubic(T);
}

static double rawProgressForVisibleAmount(double visible, bool closing) {
    visible = std::clamp(visible, 0.0, 1.0);

    if (closing)
        return std::cbrt(1.0 - visible);

    return 1.0 - std::cbrt(1.0 - visible);
}

static double lerpDouble(double from, double to, double progress) {
    return from + (to - from) * std::clamp(progress, 0.0, 1.0);
}

static CBox lerpBox(const CBox& from, const CBox& to, double progress) {
    return {
        lerpDouble(from.x, to.x, progress),
        lerpDouble(from.y, to.y, progress),
        lerpDouble(from.w, to.w, progress),
        lerpDouble(from.h, to.h, progress),
    };
}

double CWindowOverview::animationVisibleAmount() const {
    const auto ANIMATION = overviewAnimation();
    if (ANIMATION == EOverviewAnimation::NONE)
        return 1.0;

    const auto DURATION =
        animationDurationMs(closing) + (closing ? maxTileAnimationDelayMs() : 0.0);
    const auto ELAPSED =
        std::chrono::duration<double, std::milli>(Time::steadyNow() - animationStartedAt).count();

    return visibleAmountForElapsed(ELAPSED, DURATION, closing);
}

double CWindowOverview::tileAnimationVisibleAmount(size_t index) const {
    const auto ANIMATION = overviewAnimation();
    if (ANIMATION == EOverviewAnimation::NONE)
        return 1.0;

    const auto DURATION = animationDurationMs(closing);
    const auto ELAPSED =
        std::chrono::duration<double, std::milli>(Time::steadyNow() - animationStartedAt).count() -
        tileAnimationDelayMs(index);

    return visibleAmountForElapsed(ELAPSED, DURATION, closing);
}

double CWindowOverview::tileAnimationDelayMs(size_t index) const {
    const auto ANIMATION = overviewAnimation();
    if (!animationStaggersTiles(ANIMATION) || previews.empty())
        return 0.0;

    const auto STAGGER_MS     = std::max<Config::INTEGER>(0, *PANIMATIONSTAGGERMS());
    const auto MAX_STAGGER_MS = std::max<Config::INTEGER>(0, *PANIMATIONSTAGGERMAXMS());
    if (STAGGER_MS <= 0 || MAX_STAGGER_MS <= 0)
        return 0.0;

    const size_t ORDER_INDEX =
        closing ? previews.size() - 1 - std::min(index, previews.size() - 1) : index;
    return std::min<double>(static_cast<double>(ORDER_INDEX) * static_cast<double>(STAGGER_MS),
                            static_cast<double>(MAX_STAGGER_MS));
}

double CWindowOverview::maxTileAnimationDelayMs() const {
    const auto ANIMATION = overviewAnimation();
    if (!animationStaggersTiles(ANIMATION) || previews.empty())
        return 0.0;

    const auto STAGGER_MS     = std::max<Config::INTEGER>(0, *PANIMATIONSTAGGERMS());
    const auto MAX_STAGGER_MS = std::max<Config::INTEGER>(0, *PANIMATIONSTAGGERMAXMS());
    if (STAGGER_MS <= 0 || MAX_STAGGER_MS <= 0)
        return 0.0;

    return std::min<double>(static_cast<double>(previews.size() - 1) *
                                static_cast<double>(STAGGER_MS),
                            static_cast<double>(MAX_STAGGER_MS));
}

int CWindowOverview::workspacePanelIndexForWorkspace(const PHLWORKSPACE& workspace) const {
    if (workspace && !workspace->m_isSpecialWorkspace && workspace->m_id > 0)
        return std::clamp((int)workspace->m_id - 1, 0, std::max(0, workspaceGridCount - 1));

    return std::clamp(workspaceGridCount / 2, 0, std::max(0, workspaceGridCount - 1));
}

CBox CWindowOverview::workspacePanelCellLogical(int index) const {
    if (!pMonitor)
        return {};

    const int count = std::max(1, workspaceGridCount);
    const int cols  = std::max(1, workspaceGridCols);
    const int rows  = std::max(1, workspaceGridRows);

    index = std::clamp(index, 0, count - 1);

    const double margin = static_cast<double>(std::max<Config::INTEGER>(0, *PMARGIN()));
    const double gap =
        static_cast<double>(std::max<Config::INTEGER>(0, *PANIMATIONWORKSPACEZOOMGAP()));
    const double areaW = std::max(1.0, pMonitor->m_size.x - margin * 2.0);
    const double areaH = std::max(1.0, pMonitor->m_size.y - margin * 2.0);
    const double cellW = std::max(1.0, (areaW - gap * (cols - 1)) / cols);
    const double cellH = std::max(1.0, (areaH - gap * (rows - 1)) / rows);
    const int    col   = index % cols;
    const int    row   = index / cols;

    return {margin + col * (cellW + gap), margin + row * (cellH + gap), cellW, cellH};
}

CBox CWindowOverview::workspacePanelBoxForPreview(const SWindowPreview& preview) const {
    if (!preview.window || !pMonitor)
        return {};

    const auto SOURCE_MONITOR =
        preview.window->m_monitor.lock() ? preview.window->m_monitor.lock() : pMonitor.lock();
    if (!SOURCE_MONITOR)
        return preview.tileLogical;

    const auto panelIndex = workspacePanelIndexForWorkspace(preview.window->m_workspace);
    const auto CELL       = workspacePanelCellLogical(panelIndex);
    const auto WIN_POS    = preview.window->positionAnimation()->value();
    const auto WIN_SIZE   = preview.window->sizeAnimation()->value();
    const auto MONITOR_W  = std::max(1.0, SOURCE_MONITOR->m_size.x);
    const auto MONITOR_H  = std::max(1.0, SOURCE_MONITOR->m_size.y);
    const auto LOCAL_X    = (WIN_POS.x - SOURCE_MONITOR->m_position.x) / MONITOR_W;
    const auto LOCAL_Y    = (WIN_POS.y - SOURCE_MONITOR->m_position.y) / MONITOR_H;
    const auto LOCAL_W    = WIN_SIZE.x / MONITOR_W;
    const auto LOCAL_H    = WIN_SIZE.y / MONITOR_H;

    return {
        CELL.x + LOCAL_X * CELL.w,
        CELL.y + LOCAL_Y * CELL.h,
        std::max(1.0, LOCAL_W * CELL.w),
        std::max(1.0, LOCAL_H * CELL.h),
    };
}

CBox CWindowOverview::workspaceZoomCameraBoxForPanelBox(const CBox& panelBox,
                                                        double      cameraProgress) const {
    if (!pMonitor)
        return panelBox;

    const auto START_WORKSPACE = initialFocusedWorkspace ?
        initialFocusedWorkspace :
        (pMonitor ? pMonitor->m_activeWorkspace : nullptr);
    const auto START_CELL =
        workspacePanelCellLogical(workspacePanelIndexForWorkspace(START_WORKSPACE));
    const auto   PROGRESS = std::clamp(cameraProgress, 0.0, 1.0);
    const auto   VIEWPORT = CBox{0, 0, pMonitor->m_size.x, pMonitor->m_size.y};

    const double startScaleX = VIEWPORT.w / std::max(1.0, START_CELL.w);
    const double startScaleY = VIEWPORT.h / std::max(1.0, START_CELL.h);
    const double startX      = VIEWPORT.x - START_CELL.x * startScaleX;
    const double startY      = VIEWPORT.y - START_CELL.y * startScaleY;
    const double scaleX      = lerpDouble(startScaleX, 1.0, PROGRESS);
    const double scaleY      = lerpDouble(startScaleY, 1.0, PROGRESS);
    const double x           = lerpDouble(startX, 0.0, PROGRESS);
    const double y           = lerpDouble(startY, 0.0, PROGRESS);

    return {
        panelBox.x * scaleX + x,
        panelBox.y * scaleY + y,
        panelBox.w * scaleX,
        panelBox.h * scaleY,
    };
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
CBox CWindowOverview::animatedTileLogicalBox(size_t index, double progress) const {
    if (index >= previews.size())
        return {};

    if (!animationUsesWorkspaceZoom(overviewAnimation()))
        return filterTransitionTileLogicalBox(previews[index]);

    const auto& PREVIEW = previews[index];
    const auto  SPLIT   = workspaceZoomStageRatio();
    const auto  PANEL   = workspacePanelBoxForPreview(PREVIEW);
    const auto  FINAL   = filterTransitionTileLogicalBox(PREVIEW);
    const auto  RAW     = rawProgressForVisibleAmount(progress, closing);

    if (closing) {
        const auto GATHER_DURATION = 1.0 - SPLIT;
        if (RAW <= GATHER_DURATION)
            return lerpBox(FINAL, PANEL, easeInCubic(RAW / GATHER_DURATION));

        return workspaceZoomCameraBoxForPanelBox(
            PANEL, 1.0 - easeInCubic((RAW - GATHER_DURATION) / SPLIT));
    }

    if (RAW <= SPLIT)
        return workspaceZoomCameraBoxForPanelBox(PANEL, easeOutCubic(RAW / SPLIT));

    return lerpBox(PANEL, FINAL, easeOutCubic((RAW - SPLIT) / (1.0 - SPLIT)));
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
double CWindowOverview::animatedTileTextureAlpha(size_t index, double progress) const {
    if (index >= previews.size())
        return 0.0;

    if (!animationUsesWorkspaceZoom(overviewAnimation()))
        return progress;

    return 1.0;
}

CBox CWindowOverview::filterTransitionTileLogicalBox(const SWindowPreview& preview) const {
    if (closing || !filterAnimating)
        return preview.tileLogical;

    return lerpBox(preview.filterStartLogical, preview.tileLogical,
                   filterTransitionVisibleAmount());
}

double CWindowOverview::filterTransitionVisibleAmount() const {
    if (!filterAnimating)
        return 1.0;

    const double DURATION =
        static_cast<double>(std::max<Config::INTEGER>(0, *PFILTERANIMATIONMS()));
    if (DURATION <= 0.0)
        return 1.0;

    const auto ELAPSED =
        std::chrono::duration<double, std::milli>(Time::steadyNow() - filterAnimationStartedAt)
            .count();
    return easeOutCubic(ELAPSED / DURATION);
}

bool CWindowOverview::animationComplete() const {
    const auto ANIMATION = overviewAnimation();
    if (ANIMATION == EOverviewAnimation::NONE)
        return true;

    const auto DURATION = animationDurationMs(closing);
    if (DURATION <= 0.0)
        return true;

    const auto ELAPSED =
        std::chrono::duration<double, std::milli>(Time::steadyNow() - animationStartedAt).count();
    const auto TOTAL_TIME_MS = DURATION + maxTileAnimationDelayMs();
    return ELAPSED >= TOTAL_TIME_MS;
}

bool CWindowOverview::isAnimating() const {
    return !animationComplete() || (filterAnimating && filterTransitionVisibleAmount() < 1.0);
}
