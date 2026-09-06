#pragma once

#include <hyprland/src/plugins/HookSystem.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/view/View.hpp>
#include <hyprutils/math/Box.hpp>

#include <optional>

#include "manager.hpp"

inline constexpr auto LOG = Hyprutils::CLI::LOG_DEBUG;
inline constexpr auto ERR = Hyprutils::CLI::LOG_ERR;

inline HANDLE         PHANDLE = nullptr;

inline CFunctionHook* render_workspace_hook     = nullptr;
inline CFunctionHook* render_texture_hook       = nullptr;
inline CFunctionHook* render_border_hook        = nullptr;
inline CFunctionHook* render_border2_hook       = nullptr;
inline CFunctionHook* blur_optimizations_hook   = nullptr;
inline CFunctionHook* should_render_window_hook = nullptr;
inline CFunctionHook* is_solitary_blocked_hook  = nullptr;
typedef uint32_t (*origIsSolitaryBlocked)(void*, bool);
inline void* render_window = nullptr;

// Set by explicit pass elements around scaled overview content.  The render hooks run
// much later than the code which builds the render pass, so checking whether an overview
// is merely open is too broad: native workspace animations may also use renderModif in
// the same pass.  This marker keeps the fixes below scoped to content owned by us.
inline bool                       overview_scaled_render = false;
inline std::optional<CBox>        overview_scaled_clip;
inline float                      overview_scaled_alpha = 1.F;

inline std::unique_ptr<HTManager> ht_manager;

template <typename... Args>
inline void fail_exit(const std::format_string<Args...>& fmt, Args... args) {
    std::string err_string =
        "[Hyprtasking] " + std::vformat(fmt.get(), std::make_format_args(args...));

    HyprlandAPI::addNotification(PHANDLE, err_string, CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
    throw std::runtime_error(err_string);
}
