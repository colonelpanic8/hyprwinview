#include "lua_rules.hpp"

#include <format>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "../strings.hpp"

namespace Audio {
    namespace {
        std::optional<std::string_view> rawString(lua_State* L, int index) {
            if (lua_type(L, index) != LUA_TSTRING)
                return std::nullopt;
            std::size_t length = 0;
            const auto* data   = lua_tolstring(L, index, &length);
            return std::string_view(data, length);
        }

        // A Lua sequence of strings with no other keys.
        std::optional<std::vector<std::string>> readStringArray(lua_State* L, int index) {
            if (lua_type(L, index) != LUA_TTABLE)
                return std::nullopt;

            const auto               LENGTH = lua_rawlen(L, index);
            std::vector<std::string> values;
            values.reserve(LENGTH);
            for (lua_Unsigned i = 1; i <= LENGTH; ++i) {
                lua_rawgeti(L, index, static_cast<lua_Integer>(i));
                const auto VALUE = rawString(L, -1);
                lua_pop(L, 1);
                if (!VALUE)
                    return std::nullopt;
                values.emplace_back(*VALUE);
            }

            lua_Unsigned entries = 0;
            lua_pushnil(L);
            while (lua_next(L, index) != 0) {
                ++entries;
                lua_pop(L, 1);
            }
            if (entries != LENGTH)
                return std::nullopt;

            return values;
        }

        std::expected<SRule, std::string> readRule(lua_State* L, int table,
                                                   const std::string& path) {
            if (lua_type(L, table) != LUA_TTABLE)
                return std::unexpected(std::format("field \"{}\" must be a table", path));

            SRule rule;
            lua_pushnil(L);
            while (lua_next(L, table) != 0) {
                const auto KEY = rawString(L, -2);
                if (!KEY)
                    return std::unexpected("table keys must be strings");

                const auto FIELD = *KEY;
                if (FIELD == "ignore") {
                    if (lua_type(L, -1) != LUA_TBOOLEAN)
                        return std::unexpected(
                            std::format("field \"{}.ignore\" must be a boolean", path));
                    rule.ignore = lua_toboolean(L, -1) != 0;
                } else if (FIELD == "disambiguate") {
                    const auto NAMES = readStringArray(L, lua_absindex(L, -1));
                    if (!NAMES)
                        return std::unexpected(std::format(
                            "field \"{}.disambiguate\" must be an array of strings", path));
                    auto& list = rule.disambiguate.emplace();
                    for (const auto& name : *NAMES) {
                        const auto DISAMBIGUATOR = parseDisambiguator(name);
                        if (!DISAMBIGUATOR)
                            return std::unexpected(std::format(
                                "unknown disambiguator \"{}\" in \"{}.disambiguate\"", name, path));
                        list.push_back(*DISAMBIGUATOR);
                    }
                } else if (FIELD == "ambiguous") {
                    const auto VALUE = rawString(L, -1);
                    rule.ambiguous   = VALUE ? parseAmbiguous(*VALUE) : std::nullopt;
                    if (!rule.ambiguous)
                        return std::unexpected(std::format(
                            "field \"{}.ambiguous\" must be \"app\" or \"none\"", path));
                } else
                    return std::unexpected(std::format("unknown field \"{}.{}\"", path, FIELD));

                lua_pop(L, 1);
            }
            return rule;
        }

        std::expected<void, std::string> readApps(lua_State* L, int table, SRules& rules) {
            if (lua_type(L, table) != LUA_TTABLE)
                return std::unexpected("field \"apps\" must be a table");

            lua_pushnil(L);
            while (lua_next(L, table) != 0) {
                const auto KEY = rawString(L, -2);
                if (!KEY)
                    return std::unexpected("field \"apps\" keys must be window classes");

                auto app = Winview::lowercase(std::string(*KEY));
                if (rules.apps.contains(app))
                    return std::unexpected(std::format("app \"{}\" is set more than once", app));
                auto rule = readRule(L, lua_absindex(L, -1), "apps." + app);
                if (!rule)
                    return std::unexpected(std::move(rule.error()));
                rules.apps.emplace(std::move(app), std::move(*rule));
                lua_pop(L, 1);
            }
            return {};
        }
    }

    std::expected<SRules, std::string> readRules(lua_State* L, int index) {
        const int TABLE = lua_absindex(L, index);
        if (lua_type(L, TABLE) != LUA_TTABLE)
            return std::unexpected("argument must be a table");

        SRules rules;
        lua_pushnil(L);
        while (lua_next(L, TABLE) != 0) {
            const auto KEY = rawString(L, -2);
            if (!KEY)
                return std::unexpected("table keys must be strings");

            const auto FIELD = *KEY;
            const int  VALUE = lua_absindex(L, -1);
            if (FIELD == "default") {
                auto rule = readRule(L, VALUE, "default");
                if (!rule)
                    return std::unexpected(std::move(rule.error()));
                rules.fallback = std::move(*rule);
            } else if (FIELD == "apps") {
                if (auto result = readApps(L, VALUE, rules); !result)
                    return std::unexpected(std::move(result.error()));
            } else
                return std::unexpected(std::format("unknown field \"{}\"", FIELD));

            lua_pop(L, 1);
        }
        return rules;
    }
}
