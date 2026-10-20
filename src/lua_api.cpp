#include "lua_api.hpp"

#include <expected>
#include <string>
#include <utility>

#include "dispatcher.hpp"
#include "lua_args.hpp"

int luaWinviewOverview(lua_State* L) {
    return Winview::luaBoundary(L, [](lua_State* L) -> std::expected<void, std::string> {
        const auto COMMAND = Winview::readOverviewCommand(L, 1);
        if (!COMMAND)
            return std::unexpected(COMMAND.error());

        auto result = runWinviewCommand(*COMMAND);
        if (!result.success)
            return std::unexpected(std::move(result.error));
        return {};
    });
}

int luaWinviewConfigure(lua_State* L) {
    return Winview::luaBoundary(L, [](lua_State* L) -> std::expected<void, std::string> {
        auto overrides = Winview::readKeyOverrides(L, 1);
        if (!overrides)
            return std::unexpected(std::move(overrides.error()));

        Winview::keyOverrides().configure(std::move(*overrides));
        return {};
    });
}
