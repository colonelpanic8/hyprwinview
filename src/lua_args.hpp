#ifndef HYPRWINVIEW_LUA_ARGS_HPP
#define HYPRWINVIEW_LUA_ARGS_HPP

#include <lua.hpp>

#include <array>
#include <expected>
#include <string>
#include <type_traits>

#include "overview/command.hpp"
#include "overview/keys.hpp"

namespace Winview {
    // Readers use only raw Lua accessors, so no metamethod can raise while C++ objects are live.
    std::expected<SOverviewCommand, std::string> readOverviewCommand(lua_State* L, int index);
    std::expected<KeyOverrides, std::string>     readKeyOverrides(lua_State* L, int index);

    // Lua is built as C, so lua_error longjmps past C++ frames without running destructors.
    // Run `body` to completion first and raise its error from a frame that owns only trivially
    // destructible state.
    template <typename F>
    int luaBoundary(lua_State* L, F body) {
        static_assert(std::is_trivially_destructible_v<F>);
        std::array<char, 512> message{};
        std::size_t           length = 0;
        {
            const std::expected<void, std::string> RESULT = body(L);
            if (RESULT)
                return 0;
            length = RESULT.error().copy(message.data(), message.size());
        }
        static_assert(std::is_trivially_destructible_v<decltype(message)>);
        lua_pushlstring(L, message.data(), length);
        return lua_error(L);
    }
}

#endif
