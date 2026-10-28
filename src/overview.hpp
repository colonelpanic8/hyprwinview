#ifndef HYPRWINVIEW_OVERVIEW_HPP
#define HYPRWINVIEW_OVERVIEW_HPP

#define WLR_USE_UNSTABLE

#include "overview/command.hpp"
#include "overview/keys.hpp"
#include "overview/labels.hpp"

#include <chrono>
#include <cstdint>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <xkbcommon/xkbcommon.h>

class CEventLoopTimer;
namespace Render {
    class CRenderContext;
}

class CWindowOverview {
  public:
    explicit CWindowOverview(const PHLMONITOR& monitor, SWindowOverviewOptions options = {});
    ~CWindowOverview();

    void          render(Render::CRenderContext& ctx);
    void          drawBackground(Render::CRenderContext& ctx);
    void          drawForeground(Render::CRenderContext& ctx);
    void          damage();
    void          close(bool focusSelection = false, bool bringSelection = false,
                        bool replaceInitialSelection = false);
    void          selectHoveredWindow();
    void          selectAt(const Vector2D& globalPos);
    void          toggleFilterMode();
    bool          isAnimating() const;
    bool          backgroundBlurEnabled() const;
    bool          occludesScene() const;
    bool          isClosing() const;

    PHLMONITORREF pMonitor;

  private:
    struct SWindowPreview {
        PHLWINDOW window;
        CBox      tileLogical;
        CBox      filterStartLogical;
    };

    struct SFramePreview {
        PHLWINDOW window;
        CBox      tilePx;
        double    visible      = 1.0;
        double    windowAlpha  = 1.0;
        bool      selected     = false;
        double    labelWidthPx = 0.0;
    };

    void                collectWindows();
    void                applyWindowOrdering(std::vector<SWindowPreview>& windowPreviews);
    void                rebuildVisiblePreviews(bool animate);
    void                updateWorkspaceGrid();
    void                updateLayout();
    int                 hoveredIndex() const;
    void                focusWindow(const PHLWINDOW& window, bool bring, bool replaceInitial);
    void                moveSelection(int dx, int dy);
    void                runSelected(bool bring, bool replaceInitial = false);
    void                runDefaultSelected();
    bool                handleKeyPress(const IKeyboard::SKeyEvent& event);
    bool                deleteFilterCharacter();
    void                startFilterDeleteRepeat();
    void                stopFilterDeleteRepeat();
    void                setFilterQuery(std::string query, bool animate = true);
    void                finishClose();
    double              animationVisibleAmount() const;
    double              tileAnimationVisibleAmount(size_t index) const;
    double              tileAnimationDelayMs(size_t index) const;
    double              maxTileAnimationDelayMs() const;
    std::pair<int, int> visualCellForPreviewIndex(int index) const;
    int                 previewIndexForVisualCell(int row, int col) const;
    int                 workspacePanelIndexForWorkspace(const PHLWORKSPACE& workspace) const;
    CBox                workspacePanelCellLogical(int index) const;
    CBox                workspacePanelBoxForPreview(const SWindowPreview& preview) const;
    CBox   workspaceZoomCameraBoxForPanelBox(const CBox& panelBox, double cameraProgress) const;
    CBox   animatedTileLogicalBox(size_t index, double progress) const;
    double animatedTileTextureAlpha(size_t index, double progress) const;
    CBox   filterTransitionTileLogicalBox(const SWindowPreview& preview) const;
    double filterTransitionVisibleAmount() const;
    bool   animationComplete() const;

    COverviewLabels                       labels;
    std::optional<Winview::SKeyBindings>  keyBindings;
    uint64_t                              keyBindingsGeneration = 0;
    std::vector<SWindowPreview>           allPreviews;
    std::vector<SWindowPreview>           previews;
    std::vector<SWindowPreview>           exitingPreviews;
    std::vector<SFramePreview>            framePreviews;
    SWindowOverviewOptions                options;
    PHLWINDOW                             initialFocusedWindow;
    PHLWORKSPACE                          initialFocusedWorkspace;
    Vector2D                              lastMousePosLocal;
    std::string                           filterQuery;
    int                                   selectedIndex      = -1;
    int                                   gridCols           = 1;
    int                                   workspaceGridCols  = 1;
    int                                   workspaceGridRows  = 1;
    int                                   workspaceGridCount = 1;
    bool                                  closing            = false;
    bool                                  filterMode         = false;
    bool                                  filterAnimating    = false;
    bool                                  filterDeleteHeld   = false;
    bool                                  cursorOverrideSet  = false;
    bool                                  windowsDirty       = false;
    std::chrono::steady_clock::time_point animationStartedAt;
    std::chrono::steady_clock::time_point filterAnimationStartedAt;

    SP<CEventLoopTimer>                   filterDeleteRepeatTimer;
    CHyprSignalListener                   mouseMoveHook;
    CHyprSignalListener                   touchMoveHook;
    CHyprSignalListener                   touchDownHook;
    std::vector<CHyprSignalListener>      windowChangeHooks;

    friend void                           handleWindowOverviewKey(const IKeyboard::SKeyEvent& event,
                                                                  Event::SCallbackInfo&       info);
};

inline std::unique_ptr<CWindowOverview> g_pWindowOverview;

bool                                    windowOverviewActive();
void                                    closeWindowOverviewImmediately();

// Plugin-lifetime input routing; consumed releases are still swallowed after the overview closes.
void handleWindowOverviewKey(const IKeyboard::SKeyEvent& event, Event::SCallbackInfo& info);
void handleWindowOverviewButton(const IPointer::SButtonEvent& event, Event::SCallbackInfo& info);
void resetWindowOverviewInput();

#endif
