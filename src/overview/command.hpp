#ifndef HYPRWINVIEW_OVERVIEW_COMMAND_HPP
#define HYPRWINVIEW_OVERVIEW_COMMAND_HPP

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

enum class EWinviewDefaultAction : std::uint8_t {
    SELECT,
    BRING,
    BRING_REPLACE,
};

struct SWindowOverviewOptions {
    bool                  includeCurrentWorkspace = true;
    bool                  startInFilterMode       = false;
    EWinviewDefaultAction defaultAction           = EWinviewDefaultAction::SELECT;
};

namespace Winview {
    enum class EOverviewAction : std::uint8_t {
        TOGGLE,
        OPEN,
        CLOSE,
        SELECT,
        BRING,
        BRING_REPLACE,
        TOGGLE_FILTER,
    };

    struct SOverviewCommand {
        EOverviewAction        action = EOverviewAction::TOGGLE;
        SWindowOverviewOptions options;
    };

    // Lowercases, trims, and maps '_' to '-'.
    std::string                          normalizedCommandToken(std::string_view token);

    std::optional<EOverviewAction>       parseOverviewAction(std::string_view token);
    std::optional<EWinviewDefaultAction> parseDefaultAction(std::string_view token);

    // Parses the hyprwinview:overview dispatcher argument.
    std::expected<SOverviewCommand, std::string> parseOverviewCommand(std::string_view arg);
}

#endif
