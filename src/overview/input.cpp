#include "../overview.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#define private   public
#define protected public
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#undef private
#undef protected

#include <xkbcommon/xkbcommon.h>

static const CConfigValue<Config::STRING>& PKEYSLEFT() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_left");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSRIGHT() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_right");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSUP() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_up");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSDOWN() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_down");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSGO() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_go");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSDEFAULTACTION() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_default_action");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSBRING() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_bring");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSBRINGREPLACE() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_bring_replace");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSCLOSE() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_close");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSFILTERTOGGLE() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_filter_toggle");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSFILTERCLOSE() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_filter_close");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSFILTERBRING() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_filter_bring");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSFILTERBRINGREPLACE() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_filter_bring_replace");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSFILTERLEFT() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_filter_left");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSFILTERRIGHT() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_filter_right");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSFILTERUP() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_filter_up");
    return VALUE;
}

static const CConfigValue<Config::STRING>& PKEYSFILTERDOWN() {
    static const CConfigValue<Config::STRING> VALUE("plugin:hyprwinview:keys_filter_down");
    return VALUE;
}

static std::optional<SWinviewKeyConfig> g_winviewKeyConfigOverride;

SWinviewKeyConfig                       defaultWinviewKeyConfig() {
    return {
                              .left               = {"a", "h", "left"},
                              .right              = {"d", "l", "right"},
                              .up                 = {"w", "k", "up"},
                              .down               = {"s", "j", "down"},
                              .defaultAction      = {"return", "enter", "space", "g", "f"},
                              .bring              = {"b", "shift+return", "shift+space"},
                              .bringReplace       = {"shift+b"},
                              .close              = {"escape", "q"},
                              .filterToggle       = {"/"},
                              .filterClose        = {"escape", "ctrl+g"},
                              .filterBring        = {"ctrl+b"},
                              .filterBringReplace = {"ctrl+shift+b"},
                              .filterLeft         = {"left", "ctrl+a", "super+a"},
                              .filterRight        = {"right", "ctrl+d", "super+d"},
                              .filterUp           = {"up", "ctrl+p", "ctrl+w", "super+w"},
                              .filterDown         = {"down", "ctrl+n", "ctrl+s", "super+s"},
    };
}

void setWinviewKeyConfig(SWinviewKeyConfig config) {
    g_winviewKeyConfigOverride = std::move(config);
}

static std::string trimmedLower(std::string token) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    token.erase(token.begin(), std::ranges::find_if(token, notSpace));
    token.erase(std::ranges::find_if(token.rbegin(), token.rend(), notSpace).base(), token.end());
    std::ranges::transform(token, token.begin(), [](unsigned char c) { return std::tolower(c); });
    return token;
}

static std::vector<std::string> keyTokens(const std::string& keys) {
    std::vector<std::string> result;
    std::stringstream        stream(keys);
    std::string              token;

    while (std::getline(stream, token, ',')) {
        token = trimmedLower(token);
        if (!token.empty())
            result.push_back(token);
    }

    return result;
}

static std::string normalizeKeyName(std::string key) {
    key = trimmedLower(key);

    if (key == "enter")
        return "return";
    if (key == "esc")
        return "escape";
    if (key == "arrowleft")
        return "left";
    if (key == "arrowright")
        return "right";
    if (key == "arrowup")
        return "up";
    if (key == "arrowdown")
        return "down";
    if (key == "backspace" || key == "bs")
        return "backspace";
    if (key == "delete" || key == "del")
        return "delete";
    if (key == "slash")
        return "/";

    return key;
}

