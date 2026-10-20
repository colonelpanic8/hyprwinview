#include "lua_args.hpp"
#include "overview/command.hpp"
#include "overview/keys.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

using Winview::EKeyAction;
using Winview::EKeySet;
using Winview::EOverviewAction;

static void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

static Winview::SKeyBindings defaultBindings() {
    Winview::KeySetArray<std::string> configured = {
        "a,h,left",
        "d,l,right",
        "w,k,up",
        "s,j,down",
        "",
        "b,shift+return,shift+space",
        "shift+b",
        "escape,q",
        "/",
        "escape,ctrl+g",
        "ctrl+b",
        "ctrl+shift+b",
        "left,ctrl+a,super+a",
        "right,ctrl+d,super+d",
        "up,ctrl+p,ctrl+w,super+w",
        "down,ctrl+n,ctrl+s,super+s",
    };
    return Winview::compileKeyBindings(
        Winview::resolveKeySpecs({}, configured, "return,enter,space,g,f"));
}

static void testKeyChords() {
    const auto CHORD = Winview::parseKeyChord(" Ctrl + Shift + B ");
    expect(CHORD && CHORD->keysym == XKB_KEY_b &&
               CHORD->mods == (Winview::MOD_CTRL | Winview::MOD_SHIFT),
           "modified chord did not parse");
    expect(Winview::parseKeyChord("enter")->keysym == XKB_KEY_Return, "enter alias failed");
    expect(Winview::parseKeyChord("slash")->keysym == XKB_KEY_slash, "slash alias failed");
    for (const auto* invalid : {"", "ctrl", "ctrl++b", "a+b", "nosuchkey"})
        expect(!Winview::parseKeyChord(invalid), "invalid chord accepted");

    const auto BINDINGS = Winview::compileKeyBindings({{{"a", "bogus"}}});
    expect(BINDINGS.chords[0].size() == 1 && BINDINGS.invalid.size() == 1 &&
               BINDINGS.invalid[0] == "left: bogus",
           "invalid binding was not reported");
}

static void testNavigationKeys() {
    const auto KEYS = defaultBindings();
    auto       nav  = [&](xkb_keysym_t sym, uint32_t mods) {
        return Winview::classifyKey(KEYS, false, {.keysym = sym, .mods = mods}, false);
    };

    expect(nav(XKB_KEY_h, 0) == EKeyAction::LEFT, "h did not move left");
    expect(nav(XKB_KEY_Return, 0) == EKeyAction::DEFAULT_ACTION,
           "legacy keys_go fallback was not used");
    expect(nav(XKB_KEY_B, Winview::MOD_SHIFT) == EKeyAction::BRING_REPLACE,
           "shift+b did not bring-replace");
    expect(nav(XKB_KEY_b, 0) == EKeyAction::BRING, "b did not bring");
    expect(nav(XKB_KEY_Return, Winview::MOD_SHIFT) == EKeyAction::BRING,
           "shift+return did not bring");
    expect(nav(XKB_KEY_h, Winview::MOD_SUPER) == EKeyAction::PASS,
           "super+h was captured by a plain binding");
    expect(nav(XKB_KEY_Tab, Winview::MOD_SUPER) == EKeyAction::PASS, "super+tab was captured");
    expect(nav(XKB_KEY_c, Winview::MOD_CTRL) == EKeyAction::PASS,
           "unbound navigation key was captured");
}

