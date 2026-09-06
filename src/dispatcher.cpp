#include "dispatcher.hpp"

#include <hyprland/src/desktop/state/FocusState.hpp>

#include <algorithm>
#include <cctype>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>
#include <vector>

#include "overview.hpp"
#include "workspace/module.hpp"

namespace {
    struct SWinviewDispatcherArgs {
        std::string            action = "toggle";
        SWindowOverviewOptions options;
    };

    std::string normalizedArgToken(std::string token) {
        auto notSpace = [](unsigned char c) { return !std::isspace(c); };
        token.erase(token.begin(), std::ranges::find_if(token, notSpace));
        token.erase(std::ranges::find_if(token.rbegin(), token.rend(), notSpace).base(),
                    token.end());
        std::ranges::transform(token, token.begin(), [](unsigned char c) {
            if (c == '_')
                return '-';
            return (char)std::tolower(c);
        });
        return token;
    }

    std::vector<std::string> argTokens(const std::string& arg) {
        std::vector<std::string> result;
        std::string              normalized = arg;
        std::ranges::replace(normalized, ',', ' ');
        std::stringstream stream(normalized);
        std::string       token;

        while (stream >> token) {
            token = normalizedArgToken(token);
            if (!token.empty())
                result.push_back(token);
        }

        return result;
    }

    bool isDispatcherAction(const std::string& token) {
        return token == "select" || token == "bring" || token == "bring-replace" ||
            token == "replace" || token == "off" || token == "close" || token == "disable" ||
            token == "toggle" || token == "open" || token == "show" || token == "on" ||
            token == "toggle-filter" || token == "filter-toggle" || token == "toggle-search" ||
            token == "search-toggle";
    }

    std::optional<EWinviewDefaultAction> parseDefaultAction(const std::string& value) {
        if (value == "select" || value == "focus" || value == "go")
            return EWinviewDefaultAction::SELECT;
        if (value == "bring")
            return EWinviewDefaultAction::BRING;
        if (value == "bring-replace" || value == "replace")
            return EWinviewDefaultAction::BRING_REPLACE;

        return std::nullopt;
    }

    bool applyOverviewOption(const std::string& token, SWindowOverviewOptions& options) {
        constexpr std::string_view DEFAULT_ACTION_PREFIX = "default-action=";
        if (token.starts_with(DEFAULT_ACTION_PREFIX)) {
            const auto VALUE  = token.substr(DEFAULT_ACTION_PREFIX.size());
            const auto ACTION = parseDefaultAction(VALUE);
            if (!ACTION)
                return false;

            options.defaultAction = *ACTION;
            return true;
        }

        if (token == "exclude-current-workspace" || token == "without-current-workspace" ||
            token == "no-current-workspace" || token == "other-workspaces" ||
            token == "not-current-workspace" || token == "include-current-workspace=false" ||
            token == "current-workspace=false") {
            options.includeCurrentWorkspace = false;
            return true;
        }

        if (token == "all" || token == "default" || token == "include-current-workspace" ||
            token == "with-current-workspace" || token == "current-workspace" ||
            token == "include-current-workspace=true" || token == "current-workspace=true") {
            options.includeCurrentWorkspace = true;
            return true;
        }

        if (token == "filter" || token == "search" || token == "start-filter" ||
            token == "start-in-filter-mode" || token == "filter-mode" ||
            token == "start-filter=true" || token == "filter-mode=true") {
            options.startInFilterMode = true;
            return true;
        }

        if (token == "normal" || token == "navigation" || token == "nav" ||
            token == "start-filter=false" || token == "filter-mode=false") {
            options.startInFilterMode = false;
            return true;
        }

        return false;
    }

    std::optional<SWinviewDispatcherArgs> parseWinviewDispatcherArgs(const std::string& arg,
                                                                     std::string&       error) {
        SWinviewDispatcherArgs args;
        bool                   sawAction = false;

        for (const auto& token : argTokens(arg.empty() ? "toggle" : arg)) {
            if (isDispatcherAction(token)) {
                if (sawAction) {
                    error = "multiple overview actions provided";
                    return std::nullopt;
                }

                args.action = token;
                sawAction   = true;
                continue;
            }

            if (applyOverviewOption(token, args.options))
                continue;

            error = "unknown overview argument: " + token;
            return std::nullopt;
        }

        return args;
    }
}

SDispatchResult onWinviewDispatcher(const std::string& arg) {
    std::string error;
    const auto  ARGS = parseWinviewDispatcherArgs(arg, error);
    if (!ARGS)
        return {.success = false, .error = error};

    const auto& ACTION = ARGS->action;

    if (ACTION == "select") {
        if (g_pWindowOverview) {
            g_pWindowOverview->selectHoveredWindow();
            g_pWindowOverview->close(true);
        }
        return {};
    }

    if (ACTION == "bring") {
        if (g_pWindowOverview) {
            g_pWindowOverview->selectHoveredWindow();
            g_pWindowOverview->close(true, true);
        }
        return {};
    }

    if (ACTION == "bring-replace" || ACTION == "replace") {
        if (g_pWindowOverview) {
            g_pWindowOverview->selectHoveredWindow();
            g_pWindowOverview->close(true, true, true);
        }
        return {};
    }

    if (ACTION == "off" || ACTION == "close" || ACTION == "disable") {
        if (g_pWindowOverview)
            g_pWindowOverview->close(false);
        return {};
    }

    if (ACTION == "toggle" && g_pWindowOverview) {
        g_pWindowOverview->close(false);
        return {};
    }

    if (ACTION == "toggle-filter" || ACTION == "filter-toggle" || ACTION == "toggle-search" ||
        ACTION == "search-toggle") {
        if (g_pWindowOverview)
            g_pWindowOverview->toggleFilterMode();
        else {
            const auto MONITOR = Desktop::focusState()->monitor();
            if (!MONITOR)
                return {.success = false, .error = "no focused monitor"};

            auto options              = ARGS->options;
            options.startInFilterMode = true;
            closeWorkspaceOverviewImmediately();
            g_pWindowOverview = std::make_unique<CWindowOverview>(MONITOR, options);
        }
        return {};
    }

    const auto MONITOR = Desktop::focusState()->monitor();
    if (!MONITOR)
        return {.success = false, .error = "no focused monitor"};

    closeWorkspaceOverviewImmediately();
    g_pWindowOverview = std::make_unique<CWindowOverview>(MONITOR, ARGS->options);
    return {};
}
