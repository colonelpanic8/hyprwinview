#include "lua_args.hpp"

#include <algorithm>
#include <format>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace Winview {
    namespace {
        std::optional<std::string_view> rawString(lua_State* L, int index) {
            if (lua_type(L, index) != LUA_TSTRING)
                return std::nullopt;
            std::size_t length = 0;
            const auto* data   = lua_tolstring(L, index, &length);
            return std::string_view(data, length);
        }

        std::optional<bool> rawBoolean(lua_State* L, int index) {
            if (lua_type(L, index) != LUA_TBOOLEAN)
                return std::nullopt;
            return lua_toboolean(L, index) != 0;
        }

        // Alias fields may both be present only when they agree. Table iteration order is
        // unspecified, so the error names the pair rather than whichever came second.
        bool setOnce(std::optional<bool>& slot, bool value) {
            if (slot && *slot != value)
                return false;
            slot = value;
            return true;
        }

        std::expected<SOverviewCommand, std::string> readOverviewTable(lua_State* L, int table) {
            SOverviewCommand    command;
            std::optional<bool> includeCurrentWorkspace;
            std::optional<bool> startInFilterMode;

            lua_pushnil(L);
            while (lua_next(L, table) != 0) {
                const auto KEY = rawString(L, -2);
                if (!KEY)
                    return std::unexpected("table keys must be strings");

                const auto FIELD = *KEY;
                if (FIELD == "action" || FIELD == "default_action") {
                    const auto VALUE = rawString(L, -1);
                    if (!VALUE)
                        return std::unexpected(std::format("field \"{}\" must be a string", FIELD));

                    const auto TOKEN = normalizedCommandToken(*VALUE);
                    if (FIELD == "action") {
                        const auto ACTION = parseOverviewAction(TOKEN);
                        if (!ACTION)
                            return std::unexpected(std::format("unknown action \"{}\"", *VALUE));
                        command.action = *ACTION;
                    } else {
                        const auto ACTION = parseDefaultAction(TOKEN);
                        if (!ACTION)
                            return std::unexpected(
                                std::format("unknown default_action \"{}\"", *VALUE));
                        command.options.defaultAction = *ACTION;
                    }
                } else if (FIELD == "exclude_current_workspace" ||
                           FIELD == "include_current_workspace" || FIELD == "filter_mode" ||
                           FIELD == "start_in_filter_mode") {
                    const auto VALUE = rawBoolean(L, -1);
                    if (!VALUE)
                        return std::unexpected(
                            std::format("field \"{}\" must be a boolean", FIELD));

                    const bool FILTER = FIELD == "filter_mode" || FIELD == "start_in_filter_mode";
                    const bool AGREES = FILTER ?
                        setOnce(startInFilterMode, *VALUE) :
                        setOnce(includeCurrentWorkspace,
                                FIELD == "include_current_workspace" ? *VALUE : !*VALUE);
                    if (!AGREES)
                        return std::unexpected(
                            FILTER ?
                                "fields \"filter_mode\" and \"start_in_filter_mode\" conflict" :
                                "fields \"exclude_current_workspace\" and "
                                "\"include_current_workspace\" conflict");
                } else
                    return std::unexpected(std::format("unknown field \"{}\"", FIELD));

                lua_pop(L, 1);
            }

            command.options.includeCurrentWorkspace = includeCurrentWorkspace.value_or(true);
            command.options.startInFilterMode       = startInFilterMode.value_or(false);
            return command;
        }

        std::optional<EKeySet> keySetForField(std::string_view field) {
            if (field.starts_with("keys_"))
                field.remove_prefix(5);
            if (field == "go" || field == "default")
                return EKeySet::DEFAULT_ACTION;

            const auto IT = std::ranges::find(KEY_SET_NAMES, field);
            if (IT == KEY_SET_NAMES.end())
                return std::nullopt;
            return static_cast<EKeySet>(IT - KEY_SET_NAMES.begin());
        }

        std::expected<std::vector<std::string>, std::string> readKeyList(lua_State* L, int index,
                                                                         std::string_view field) {
            if (const auto KEYS = rawString(L, index))
                return splitKeyList(*KEYS);

            const auto ERROR =
                std::format("field \"{}\" must be a string or an array of strings", field);
            if (lua_type(L, index) != LUA_TTABLE)
                return std::unexpected(ERROR);

            const auto               LENGTH = lua_rawlen(L, index);
            std::vector<std::string> keys;
            keys.reserve(LENGTH);
            for (lua_Unsigned i = 1; i <= LENGTH; ++i) {
                lua_rawgeti(L, index, static_cast<lua_Integer>(i));
                const auto KEY = rawString(L, -1);
                if (!KEY)
                    return std::unexpected(ERROR);
                keys.emplace_back(*KEY);
                lua_pop(L, 1);
            }

            lua_Unsigned entries = 0;
            lua_pushnil(L);
            while (lua_next(L, index) != 0) {
                ++entries;
                lua_pop(L, 1);
            }
            if (entries != LENGTH)
                return std::unexpected(ERROR);

            return keys;
        }

        std::expected<void, std::string> readKeyFields(lua_State* L, int table,
                                                       KeyOverrides& overrides, bool topLevel) {
            lua_pushnil(L);
            while (lua_next(L, table) != 0) {
                const auto KEY = rawString(L, -2);
                if (!KEY)
                    return std::unexpected("table keys must be strings");

                const auto FIELD = *KEY;
                if (topLevel && FIELD == "keys") {
                    if (lua_type(L, -1) != LUA_TTABLE)
                        return std::unexpected("field \"keys\" must be a table");
                    auto nested = readKeyFields(L, lua_absindex(L, -1), overrides, false);
                    if (!nested)
                        return nested;
                    lua_pop(L, 1);
                    continue;
                }

                const auto SET = keySetForField(FIELD);
                if (!SET)
                    return std::unexpected(std::format("unknown field \"{}\"", FIELD));

                auto& slot = overrides[static_cast<std::size_t>(*SET)];
                if (slot)
                    return std::unexpected(
                        std::format("key set \"{}\" is set more than once", keySetName(*SET)));

                auto keys = readKeyList(L, lua_absindex(L, -1), FIELD);
                if (!keys)
                    return std::unexpected(std::move(keys.error()));
                slot = std::move(*keys);
                lua_pop(L, 1);
            }

            return {};
        }

        template <typename T>
        std::expected<T, std::string> withPrefix(std::string_view              prefix,
                                                 std::expected<T, std::string> result) {
            if (!result)
                return std::unexpected(std::format("{}: {}", prefix, result.error()));
            return result;
        }
    }

    std::expected<SOverviewCommand, std::string> readOverviewCommand(lua_State* L, int index) {
        constexpr std::string_view PREFIX = "hyprwinview.overview";
        index                             = lua_absindex(L, index);

        if (const auto ARG = rawString(L, index))
            return withPrefix(PREFIX, parseOverviewCommand(*ARG));

        switch (lua_type(L, index)) {
            case LUA_TNONE:
            case LUA_TNIL: return SOverviewCommand{};
            case LUA_TTABLE: return withPrefix(PREFIX, readOverviewTable(L, index));
            default:
                return std::unexpected(
                    std::format("{}: argument must be a string or table", PREFIX));
        }
    }

    std::expected<KeyOverrides, std::string> readKeyOverrides(lua_State* L, int index) {
        constexpr std::string_view PREFIX = "hyprwinview.configure";
        index                             = lua_absindex(L, index);

        if (lua_type(L, index) != LUA_TTABLE)
            return std::unexpected(std::format("{}: argument must be a table", PREFIX));

        KeyOverrides overrides;
        if (auto result = readKeyFields(L, index, overrides, true); !result)
            return std::unexpected(std::format("{}: {}", PREFIX, result.error()));
        return overrides;
    }
}
