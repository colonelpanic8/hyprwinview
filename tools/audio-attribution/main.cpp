// Runs the audio evidence adapters and attribution against the live session without loading
// the plugin, and prints how each audible stream is tied to windows.

#include "audio/attribution.hpp"
#include "audio/media_sessions.hpp"
#include "audio/pipewire_streams.hpp"
#include "audio/proc.hpp"

#include <glaze/json.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <iostream>
#include <memory>
#include <poll.h>
#include <print>
#include <string>
#include <vector>

using Audio::Clock;

static void drive(const std::vector<Audio::IEvidenceSource*>& sources, Clock::time_point until) {
    while (Clock::now() < until) {
        std::vector<pollfd> fds;
        auto                wake = until;
        for (auto* source : sources) {
            fds.push_back({.fd = source->fd(), .events = source->events(), .revents = 0});
            if (const auto DEADLINE = source->deadline())
                wake = std::min(wake, *DEADLINE);
        }
        const auto WAIT = std::chrono::ceil<std::chrono::milliseconds>(wake - Clock::now());
        poll(fds.data(), fds.size(), static_cast<int>(std::max<int64_t>(0, WAIT.count())));

        for (std::size_t i = 0; i < sources.size(); ++i) {
            auto*      source   = sources[i];
            const auto DEADLINE = source->deadline();
            if (fds[i].revents == 0 && !(DEADLINE && *DEADLINE <= Clock::now()))
                continue;
            source->dispatch();
            if (source->failed())
                source->reset();
        }
    }
}

static std::string run(const char* command) {
    std::string                                                 output;
    std::unique_ptr<FILE, decltype([](FILE* f) { pclose(f); })> pipe(popen(command, "r"));
    if (!pipe)
        return output;
    std::array<char, 65536> buffer{};
    std::size_t             read = 0;
    while ((read = fread(buffer.data(), 1, buffer.size(), pipe.get())) > 0)
        output.append(buffer.data(), read);
    return output;
}

static std::vector<Audio::SWindow> hyprlandWindows() {
    glz::generic clients;
    const auto   JSON = run("hyprctl clients -j");
    if (glz::read_json(clients, JSON) || !clients.is_array()) {
        std::println(stderr, "cannot read `hyprctl clients -j`");
        return {};
    }

    std::vector<Audio::SWindow> windows;
    for (auto& client : clients.get_array()) {
        auto text = [&](const char* key) {
            const auto* value = client.contains(key) ? client[key].get_if<std::string>() : nullptr;
            return value ? *value : std::string{};
        };
        const auto* pid     = client.contains("pid") ? client["pid"].get_if<double>() : nullptr;
        const auto  ADDRESS = text("address");
        uint64_t    key     = 0;
        if (ADDRESS.starts_with("0x"))
            std::from_chars(ADDRESS.data() + 2, ADDRESS.data() + ADDRESS.size(), key, 16);
        windows.push_back({.key          = key,
                           .pid          = pid ? static_cast<Audio::Pid>(*pid) : 0,
                           .cls          = text("class"),
                           .initialClass = text("initialClass"),
                           .title        = text("title"),
                           .x11Xid       = std::nullopt});
    }
    return windows;
}

static std::string lineage(Audio::Pid pid) {
    std::string result = std::to_string(pid);
    for (int hops = 0; pid > 1 && hops < 8; ++hops) {
        const auto PARENT = Audio::parentPid(pid);
        if (!PARENT)
            break;
        pid = *PARENT;
        result += " <- " + std::to_string(pid);
    }
    return result;
}

int main(int argc, char** argv) {
    double seconds = 2.0;
    if (argc == 3 && std::string_view(argv[1]) == "--seconds")
        seconds = std::stod(argv[2]);
    else if (argc != 1) {
        std::println(stderr, "usage: {} [--seconds N]", argv[0]);
        return 2;
    }

    auto logger = [](std::string_view message) { std::println(stderr, "{}", message); };
    Audio::CPipeWireStreams pipewire(logger);
    Audio::CMediaSessions   mpris(logger);
    drive({&pipewire, &mpris},
          Clock::now() +
              std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds)));

    Audio::SSnapshot snapshot{.streams  = pipewire.streams(),
                              .sessions = mpris.sessions(),
                              .windows  = hyprlandWindows(),
                              .parentOf = Audio::parentPid};

    std::println("windows: {}", snapshot.windows.size());
    std::println("playing MPRIS sessions: {}", snapshot.sessions.size());
    for (const auto& session : snapshot.sessions)
        std::println("  pid {} ({}): \"{}\"", session.pid, lineage(session.pid), session.title);
    std::println("audible streams: {}", snapshot.streams.size());
    for (const auto& stream : snapshot.streams)
        std::println("  node {} pid {} ({}) app \"{}\" binary \"{}\" app_id \"{}\" media \"{}\"",
                     stream.id, stream.pid, lineage(stream.pid), stream.appName, stream.binary,
                     stream.appId, stream.mediaName);

    auto titleOf = [&](uint64_t key) {
        const auto IT = std::ranges::find(snapshot.windows, key, &Audio::SWindow::key);
        return IT == snapshot.windows.end() ? std::string("?") : IT->cls + ": " + IT->title;
    };
    for (const auto& a : Audio::attribute(snapshot, {})) {
        std::println("stream {}: owner {}, {} candidates, rule {}, narrowed by {}, indicator {}",
                     a.streamId, a.ownerVia ? Audio::name(*a.ownerVia) : "none",
                     a.candidates.size(), a.ruleApp.empty() ? "default" : a.ruleApp,
                     a.narrowedBy ? Audio::name(*a.narrowedBy) : "nothing",
                     Audio::name(a.indicator));
        for (const auto KEY : a.windows)
            std::println("  -> 0x{:x} {}", KEY, titleOf(KEY));
    }
}
