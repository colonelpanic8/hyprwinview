#include "keys.hpp"

#include <algorithm>
#include <ranges>

#include "../strings.hpp"

namespace Winview {
    namespace {
        xkb_keysym_t keysymForName(const std::string& key) {
            static constexpr std::array<std::pair<std::string_view, xkb_keysym_t>, 17> ALIASES = {{
                {"left", XKB_KEY_Left},
                {"arrowleft", XKB_KEY_Left},
                {"right", XKB_KEY_Right},
                {"arrowright", XKB_KEY_Right},
                {"up", XKB_KEY_Up},
                {"arrowup", XKB_KEY_Up},
                {"down", XKB_KEY_Down},
                {"arrowdown", XKB_KEY_Down},
                {"return", XKB_KEY_Return},
                {"enter", XKB_KEY_Return},
                {"space", XKB_KEY_space},
                {"escape", XKB_KEY_Escape},
                {"esc", XKB_KEY_Escape},
                {"bs", XKB_KEY_BackSpace},
                {"del", XKB_KEY_Delete},
                {"/", XKB_KEY_slash},
                {"slash", XKB_KEY_slash},
            }};

            for (const auto& [name, keysym] : ALIASES) {
                if (key == name)
                    return keysym;
            }

            return xkb_keysym_from_name(key.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
        }

        std::optional<std::uint32_t> modifierForName(std::string_view name) {
            if (name == "shift")
                return MOD_SHIFT;
            if (name == "ctrl" || name == "control")
                return MOD_CTRL;
            if (name == "alt")
                return MOD_ALT;
            if (name == "super" || name == "mod" || name == "meta")
                return MOD_SUPER;
            return std::nullopt;
        }

        struct SKeyRule {
            EKeySet    set;
            EKeyAction action;
        };

        constexpr std::array NAVIGATION_RULES = {
            SKeyRule{EKeySet::LEFT, EKeyAction::LEFT},
            SKeyRule{EKeySet::RIGHT, EKeyAction::RIGHT},
            SKeyRule{EKeySet::UP, EKeyAction::UP},
            SKeyRule{EKeySet::DOWN, EKeyAction::DOWN},
            SKeyRule{EKeySet::BRING_REPLACE, EKeyAction::BRING_REPLACE},
            SKeyRule{EKeySet::BRING, EKeyAction::BRING},
            SKeyRule{EKeySet::DEFAULT_ACTION, EKeyAction::DEFAULT_ACTION},
            SKeyRule{EKeySet::CLOSE, EKeyAction::CLOSE},
            SKeyRule{EKeySet::FILTER_TOGGLE, EKeyAction::TOGGLE_FILTER},
        };

        constexpr std::array FILTER_RULES = {
            SKeyRule{EKeySet::FILTER_CLOSE, EKeyAction::CLOSE},
            SKeyRule{EKeySet::FILTER_TOGGLE, EKeyAction::TOGGLE_FILTER},
            SKeyRule{EKeySet::FILTER_BRING_REPLACE, EKeyAction::BRING_REPLACE},
            SKeyRule{EKeySet::FILTER_BRING, EKeyAction::BRING},
        };

        // Navigation action sets apply in filter mode only through Return, which never types text.
        constexpr std::array FILTER_RETURN_RULES = {
            SKeyRule{EKeySet::BRING_REPLACE, EKeyAction::BRING_REPLACE},
            SKeyRule{EKeySet::BRING, EKeyAction::BRING},
            SKeyRule{EKeySet::DEFAULT_ACTION, EKeyAction::DEFAULT_ACTION},
        };

        constexpr std::array FILTER_NAVIGATION_RULES = {
            SKeyRule{EKeySet::FILTER_LEFT, EKeyAction::LEFT},
            SKeyRule{EKeySet::FILTER_RIGHT, EKeyAction::RIGHT},
            SKeyRule{EKeySet::FILTER_UP, EKeyAction::UP},
            SKeyRule{EKeySet::FILTER_DOWN, EKeyAction::DOWN},
        };

        bool isModifierKeysym(xkb_keysym_t keysym) {
            return (keysym >= XKB_KEY_Shift_L && keysym <= XKB_KEY_Hyper_R) ||
                (keysym >= XKB_KEY_ISO_Lock && keysym <= XKB_KEY_ISO_Level5_Lock) ||
                keysym == XKB_KEY_Mode_switch || keysym == XKB_KEY_Num_Lock;
        }

        template <std::size_t N>
        std::optional<EKeyAction> firstMatch(const std::array<SKeyRule, N>& rules,
                                             const SKeyBindings&            bindings,
                                             const SKeyChord&               pressed) {
            for (const auto& rule : rules) {
                if (bindings.matches(rule.set, pressed))
                    return rule.action;
            }
            return std::nullopt;
        }
    }

    std::vector<std::string> splitKeyList(std::string_view keys) {
        std::vector<std::string> result;
        for (const auto PART : std::views::split(keys, ',')) {
            auto token = trimmedLower(std::string_view(PART.begin(), PART.end()));
            if (!token.empty())
                result.push_back(std::move(token));
        }
        return result;
    }

    KeySpecs resolveKeySpecs(const KeyOverrides&             overrides,
                             const KeySetArray<std::string>& configured,
                             std::string_view                legacyDefaultAction) {
        KeySpecs specs;
        for (std::size_t i = 0; i < KEY_SET_COUNT; ++i)
            if (const auto& override = overrides[i])
                specs[i] = *override;
            else
                specs[i] = splitKeyList(configured[i]);

        constexpr auto DEFAULT_ACTION = static_cast<std::size_t>(EKeySet::DEFAULT_ACTION);
        if (!overrides[DEFAULT_ACTION] && specs[DEFAULT_ACTION].empty())
            specs[DEFAULT_ACTION] = splitKeyList(legacyDefaultAction);

        return specs;
    }