static void testFilterKeys() {
    const auto KEYS   = defaultBindings();
    auto       filter = [&](xkb_keysym_t sym, uint32_t mods, bool text) {
        return Winview::classifyKey(KEYS, true, {.keysym = sym, .mods = mods}, text);
    };

    expect(filter(XKB_KEY_a, 0, true) == EKeyAction::INSERT_TEXT, "printable key not typed");
    expect(filter(XKB_KEY_q, 0, true) == EKeyAction::INSERT_TEXT,
           "navigation close key fired in filter mode");
    expect(filter(XKB_KEY_space, 0, true) == EKeyAction::INSERT_TEXT,
           "space ran the default action in filter mode");
    expect(filter(XKB_KEY_Return, 0, false) == EKeyAction::DEFAULT_ACTION,
           "return did not run the default action");
    expect(filter(XKB_KEY_b, Winview::MOD_CTRL, false) == EKeyAction::BRING, "ctrl+b not bring");
    expect(filter(XKB_KEY_B, Winview::MOD_CTRL | Winview::MOD_SHIFT, false) ==
               EKeyAction::BRING_REPLACE,
           "ctrl+shift+b not bring-replace");
    expect(filter(XKB_KEY_Escape, 0, false) == EKeyAction::CLOSE, "escape did not close");
    expect(filter(XKB_KEY_a, Winview::MOD_SUPER, false) == EKeyAction::LEFT,
           "super+a filter binding lost");
    expect(filter(XKB_KEY_BackSpace, 0, false) == EKeyAction::DELETE_CHARACTER,
           "backspace did not delete");
    expect(filter(XKB_KEY_u, Winview::MOD_CTRL, false) == EKeyAction::CLEAR_FILTER,
           "ctrl+u did not clear");
    expect(filter(XKB_KEY_u, Winview::MOD_CTRL | Winview::MOD_SUPER, false) == EKeyAction::PASS,
           "ctrl+super+u was swallowed in filter mode");
    expect(filter(XKB_KEY_u, Winview::MOD_CTRL | Winview::MOD_ALT, false) == EKeyAction::PASS,
           "ctrl+alt+u was swallowed in filter mode");

    expect(filter(XKB_KEY_Tab, Winview::MOD_SUPER, false) == EKeyAction::PASS,
           "super+tab was swallowed in filter mode");
    expect(filter(XKB_KEY_Escape, Winview::MOD_SUPER, false) == EKeyAction::PASS,
           "super+escape was swallowed in filter mode");
    expect(filter(XKB_KEY_x, Winview::MOD_ALT, false) == EKeyAction::PASS,
           "alt+x was swallowed in filter mode");
    expect(filter(XKB_KEY_BackSpace, Winview::MOD_SUPER, false) == EKeyAction::PASS,
           "super+backspace was swallowed in filter mode");
    expect(filter(XKB_KEY_Super_L, Winview::MOD_SUPER, false) == EKeyAction::PASS,
           "modifier press was swallowed in filter mode");
    expect(filter(XKB_KEY_Control_L, Winview::MOD_CTRL, false) == EKeyAction::PASS,
           "ctrl press was swallowed in filter mode");
    expect(filter(XKB_KEY_Shift_L, 0, false) == EKeyAction::PASS,
           "shift press was swallowed in filter mode");

    expect(filter(XKB_KEY_c, Winview::MOD_CTRL, false) == EKeyAction::SWALLOW,
           "ctrl+c leaked to the client in filter mode");
    expect(filter(XKB_KEY_q, Winview::MOD_CTRL, false) == EKeyAction::SWALLOW,
           "ctrl+q leaked to the client in filter mode");
    expect(filter(XKB_KEY_Tab, 0, false) == EKeyAction::SWALLOW,
           "tab leaked to the client in filter mode");
    expect(filter(XKB_KEY_F5, 0, false) == EKeyAction::SWALLOW,
           "f5 leaked to the client in filter mode");
}

static void testConsumedKeys() {
    Winview::CConsumedKeys keys;
    expect(!keys.release(30), "release of an unconsumed press was consumed");

    keys.press(14, EKeyAction::DELETE_CHARACTER);
    expect(keys.held(14), "consumed press not held");
    expect(keys.release(14) == EKeyAction::DELETE_CHARACTER, "release lost its action");
    expect(!keys.release(14), "release consumed twice");
}

static void testConsumedButtons() {
    Winview::CConsumedButtons buttons;
    expect(!buttons.release(272), "release of an unconsumed button was consumed");

    buttons.press(272);
    buttons.press(272);
    expect(buttons.release(272), "consumed button release was not consumed");
    expect(!buttons.release(272), "button release consumed twice");
}

static void testKeyOverrideReloads() {
    auto overridesFor = [](std::string key) {
        Winview::KeyOverrides overrides;
        overrides[static_cast<size_t>(EKeySet::CLOSE)] = std::vector<std::string>{std::move(key)};
        return overrides;
    };

    Winview::CKeyOverrideState state;
    state.configure(overridesFor("x"));
    expect(state.active() == overridesFor("x"), "configure outside a reload was not applied");

    state.beginReload();
    state.configure(overridesFor("y"));
    expect(state.active() == overridesFor("x"), "staged overrides applied before the reload ended");
    state.endReload(false);
    expect(state.active() == overridesFor("y"), "staged overrides were not committed");

    auto generation = state.generation();
    state.beginReload();
    state.endReload(true);
    expect(state.active() == overridesFor("y") && state.generation() == generation,
           "a reload that failed before configure() dropped the overrides");

    state.beginReload();
    state.configure(overridesFor("z"));
    state.endReload(true);
    expect(state.active() == overridesFor("z"),
           "configure() before a later reload error was not committed");

    state.beginReload();
    state.endReload(false);
    expect(state.active() == Winview::KeyOverrides{},
           "a successful reload without configure() kept the overrides");

    generation = state.generation();
    state.endReload(false);
    expect(state.generation() == generation, "a reload end without a begin changed overrides");
}

