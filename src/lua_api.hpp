#ifndef HYPRWINVIEW_LUA_API_HPP
#define HYPRWINVIEW_LUA_API_HPP

struct lua_State;

int luaWinviewOverview(lua_State* L);
int luaWinviewConfigure(lua_State* L);

#endif
