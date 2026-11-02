#include "attribution.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <utility>

namespace Audio {
    namespace {
        constexpr std::size_t MIN_NEEDLE_LENGTH = 6;
        constexpr int         MAX_PARENT_HOPS   = 64;

        std::string           lower(std::string_view text) {
            std::string result(text);
            std::ranges::transform(result, result.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return result;
        }

        // ASCII casefold with whitespace runs collapsed to one space and trimmed.
        std::string normalized(std::string_view text) {
            std::string result;
            result.reserve(text.size());
            bool pendingSpace = false;
            for (const unsigned char C : text) {
                if (std::isspace(C)) {
                    pendingSpace = !result.empty();
                    continue;
                }
                if (pendingSpace)
                    result.push_back(' ');
                pendingSpace = false;
                result.push_back(static_cast<char>(std::tolower(C)));
            }
            return result;
        }

        struct SIndex {
            const SSnapshot&                                  snapshot;
            std::unordered_map<Pid, std::vector<std::size_t>> windowsByPid;
        };

        struct SStreamEvidence {
            const SStream&   stream;
            std::vector<Pid> lineage; // the stream's PID, then its ancestors
        };

        using Candidates = std::vector<std::size_t>;

        std::vector<Pid> lineageOf(Pid pid, const SSnapshot& snapshot) {
            std::vector<Pid> lineage;
            while (pid > 1 && lineage.size() < MAX_PARENT_HOPS &&
                   std::ranges::find(lineage, pid) == lineage.end()) {
                lineage.push_back(pid);
                const auto PARENT = snapshot.parentOf ? snapshot.parentOf(pid) : std::nullopt;
                if (!PARENT)
                    break;
                pid = *PARENT;
            }
            return lineage;
        }

        Candidates ownerByX11Window(const SIndex& index, const SStreamEvidence& evidence) {
            Candidates candidates;
            if (!evidence.stream.x11Xid)
                return candidates;
            const auto& WINDOWS = index.snapshot.windows;
            for (std::size_t i = 0; i < WINDOWS.size(); ++i) {
                if (WINDOWS[i].x11Xid == evidence.stream.x11Xid)
                    candidates.push_back(i);
            }
            return candidates;
        }

        Candidates ownerByProcess(const SIndex& index, const SStreamEvidence& evidence) {
            for (const auto PID : evidence.lineage) {
                if (const auto IT = index.windowsByPid.find(PID); IT != index.windowsByPid.end())
                    return IT->second;
            }
            return {};
        }

        Candidates ownerByAppId(const SIndex& index, const SStreamEvidence& evidence) {
            std::vector<std::string> ids;
            for (const auto& id :
                 {evidence.stream.appId, evidence.stream.binary, evidence.stream.appName}) {
                if (!id.empty())
                    ids.push_back(lower(id));
            }

            Candidates  candidates;
            const auto& WINDOWS = index.snapshot.windows;
            for (std::size_t i = 0; i < WINDOWS.size(); ++i) {
                const auto CLS     = lower(WINDOWS[i].cls);
                const auto INITIAL = lower(WINDOWS[i].initialClass);
                if (std::ranges::any_of(ids, [&](const auto& id) {
                        return (!CLS.empty() && id == CLS) || (!INITIAL.empty() && id == INITIAL);
                    }))
                    candidates.push_back(i);
            }
            return candidates;
        }

        struct SOwnerStrategy {
            eOwnerVia via;
            Candidates (*find)(const SIndex&, const SStreamEvidence&);
        };

        // First strategy with any candidate wins.
        constexpr std::array OWNER_STRATEGIES = {
            SOwnerStrategy{eOwnerVia::X11Window, ownerByX11Window},
            SOwnerStrategy{eOwnerVia::Process, ownerByProcess},
            SOwnerStrategy{eOwnerVia::AppId, ownerByAppId},
        };

        // Playerctld-style proxies re-export other players under their own PID, so a session
        // counts only when its PID belongs to the stream's process lineage or to a candidate.
        std::vector<std::string> mediaSessionNeedles(const SIndex&          index,
                                                     const SStreamEvidence& evidence,
                                                     const Candidates&      candidates) {
            const auto& WINDOWS = index.snapshot.windows;
            auto        owned   = [&](Pid pid) {
                return std::ranges::find(evidence.lineage, pid) != evidence.lineage.end() ||
                    std::ranges::any_of(candidates,
                                                 [&](std::size_t i) { return WINDOWS[i].pid == pid; });
            };

            std::vector<std::string> needles;
            for (const auto& session : index.snapshot.sessions) {
                if (session.pid > 0 && owned(session.pid))
                    needles.push_back(session.title);
            }
            return needles;
        }

        struct SDisambiguatorEntry {
            eDisambiguator   id;
            std::string_view name;
            std::vector<std::string> (*needles)(const SIndex&, const SStreamEvidence&,
                                                const Candidates&);
        };

        // Indexed by eDisambiguator. A disambiguator only produces needles; matching them against
        // window titles is shared.
        constexpr std::array DISAMBIGUATORS = {
            SDisambiguatorEntry{eDisambiguator::MediaSession, "media_session", mediaSessionNeedles},
        };

        constexpr bool disambiguatorsIndexedByEnum() {
            for (std::size_t i = 0; i < DISAMBIGUATORS.size(); ++i) {
                if (static_cast<std::size_t>(DISAMBIGUATORS[i].id) != i)
                    return false;
            }
            return true;
        }
        static_assert(disambiguatorsIndexedByEnum());

        Candidates narrow(const SIndex& index, const Candidates& candidates,
                          const std::vector<std::string>& rawNeedles) {
            std::vector<std::string> needles;
            for (const auto& raw : rawNeedles) {
                auto needle = normalized(raw);
                if (needle.size() >= MIN_NEEDLE_LENGTH)
                    needles.push_back(std::move(needle));
            }

            Candidates matched;
            if (needles.empty())
                return matched;
            for (const auto I : candidates) {
                const auto TITLE = normalized(index.snapshot.windows[I].title);
                if (std::ranges::any_of(needles, [&](const auto& needle) {
                        return TITLE.find(needle) != std::string::npos;
                    }))
                    matched.push_back(I);
            }
            return matched;
        }

        struct SResolvedRule {
            bool                        ignore = false;
            std::vector<eDisambiguator> disambiguate;
            eAmbiguous                  ambiguous = eAmbiguous::App;
        };

        const SRule HARDCODED_DEFAULT{
            .ignore       = false,
            .disambiguate = std::vector{eDisambiguator::MediaSession},
            .ambiguous    = eAmbiguous::App,
        };

        // Earlier layers win, field by field.
        SResolvedRule mergeRules(std::initializer_list<const SRule*> layers) {
            auto first = [&](auto field) {
                for (const auto* layer : layers) {
                    if (layer && layer->*field)
                        return *(layer->*field);
                }
                return *(HARDCODED_DEFAULT.*field);
            };
            return {
                .ignore       = first(&SRule::ignore),
                .disambiguate = first(&SRule::disambiguate),
                .ambiguous    = first(&SRule::ambiguous),
            };
        }

        // The most frequent candidate class that has an app rule, class before initialClass;
        // ties go to the earlier candidate.
        std::string ruleAppFor(const SIndex& index, const Candidates& candidates,
                               const SRules& rules) {
            std::vector<std::pair<std::string, std::size_t>> counts;
            for (const auto I : candidates) {
                const auto& WINDOW = index.snapshot.windows[I];
                for (const auto& cls : {lower(WINDOW.cls), lower(WINDOW.initialClass)}) {
                    if (!rules.apps.contains(cls))
                        continue;
                    auto it =
                        std::ranges::find(counts, cls, &std::pair<std::string, std::size_t>::first);
                    if (it == counts.end())
                        counts.emplace_back(cls, 1);
                    else
                        ++it->second;
                    break;
                }
            }

            const auto BEST = std::ranges::max_element(
                counts, [](const auto& a, const auto& b) { return a.second < b.second; });
            return BEST == counts.end() ? std::string{} : BEST->first;
        }

        std::vector<uint64_t> keysOf(const SIndex& index, const Candidates& candidates) {
            std::vector<uint64_t> keys;
            keys.reserve(candidates.size());
            for (const auto I : candidates)
                keys.push_back(index.snapshot.windows[I].key);
            return keys;
        }

        SAttribution attributeStream(const SIndex& index, const SStream& stream,
                                     const SRules& rules) {
            SAttribution          result{.streamId = stream.id};
            const SStreamEvidence EVIDENCE{stream, lineageOf(stream.pid, index.snapshot)};

            Candidates            candidates;
            for (const auto& strategy : OWNER_STRATEGIES) {
                candidates = strategy.find(index, EVIDENCE);
                if (!candidates.empty()) {
                    result.ownerVia = strategy.via;
                    break;
                }
            }
            if (candidates.empty())
                return result;
            result.candidates = keysOf(index, candidates);

            result.ruleApp    = ruleAppFor(index, candidates, rules);
            const auto APP_IT = rules.apps.find(result.ruleApp);
            const auto RULE   = mergeRules(
                {APP_IT == rules.apps.end() ? nullptr : &APP_IT->second, &rules.fallback});
            if (RULE.ignore)
                return result;

            if (candidates.size() == 1) {
                result.windows   = result.candidates;
                result.indicator = eIndicator::Window;
                return result;
            }

            for (const auto ID : RULE.disambiguate) {
                const auto& ENTRY = DISAMBIGUATORS[static_cast<std::size_t>(ID)];
                const auto  MATCHED =
                    narrow(index, candidates, ENTRY.needles(index, EVIDENCE, candidates));
                if (!MATCHED.empty() && MATCHED.size() < candidates.size()) {
                    result.narrowedBy = ID;
                    result.windows    = keysOf(index, MATCHED);
                    result.indicator  = eIndicator::Window;
                    return result;
                }
            }

            if (RULE.ambiguous == eAmbiguous::App) {
                result.windows   = result.candidates;
                result.indicator = eIndicator::App;
            }
            return result;
        }
    }

