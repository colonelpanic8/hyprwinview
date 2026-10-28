#pragma once

#include <charconv>
#include <cstdint>
#include <string>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/shared/workspace/WorkspaceRule.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/state/WorkspaceState.hpp>
#include <hyprland/src/workspace/HLWorkspace.hpp>
#include <hyprutils/math/Box.hpp>

using namespace Render;

typedef void (*render_workspace_t)(void* thisptr, CRenderContext& ctx, PHLMONITOR pMonitor,
                                   PHLWORKSPACE pWorkspace, const Time::steady_tp& now,
                                   const CBox& geometry);

typedef bool (*should_render_window_t)(void* thisptr, PHLWINDOW pWindow, PHLMONITOR pMonitor);
typedef void (*render_window_t)(void* thisptr, CRenderContext& ctx, PHLWINDOW pWindow,
                                PHLMONITOR pMonitor, const SWindowRenderPresentation& presentation,
                                const Time::steady_tp& time, bool decorate, eRenderPassMode mode,
                                bool ignorePosition, bool standalone);

typedef long VIEWID;

// Grid slots are keyed by numbered workspace IDs. Named and special workspaces have no number
// and map to WORKSPACE_INVALID.
using WORKSPACEID                              = int64_t;
inline constexpr WORKSPACEID WORKSPACE_INVALID = -1;

namespace HT {
    inline WORKSPACEID workspace_id(const PHLWORKSPACE& workspace) {
        if (workspace == nullptr)
            return WORKSPACE_INVALID;
        const auto NUMBERED = workspace->numberedID();
        return NUMBERED ? static_cast<WORKSPACEID>(*NUMBERED) : WORKSPACE_INVALID;
    }

    inline bool valid_workspace_id(WORKSPACEID id) {
        return id > 0 && id <= UINT32_MAX;
    }

    inline PHLWORKSPACE find_workspace(WORKSPACEID id) {
        if (!valid_workspace_id(id))
            return nullptr;
        return State::workspaceState()
            ->query()
            .numbered(
                Workspace::SWorkspaceNumberedID{static_cast<Workspace::WorkspaceIDContainer>(id)})
            .run();
    }

    inline PHLWORKSPACE create_workspace(WORKSPACEID id, const PHLMONITOR& monitor) {
        if (!valid_workspace_id(id))
            return nullptr;
        return State::workspaceState()->createNumbered(
            Workspace::SWorkspaceNumberedID{static_cast<Workspace::WorkspaceIDContainer>(id)},
            monitor);
    }

    // Numbered workspace a rule targets, or WORKSPACE_INVALID for selectors and named rules.
    inline WORKSPACEID rule_workspace_id(const Config::CWorkspaceRule& rule) {
        const auto& SELECTOR = rule.m_workspaceString;
        WORKSPACEID id       = WORKSPACE_INVALID;
        const auto [END, EC] =
            std::from_chars(SELECTOR.data(), SELECTOR.data() + SELECTOR.size(), id);
        if (EC != std::errc{} || END != SELECTOR.data() + SELECTOR.size() ||
            !valid_workspace_id(id))
            return WORKSPACE_INVALID;
        return id;
    }
}
