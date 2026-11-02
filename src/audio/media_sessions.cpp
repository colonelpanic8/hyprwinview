#include "media_sessions.hpp"

#include <systemd/sd-bus.h>

#include <cstring>
#include <format>
#include <poll.h>
#include <string_view>

namespace Audio {
    namespace {
        constexpr std::string_view MPRIS_PREFIX = "org.mpris.MediaPlayer2.";
        constexpr const char*      MPRIS_PATH   = "/org/mpris/MediaPlayer2";
        constexpr const char*      PLAYER_IFACE = "org.mpris.MediaPlayer2.Player";
        constexpr const char*      DBUS_NAME    = "org.freedesktop.DBus";
        constexpr const char*      DBUS_PATH    = "/org/freedesktop/DBus";

        bool                       isPlayerName(std::string_view name) {
            return name.starts_with(MPRIS_PREFIX);
        }

        // Reads a variant holding `signature` with `read`, or skips a variant of any other type.
        template <typename F>
        int readVariant(sd_bus_message* message, std::string_view signature, F read) {
            char        type     = 0;
            const char* contents = nullptr;
            int         r        = sd_bus_message_peek_type(message, &type, &contents);
            if (r <= 0)
                return r < 0 ? r : -EBADMSG;
            if (type != SD_BUS_TYPE_VARIANT || !contents || signature != contents)
                return sd_bus_message_skip(message, "v");

            r = sd_bus_message_enter_container(message, SD_BUS_TYPE_VARIANT, contents);
            if (r < 0)
                return r;
            r = read();
            if (r < 0)
                return r;
            return sd_bus_message_exit_container(message);
        }

        int readString(sd_bus_message* message, std::string& out) {
            const char* text = nullptr;
            const int   r    = sd_bus_message_read(message, "s", &text);
            if (r >= 0)
                out = text ? text : "";
            return r;
        }

        // Calls `entry(key)` for each a{sv} entry; `entry` must consume the variant.
        template <typename F>
        int readDictionary(sd_bus_message* message, F entry) {
            int r = sd_bus_message_enter_container(message, SD_BUS_TYPE_ARRAY, "{sv}");
            if (r < 0)
                return r;
            while ((r = sd_bus_message_enter_container(message, SD_BUS_TYPE_DICT_ENTRY, "sv")) >
                   0) {
                const char* key = nullptr;
                r               = sd_bus_message_read(message, "s", &key);
                if (r < 0)
                    return r;
                r = entry(std::string_view(key ? key : ""));
                if (r < 0)
                    return r;
                r = sd_bus_message_exit_container(message);
                if (r < 0)
                    return r;
            }
            if (r < 0)
                return r;
            return sd_bus_message_exit_container(message);
        }

        struct SPlayerState {
            std::string status, title;
            bool        hasStatus = false, hasTitle = false;
        };

        int readPlayerProperties(sd_bus_message* message, SPlayerState& state) {
            return readDictionary(message, [&](std::string_view key) {
                if (key == "PlaybackStatus")
                    return readVariant(message, "s", [&] {
                        state.hasStatus = true;
                        return readString(message, state.status);
                    });
                if (key == "Metadata")
                    return readVariant(message, "a{sv}", [&] {
                        state.hasTitle = true;
                        state.title.clear();
                        return readDictionary(message, [&](std::string_view field) {
                            if (field != "xesam:title")
                                return sd_bus_message_skip(message, "v");
                            return readVariant(message, "s",
                                               [&] { return readString(message, state.title); });
                        });
                    });
                return sd_bus_message_skip(message, "v");
            });
        }

        bool methodFailed(sd_bus_message* message) {
            return sd_bus_message_is_method_error(message, nullptr) != 0;
        }
    }

    void CMediaSessions::SSlotUnref::operator()(sd_bus_slot* slot) const {
        sd_bus_slot_unref(slot);
    }

    CMediaSessions::CMediaSessions(Logger logger) : log(std::move(logger)) {}

    CMediaSessions::~CMediaSessions() {
        disconnect();
    }

    int CMediaSessions::fd() const {
        return bus ? sd_bus_get_fd(bus) : -1;
    }

    short CMediaSessions::events() const {
        const int EVENTS = bus ? sd_bus_get_events(bus) : 0;
        return static_cast<short>(EVENTS > 0 ? EVENTS : 0);
    }