static xkb_keysym_t keysymForName(const std::string& key) {
    const auto NORMALIZED = normalizeKeyName(key);

    if (NORMALIZED == "left")
        return XKB_KEY_Left;
    if (NORMALIZED == "right")
        return XKB_KEY_Right;
    if (NORMALIZED == "up")
        return XKB_KEY_Up;
    if (NORMALIZED == "down")
        return XKB_KEY_Down;
    if (NORMALIZED == "return")
        return XKB_KEY_Return;
    if (NORMALIZED == "space")
        return XKB_KEY_space;
    if (NORMALIZED == "escape")
        return XKB_KEY_Escape;
    if (NORMALIZED == "backspace")
        return XKB_KEY_BackSpace;
    if (NORMALIZED == "delete")
        return XKB_KEY_Delete;
    if (NORMALIZED == "/")
        return XKB_KEY_slash;

    return xkb_keysym_from_name(NORMALIZED.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
}

static bool tokenMatchesKey(const std::string& token, xkb_keysym_t keysym, uint32_t mods) {
    std::stringstream stream(token);
    std::string       part;
    uint32_t          requiredMods = 0;
    xkb_keysym_t      requiredKey  = XKB_KEY_NoSymbol;

    while (std::getline(stream, part, '+')) {
        part = normalizeKeyName(part);

        if (part == "shift")
            requiredMods |= HL_MODIFIER_SHIFT;
        else if (part == "ctrl" || part == "control")
            requiredMods |= HL_MODIFIER_CTRL;
        else if (part == "alt")
            requiredMods |= HL_MODIFIER_ALT;
        else if (part == "super" || part == "mod" || part == "meta")
            requiredMods |= HL_MODIFIER_META;
        else
            requiredKey = keysymForName(part);
    }

    constexpr uint32_t HANDLED_MODS =
        HL_MODIFIER_SHIFT | HL_MODIFIER_CTRL | HL_MODIFIER_ALT | HL_MODIFIER_META;

    return requiredKey != XKB_KEY_NoSymbol &&
        xkb_keysym_to_lower(requiredKey) == xkb_keysym_to_lower(keysym) &&
        (mods & HANDLED_MODS) == requiredMods;
}

static bool matchesKeySet(const std::vector<std::string>& keys, xkb_keysym_t keysym,
                          uint32_t mods) {
    for (const auto& token : keys) {
        if (tokenMatchesKey(token, keysym, mods))
            return true;
    }

    return false;
}

static std::string configStringOr(const CConfigValue<Config::STRING>& value,
                                  const std::string&                  fallback) {
    try {
        return *value;
    } catch (...) { return fallback; }
}

static SWinviewKeyConfig keyConfigFromConfigValues() {
    auto defaultAction = keyTokens(configStringOr(PKEYSDEFAULTACTION(), ""));
    if (defaultAction.empty())
        defaultAction = keyTokens(configStringOr(PKEYSGO(), "return,enter,space,g,f"));

    return {
        .left               = keyTokens(configStringOr(PKEYSLEFT(), "a,h,left")),
        .right              = keyTokens(configStringOr(PKEYSRIGHT(), "d,l,right")),
        .up                 = keyTokens(configStringOr(PKEYSUP(), "w,k,up")),
        .down               = keyTokens(configStringOr(PKEYSDOWN(), "s,j,down")),
        .defaultAction      = defaultAction,
        .bring              = keyTokens(configStringOr(PKEYSBRING(), "b,shift+return,shift+space")),
        .bringReplace       = keyTokens(configStringOr(PKEYSBRINGREPLACE(), "shift+b")),
        .close              = keyTokens(configStringOr(PKEYSCLOSE(), "escape,q")),
        .filterToggle       = keyTokens(configStringOr(PKEYSFILTERTOGGLE(), "/")),
        .filterClose        = keyTokens(configStringOr(PKEYSFILTERCLOSE(), "escape,ctrl+g")),
        .filterBring        = keyTokens(configStringOr(PKEYSFILTERBRING(), "ctrl+b")),
        .filterBringReplace = keyTokens(configStringOr(PKEYSFILTERBRINGREPLACE(), "ctrl+shift+b")),
        .filterLeft         = keyTokens(configStringOr(PKEYSFILTERLEFT(), "left,ctrl+a,super+a")),
        .filterRight        = keyTokens(configStringOr(PKEYSFILTERRIGHT(), "right,ctrl+d,super+d")),
        .filterUp   = keyTokens(configStringOr(PKEYSFILTERUP(), "up,ctrl+p,ctrl+w,super+w")),
        .filterDown = keyTokens(configStringOr(PKEYSFILTERDOWN(), "down,ctrl+n,ctrl+s,super+s")),
    };
}

static SWinviewKeyConfig activeKeyConfig() {
    return g_winviewKeyConfigOverride.value_or(keyConfigFromConfigValues());
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static std::string printableTextForKey(xkb_state* state, xkb_keycode_t keycode, uint32_t mods) {
    constexpr uint32_t TEXT_BLOCKING_MODS = HL_MODIFIER_CTRL | HL_MODIFIER_ALT | HL_MODIFIER_META;
    if (!state || (mods & TEXT_BLOCKING_MODS) != 0)
        return "";

    char      buffer[64] = {};
    const int len        = xkb_state_key_get_utf8(state, keycode, buffer, sizeof(buffer));
    if (len <= 0)
        return "";

    std::string result(buffer, std::min<int>(len, sizeof(buffer) - 1));
    if (std::ranges::any_of(result, [](unsigned char c) { return std::iscntrl(c); }))
        return "";

    return result;
}

static SP<IKeyboard> keyboardForKeyEvent(const IKeyboard::SKeyEvent& event) {
    if (event.state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        for (const auto& keyboard : g_pInputManager->m_keyboards) {
            if (keyboard && keyboard->m_enabled && keyboard->getPressed(event.keycode))
                return keyboard;
        }
    }

    return g_pSeatManager && !g_pSeatManager->m_keyboard.expired() ?
        g_pSeatManager->m_keyboard.lock() :
        nullptr;
}

bool CWindowOverview::handleKey(const IKeyboard::SKeyEvent& event) {
    const auto KEYBOARD = keyboardForKeyEvent(event);
    if (!KEYBOARD || !KEYBOARD->m_xkbState)
        return false;

    const auto KEYCODE = event.keycode + 8;
    const auto KEYSYM  = xkb_state_key_get_one_sym(KEYBOARD->m_xkbState, KEYCODE);
    const auto MODS    = g_pInputManager->getModsFromAllKBs();
    const auto KEYS    = activeKeyConfig();

    if (filterMode)
        return handleFilterKey(event, KEYSYM, KEYBOARD->m_xkbState, MODS, KEYS);

    const bool RECOGNIZED = matchesKeySet(KEYS.left, KEYSYM, MODS) ||
        matchesKeySet(KEYS.right, KEYSYM, MODS) || matchesKeySet(KEYS.up, KEYSYM, MODS) ||
        matchesKeySet(KEYS.down, KEYSYM, MODS) || matchesKeySet(KEYS.defaultAction, KEYSYM, MODS) ||
        matchesKeySet(KEYS.bring, KEYSYM, MODS) || matchesKeySet(KEYS.bringReplace, KEYSYM, MODS) ||
        matchesKeySet(KEYS.close, KEYSYM, MODS) || matchesKeySet(KEYS.filterToggle, KEYSYM, MODS);

    if (!RECOGNIZED)
        return false;

    if (event.state != WL_KEYBOARD_KEY_STATE_PRESSED)
        return true;

    if (matchesKeySet(KEYS.left, KEYSYM, MODS))
        moveSelection(-1, 0);
    else if (matchesKeySet(KEYS.right, KEYSYM, MODS))
        moveSelection(1, 0);
    else if (matchesKeySet(KEYS.up, KEYSYM, MODS))
        moveSelection(0, -1);
    else if (matchesKeySet(KEYS.down, KEYSYM, MODS))
        moveSelection(0, 1);
    else if (matchesKeySet(KEYS.bringReplace, KEYSYM, MODS))
        runSelected(true, true);
    else if (matchesKeySet(KEYS.bring, KEYSYM, MODS))
        runSelected(true);
    else if (matchesKeySet(KEYS.defaultAction, KEYSYM, MODS))
        runDefaultSelected();
    else if (matchesKeySet(KEYS.close, KEYSYM, MODS))
        close(false);
    else if (matchesKeySet(KEYS.filterToggle, KEYSYM, MODS))
        toggleFilterMode();

    return true;
}

bool CWindowOverview::handleFilterKey(const IKeyboard::SKeyEvent& event, xkb_keysym_t keysym,
                                      xkb_state* keyboardState, uint32_t mods,
                                      const SWinviewKeyConfig& keys) {
    const bool DELETE_KEY = keysym == XKB_KEY_BackSpace || keysym == XKB_KEY_Delete;
    if (event.state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        if (DELETE_KEY)
            stopFilterDeleteRepeat();
        return true;
    }

    if (event.state != WL_KEYBOARD_KEY_STATE_PRESSED)
        return true;

    if (matchesKeySet(keys.filterClose, keysym, mods)) {
        close(false);
        return true;
    }

    if (matchesKeySet(keys.filterToggle, keysym, mods)) {
        toggleFilterMode();
        return true;
    }

    if (matchesKeySet(keys.filterBringReplace, keysym, mods)) {
        runSelected(true, true);
        return true;
    }

    if (matchesKeySet(keys.filterBring, keysym, mods)) {
        runSelected(true);
        return true;
    }

    const bool RETURN_KEY = keysym == XKB_KEY_Return || keysym == XKB_KEY_KP_Enter;
    if (RETURN_KEY && matchesKeySet(keys.bringReplace, keysym, mods)) {
        runSelected(true, true);
        return true;
    }

    if (RETURN_KEY && matchesKeySet(keys.bring, keysym, mods)) {
        runSelected(true);
        return true;
    }

    if (RETURN_KEY && matchesKeySet(keys.defaultAction, keysym, mods)) {
        runDefaultSelected();
        return true;
    }

    if (matchesKeySet(keys.filterLeft, keysym, mods)) {
        moveSelection(-1, 0);
        return true;
    }

    if (matchesKeySet(keys.filterRight, keysym, mods)) {
        moveSelection(1, 0);
        return true;
    }

    if (matchesKeySet(keys.filterUp, keysym, mods)) {
        moveSelection(0, -1);
        return true;
    }

    if (matchesKeySet(keys.filterDown, keysym, mods)) {
        moveSelection(0, 1);
        return true;
    }

    if (DELETE_KEY) {
        deleteFilterCharacter();
        startFilterDeleteRepeat();
        return true;
    }

    if ((mods & HL_MODIFIER_CTRL) != 0 &&
        xkb_keysym_to_lower(keysym) == xkb_keysym_to_lower(XKB_KEY_u)) {
        setFilterQuery("");
        return true;
    }

    const auto TEXT = printableTextForKey(keyboardState, event.keycode + 8, mods);
    if (!TEXT.empty()) {
        setFilterQuery(filterQuery + TEXT);
        return true;
    }

    return true;
}
