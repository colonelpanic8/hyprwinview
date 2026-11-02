#ifndef HYPRWINVIEW_AUDIO_ATTRIBUTION_HPP
#define HYPRWINVIEW_AUDIO_ATTRIBUTION_HPP

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Ties audible streams to windows. Pure: callers supply every piece of evidence in a snapshot.
namespace Audio {
    using Pid = int32_t;

    // Audible streams only.
    struct SStream {
        uint32_t                id  = 0;
        Pid                     pid = 0; // 0 = unknown
        std::string             appName, binary, appId, mediaName;
        std::optional<uint32_t> x11Xid;

        bool                    operator==(const SStream&) const = default;
    };

    // Playing sessions only.
    struct SMediaSession {
        Pid         pid = 0;
        std::string title;

        bool        operator==(const SMediaSession&) const = default;
    };

    struct SWindow {
        uint64_t                key = 0; // opaque to attribution
        Pid                     pid = 0;
        std::string             cls, initialClass, title;
        std::optional<uint32_t> x11Xid;
    };

    struct SSnapshot {
        std::vector<SStream>                   streams;
        std::vector<SMediaSession>             sessions;
        std::vector<SWindow>                   windows;
        std::function<std::optional<Pid>(Pid)> parentOf;
    };

    enum class eOwnerVia : uint8_t {
        X11Window,
        Process,
        AppId,
    };

    enum class eDisambiguator : uint8_t {
        MediaSession,
    };

    enum class eAmbiguous : uint8_t {
        App,
        None,
    };

    // Every field is optional so rule layers merge field by field.
    struct SRule {
        std::optional<bool>                        ignore;
        std::optional<std::vector<eDisambiguator>> disambiguate;
        std::optional<eAmbiguous>                  ambiguous;

        bool                                       operator==(const SRule&) const = default;
    };

    struct SRules {
        SRule                                  fallback; // the user's "default"
        std::unordered_map<std::string, SRule> apps;     // keyed by lowercased window class

        bool                                   operator==(const SRules&) const = default;
    };

    enum class eIndicator : uint8_t {
        None,
        App,
        Window,
    }; // ordered by strength

    struct SAttribution {
        uint32_t                      streamId = 0;
        std::optional<eOwnerVia>      ownerVia;
        std::vector<uint64_t>         candidates;
        std::string                   ruleApp; // empty when no app rule applied
        std::optional<eDisambiguator> narrowedBy;
        std::vector<uint64_t>         windows;
        eIndicator                    indicator = eIndicator::None;
    };

    std::vector<SAttribution> attribute(const SSnapshot& snapshot, const SRules& rules);
    // Strongest indicator per window across all attributions.
    std::unordered_map<uint64_t, eIndicator>
                                  indicatorsByWindow(const std::vector<SAttribution>& attributions);

    std::optional<eDisambiguator> parseDisambiguator(std::string_view name);
    std::optional<eAmbiguous>     parseAmbiguous(std::string_view name);
    std::string_view              name(eOwnerVia via);
    std::string_view              name(eDisambiguator disambiguator);
    std::string_view              name(eIndicator indicator);
}

#endif
