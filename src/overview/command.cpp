#include "command.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

#include "../strings.hpp"

namespace Winview {
    namespace {
        template <typename T, std::size_t N>
        std::optional<T> lookup(const std::array<std::pair<std::string_view, T>, N>& table,
                                std::string_view                                     token) {
            const auto IT = std::ranges::find(table, token, &std::pair<std::string_view, T>::first);
            if (IT == table.end())
                return std::nullopt;
            return IT->second;
        }

        constexpr std::array<std::pair<std::string_view, EOverviewAction>, 15> ACTIONS = {{
            {"toggle", EOverviewAction::TOGGLE},
            {"open", EOverviewAction::OPEN},
            {"show", EOverviewAction::OPEN},
            {"on", EOverviewAction::OPEN},
            {"close", EOverviewAction::CLOSE},
            {"off", EOverviewAction::CLOSE},
            {"disable", EOverviewAction::CLOSE},
            {"select", EOverviewAction::SELECT},
            {"bring", EOverviewAction::BRING},
            {"bring-replace", EOverviewAction::BRING_REPLACE},
            {"replace", EOverviewAction::BRING_REPLACE},
            {"toggle-filter", EOverviewAction::TOGGLE_FILTER},
            {"filter-toggle", EOverviewAction::TOGGLE_FILTER},
            {"toggle-search", EOverviewAction::TOGGLE_FILTER},
            {"search-toggle", EOverviewAction::TOGGLE_FILTER},
        }};

        constexpr std::array<std::pair<std::string_view, EWinviewDefaultAction>, 6>
            DEFAULT_ACTIONS = {{
                {"select", EWinviewDefaultAction::SELECT},
                {"focus", EWinviewDefaultAction::SELECT},
                {"go", EWinviewDefaultAction::SELECT},
                {"bring", EWinviewDefaultAction::BRING},
                {"bring-replace", EWinviewDefaultAction::BRING_REPLACE},
                {"replace", EWinviewDefaultAction::BRING_REPLACE},
            }};

        enum class EOptionFlag : std::uint8_t {
            EXCLUDE_CURRENT_WORKSPACE,
            INCLUDE_CURRENT_WORKSPACE,
            START_IN_FILTER_MODE,
            START_IN_NAVIGATION_MODE,
        };

        constexpr std::array<std::pair<std::string_view, EOptionFlag>, 26> OPTION_FLAGS = {{
            {"exclude-current-workspace", EOptionFlag::EXCLUDE_CURRENT_WORKSPACE},
            {"without-current-workspace", EOptionFlag::EXCLUDE_CURRENT_WORKSPACE},
            {"no-current-workspace", EOptionFlag::EXCLUDE_CURRENT_WORKSPACE},
            {"other-workspaces", EOptionFlag::EXCLUDE_CURRENT_WORKSPACE},
            {"not-current-workspace", EOptionFlag::EXCLUDE_CURRENT_WORKSPACE},
            {"include-current-workspace=false", EOptionFlag::EXCLUDE_CURRENT_WORKSPACE},
            {"current-workspace=false", EOptionFlag::EXCLUDE_CURRENT_WORKSPACE},
            {"all", EOptionFlag::INCLUDE_CURRENT_WORKSPACE},
            {"default", EOptionFlag::INCLUDE_CURRENT_WORKSPACE},
            {"include-current-workspace", EOptionFlag::INCLUDE_CURRENT_WORKSPACE},
            {"with-current-workspace", EOptionFlag::INCLUDE_CURRENT_WORKSPACE},
            {"current-workspace", EOptionFlag::INCLUDE_CURRENT_WORKSPACE},
            {"include-current-workspace=true", EOptionFlag::INCLUDE_CURRENT_WORKSPACE},
            {"current-workspace=true", EOptionFlag::INCLUDE_CURRENT_WORKSPACE},
            {"filter", EOptionFlag::START_IN_FILTER_MODE},
            {"search", EOptionFlag::START_IN_FILTER_MODE},
            {"start-filter", EOptionFlag::START_IN_FILTER_MODE},
            {"start-in-filter-mode", EOptionFlag::START_IN_FILTER_MODE},
            {"filter-mode", EOptionFlag::START_IN_FILTER_MODE},
            {"start-filter=true", EOptionFlag::START_IN_FILTER_MODE},
            {"filter-mode=true", EOptionFlag::START_IN_FILTER_MODE},
            {"normal", EOptionFlag::START_IN_NAVIGATION_MODE},
            {"navigation", EOptionFlag::START_IN_NAVIGATION_MODE},
            {"nav", EOptionFlag::START_IN_NAVIGATION_MODE},
            {"start-filter=false", EOptionFlag::START_IN_NAVIGATION_MODE},
            {"filter-mode=false", EOptionFlag::START_IN_NAVIGATION_MODE},
        }};

        void applyFlag(EOptionFlag flag, SWindowOverviewOptions& options) {
            switch (flag) {
                case EOptionFlag::EXCLUDE_CURRENT_WORKSPACE:
                    options.includeCurrentWorkspace = false;
                    return;
                case EOptionFlag::INCLUDE_CURRENT_WORKSPACE:
                    options.includeCurrentWorkspace = true;
                    return;
                case EOptionFlag::START_IN_FILTER_MODE: options.startInFilterMode = true; return;
                case EOptionFlag::START_IN_NAVIGATION_MODE:
                    options.startInFilterMode = false;
                    return;
            }
        }

        bool applyOption(std::string_view token, SWindowOverviewOptions& options) {
            constexpr std::string_view DEFAULT_ACTION_PREFIX = "default-action=";
            if (token.starts_with(DEFAULT_ACTION_PREFIX)) {
                const auto ACTION = parseDefaultAction(token.substr(DEFAULT_ACTION_PREFIX.size()));
                if (!ACTION)
                    return false;
                options.defaultAction = *ACTION;
                return true;
            }

            const auto FLAG = lookup(OPTION_FLAGS, token);
            if (!FLAG)
                return false;

            applyFlag(*FLAG, options);
            return true;
        }
    }

    std::string normalizedCommandToken(std::string_view token) {
        auto result = trimmedLower(token);
        std::ranges::replace(result, '_', '-');
        return result;
    }

    std::optional<EOverviewAction> parseOverviewAction(std::string_view token) {
        return lookup(ACTIONS, token);
    }

    std::optional<EWinviewDefaultAction> parseDefaultAction(std::string_view token) {
        return lookup(DEFAULT_ACTIONS, token);
    }

    std::expected<SOverviewCommand, std::string> parseOverviewCommand(std::string_view arg) {
        SOverviewCommand command;
        bool             sawAction = false;

        auto             isSeparator = [](char c) {
            return c == ',' || std::isspace(static_cast<unsigned char>(c));
        };
        auto rest = arg;
        while (true) {
            const auto START = std::ranges::find_if_not(rest, isSeparator);
            if (START == rest.end())
                break;
            const auto STOP  = std::find_if(START, rest.end(), isSeparator);
            const auto TOKEN = normalizedCommandToken(std::string_view(START, STOP));
            rest.remove_prefix(STOP - rest.begin());

            if (const auto ACTION = parseOverviewAction(TOKEN)) {
                if (sawAction)
                    return std::unexpected("multiple overview actions provided");
                command.action = *ACTION;
                sawAction      = true;
                continue;
            }

            if (!applyOption(TOKEN, command.options))
                return std::unexpected("unknown overview argument: " + TOKEN);
        }

        return command;
    }
}
