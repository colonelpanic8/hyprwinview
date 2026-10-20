#include "../overview.hpp"
#include "grouping.hpp"
#include "../config_helpers.hpp"
#include "../settings.hpp"

using namespace WinviewConfig;
using Winview::configStringOr;
using Winview::trimmedLower;

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

#define private   public
#define protected public
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#undef private
#undef protected

struct SWindowOrderingStrategy {
    const char* name;
    std::string (*groupKeyForWindow)(const PHLWINDOW& window, size_t originalIndex);
};

static std::string uniqueWindowGroupKey(const PHLWINDOW&, size_t originalIndex) {
    return "window:" + std::to_string(originalIndex);
}

static std::string applicationGroupKey(const PHLWINDOW& window, size_t originalIndex) {
    if (window) {
        for (const auto& candidate : {window->m_class, window->m_initialClass}) {
            const auto KEY = trimmedLower(candidate);
            if (!KEY.empty())
                return "app:" + KEY;
        }
    }

    return uniqueWindowGroupKey(window, originalIndex);
}

static const SWindowOrderingStrategy& naturalOrderStrategy() {
    static const SWindowOrderingStrategy STRATEGY = {
        .name              = "natural",
        .groupKeyForWindow = uniqueWindowGroupKey,
    };
    return STRATEGY;
}

static const SWindowOrderingStrategy& applicationOrderStrategy() {
    static const SWindowOrderingStrategy STRATEGY = {
        .name              = "application",
        .groupKeyForWindow = applicationGroupKey,
    };
    return STRATEGY;
}

static const SWindowOrderingStrategy& activeWindowOrderingStrategy() {
    const auto NAME = trimmedLower(configStringOr(PWINDOWORDER(), "natural"));

    if (NAME.empty() || NAME == "none" || NAME == "natural" || NAME == "compositor")
        return naturalOrderStrategy();

    if (NAME == "app" || NAME == "application" || NAME == "application_grouped" ||
        NAME == "group_app" || NAME == "group_by_app" || NAME == "grouped_by_app")
        return applicationOrderStrategy();

    return naturalOrderStrategy();
}

void CWindowOverview::applyWindowOrdering(std::vector<SWindowPreview>& windowPreviews) {
    if (windowPreviews.empty())
        return;

    const auto&              STRATEGY = activeWindowOrderingStrategy();
    std::vector<std::string> groupKeys;
    groupKeys.reserve(windowPreviews.size());
    for (size_t i = 0; i < windowPreviews.size(); ++i)
        groupKeys.push_back(STRATEGY.groupKeyForWindow(windowPreviews[i].window, i));

    auto original = std::move(windowPreviews);
    windowPreviews.clear();
    windowPreviews.reserve(original.size());
    for (const auto index : Winview::stableGroupOrder(groupKeys))
        windowPreviews.push_back(std::move(original[index]));
}