    std::vector<SAttribution> attribute(const SSnapshot& snapshot, const SRules& rules) {
        SIndex index{.snapshot = snapshot, .windowsByPid = {}};
        for (std::size_t i = 0; i < snapshot.windows.size(); ++i) {
            if (snapshot.windows[i].pid > 0)
                index.windowsByPid[snapshot.windows[i].pid].push_back(i);
        }

        std::vector<SAttribution> attributions;
        attributions.reserve(snapshot.streams.size());
        for (const auto& stream : snapshot.streams)
            attributions.push_back(attributeStream(index, stream, rules));
        return attributions;
    }

    std::unordered_map<uint64_t, eIndicator>
    indicatorsByWindow(const std::vector<SAttribution>& attributions) {
        std::unordered_map<uint64_t, eIndicator> indicators;
        for (const auto& attribution : attributions) {
            for (const auto KEY : attribution.windows) {
                auto& slot = indicators[KEY];
                slot       = std::max(slot, attribution.indicator);
            }
        }
        return indicators;
    }

    std::optional<eDisambiguator> parseDisambiguator(std::string_view name) {
        const auto IT = std::ranges::find(DISAMBIGUATORS, name, &SDisambiguatorEntry::name);
        if (IT == DISAMBIGUATORS.end())
            return std::nullopt;
        return IT->id;
    }

    std::optional<eAmbiguous> parseAmbiguous(std::string_view name) {
        if (name == "app")
            return eAmbiguous::App;
        if (name == "none")
            return eAmbiguous::None;
        return std::nullopt;
    }

    std::string_view name(eOwnerVia via) {
        switch (via) {
            case eOwnerVia::X11Window: return "x11_window";
            case eOwnerVia::Process: return "process";
            case eOwnerVia::AppId: return "app_id";
        }
        return "?";
    }

    std::string_view name(eDisambiguator disambiguator) {
        return DISAMBIGUATORS[static_cast<std::size_t>(disambiguator)].name;
    }

    std::string_view name(eIndicator indicator) {
        switch (indicator) {
            case eIndicator::None: return "none";
            case eIndicator::App: return "app";
            case eIndicator::Window: return "window";
        }
        return "?";
    }
}
