#ifndef HYPRWINVIEW_AUDIO_PW_DUMP_HPP
#define HYPRWINVIEW_AUDIO_PW_DUMP_HPP

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "attribution.hpp"

namespace Audio {
    // Tracks playback streams from `pw-dump -m -N` output. Each update is a top-level JSON
    // array whose closing bracket sits alone on a line; a removed object appears as
    // {"id": N, "info": null}.
    class CPwDumpReader {
      public:
        // Returns whether the audible streams changed.
        std::expected<bool, std::string> feed(std::string_view bytes);
        const std::vector<SStream>&      audibleStreams() const;
        void                             clear();

        struct SProps {
            std::string             api, appName, binary, appId, mediaName;
            std::optional<Pid>      processId, secPid;
            std::optional<uint32_t> x11Xid, clientId;
        };

      private:
        struct SNode {
            SProps props;
            bool   audible = false;
        };

        std::expected<void, std::string>     applyBatch(const std::string& json);
        std::vector<SStream>                 computeStreams() const;

        std::string                          pending;
        std::unordered_map<uint32_t, SNode>  nodes;
        std::unordered_map<uint32_t, SProps> clients;
        std::vector<SStream>                 streams;
    };
}

#endif
