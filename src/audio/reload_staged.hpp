#ifndef HYPRWINVIEW_AUDIO_RELOAD_STAGED_HPP
#define HYPRWINVIEW_AUDIO_RELOAD_STAGED_HPP

#include <cstdint>
#include <optional>
#include <utility>

namespace Audio {
    // Config set from Lua. Calls made during a config reload are staged and applied
    // when it finishes, and a successful reload that never called configure() restores the
    // default. A failed reload that never reached configure(), such as a syntax error that keeps
    // the old config, keeps the current value.
    template <typename T>
    class CReloadStaged {
      public:
        void configure(T value) {
            if (reloading) {
                staged = std::move(value);
                return;
            }

            current = std::move(value);
            ++changes;
        }

        void beginReload() {
            reloading = true;
            staged.reset();
        }

        void endReload(bool failed) {
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

        const T& active() const {
            return current;
        }

        // Bumped whenever the active value changes.
        std::uint64_t generation() const {
            return changes;
        }

      private:
        T                current{};
        std::optional<T> staged;
        bool             reloading = false;
        std::uint64_t    changes   = 0;
    };
}

#endif