    std::optional<Clock::time_point> CMediaSessions::deadline() const {
        if (!bus)
            return retryAt;
        uint64_t usec = 0;
        if (sd_bus_get_timeout(bus, &usec) < 0 || usec == UINT64_MAX)
            return std::nullopt;
        // sd-bus timeouts are CLOCK_MONOTONIC, the steady_clock epoch on Linux.
        return Clock::time_point(std::chrono::microseconds(usec));
    }

    bool CMediaSessions::failed() const {
        return broken;
    }

    uint64_t CMediaSessions::generation() const {
        return changes;
    }

    std::vector<SMediaSession> CMediaSessions::sessions() const {
        std::vector<SMediaSession> result;
        for (const auto& [name, player] : players) {
            if (player->playing && player->pid > 0)
                result.push_back({.pid = player->pid, .title = player->title});
        }
        return result;
    }

    void CMediaSessions::changed() {
        ++changes;
    }

    void CMediaSessions::connect() {
        auto fail = [this](std::string_view what, int r) {
            log(std::format("audio: MPRIS {} failed: {}", what, std::strerror(-r)));
            disconnect();
            retryAt = Clock::now() + RETRY_DELAY;
        };

        int r = sd_bus_open_user(&bus);
        if (r < 0) {
            bus = nullptr;
            fail("session bus connection", r);
            return;
        }

        sd_bus_slot* slot = nullptr;
        r                 = sd_bus_add_match_async(
            bus, &slot,
            "type='signal',sender='org.freedesktop.DBus',interface='org.freedesktop.DBus',"
                            "member='NameOwnerChanged',arg0namespace='org.mpris.MediaPlayer2'",
            onNameOwnerChanged, nullptr, this);
        nameOwnerMatch.reset(slot);
        if (r < 0)
            return fail("NameOwnerChanged match", r);

        slot = nullptr;
        r    = sd_bus_add_match_async(
            bus, &slot,
            "type='signal',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged',"
               "path='/org/mpris/MediaPlayer2',arg0='org.mpris.MediaPlayer2.Player'",
            onPropertiesChanged, nullptr, this);
        propertiesMatch.reset(slot);
        if (r < 0)
            return fail("PropertiesChanged match", r);

        slot = nullptr;
        r    = sd_bus_call_method_async(bus, &slot, DBUS_NAME, DBUS_PATH, DBUS_NAME, "ListNames",
                                        onListNames, this, "");
        listCall.reset(slot);
        if (r < 0)
            return fail("ListNames", r);
    }

    void CMediaSessions::disconnect() {
        if (!players.empty())
            changed();
        players.clear();
        nameOwnerMatch.reset();
        propertiesMatch.reset();
        listCall.reset();
        // Slots hold bus references, so they go first.
        if (bus)
            sd_bus_close_unref(bus);
        bus = nullptr;
    }

    void CMediaSessions::dispatch() {
        if (broken)
            return;
        if (!bus) {
            if (Clock::now() >= retryAt)
                connect();
            return;
        }

        int r = 0;
        while ((r = sd_bus_process(bus, nullptr)) > 0) {}
        if (r < 0) {
            log(std::format("audio: MPRIS bus failed: {}", std::strerror(-r)));
            broken = true;
            return;
        }
        log.clear();
    }

    void CMediaSessions::reset() {
        disconnect();
        broken  = false;
        retryAt = Clock::now() + RETRY_DELAY;
    }

    void CMediaSessions::addPlayer(const std::string& name) {
        auto  player = std::make_unique<SPlayer>();
        auto& p      = *player;
        p.sessions   = this;
        p.name       = name;

        sd_bus_slot* slot = nullptr;
        if (sd_bus_call_method_async(bus, &slot, DBUS_NAME, DBUS_PATH, DBUS_NAME, "GetNameOwner",
                                     onNameOwner, &p, "s", name.c_str()) >= 0)
            p.ownerCall.reset(slot);
        slot = nullptr;
        if (sd_bus_call_method_async(bus, &slot, DBUS_NAME, DBUS_PATH, DBUS_NAME,
                                     "GetConnectionUnixProcessID", onPid, &p, "s",
                                     name.c_str()) >= 0)
            p.pidCall.reset(slot);
        requestProperties(p);

        players.insert_or_assign(name, std::move(player));
        changed();
    }

