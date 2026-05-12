#include "lua_api.hpp"

#include <lua.hpp>

#include <string>
#include <utility>
#include <vector>

#include "dispatcher.hpp"
#include "overview.hpp"

namespace {
    std::vector<std::string> luaStringListField(lua_State* L, int tableIdx, const char* field,
                                                const std::vector<std::string>& fallback) {
        tableIdx = lua_absindex(L, tableIdx);
        lua_getfield(L, tableIdx, field);

        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            return fallback;
        }

        if (!lua_istable(L, -1))
            luaL_error(L, "hyprwinview.configure: field \"%s\" must be an array of strings", field);

        std::vector<std::string> result;
        const auto               LEN = lua_rawlen(L, -1);
        result.reserve(LEN);

        for (size_t i = 1; i <= LEN; ++i) {
            lua_rawgeti(L, -1, static_cast<lua_Integer>(i));
            if (!lua_isstring(L, -1))
                luaL_error(L, "hyprwinview.configure: field \"%s\" item %zu must be a string",
                           field, i);
            result.emplace_back(lua_tostring(L, -1));
            lua_pop(L, 1);
        }

        lua_pop(L, 1);
        return result;
    }

    void readKeyTable(lua_State* L, int tableIdx, SWinviewKeyConfig& config) {
        config.left          = luaStringListField(L, tableIdx, "left", config.left);
        config.right         = luaStringListField(L, tableIdx, "right", config.right);
        config.up            = luaStringListField(L, tableIdx, "up", config.up);
        config.down          = luaStringListField(L, tableIdx, "down", config.down);
        config.defaultAction = luaStringListField(L, tableIdx, "go", config.defaultAction);
        config.defaultAction = luaStringListField(L, tableIdx, "default", config.defaultAction);
        config.defaultAction =
            luaStringListField(L, tableIdx, "default_action", config.defaultAction);
        config.bring        = luaStringListField(L, tableIdx, "bring", config.bring);
        config.bringReplace = luaStringListField(L, tableIdx, "bring_replace", config.bringReplace);
        config.close        = luaStringListField(L, tableIdx, "close", config.close);
        config.filterToggle = luaStringListField(L, tableIdx, "filter_toggle", config.filterToggle);
        config.filterClose  = luaStringListField(L, tableIdx, "filter_close", config.filterClose);
        config.filterBring  = luaStringListField(L, tableIdx, "filter_bring", config.filterBring);
        config.filterBringReplace =
            luaStringListField(L, tableIdx, "filter_bring_replace", config.filterBringReplace);
        config.filterLeft  = luaStringListField(L, tableIdx, "filter_left", config.filterLeft);
        config.filterRight = luaStringListField(L, tableIdx, "filter_right", config.filterRight);
        config.filterUp    = luaStringListField(L, tableIdx, "filter_up", config.filterUp);
        config.filterDown  = luaStringListField(L, tableIdx, "filter_down", config.filterDown);

        config.left          = luaStringListField(L, tableIdx, "keys_left", config.left);
        config.right         = luaStringListField(L, tableIdx, "keys_right", config.right);
        config.up            = luaStringListField(L, tableIdx, "keys_up", config.up);
        config.down          = luaStringListField(L, tableIdx, "keys_down", config.down);
        config.defaultAction = luaStringListField(L, tableIdx, "keys_go", config.defaultAction);
        config.defaultAction =
            luaStringListField(L, tableIdx, "keys_default_action", config.defaultAction);
        config.bring = luaStringListField(L, tableIdx, "keys_bring", config.bring);
        config.bringReplace =
            luaStringListField(L, tableIdx, "keys_bring_replace", config.bringReplace);
        config.close = luaStringListField(L, tableIdx, "keys_close", config.close);
        config.filterToggle =
            luaStringListField(L, tableIdx, "keys_filter_toggle", config.filterToggle);
        config.filterClose =
            luaStringListField(L, tableIdx, "keys_filter_close", config.filterClose);
        config.filterBring =
            luaStringListField(L, tableIdx, "keys_filter_bring", config.filterBring);
        config.filterBringReplace =
            luaStringListField(L, tableIdx, "keys_filter_bring_replace", config.filterBringReplace);
        config.filterLeft = luaStringListField(L, tableIdx, "keys_filter_left", config.filterLeft);
        config.filterRight =
            luaStringListField(L, tableIdx, "keys_filter_right", config.filterRight);
        config.filterUp   = luaStringListField(L, tableIdx, "keys_filter_up", config.filterUp);
        config.filterDown = luaStringListField(L, tableIdx, "keys_filter_down", config.filterDown);
    }
}