static void testKeyOverrides() {
    Winview::KeySetArray<std::string> configured;
    configured[static_cast<size_t>(EKeySet::CLOSE)]          = "x";
    configured[static_cast<size_t>(EKeySet::DEFAULT_ACTION)] = "";

    Winview::KeyOverrides overrides;
    overrides[static_cast<size_t>(EKeySet::LEFT)] = std::vector<std::string>{"y"};

    const auto SPECS = Winview::resolveKeySpecs(overrides, configured, "g");
    expect(SPECS[static_cast<size_t>(EKeySet::LEFT)] == std::vector<std::string>{"y"},
           "override was not applied");
    expect(SPECS[static_cast<size_t>(EKeySet::CLOSE)] == std::vector<std::string>{"x"},
           "partial override dropped the configured keys_close");
    expect(SPECS[static_cast<size_t>(EKeySet::DEFAULT_ACTION)] == std::vector<std::string>{"g"},
           "keys_go fallback was not used");
}

static void testOverviewCommands() {
    auto parsed = Winview::parseOverviewCommand(" Toggle, other_workspaces default-action=bring ");
    expect(parsed && parsed->action == EOverviewAction::TOGGLE &&
               !parsed->options.includeCurrentWorkspace &&
               parsed->options.defaultAction == EWinviewDefaultAction::BRING,
           "dispatcher arguments did not parse");

    parsed = Winview::parseOverviewCommand("");
    expect(parsed && parsed->action == EOverviewAction::TOGGLE, "empty argument is not toggle");
    expect(Winview::parseOverviewCommand("show filter")->action == EOverviewAction::OPEN,
           "show alias failed");
    expect(Winview::parseOverviewCommand("search-toggle")->action == EOverviewAction::TOGGLE_FILTER,
           "search-toggle alias failed");

    const auto TWO_ACTIONS = Winview::parseOverviewCommand("bring replace");
    expect(!TWO_ACTIONS && TWO_ACTIONS.error() == "multiple overview actions provided",
           "two actions were accepted");
    const auto UNKNOWN = Winview::parseOverviewCommand("toggle sideways");
    expect(!UNKNOWN && UNKNOWN.error() == "unknown overview argument: sideways",
           "unknown token was accepted");
    expect(!Winview::parseOverviewCommand("default-action=nope"), "bad default action accepted");
}

static std::optional<Winview::SOverviewCommand> g_command;
static std::optional<Winview::KeyOverrides>     g_overrides;

static int                                      luaOverview(lua_State* L) {
    return Winview::luaBoundary(L, [](lua_State* L) -> std::expected<void, std::string> {
        auto command = Winview::readOverviewCommand(L, 1);
        if (!command)
            return std::unexpected(std::move(command.error()));
        g_command = *command;
        return {};
    });
}

static int luaConfigure(lua_State* L) {
    return Winview::luaBoundary(L, [](lua_State* L) -> std::expected<void, std::string> {
        auto overrides = Winview::readKeyOverrides(L, 1);
        if (!overrides)
            return std::unexpected(std::move(overrides.error()));
        g_overrides = std::move(*overrides);
        return {};
    });
}

// Runs `chunk` under lua_pcall; returns the error message, or "" on success.
static std::string runLua(lua_State* L, const char* chunk) {
    g_command.reset();
    g_overrides.reset();
    if (luaL_loadstring(L, chunk) != LUA_OK || lua_pcall(L, 0, 0, 0) != LUA_OK) {
        std::string error = lua_tostring(L, -1);
        lua_pop(L, 1);
        return error;
    }
    return "";
}

static void expectLuaError(lua_State* L, const char* chunk, std::string_view expected) {
    const auto ERROR = runLua(L, chunk);
    if (ERROR != expected) {
        std::cerr << "chunk: " << chunk << "\nexpected: " << expected << "\nactual: " << ERROR
                  << '\n';
        std::exit(1);
    }
}

