#include "dispatcher.hpp"

#include <hyprland/src/desktop/state/FocusState.hpp>

#include <memory>

#include "overview.hpp"
#include "workspace/module.hpp"

namespace {
    // An overview that is animating closed no longer takes commands; toggling or opening
    // replaces it instead.
    CWindowOverview* interactiveOverview() {
        return g_pWindowOverview && !g_pWindowOverview->isClosing() ? g_pWindowOverview.get() :
                                                                      nullptr;
    }

    SDispatchResult openOverview(const SWindowOverviewOptions& options) {
        const auto MONITOR = Desktop::focusState()->monitor();
        if (!MONITOR)
            return {.success = false, .error = "no focused monitor"};

        closeWorkspaceOverviewImmediately();
        closeWindowOverviewImmediately();
        g_pWindowOverview = std::make_unique<CWindowOverview>(MONITOR, options);
        return {};
    }

    void closeWithSelection(bool bring, bool replaceInitial) {
        if (auto* overview = interactiveOverview()) {
            overview->selectHoveredWindow();
            overview->close(true, bring, replaceInitial);
        }
    }
}

SDispatchResult runWinviewCommand(const Winview::SOverviewCommand& command) {
    using Winview::EOverviewAction;

    switch (command.action) {
        case EOverviewAction::SELECT: closeWithSelection(false, false); return {};
        case EOverviewAction::BRING: closeWithSelection(true, false); return {};
        case EOverviewAction::BRING_REPLACE: closeWithSelection(true, true); return {};
        case EOverviewAction::CLOSE:
            if (auto* overview = interactiveOverview())
                overview->close(false);
            return {};
        case EOverviewAction::TOGGLE:
            if (auto* overview = interactiveOverview()) {
                overview->close(false);
                return {};
            }
            return openOverview(command.options);
        case EOverviewAction::TOGGLE_FILTER:
            if (auto* overview = interactiveOverview()) {
                overview->toggleFilterMode();
                return {};
            }
            {
                auto options              = command.options;
                options.startInFilterMode = true;
                return openOverview(options);
            }
        case EOverviewAction::OPEN: return openOverview(command.options);
    }

    return {.success = false, .error = "unhandled overview action"};
}

SDispatchResult onWinviewDispatcher(const std::string& arg) {
    const auto COMMAND = Winview::parseOverviewCommand(arg);
    if (!COMMAND)
        return {.success = false, .error = COMMAND.error()};
    return runWinviewCommand(*COMMAND);
}
