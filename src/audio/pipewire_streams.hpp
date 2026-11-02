#ifndef HYPRWINVIEW_AUDIO_PIPEWIRE_STREAMS_HPP
#define HYPRWINVIEW_AUDIO_PIPEWIRE_STREAMS_HPP

#include <sys/types.h>

#include "pw_dump.hpp"
#include "source.hpp"

namespace Audio {
    // Audible PipeWire playback streams, read from a `pw-dump -m -N` child so the audio stack
    // never loads into this process. The child is respawned after RETRY_DELAY when it exits.
    class CPipeWireStreams final : public IEvidenceSource {
      public:
        explicit CPipeWireStreams(Logger logger);
        ~CPipeWireStreams() override;

        CPipeWireStreams(const CPipeWireStreams&)                           = delete;
        CPipeWireStreams&                operator=(const CPipeWireStreams&) = delete;

        int                              fd() const override;
        short                            events() const override;
        std::optional<Clock::time_point> deadline() const override;
        void                             dispatch() override;
        bool                             failed() const override;
        void                             reset() override;
        uint64_t                         generation() const override;

        const std::vector<SStream>&      streams() const;

      private:
        void              spawn();
        void              stopChild();

        CLogOnce          log;
        CPwDumpReader     reader;
        pid_t             child  = -1;
        int               output = -1;
        bool              broken = false;
        Clock::time_point retryAt;
        uint64_t          changes = 0;
    };
}

#endif
