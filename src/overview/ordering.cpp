#include "../overview.hpp"

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

static const CConfigValue<Config::STRING>& PWINDOWORDER() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:window_order");
    return VALUE;
}

struct SWindowOrderingStrategy {
    const char* name;
    std::string (*groupKeyForWindow)(const PHLWINDOW& window, size_t originalIndex);
};

static std::string trimmedLower(std::string token) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    token.erase(token.begin(), std::ranges::find_if(token, notSpace));
    token.erase(std::ranges::find_if(token.rbegin(), token.rend(), notSpace).base(), token.end());
    std::ranges::transform(token, token.begin(), [](unsigned char c) { return std::tolower(c); });
    return token;
}

static std::string configStringOr(const CConfigValue<Config::STRING>& value,
                                  const std::string&                  fallback) {
    try {
        return *value;
    } catch (...) { return fallback; }
}

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
    std::vector<std::string> groupOrder;

    for (size_t i = 0; i < windowPreviews.size(); ++i) {
        auto& preview               = windowPreviews[i];
        preview.orderOriginalIndex  = i;
        preview.orderGroupKey       = STRATEGY.groupKeyForWindow(preview.window, i);
        const auto GROUP_ORDER_ITER = std::ranges::find(groupOrder, preview.orderGroupKey);

        if (GROUP_ORDER_ITER == groupOrder.end()) {
            preview.orderGroupIndex = groupOrder.size();
            groupOrder.push_back(preview.orderGroupKey);
        } else {
            preview.orderGroupIndex = GROUP_ORDER_ITER - groupOrder.begin();
        }
    }

    std::ranges::stable_sort(windowPreviews, [](const SWindowPreview& a, const SWindowPreview& b) {
        if (a.orderGroupIndex != b.orderGroupIndex)
            return a.orderGroupIndex < b.orderGroupIndex;

        return a.orderOriginalIndex < b.orderOriginalIndex;
    });
}
