#pragma once

#define WLR_USE_UNSTABLE

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/render/Framebuffer.hpp>
#include <vector>

class CWindowOverview {
  public:
    explicit CWindowOverview(PHLMONITOR monitor);
    ~CWindowOverview();

    void render();
    void draw();
    void damage();
    void close(bool focusSelection = false, bool bringSelection = false);
    void selectHoveredWindow();

    PHLMONITORREF pMonitor;

  private:
    struct SWindowPreview {
        PHLWINDOW                 window;
        SP<Render::IFramebuffer>  fb;
        CBox                     tileLogical;
    };

    void                  collectWindows();
    void                  renderSnapshots();
    void                  updateLayout();
    int                   hoveredIndex() const;
    void                  focusWindow(PHLWINDOW window, bool bring);

    std::vector<SWindowPreview> previews;
    Vector2D                    lastMousePosLocal;
    int                         selectedIndex = -1;
    bool                        closing       = false;

    CHyprSignalListener         mouseMoveHook;
    CHyprSignalListener         mouseButtonHook;
    CHyprSignalListener         touchMoveHook;
    CHyprSignalListener         touchDownHook;
};

inline std::unique_ptr<CWindowOverview> g_pWindowOverview;
