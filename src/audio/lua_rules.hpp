#ifndef HYPRWINVIEW_AUDIO_LUA_RULES_HPP
#define HYPRWINVIEW_AUDIO_LUA_RULES_HPP

#include <lua.hpp>

#include <expected>
#include <string>

#include "attribution.hpp"

namespace Audio {
    // Reads the table passed to hyprwinview.configure_audio with raw Lua accessors only, so no
    // metamethod can raise while C++ objects are live.
    std::expected<SRules, std::string> readRules(lua_State* L, int index);
}

#endif