static void testLuaOverview(lua_State* L) {
    expect(runLua(L, "overview()").empty() && g_command->action == EOverviewAction::TOGGLE,
           "overview() did not toggle");
    expect(runLua(L, "overview('show other-workspaces')").empty() &&
               g_command->action == EOverviewAction::OPEN &&
               !g_command->options.includeCurrentWorkspace,
           "string overview argument failed");
    expect(runLua(L,
                  "overview({action = 'show', include_current_workspace = false, "
                  "start_in_filter_mode = true, default_action = 'bring_replace'})")
                   .empty() &&
               g_command->action == EOverviewAction::OPEN &&
               !g_command->options.includeCurrentWorkspace &&
               g_command->options.startInFilterMode &&
               g_command->options.defaultAction == EWinviewDefaultAction::BRING_REPLACE,
           "table overview argument failed");

    expectLuaError(L, "overview({default_action = 'bring replace'})",
                   "hyprwinview.overview: unknown default_action \"bring replace\"");
    expectLuaError(L, "overview({action = 'toggle filter'})",
                   "hyprwinview.overview: unknown action \"toggle filter\"");
    expectLuaError(L, "overview({filter_mode = 1})",
                   "hyprwinview.overview: field \"filter_mode\" must be a boolean");
    expectLuaError(L, "overview({action = 7})",
                   "hyprwinview.overview: field \"action\" must be a string");
    expectLuaError(L,
                   "overview({exclude_current_workspace = true, include_current_workspace = true})",
                   "hyprwinview.overview: fields \"exclude_current_workspace\" and "
                   "\"include_current_workspace\" conflict");
    expectLuaError(L, "overview({filter_mode = true, start_in_filter_mode = false})",
                   "hyprwinview.overview: fields \"filter_mode\" and \"start_in_filter_mode\" "
                   "conflict");
    expect(runLua(L,
                  "overview({exclude_current_workspace = false, include_current_workspace = "
                  "true, filter_mode = true, start_in_filter_mode = true})")
                   .empty() &&
               g_command->options.includeCurrentWorkspace && g_command->options.startInFilterMode,
           "agreeing alias fields were rejected");
    expectLuaError(L, "overview({actoin = 'show'})",
                   "hyprwinview.overview: unknown field \"actoin\"");
    expectLuaError(L, "overview(5)", "hyprwinview.overview: argument must be a string or table");
    expectLuaError(L, "overview('toggle sideways')",
                   "hyprwinview.overview: unknown overview argument: sideways");
    expectLuaError(L, "overview(setmetatable({}, {__index = function() error('meta') end}))", "");
}

static void testLuaConfigure(lua_State* L) {
    expect(
        runLua(L, "configure({keys = {left = {'a', 'shift + h'}, close = 'q, escape'}, go = {}})")
            .empty(),
        "valid configure failed");
    const auto& OVERRIDES = *g_overrides;
    expect(OVERRIDES[static_cast<size_t>(EKeySet::LEFT)] ==
               std::vector<std::string>{"a", "shift + h"},
           "array key set not read");
    expect(OVERRIDES[static_cast<size_t>(EKeySet::CLOSE)] ==
               std::vector<std::string>{"q", "escape"},
           "string key set not split");
    expect(OVERRIDES[static_cast<size_t>(EKeySet::DEFAULT_ACTION)] == std::vector<std::string>{},
           "go alias not read");
    expect(!OVERRIDES[static_cast<size_t>(EKeySet::RIGHT)], "unset key set was overridden");

    expectLuaError(L, "configure({keys = {left = 'a'}, keys_left = 'b'})",
                   "hyprwinview.configure: key set \"left\" is set more than once");
    expectLuaError(L, "configure({keys = {left = {'a', 3}}})",
                   "hyprwinview.configure: field \"left\" must be a string or an array of "
                   "strings");
    expectLuaError(L, "configure({keys = {left = {'a', x = 'b'}}})",
                   "hyprwinview.configure: field \"left\" must be a string or an array of "
                   "strings");
    expectLuaError(L, "configure({keys = {lefft = {'a'}}})",
                   "hyprwinview.configure: unknown field \"lefft\"");
    expectLuaError(L, "configure({keys = 'a'})",
                   "hyprwinview.configure: field \"keys\" must be a table");
    expectLuaError(L, "configure({{'a'}})", "hyprwinview.configure: table keys must be strings");
    expectLuaError(L, "configure()", "hyprwinview.configure: argument must be a table");
}

int main() {
    testKeyChords();
    testNavigationKeys();
    testFilterKeys();
    testConsumedKeys();
    testConsumedButtons();
    testKeyOverrideReloads();
    testKeyOverrides();
    testOverviewCommands();

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "overview", luaOverview);
    lua_register(L, "configure", luaConfigure);
    testLuaOverview(L);
    testLuaConfigure(L);
    lua_close(L);

    std::cout << "Input and Lua behavior tests passed\n";
}
