#pragma once

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/devices/IKeyboard.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprland/src/render/Texture.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "overview.hpp"

class HTManager {
  public:
    HTManager();

    std::vector<PHTVIEW> views;

    PHTVIEW              get_view_from_monitor(PHLMONITOR pMonitor);
    PHTVIEW              get_view_from_cursor();
    PHTVIEW              get_view_from_id(VIEWID view_id);

    PHLWINDOW            get_window_from_cursor(bool return_focused = true);

    void                 reset();

    void                 show_all_views();
    void                 hide_all_views();
    void                 hide_all_views_immediately();
    void                 show_cursor_view();

    void                 register_monitors();
    void                 refresh_all_grid_caches(bool enforce_rules = true);
    void                 mark_grid_caches_dirty();
    void                 refresh_grid_caches_if_dirty(bool enforce_rules = true);
    void                 remove_view_for_monitor_id(MONITORID mid);

    bool                 start_window_drag();
    bool                 end_window_drag();
    bool                 exit_to_workspace();
    bool                 on_key(IKeyboard::SKeyEvent event);
    bool                 on_mouse_move();
    bool                 on_mouse_axis(double delta);

    enum swipe_state_t {
        HT_SWIPE_OPEN,
        HT_SWIPE_MOVE,
        HT_SWIPE_NONE,
    };

    swipe_state_t                swipe_state;
    float                        swipe_amt;
    std::unordered_set<uint32_t> jump_pressed_keys;
    void                         swipe_start();
    bool                         swipe_update(IPointer::SSwipeUpdateEvent e);
    bool                         swipe_end();

    bool                         has_active_view();
    bool                         cursor_view_active();

    // Rasterized jump labels, owned here so the GL textures are released at plugin exit.
    std::unordered_map<std::string, SP<Render::ITexture>> jump_label_textures;

  private:
    bool grid_caches_dirty = false;
};