    std::optional<SKeyChord> parseKeyChord(std::string_view spec) {
        SKeyChord chord;
        for (const auto PART : std::views::split(spec, '+')) {
            const auto NAME = trimmedLower(std::string_view(PART.begin(), PART.end()));
            if (NAME.empty())
                return std::nullopt;

            if (const auto MOD = modifierForName(NAME)) {
                chord.mods |= *MOD;
                continue;
            }

            if (chord.keysym != XKB_KEY_NoSymbol)
                return std::nullopt;

            chord.keysym = keysymForName(NAME);
            if (chord.keysym == XKB_KEY_NoSymbol)
                return std::nullopt;
        }

        if (chord.keysym == XKB_KEY_NoSymbol)
            return std::nullopt;

        chord.keysym = xkb_keysym_to_lower(chord.keysym);
        return chord;
    }

    bool SKeyBindings::matches(EKeySet set, const SKeyChord& pressed) const {
        constexpr std::uint32_t HANDLED_MODS = MOD_SHIFT | MOD_CTRL | MOD_ALT | MOD_SUPER;
        const auto              LOWER        = xkb_keysym_to_lower(pressed.keysym);
        return std::ranges::any_of(chords[static_cast<std::size_t>(set)], [&](const SKeyChord& c) {
            return c.keysym == LOWER && c.mods == (pressed.mods & HANDLED_MODS);
        });
    }

    SKeyBindings compileKeyBindings(const KeySpecs& specs) {
        SKeyBindings bindings;
        for (std::size_t i = 0; i < KEY_SET_COUNT; ++i) {
            for (const auto& spec : specs[i]) {
                if (const auto CHORD = parseKeyChord(spec))
                    bindings.chords[i].push_back(*CHORD);
                else
                    bindings.invalid.push_back(std::string(KEY_SET_NAMES[i]) + ": " + spec);
            }
        }
        return bindings;
    }

    EKeyAction classifyKey(const SKeyBindings& bindings, bool filterMode, const SKeyChord& pressed,
                           bool producesText) {
        const auto KEYSYM = pressed.keysym;
        const auto MODS   = pressed.mods;
        if (!filterMode)
            return firstMatch(NAVIGATION_RULES, bindings, pressed).value_or(EKeyAction::PASS);

        if (const auto ACTION = firstMatch(FILTER_RULES, bindings, pressed))
            return *ACTION;

        if (KEYSYM == XKB_KEY_Return || KEYSYM == XKB_KEY_KP_Enter) {
            if (const auto ACTION = firstMatch(FILTER_RETURN_RULES, bindings, pressed))
                return *ACTION;
        }

        if (const auto ACTION = firstMatch(FILTER_NAVIGATION_RULES, bindings, pressed))
            return *ACTION;

        const bool GLOBAL_CHORD = (MODS & (MOD_ALT | MOD_SUPER)) != 0;
        if ((KEYSYM == XKB_KEY_BackSpace || KEYSYM == XKB_KEY_Delete) && !GLOBAL_CHORD)
            return EKeyAction::DELETE_CHARACTER;

        if ((MODS & MOD_CTRL) != 0 && !GLOBAL_CHORD && xkb_keysym_to_lower(KEYSYM) == XKB_KEY_u)
            return EKeyAction::CLEAR_FILTER;

        if (producesText)
            return EKeyAction::INSERT_TEXT;

        // Only global chords and bare modifiers reach Hyprland; anything else would land in the
        // client hidden under the overview.
        return GLOBAL_CHORD || isModifierKeysym(KEYSYM) ? EKeyAction::PASS : EKeyAction::SWALLOW;
    }

    void CConsumedKeys::press(std::uint32_t keycode, EKeyAction action) {
        std::erase_if(keys, [keycode](const auto& key) { return key.first == keycode; });
        keys.emplace_back(keycode, action);
    }

    std::optional<EKeyAction> CConsumedKeys::release(std::uint32_t keycode) {
        const auto IT =
            std::ranges::find(keys, keycode, &std::pair<std::uint32_t, EKeyAction>::first);
        if (IT == keys.end())
            return std::nullopt;

        const auto ACTION = IT->second;
        keys.erase(IT);
        return ACTION;
    }

    bool CConsumedKeys::held(std::uint32_t keycode) const {
        return std::ranges::contains(keys, keycode, &std::pair<std::uint32_t, EKeyAction>::first);
    }

    void CConsumedKeys::clear() {
        keys.clear();
    }

    void CConsumedButtons::press(std::uint32_t button) {
        if (!std::ranges::contains(buttons, button))
            buttons.push_back(button);
    }

    bool CConsumedButtons::release(std::uint32_t button) {
        return std::erase(buttons, button) > 0;
    }

    void CConsumedButtons::clear() {
        buttons.clear();
    }

    void CKeyOverrideState::configure(KeyOverrides overrides) {
        if (reloading) {
            staged = std::move(overrides);
            return;
        }

        current = std::move(overrides);
        ++changes;
    }

    void CKeyOverrideState::beginReload() {
        reloading = true;
        staged.reset();
    }

    void CKeyOverrideState::endReload(bool failed) {
        if (!reloading)
            return;

        reloading = false;
        if (staged) {
            current = std::move(*staged);
            staged.reset();
        } else if (!failed)
            current = {};
        else
            return;
        ++changes;
    }

    void CKeyOverrideState::reset() {
        current = {};
        staged.reset();
        reloading = false;
        ++changes;
    }

    const KeyOverrides& CKeyOverrideState::active() const {
        return current;
    }

    std::uint64_t CKeyOverrideState::generation() const {
        return changes;
    }
}