    void CMediaSessions::removePlayer(const std::string& name) {
        if (players.erase(name) > 0)
            changed();
    }

    void CMediaSessions::requestProperties(SPlayer& player) {
        sd_bus_slot* slot = nullptr;
        if (sd_bus_call_method_async(bus, &slot, player.name.c_str(), MPRIS_PATH,
                                     "org.freedesktop.DBus.Properties", "GetAll", onProperties,
                                     &player, "s", PLAYER_IFACE) >= 0)
            player.propertiesCall.reset(slot);
    }

    int CMediaSessions::onListNames(sd_bus_message* message, void* userdata, sd_bus_error*) {
        auto* self = static_cast<CMediaSessions*>(userdata);
        if (methodFailed(message))
            return 0;
        if (sd_bus_message_enter_container(message, SD_BUS_TYPE_ARRAY, "s") < 0)
            return 0;
        const char* name = nullptr;
        while (sd_bus_message_read(message, "s", &name) > 0) {
            if (name && isPlayerName(name) && !self->players.contains(name))
                self->addPlayer(name);
        }
        return 0;
    }

    int CMediaSessions::onNameOwnerChanged(sd_bus_message* message, void* userdata, sd_bus_error*) {
        auto*       self     = static_cast<CMediaSessions*>(userdata);
        const char* name     = nullptr;
        const char* oldOwner = nullptr;
        const char* newOwner = nullptr;
        if (sd_bus_message_read(message, "sss", &name, &oldOwner, &newOwner) < 0 || !name ||
            !isPlayerName(name))
            return 0;

        self->removePlayer(name);
        if (newOwner && *newOwner)
            self->addPlayer(name);
        return 0;
    }

    int CMediaSessions::onPropertiesChanged(sd_bus_message* message, void* userdata,
                                            sd_bus_error*) {
        auto*       self   = static_cast<CMediaSessions*>(userdata);
        const char* sender = sd_bus_message_get_sender(message);
        const char* iface  = nullptr;
        if (!sender || sd_bus_message_read(message, "s", &iface) < 0)
            return 0;

        for (auto& [name, player] : self->players) {
            if (player->uniqueName != sender)
                continue;

            // Every player sharing the connection sees the same message, so read a fresh copy.
            if (sd_bus_message_rewind(message, true) < 0 || sd_bus_message_skip(message, "s") < 0)
                return 0;
            SPlayerState state;
            if (readPlayerProperties(message, state) < 0)
                return 0;
            if (state.hasStatus)
                player->playing = state.status == "Playing";
            if (state.hasTitle)
                player->title = state.title;
            self->changed();

            const char* invalidated = nullptr;
            if (sd_bus_message_enter_container(message, SD_BUS_TYPE_ARRAY, "s") < 0)
                continue;
            while (sd_bus_message_read(message, "s", &invalidated) > 0) {
                const std::string_view KEY = invalidated ? invalidated : "";
                if (KEY == "PlaybackStatus" || KEY == "Metadata") {
                    self->requestProperties(*player);
                    break;
                }
            }
        }
        return 0;
    }

    int CMediaSessions::onNameOwner(sd_bus_message* message, void* userdata, sd_bus_error*) {
        auto* player = static_cast<SPlayer*>(userdata);
        if (!methodFailed(message))
            readString(message, player->uniqueName);
        return 0;
    }

    int CMediaSessions::onPid(sd_bus_message* message, void* userdata, sd_bus_error*) {
        auto*    player = static_cast<SPlayer*>(userdata);
        uint32_t pid    = 0;
        if (methodFailed(message) || sd_bus_message_read(message, "u", &pid) < 0)
            return 0;
        player->pid = static_cast<Pid>(pid);
        player->sessions->changed();
        return 0;
    }

    int CMediaSessions::onProperties(sd_bus_message* message, void* userdata, sd_bus_error*) {
        auto*        player = static_cast<SPlayer*>(userdata);
        SPlayerState state;
        if (methodFailed(message) || readPlayerProperties(message, state) < 0)
            return 0;
        player->playing = state.hasStatus && state.status == "Playing";
        player->title   = state.title;
        player->sessions->changed();
        return 0;
    }
}
