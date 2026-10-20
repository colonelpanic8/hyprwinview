#ifndef HYPRWINVIEW_OVERVIEW_KEYS_HPP
#define HYPRWINVIEW_OVERVIEW_KEYS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <xkbcommon/xkbcommon.h>

namespace Winview {
    enum class EKeySet : std::uint8_t {
        LEFT,
        RIGHT,
        UP,
        DOWN,
        DEFAULT_ACTION,
        BRING,
        BRING_REPLACE,
        CLOSE,
        FILTER_TOGGLE,
        FILTER_CLOSE,
        FILTER_BRING,
        FILTER_BRING_REPLACE,
        FILTER_LEFT,
        FILTER_RIGHT,
        FILTER_UP,
        FILTER_DOWN,
    };

    inline constexpr std::size_t KEY_SET_COUNT = 16;

    template <typename T>
    using KeySetArray = std::array<T, KEY_SET_COUNT>;

    // Indexed by EKeySet: the Lua field name and the suffix of plugin:hyprwinview:keys_<name>.
    inline constexpr KeySetArray<std::string_view> KEY_SET_NAMES = {
        "left",          "right",          "up",
        "down",          "default_action", "bring",
        "bring_replace", "close",          "filter_toggle",
        "filter_close",  "filter_bring",   "filter_bring_replace",
        "filter_left",   "filter_right",   "filter_up",
        "filter_down",
    };

    constexpr std::string_view keySetName(EKeySet set) {
        return KEY_SET_NAMES[static_cast<std::size_t>(set)];
    }

    using KeySpecs     = KeySetArray<std::vector<std::string>>;
    using KeyOverrides = KeySetArray<std::optional<std::vector<std::string>>>;

    std::vector<std::string> splitKeyList(std::string_view keys);

    // `configured` holds the keys_* option strings. keys_default_action falls back to the legacy
    // keys_go option when empty.
    KeySpecs resolveKeySpecs(const KeyOverrides&             overrides,
                             const KeySetArray<std::string>& configured,
                             std::string_view                legacyDefaultAction);

    // Mirror Hyprland's HL_MODIFIER_* bits.
    inline constexpr std::uint32_t MOD_SHIFT = 1U << 0;
    inline constexpr std::uint32_t MOD_CTRL  = 1U << 2;
    inline constexpr std::uint32_t MOD_ALT   = 1U << 3;
    inline constexpr std::uint32_t MOD_SUPER = 1U << 6;

    struct SKeyChord {
        xkb_keysym_t  keysym = XKB_KEY_NoSymbol;
        std::uint32_t mods   = 0;
    };

    std::optional<SKeyChord> parseKeyChord(std::string_view spec);

    struct SKeyBindings {
        KeySetArray<std::vector<SKeyChord>> chords;
        std::vector<std::string>            invalid;

        bool                                matches(EKeySet set, const SKeyChord& pressed) const;
    };

    SKeyBindings compileKeyBindings(const KeySpecs& specs);

    enum class EKeyAction : std::uint8_t {
        PASS,
        LEFT,
        RIGHT,
        UP,
        DOWN,
        DEFAULT_ACTION,
        BRING,
        BRING_REPLACE,
        CLOSE,
        TOGGLE_FILTER,
        DELETE_CHARACTER,
        CLEAR_FILTER,
        INSERT_TEXT,
        SWALLOW,
    };

    EKeyAction classifyKey(const SKeyBindings& bindings, bool filterMode, const SKeyChord& pressed,
                           bool producesText);

    // Releases are consumed only for presses the overview consumed, so other key state stays
    // balanced for Hyprland binds and clients. Entries outlive the overview so releases that
    // arrive after it closes are still consumed.
    class CConsumedKeys {
      public:
        void                      press(std::uint32_t keycode, EKeyAction action);
        std::optional<EKeyAction> release(std::uint32_t keycode);
        bool                      held(std::uint32_t keycode) const;
        void                      clear();

      private:
        std::vector<std::pair<std::uint32_t, EKeyAction>> keys;
    };

    class CConsumedButtons {
      public:
        void press(std::uint32_t button);
        bool release(std::uint32_t button);
        void clear();

      private:
        std::vector<std::uint32_t> buttons;
    };

    // configure() calls made during a config reload are staged and applied when it finishes. A
    // failed reload that never reached configure(), such as a syntax error that keeps the old
    // config, keeps the current overrides.
    class CKeyOverrideState {
      public:
        void                configure(KeyOverrides overrides);
        void                beginReload();
        void                endReload(bool failed);
        void                reset();
        const KeyOverrides& active() const;
        // Bumped whenever the active overrides change.
        std::uint64_t generation() const;

      private:
        KeyOverrides                current;
        std::optional<KeyOverrides> staged;
        bool                        reloading = false;
        std::uint64_t               changes   = 0;
    };

    // Defined by the plugin.
    CKeyOverrideState& keyOverrides();
}

#endif
