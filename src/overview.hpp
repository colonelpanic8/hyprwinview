#pragma once

#define WLR_USE_UNSTABLE

#include <chrono>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/render/Framebuffer.hpp>
#include <string>
#include <vector>

struct SWinviewKeyConfig {
    std::vector<std::string> left;
    std::vector<std::string> right;
    std::vector<std::string> up;
    std::vector<std::string> down;
    std::vector<std::string> go;
    std::vector<std::string> bring;
    std::vector<std::string> bringReplace;
    std::vector<std::string> close;
};

struct SWindowOverviewOptions {
    bool includeCurrentWorkspace = true;
};

SWinviewKeyConfig defaultWinviewKeyConfig();
void              setWinviewKeyConfig(SWinviewKeyConfig config);

class CWindowOverview {
  public:
    explicit CWindowOverview(PHLMONITOR monitor, SWindowOverviewOptions options = {});
    ~CWindowOverview();

    void render();
    void draw();
    void damage();
    void close(bool focusSelection = false, bool bringSelection = false, bool replaceInitialSelection = false);
    void selectHoveredWindow();
    bool isAnimating() const;
    bool backgroundBlurEnabled() const;
    bool occludesScene() const;

    PHLMONITORREF pMonitor;

  private:
    struct SWindowPreview {
        PHLWINDOW                 window;
        SP<Render::IFramebuffer>  fb;
        CBox                     tileLogical;
        std::string              orderGroupKey;
        size_t                   orderOriginalIndex = 0;
        size_t                   orderGroupIndex    = 0;
    };

    void                  collectWindows();
    void                  applyWindowOrdering();
    void                  updateWorkspaceGrid();
    void                  renderSnapshots();
    void                  updateLayout();
    int                   hoveredIndex() const;
    void                  focusWindow(PHLWINDOW window, bool bring, bool replaceInitial);
    void                  moveSelection(int dx, int dy);
    void                  runSelected(bool bring, bool replaceInitial = false);
    bool                  handleKey(const IKeyboard::SKeyEvent& event);
    void                  finishClose();
    double                animationVisibleAmount() const;
    double                tileAnimationVisibleAmount(size_t index) const;
    double                tileAnimationDelayMs(size_t index) const;
    double                maxTileAnimationDelayMs() const;
    int                   workspacePanelIndexForWorkspace(PHLWORKSPACE workspace) const;
    CBox                  workspacePanelCellLogical(int index) const;
    CBox                  workspacePanelBoxForPreview(const SWindowPreview& preview) const;
    CBox                  workspaceZoomCameraBoxForPanelBox(const CBox& panelBox, double cameraProgress) const;
    CBox                  animatedTileLogicalBox(size_t index, double progress) const;
    double                animatedTileTextureAlpha(size_t index, double progress) const;
    bool                  animationComplete() const;

    std::vector<SWindowPreview> previews;
    SWindowOverviewOptions      options;
    PHLWINDOW                   initialFocusedWindow;
    PHLWORKSPACE                initialFocusedWorkspace;
    Vector2D                    lastMousePosLocal;
    int                         selectedIndex = -1;
    int                         gridCols      = 1;
    int                         workspaceGridCols  = 1;
    int                         workspaceGridRows  = 1;
    int                         workspaceGridCount = 1;
    bool                        closing       = false;
    std::chrono::steady_clock::time_point animationStartedAt;

    CHyprSignalListener         mouseMoveHook;
    CHyprSignalListener         mouseButtonHook;
    CHyprSignalListener         keyboardHook;
    CHyprSignalListener         touchMoveHook;
    CHyprSignalListener         touchDownHook;
};

inline std::unique_ptr<CWindowOverview> g_pWindowOverview;
