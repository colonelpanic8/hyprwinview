#include "../overview.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

#define private   public
#define protected public
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#undef private
#undef protected

#include <xkbcommon/xkbcommon.h>

#include "../settings.hpp"

static_assert(Winview::MOD_SHIFT == HL_MODIFIER_SHIFT && Winview::MOD_CTRL == HL_MODIFIER_CTRL &&
              Winview::MOD_ALT == HL_MODIFIER_ALT && Winview::MOD_SUPER == HL_MODIFIER_META);

using KeySetting = const CConfigValue<Config::STRING>& (*)();

// Indexed by Winview::EKeySet.
static constexpr Winview::KeySetArray<KeySetting> KEY_SET_SETTINGS = {
    &WinviewConfig::PKEYSLEFT,
    &WinviewConfig::PKEYSRIGHT,
    &WinviewConfig::PKEYSUP,
    &WinviewConfig::PKEYSDOWN,
    &WinviewConfig::PKEYSDEFAULTACTION,
    &WinviewConfig::PKEYSBRING,
    &WinviewConfig::PKEYSBRINGREPLACE,
    &WinviewConfig::PKEYSCLOSE,
    &WinviewConfig::PKEYSFILTERTOGGLE,
    &WinviewConfig::PKEYSFILTERCLOSE,
    &WinviewConfig::PKEYSFILTERBRING,
    &WinviewConfig::PKEYSFILTERBRINGREPLACE,
    &WinviewConfig::PKEYSFILTERLEFT,
    &WinviewConfig::PKEYSFILTERRIGHT,
    &WinviewConfig::PKEYSFILTERUP,
    &WinviewConfig::PKEYSFILTERDOWN,
};

static Winview::CKeyOverrideState g_keyOverrides;
static Winview::CConsumedKeys     g_consumedKeys;
static Winview::CConsumedButtons  g_consumedButtons;

Winview::CKeyOverrideState&       Winview::keyOverrides() {
    return g_keyOverrides;
}

static Winview::SKeyBindings activeKeyBindings() {
    const auto&                       OVERRIDES = g_keyOverrides.active();
    Winview::KeySetArray<std::string> configured;
    for (size_t i = 0; i < Winview::KEY_SET_COUNT; ++i) {
        if (!OVERRIDES[i])
            configured[i] = *KEY_SET_SETTINGS[i]();
    }

    auto bindings = Winview::compileKeyBindings(
        Winview::resolveKeySpecs(OVERRIDES, configured, *WinviewConfig::PKEYSGO()));
    for (const auto& invalid : bindings.invalid)
        Log::logger->log(Log::WARN, "[hyprwinview] ignoring invalid key binding {}", invalid);
    return bindings;
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

bool CWindowOverview::isClosing() const {
    return closing;
}

bool CWindowOverview::handleKeyPress(const IKeyboard::SKeyEvent& event) {
    using Winview::EKeyAction;

    if (closing)
        return false;

    const auto KEYBOARD = keyboardForKeyEvent(event);
    if (!KEYBOARD || !KEYBOARD->m_xkbState)
        return false;

    if (!keyBindings || keyBindingsGeneration != g_keyOverrides.generation()) {
        keyBindings           = activeKeyBindings();
        keyBindingsGeneration = g_keyOverrides.generation();
    }

    const auto KEYCODE = event.keycode + 8;
    const auto KEYSYM  = xkb_state_key_get_one_sym(KEYBOARD->m_xkbState, KEYCODE);
    const auto MODS    = g_pInputManager->getModsFromAllKBs();
    const auto TEXT =
        filterMode ? printableTextForKey(KEYBOARD->m_xkbState, KEYCODE, MODS) : std::string();
    const auto ACTION = Winview::classifyKey(*keyBindings, filterMode,
                                             {.keysym = KEYSYM, .mods = MODS}, !TEXT.empty());
    if (ACTION == EKeyAction::PASS)
        return false;

    g_consumedKeys.press(event.keycode, ACTION);

    switch (ACTION) {
        case EKeyAction::PASS:
        case EKeyAction::SWALLOW: break;
        case EKeyAction::LEFT: moveSelection(-1, 0); break;
        case EKeyAction::RIGHT: moveSelection(1, 0); break;
        case EKeyAction::UP: moveSelection(0, -1); break;
        case EKeyAction::DOWN: moveSelection(0, 1); break;
        case EKeyAction::DEFAULT_ACTION: runDefaultSelected(); break;
        case EKeyAction::BRING: runSelected(true); break;
        case EKeyAction::BRING_REPLACE: runSelected(true, true); break;
        case EKeyAction::CLOSE: close(false); break;
        case EKeyAction::TOGGLE_FILTER: toggleFilterMode(); break;
        case EKeyAction::DELETE_CHARACTER:
            deleteFilterCharacter();
            startFilterDeleteRepeat();
            break;
        case EKeyAction::CLEAR_FILTER: setFilterQuery(""); break;
        case EKeyAction::INSERT_TEXT: setFilterQuery(filterQuery + TEXT); break;
    }

    return true;
}

void handleWindowOverviewKey(const IKeyboard::SKeyEvent& event, Event::SCallbackInfo& info) {
    bool consumed = false;
    if (event.state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        // A new press supersedes a consumed press whose release never arrived.
        g_consumedKeys.release(event.keycode);
        consumed = g_pWindowOverview && g_pWindowOverview->handleKeyPress(event);
    } else if (event.state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        const auto PRESSED = g_consumedKeys.release(event.keycode);
        if (PRESSED == Winview::EKeyAction::DELETE_CHARACTER && g_pWindowOverview)
            g_pWindowOverview->stopFilterDeleteRepeat();
        consumed = PRESSED.has_value();
    } else
        consumed = g_consumedKeys.held(event.keycode);

    if (consumed)
        info.cancelled = true;
}

void handleWindowOverviewButton(const IPointer::SButtonEvent& event, Event::SCallbackInfo& info) {
    if (event.state != WL_POINTER_BUTTON_STATE_PRESSED) {
        if (g_consumedButtons.release(event.button))
            info.cancelled = true;
        return;
    }

    // A new press supersedes a consumed press whose release never arrived.
    g_consumedButtons.release(event.button);
    if (!g_pWindowOverview)
        return;

    info.cancelled = true;
    g_consumedButtons.press(event.button);
    g_pWindowOverview->selectAt(g_pInputManager->getMouseCoordsInternal());
}

void resetWindowOverviewInput() {
    g_keyOverrides.reset();
    g_consumedKeys.clear();
    g_consumedButtons.clear();
}
