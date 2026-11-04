#include "audio/lua_rules.hpp"

#include <cstdlib>
#include <expected>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

static void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

static std::expected<Audio::SRules, std::string> rulesFrom(lua_State* L, const char* expression) {
    const std::string CHUNK = std::string("return ") + expression;
    if (luaL_dostring(L, CHUNK.c_str()) != LUA_OK) {
        std::cerr << "bad test chunk: " << lua_tostring(L, -1) << '\n';
        std::exit(1);
    }
    auto rules = Audio::readRules(L, -1);
    lua_settop(L, 0);
    return rules;
}

static void expectError(lua_State* L, const char* expression, std::string_view message) {
    const auto RULES = rulesFrom(L, expression);
    if (RULES || RULES.error() != message) {
        std::cerr << expression << "\n  expected: " << message
                  << "\n  got:      " << (RULES ? "success" : RULES.error()) << '\n';
        std::exit(1);
    }
}

int main() {
    lua_State*    L = luaL_newstate();

    const auto    RULES = rulesFrom(L,
                                    "{default = {ambiguous = 'none'}, apps = {"
                                       "['Google-Chrome'] = {disambiguate = {'media_session'}, "
                                       "ambiguous = 'app'}, spotify = {ignore = true}, "
                                       "ghostty = {disambiguate = {}}}}");
    Audio::SRules expected;
    expected.fallback.ambiguous           = Audio::eAmbiguous::None;
    expected.apps["google-chrome"]        = {.ignore = std::nullopt,
                                             .disambiguate =
                                                 std::vector{Audio::eDisambiguator::MediaSession},
                                             .ambiguous = Audio::eAmbiguous::App};
    expected.apps["spotify"].ignore       = true;
    expected.apps["ghostty"].disambiguate = std::vector<Audio::eDisambiguator>{};
    expect(RULES && *RULES == expected, "audio rules not read as written");
    expect(rulesFrom(L, "{}") == Audio::SRules{}, "empty table did not give default rules");

    expectError(L, "{default = {ambiguous = 'all'}}",
                "field \"default.ambiguous\" must be \"app\" or \"none\"");
    expectError(L, "{apps = {chrome = {disambiguate = {'stream_title'}}}}",
                "unknown disambiguator \"stream_title\" in \"apps.chrome.disambiguate\"");
    expectError(L, "{apps = {chrome = {disambiguate = 'media_session'}}}",
                "field \"apps.chrome.disambiguate\" must be an array of strings");
    expectError(L, "{apps = {chrome = {ignore = 'yes'}}}",
                "field \"apps.chrome.ignore\" must be a boolean");
    expectError(L, "{apps = {Spotify = {}, spotify = {}}}",
                "app \"spotify\" is set more than once");
    expectError(L, "{default = {ignroe = true}}", "unknown field \"default.ignroe\"");
    expectError(L, "{fallback = {}}", "unknown field \"fallback\"");
    expectError(L, "{apps = {chrome = 5}}", "field \"apps.chrome\" must be a table");
    expectError(L, "'on'", "argument must be a table");

    lua_close(L);
    std::cout << "Audio Lua rule tests passed\n";
}
