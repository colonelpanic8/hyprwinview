#ifndef HYPRWINVIEW_AUDIO_MEDIA_SESSIONS_HPP
#define HYPRWINVIEW_AUDIO_MEDIA_SESSIONS_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "attribution.hpp"
#include "source.hpp"

struct sd_bus;
struct sd_bus_slot;
struct sd_bus_message;
struct sd_bus_error;

namespace Audio {
    // Playing MPRIS players on the session bus, with each bus name's connection PID. All calls
    // are asynchronous and each is held in an owned slot, so destroying a player or the adapter
    // cancels its pending callbacks.
    class CMediaSessions final : public IEvidenceSource {
      public:
        explicit CMediaSessions(Logger logger);
        ~CMediaSessions() override;

        CMediaSessions(const CMediaSessions&)                             = delete;
        CMediaSessions&                  operator=(const CMediaSessions&) = delete;

        int                              fd() const override;
        short                            events() const override;
        std::optional<Clock::time_point> deadline() const override;
        void                             dispatch() override;
        bool                             failed() const override;
        void                             reset() override;
        uint64_t                         generation() const override;

        std::vector<SMediaSession>       sessions() const;

      private:
        struct SSlotUnref {
            void operator()(sd_bus_slot* slot) const;
        };
        using Slot = std::unique_ptr<sd_bus_slot, SSlotUnref>;

        struct SPlayer {
            CMediaSessions* sessions = nullptr;
            std::string     name, uniqueName, title;
            Pid             pid     = 0;
            bool            playing = false;
            Slot            ownerCall, pidCall, propertiesCall;
        };

        void       connect();
        void       disconnect();
        void       addPlayer(const std::string& name);
        void       removePlayer(const std::string& name);
        void       requestProperties(SPlayer& player);
        void       changed();

        static int onListNames(sd_bus_message* message, void* userdata, sd_bus_error* error);
        static int onNameOwnerChanged(sd_bus_message* message, void* userdata, sd_bus_error* error);
        static int onPropertiesChanged(sd_bus_message* message, void* userdata,
                                       sd_bus_error* error);
        static int onNameOwner(sd_bus_message* message, void* userdata, sd_bus_error* error);
        static int onPid(sd_bus_message* message, void* userdata, sd_bus_error* error);
        static int onProperties(sd_bus_message* message, void* userdata, sd_bus_error* error);

        CLogOnce   log;
        sd_bus*    bus    = nullptr;
        bool       broken = false;
        Clock::time_point retryAt;
        uint64_t          changes = 0;
        Slot              nameOwnerMatch, propertiesMatch, listCall;
        std::unordered_map<std::string, std::unique_ptr<SPlayer>> players;
    };
}

#endif