int luaWinviewOverview(lua_State* L) {
    std::string arg = "toggle";

    if (lua_gettop(L) >= 1 && !lua_isnil(L, 1)) {
        if (lua_istable(L, 1)) {
            lua_getfield(L, 1, "action");
            if (lua_isstring(L, -1))
                arg = lua_tostring(L, -1);
            else if (!lua_isnil(L, -1))
                return luaL_error(L, "hyprwinview.overview: field \"action\" must be a string");
            lua_pop(L, 1);

            lua_getfield(L, 1, "exclude_current_workspace");
            if (lua_isboolean(L, -1) && lua_toboolean(L, -1))
                arg += " exclude-current-workspace";
            else if (!lua_isnil(L, -1) && !lua_isboolean(L, -1))
                return luaL_error(
                    L,
                    "hyprwinview.overview: field \"exclude_current_workspace\" must be a boolean");
            lua_pop(L, 1);

            lua_getfield(L, 1, "include_current_workspace");
            if (lua_isboolean(L, -1) && !lua_toboolean(L, -1))
                arg += " exclude-current-workspace";
            else if (!lua_isnil(L, -1) && !lua_isboolean(L, -1))
                return luaL_error(
                    L,
                    "hyprwinview.overview: field \"include_current_workspace\" must be a boolean");
            lua_pop(L, 1);

            lua_getfield(L, 1, "filter_mode");
            if (lua_isboolean(L, -1) && lua_toboolean(L, -1))
                arg += " filter";
            else if (!lua_isnil(L, -1) && !lua_isboolean(L, -1))
                return luaL_error(L,
                                  "hyprwinview.overview: field \"filter_mode\" must be a boolean");
            lua_pop(L, 1);

            lua_getfield(L, 1, "start_in_filter_mode");
            if (lua_isboolean(L, -1) && lua_toboolean(L, -1))
                arg += " filter";
            else if (!lua_isnil(L, -1) && !lua_isboolean(L, -1))
                return luaL_error(
                    L, "hyprwinview.overview: field \"start_in_filter_mode\" must be a boolean");
            lua_pop(L, 1);

            lua_getfield(L, 1, "default_action");
            if (lua_isstring(L, -1))
                arg += std::string(" default-action=") + lua_tostring(L, -1);
            else if (!lua_isnil(L, -1))
                return luaL_error(
                    L, "hyprwinview.overview: field \"default_action\" must be a string");
            lua_pop(L, 1);
        } else if (lua_isstring(L, 1))
            arg = lua_tostring(L, 1);
        else
            return luaL_error(L, "hyprwinview.overview: argument must be a string or table");
    }

    const auto RESULT = onWinviewDispatcher(arg);
    if (!RESULT.success)
        return luaL_error(L, "%s", RESULT.error.c_str());
    return 0;
}

int luaWinviewConfigure(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);

    auto config = defaultWinviewKeyConfig();

    readKeyTable(L, 1, config);

    lua_getfield(L, 1, "keys");
    if (!lua_isnil(L, -1)) {
        if (!lua_istable(L, -1))
            return luaL_error(L, "hyprwinview.configure: field \"keys\" must be a table");
        readKeyTable(L, -1, config);
    }
    lua_pop(L, 1);

    setWinviewKeyConfig(std::move(config));
    return 0;
}
